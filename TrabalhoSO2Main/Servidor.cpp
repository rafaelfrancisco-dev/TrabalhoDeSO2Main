#include "Servidor.h"

static Jogo jogo;
static mutex mtx;
static bool enviarTodos = false;
static Mensagem globalM;
static Registo registo;

TCHAR szName[] = TEXT("Local\\MyFileMappingObject");

Servidor::Servidor()
{
}

Servidor::~Servidor()
{
}

DWORD WINAPI confMemPartilhada(LPVOID lpvParam) {
	HANDLE hMapFile;
	volatile LPCSTR pBuf;
	MemoryShare* temp;

	hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security
		PAGE_READWRITE,          // read/write access
		0,                       // maximum object size (high-order DWORD)
		BUFSIZE,                // maximum object size (low-order DWORD)
		szName);                 // name of mapping object

	if (hMapFile == NULL)
	{
		_tprintf(TEXT("Could not create file mapping object (%d).\n"),
			GetLastError());
		return 1;
	}

	pBuf = (LPCSTR)MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		BUFSIZE);

	if (pBuf == NULL)
	{
		_tprintf(TEXT("Could not map view of file (%d).\n"),
			GetLastError());

		CloseHandle(hMapFile);

		return 1;
	}

	_tprintf(TEXT("Configurada memoria partilhada\n"));
	jogo.ms.monstrosLigados = 0;
	CopyMemory((PVOID)pBuf, &jogo.ms, sizeof(jogo.ms));
	temp = (MemoryShare *)pBuf;

	HANDLE ghWriteEvent = CreateEvent(
		NULL,               // default security attributes
		FALSE,               // manual-reset event
		FALSE,              // initial state is nonsignaled
		TEXT("WriteEvent")  // object name
		);

	if (ghWriteEvent == NULL)
	{
		printf("CreateEvent failed (%d)\n", GetLastError());
		return 1;
	}

	while (true)
	{
		if (WaitForSingleObject(ghWriteEvent, INFINITE) == WAIT_OBJECT_0)
		{
			_tprintf(TEXT("DEBUG INFO - A mover um monstro, evento\n"));
			jogo.ms = *temp;
		}
	}

	
	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);
}

int Servidor::loop() {
	BOOL   fConnected = FALSE;
	DWORD  dwThreadId = 0;
	HANDLE hPipe = INVALID_HANDLE_VALUE, hPipeGeral = INVALID_HANDLE_VALUE, hThread = NULL, hThread2 = NULL;
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		confMemPartilhada,       // thread function name
		NULL,                  // argument to thread function 
		0,                      // use default creation flags 
		NULL);                 // returns the thread identifier 

	// The main loop creates an instance of the named pipe and 
	// then waits for a client to connect to it. When the client 
	// connects, a thread is created to handle communications 
	// with that client, and this loop is free to wait for the
	// next client connect request. It is an infinite loop.
	for (;;)
	{
		_tprintf(TEXT("\nPipe do Servidor: Thread principal a espera da ligacao de um cliente em %s\n"), lpszPipename);
		hPipe = CreateNamedPipe(
			lpszPipename,             // pipe name 
			PIPE_ACCESS_DUPLEX,       // read/write access 
			PIPE_TYPE_MESSAGE |       // message type pipe 
			PIPE_READMODE_MESSAGE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			BUFSIZE,                  // output buffer size 
			BUFSIZE,                  // input buffer size 
			0,                        // client time-out 
			NULL);                    // default security attribute 

		if (hPipe == INVALID_HANDLE_VALUE)
		{
			_tprintf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
			return -1;
		}

		// Wait for the client to connect; if it succeeds, 
		// the function returns a nonzero value. If the function
		// returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 
		fConnected = ConnectNamedPipe(hPipe, NULL) ?
			TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (fConnected)
		{
			printf("Client connected, creating a processing thread.\n");

			// Create a thread for this client. 
			hThread = CreateThread(
				NULL,              // no security attribute 
				0,                 // default stack size 
				InstanceThread,    // thread proc
				(LPVOID)hPipe,    // thread parameter 
				0,                 // not suspended 
				&dwThreadId);      // returns thread ID 

			if (hThread == NULL)
			{
				_tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
				return -1;
			}
			else CloseHandle(hThread);
		}
		else
			// The client could not connect, so close the pipe. 
			CloseHandle(hPipe);
	}

	return 0;
}

DWORD WINAPI Servidor::InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
	HANDLE hHeap = GetProcessHeap();
	Mensagem pchRequest, pchReply, pchGlobal;
	strcpy(pchReply.msg, "empty");
	strcpy(pchGlobal.msg, "empty");

	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
	BOOL fSuccess = FALSE;
	HANDLE hPipe = NULL;

	// Do some extra error checking since the app will keep running even if this
	// thread fails.
	if (lpvParam == NULL)
	{
		printf("\nERROR - Pipe Server Failure:\n");
		printf("   InstanceThread got an unexpected NULL value in lpvParam.\n");
		printf("   InstanceThread exitting.\n");
		//if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
		return (DWORD)-1;
	}

	// Print verbose messages. In production code, this should be for debugging only.
	printf("InstanceThread created, receiving and processing messages.\n");

	// The thread's parameter is a handle to a pipe object instance. 
	hPipe = (HANDLE)lpvParam;

	// Loop until done reading
	while (1)
	{
		// Read client requests from the pipe. This simplistic code only allows messages
		// up to BUFSIZE characters in length.
		fSuccess = ReadFile(
			hPipe,        // handle to pipe 
			&pchRequest,    // buffer to receive data 
			sizeof(Mensagem), // size of buffer 
			&cbBytesRead, // number of bytes read 
			NULL);        // not overlapped I/O 

		if (!fSuccess || cbBytesRead == 0)
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
			{
				_tprintf(TEXT("InstanceThread: client disconnected.\n"), GetLastError());
			}
			else
			{
				_tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
			}
			break;
		}

		// Process the incoming message.
		pchReply = GetAnswerToRequest(pchRequest, pchReply, &cbReplyBytes);

		// Write the reply to the pipe. 
		fSuccess = WriteFile(
			hPipe,        // handle to pipe 
			&pchReply,     // buffer to write from 
			cbReplyBytes, // number of bytes to write 
			&cbWritten,   // number of bytes written 
			NULL);        // not overlapped I/O 

		if (!fSuccess || cbReplyBytes != cbWritten)
		{
			_tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
			break;
		}
	}

	// Flush the pipe to allow the client to read the pipe's contents 
	// before disconnecting. Then disconnect the pipe, and close the 
	// handle to this pipe instance. 

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	//HeapFree(hHeap, 0, pchRequest);
	//HeapFree(hHeap, 0, pchReply);

	printf("InstanceThread exitting.\n");
	return 1;
}

Mensagem Servidor::GetAnswerToRequest(Mensagem pchRequest, Mensagem pchReply, LPDWORD pchBytes)
// This routine is a simple function to print the client request to the console
// and populate the reply buffer with a default data string. This is where you
// would put the actual client request processing code that runs in the context
// of an instance thread. Keep in mind the main thread will continue to wait for
// and receive other client connections while the instance thread is working.
{
	printf("Client Request String:\"%s\"\n", pchRequest.msg);
	string entrada(pchRequest.msg);
	string temp = "Comando n�o reconhecido";

	istringstream iss(entrada);
	vector<string> tokens{ istream_iterator<string>{iss},               //Separador super elegante pls http://stackoverflow.com/questions/236129/split-a-string-in-c
		istream_iterator<string>{} };

	if (tokens.size() == 0)
		tokens.push_back("empty string");

	if (tokens[0] == "login") {
		if (jogo.getEstado() == jogo.A_PROCURAR_CLIENTES) {
			int decider = 1;

			for (size_t i = 0; i < jogo.jogadores.size(); i++)
			{
				if (jogo.jogadores[i].getName() == tokens[1])
				{
					temp = "Jogador j� existente, tente de novo";
					decider = 0;
				}
			}
			if (decider == 1)
			{
				if (registo.lerChavePassos(tokens[1]) == -1)
				{
					temp = "Jogador novo, bem vindo !";
				}
				else
				{
					temp = "Bem vindo de volta, j� deu ";
					temp.append(to_string(registo.lerChavePassos(tokens[1])));
					temp.append(" passos");
				}
				Jogador* temp_jogar = new Jogador(tokens[1], pchRequest.pid, 100);              //construtor de jogador - 100 hp
				jogo.adicionarJogador(*temp_jogar);
			}
		}
		else {
			temp = "O servidor j� tem um jogo a decorrer ou a iniciar";
		}
	}
	if (tokens[0] == "jogar") {
		bool encontrado = false;

		for (int i = 0; i < jogo.jogadores.size(); i++) {
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
				encontrado = true;
		}

		if (encontrado) {
			temp = "A iniciar jogo...";
			strcpy_s(globalM.msg, "EM FASE DE JOGO!!!");
			for (int i = 0; i < jogo.jogadores.size(); i++) {
				globalM.pidsEnviar[i] = jogo.jogadores[i].getPid();
			}
			enviarTodos = true;
			jogo.setEstado(jogo.A_JOGAR);
		}
		else {
			temp = "Jogador n�o logado";
		}
	}
	if (tokens[0] == "terminar") {
		temp = "A terminar jogo...";
		strcpy_s(globalM.msg, "EM FASE DE SAIR!!!");
		enviarTodos = true;
	}
	if (tokens[0] == "logout")
	{
		temp = "A sair...";
		for (size_t i = 0; i < jogo.jogadores.size(); i++)
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
			{
				jogo.jogadores.erase(jogo.jogadores.begin() + i);
			}
		}
	}
	if (tokens[0] == "actualizar") {
		temp = "A actualizar mapa";
		pchReply.mapa = jogo.getCMap();
	}

	if (tokens[0] == "esquerda") 
	{ //por dentro duma fun�ao jogando() quando existir um jogo a decorrer/iniciar
		//temp = "jogador andou pra esquerda"; //nao aparece
		for (int i = 0; i < jogo.jogadores.size(); i++) 
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
			{
				int x= jogo.jogadores[i].getX();
				x-=1;
				jogo.jogadores[i].setPos(x, jogo.jogadores[i].getY());
				registo.abreChave(jogo.jogadores[i].getName(), registo.lerChavePassos(jogo.jogadores[i].getName()) + 1, 0);
			}
		}
		//actualiza
		pchReply.mapa = jogo.getCMap();
	}
	if (tokens[0] == "direita")
	{ 
		for (int i = 0; i < jogo.jogadores.size(); i++)
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
			{
				int x = jogo.jogadores[i].getX();
				x+= 1;
				jogo.jogadores[i].setPos(x, jogo.jogadores[i].getY());
				registo.abreChave(jogo.jogadores[i].getName(), registo.lerChavePassos(jogo.jogadores[i].getName()) + 1, 0);
			}
		}
		//actualiza
		pchReply.mapa = jogo.getCMap();
	}
	if (tokens[0] == "baixo")
	{
		for (int i = 0; i < jogo.jogadores.size(); i++)
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
			{
				int y = jogo.jogadores[i].getY();
				y += 1;
				jogo.jogadores[i].setPos(jogo.jogadores[i].getX(), y);
				registo.abreChave(jogo.jogadores[i].getName(), registo.lerChavePassos(jogo.jogadores[i].getName()) + 1, 0);
			}
		}
		//actualiza
		pchReply.mapa = jogo.getCMap();
	}
	if (tokens[0] == "cima")
	{ 
		for (int i = 0; i < jogo.jogadores.size(); i++)
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
			{
				int y = jogo.jogadores[i].getY();
				y -= 1;
				jogo.jogadores[i].setPos(jogo.jogadores[i].getX(), y);
				registo.abreChave(jogo.jogadores[i].getName(), registo.lerChavePassos(jogo.jogadores[i].getName()) + 1, 0);
			}
		}
		//actualiza
		pchReply.mapa = jogo.getCMap();
	}
	if (tokens[0] == "pedra") 
	{
		for (int i = 0; i < jogo.jogadores.size(); i++)
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
				jogo.jogadores[i].togglePedra();
		}
	}
	if (tokens[0] == "machado")
	{
		for (int i = 0; i < jogo.jogadores.size(); i++)
		{
			if (jogo.jogadores[i].getPid() == pchRequest.pid)
				jogo.jogadores[i].toggleMachado();
		}		
	}
	if (tokens[0] == "atacar")
	{

		for (int i = 0; i < jogo.jogadores.size(); i++)
		{
			if ((jogo.jogadores[i].getPid() == pchRequest.pid)){
				Jogador temp=jogo.verificaVizinhos(jogo.jogadores[i]);
				if (temp.getHP()>0)//encontra alguem
				{ 
					jogo.jogadores[i].atacar(jogo.verificaVizinhos(jogo.jogadores[i]));
					int aux=jogo.jogadores[i].getHP();
					if (aux<=0) 
					{
						for (size_t i = 0; i < jogo.jogadores.size(); i++)
						{
							if (jogo.jogadores[i].getPid() == pchRequest.pid)
							{
								jogo.jogadores.erase(jogo.jogadores.begin() + i);
							}
						}
					}
				pchReply.vida=aux;
				}
			}
		}
	}

	for (size_t i = 0; i < jogo.jogadores.size(); i++)
	{
		if (pchRequest.pid == jogo.jogadores[i].getPid())
		{
			pchReply.vida = jogo.jogadores[i].getHP();
		}
	}

	strcpy_s(pchReply.msg, temp.c_str());
	*pchBytes = sizeof(pchReply);

	return pchReply;
}