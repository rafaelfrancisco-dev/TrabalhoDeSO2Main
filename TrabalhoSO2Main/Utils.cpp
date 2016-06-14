#include "Utils.h"

Labirinto::Labirinto()
{
	gerarLab();
}

void Labirinto::gerarLab()
{
	for (size_t i = 0; i < TAM_LABIRINTO; i++)
	{
		for (size_t j = 0; j < TAM_LABIRINTO; j++)
		{
			if (i == 0 && j == 0)
			{
				mapa[i][j] = "*";
			}
			if (i == TAM_LABIRINTO - 1 && j == 0)
			{
				mapa[i][j] = "*";
			}
			if (i == 0 && j == TAM_LABIRINTO - 1)
			{
				mapa[i][j] = "*";
			}
			if (i == TAM_LABIRINTO - 1 && j == TAM_LABIRINTO - 1)
			{
				mapa[i][j] = "*";
			}
			gerado = rand() % 100;
			if (gerado <= 5)
			{
				gerado = rand() % 100;
				if (gerado <= 50) mapa[i][j] = "P";
				else if (gerado>50 && gerado <= 80) mapa[i][j] = "V";
				else if (gerado>81 && gerado <= 90) mapa[i][j] = "O";
				else if (gerado>91 && gerado <= 99) mapa[i][j] = "C";
				else if (gerado = 100) mapa[i][j] = "M";
			}

			mapa[i][j] = "_";
		}
	}
}

Mapa Jogo::getCMap() { //converte string C++ pra C 
	Mapa m;
	char temp[TAM_LABIRINTO][TAM_LABIRINTO * 3];

	for (size_t i = 0; i < jogadores.size(); i++)
	{
		Jogador temp = jogadores[i];
		lab->mapa[temp.getX()][temp.getY()] = "J";
	}

	for (size_t i = 0; i < TAM_LABIRINTO; i++)
	{
		string tempS;
		for (size_t j = 0; j < TAM_LABIRINTO; j++)
		{
			tempS.append(lab->mapa[i][j] + " ");
		}
		strcpy_s(temp[i], tempS.c_str());
	}

	for (size_t i = 0; i < TAM_LABIRINTO; i++)
	{
		strcpy_s(m.mapaEnv[i], temp[i]);
	}

	return m;
}

Jogo::Jogo()
{
	lab = new Labirinto();
}

Jogo::~Jogo()
{
}

void Jogo::adicionarJogador(Jogador j)
{
	srand(time(NULL));
	int x = rand() % TAM_LABIRINTO;
	int y = rand() % TAM_LABIRINTO;

	j.setPos(x, y);
	jogadores.push_back(j);
}

void Jogo::setEstado(int e)
{
	estado = e;
}

int Jogo::getEstado()
{
	return estado;
}

Jogador::Jogador(string nome, int p, int vida)
{
	name = nome;
	pid = p;
	hp = vida;
	
}

Jogador::~Jogador()
{
}

Jogador::Jogador()
{
}

int Jogador::getPid()
{
	return pid;
}

void Jogador::setPos(int x, int y)
{
	posX = x;
	posY = y;
}

int Jogador::getX()
{
	return posX;
}

int Jogador::getY()
{
	return posY;
}

bool Jogador::getPedra(){ return pedra;}
bool Jogador::getMachado(){return machado;}

void Jogador::setPedra(bool p){pedra=p;}
void Jogador::setMachado(bool m) { machado = m; }

void Jogador::togglePedra()
{
	if(getPedra()==false) setPedra(true);
	else setPedra(false);

}

void Jogador::toggleMachado()
{
	if (getMachado() == false) setMachado(true);
	else setMachado(false);

}

int Jogador::atacar()
{
	//fucking important
	//FALTA verificar quem ou o que est� na mesma posi�ao q ele  !!!
	//
	if (getPedra()==false && getMachado()==false)
		{
			//remover 1 hp ao outro jogador/monstro
		}	
		else if ((getPedra() == true && getMachado() == false))
		{
			//~remover 2hp do outro jogador/monstro
		}
		else if( (getPedra() == true && getMachado() == true) || (getPedra() == false && getMachado() == true))
		{
			//machado prevalece independentemente de pedra on ou off - tira 5 hp 
		}

} 

int Jogo::quemEstaAqui(Jogador j)
{
	int x=j.getX();
	int y=j.getY();

	int x1,y1;

	for (size_t i = 0; i < jogadores.size(); i++)
	{
		Jogador temp = jogadores[i];
		x1=temp.getX();
		y1=temp.getY();
		if (x==x1 && y==y1)
			return temp.getPid();
	}
}