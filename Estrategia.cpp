/*
 * Estrategia.cpp
 *
 *  Created on: Sep 7, 2009
 *      Author: rene
 */

#include "TiposClasses.h"
#include "Auxiliares.h"
#include "Controle.h"
#include "Serial.h"

extern int maxPixelsPorSegundo; // velocidade maxima que um robo pode alcancar
extern Estado estado[NUM_ROBOS_TIME * 2 + 1],
		estadoAnt[NUM_ROBOS_TIME * 2 + 1], estadoPrev[NUM_ROBOS_TIME * 2 + 1];
extern Lado nossoLado;
extern bool emJogo;
extern bool tiroMeta;
extern bool emPenalidade;
extern bool emPosiciona;
extern bool emInicio;


extern Objetivo objetivoRobo[NUM_ROBOS_TIME];

enum EstadoAtacante {
	POSICIONA, ATACA
} estadoAtacante;
int trocarVolanteAtacante = 0;

CmdEnviado cmdEnviado[10][NUM_ROBOS_TIME]; //comando enviado aos robos

extern int indGoleiro;
extern int indVolante;
extern int indAtacante;

void IniciaEstrategia(void) {
	int i, j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 10; j++) {
			cmdEnviado[j][i].dir = 0;
			cmdEnviado[j][i].esq = 0;
		}
	}
}

void SAI(char *s) {
	FILE *fp = fopen("debug.txt", "at");
	fprintf(fp, s);
	fflush(fp);
	fclose(fp);
}

void calculaPrevisao(void) {
#ifdef SEM_PREVISAO
	for (int i = 0; i < 7; i++) {
		estadoPrev[i] = estado[i];
	}
#else
	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		float x, y, dx, dy, ang, v_cm, v_unid, ve, vd, de, dd, aux,
		difVelRodas, me, md;
		ang = estado[i].angulo;
		x = estado[i].x;
		y = estado[i].y;
		dx = estado[i].dx;
		dy = estado[i].dy;
		v_cm = sqrt(dx * dx + dy * dy); //Calcula a velocidade do Robo em cm
		//Calcula as velocidade dos motores em unidades
		v_unid = v_cm * FATOR_VEL_ROBO_UNID_POR_CM;
		if (v_unid <= VEL_MAXIMA_ROBO_UNID) {
			if (abs(difAngMenos180a180(atang2(dy, dx), ang)) > 90) {
				//Robo andando de re';
				v_unid = -v_unid; //frente (etiquetas) para um lado, deslocamento para o outro
			}

			if (!emJogo)
			printf("(%3.0f, %3d)", v_unid, cmdEnviado[3][i].vel);

			aux = difAngMenos180a180(ang, estadoAnt[i].angulo);
			// a cada FATOR_ANGULO_DIF_RODAS_UNID_VEL graus significa uma diferenca entre rodas de uma unidade de velocidade
			difVelRodas = abs(aux / FATOR_ANGULO_DIF_RODAS_UNID_VEL);

			if (difVelRodas > VEL_MAXIMA_ROBO_UNID * 2)
			difVelRodas = VEL_MAXIMA_ROBO_UNID * 2;
			if (aux > 0) { //Robo virando para dir.
				ve = v_unid - difVelRodas / 2;
				vd = v_unid + difVelRodas / 2;
				if (ve < -VEL_MAXIMA_ROBO_UNID) {
					ve = -VEL_MAXIMA_ROBO_UNID;
					vd = ve + difVelRodas;
				} else if (vd > VEL_MAXIMA_ROBO_UNID) {
					vd = VEL_MAXIMA_ROBO_UNID;
					ve = vd - difVelRodas;
				}
			} else { //Robo virando para esq
				ve = v_unid + difVelRodas / 2;
				vd = v_unid - difVelRodas / 2;
				if (ve > VEL_MAXIMA_ROBO_UNID) {
					ve = VEL_MAXIMA_ROBO_UNID;
					vd = ve - difVelRodas;
				} else if (vd < -VEL_MAXIMA_ROBO_UNID) {
					vd = -VEL_MAXIMA_ROBO_UNID;
					ve = vd + difVelRodas;
				}
			}
		} else { //robo muito rapido, possivelmente erro
			ve = (cmdEnviado[0][i].esq + cmdEnviado[1][i].esq
					+ cmdEnviado[2][i].esq + cmdEnviado[3][i].esq) / 4;
			vd = (cmdEnviado[0][i].dir + cmdEnviado[1][i].dir
					+ cmdEnviado[2][i].dir + cmdEnviado[3][i].dir) / 4;
		}

		md = (cmdEnviado[0][i].dir + cmdEnviado[1][i].dir
				+ cmdEnviado[2][i].dir + cmdEnviado[3][i].dir) / 4;//+Cmd[5][i].d+Cmd[6][i].d)/6;//+Cmd[7][i].d+Cmd[8][i].d)/8;
		me = (cmdEnviado[0][i].esq + cmdEnviado[1][i].esq
				+ cmdEnviado[2][i].esq + cmdEnviado[3][i].esq) / 4;//+Cmd[5][i].e+Cmd[6][i].e)/6;//+Cmd[7][i].e+Cmd[8][i].e)/8;

		de = ve - me;
		dd = vd - md;

		// Calcula as velocidades previstas de cada roda
		if (de) { // Se nao esta' em velocidade constante
			if (de > 0) { // Velocidade aumentando?
				ve += 1; // Sim, velocidade prevista deve ser maior que a observada
			} else {
				ve -= 1; // Nao, velocidade prevista deve ser menor que a observada
			}
		}
		if (dd) {
			if (dd > 0) {
				vd += 1;
			} else {
				vd -= 1;
			}
		}
		// Calcula o angulo previsto a partir das velocidades previstas
		ang += (vd - ve) * FATOR_ANGULO_DIF_RODAS_UNID_VEL;
		while (ang >= 360)
		ang -= 360;
		while (ang < 0) {
			ang += 360;
		}

		// Calcula o deslocamento previsto para o robo
		v_unid = (ve + vd) / 2; //media
		v_cm = v_unid / FATOR_VEL_ROBO_UNID_POR_CM;
		dx = v_cm * coss(ang);
		dy = v_cm * seno(ang);
		x += dx;
		y += dy;
		if (x > TAM_X_CAMPO - TAM_X_DO_GOL) { // Verifica limite do campo
			x = TAM_X_CAMPO - TAM_X_DO_GOL;
		} else if (x < TAM_X_DO_GOL) { // verifica limites do nosso gol
			if (y < TAM_Y_DO_GOL && y > TAM_Y_CAMPO - TAM_Y_DO_GOL) {
				x = TAM_X_DO_GOL;
			} else { //esta ao lado do nosso gol
				if (x < TAM_Y_DO_GOL) { //esta dentro do nosso gol
					x = estado[i].x; // para evitar que fique batendo nas laterais, usa as coordenadas atuais observadas na imagem
					y = estado[i].y;
				}
			}
		}
		if (y > TAM_Y_CAMPO - TAM_ROBO / 2) {
			y = TAM_Y_CAMPO - TAM_ROBO / 2;
		} else if (y < TAM_ROBO / 2) {
			y = TAM_ROBO / 2;
		}

		cmdEnviado[0][i].vel = v_unid / 2; //Calcula media

		estadoPrev[i].angulo = ang;
		estadoPrev[i].x = x;
		estadoPrev[i].y = y;
		estadoPrev[i].dx = dx;
		estadoPrev[i].dy = dy;
	}

#endif

	for (int i = NUM_ROBOS_TIME; i < NUM_ROBOS_TIME * 2 + 1; i++) {
		float x, y;
		float dx, dy;
		//media do angulo
		dx = estado[i].dx * 0.75 + estadoAnt[i].dx * 0.25;
		dy = estado[i].dy * 0.75 + estadoAnt[i].dy * 0.25;
		estadoPrev[i].angulo = atang2(dy, dx) * 0.5 + estado[i].angulo * 0.3
				+ estadoAnt[i].angulo * 0.2;
		estadoPrev[i].dx = dx;
		estadoPrev[i].dy = dy;

		x = estado[i].x + dx * N_IMAGENS_ATRASO;
		y = estado[i].y + dy * N_IMAGENS_ATRASO;
		if (x > TAM_X_CAMPO - TAM_X_DO_GOL) {
			x = TAM_X_CAMPO - TAM_X_DO_GOL;
		} else if (x < TAM_X_DO_GOL) {
			x = TAM_X_DO_GOL;
		}
		if (y > TAM_Y_CAMPO - TAM_ROBO) {
			y = TAM_Y_CAMPO - TAM_ROBO;
		} else if (y < TAM_ROBO) {
			y = TAM_ROBO;
		}
		estadoPrev[i].x = x;
		estadoPrev[i].y = y;
	}

	/*// Verifica se algum robo vai chocar com a bola

	 // Trajetoria da bola (posicao atual e posicao prevista)
	 xb1=estado[3].X, yb1=estado[3].Y, xb2=estadoPrev[3].X, yb2=estadoPrev[3].Y;

	 dxb=xb2-xb1;
	 dyb=yb2-yb1;
	 if (dxb==0)		// Se dxb e' zero a bola esta com variacao apenas em Y
	 if (dyb>0)		// cb sera' infinito
	 cb=1e8;		// +infinito
	 else if (dyb<0)
	 cb=-1e8;		// -infinito
	 else
	 cb=0;		// Bola esta' parada
	 else
	 cb=(long)dyb/dxb;
	 for (i=0; i<3; i++) {
	 int xr1=estado[i].X,     yr1=estado[i].Y;
	 int	xr2=estadoPrev[i].X, yr2=estadoPrev[i].Y;
	 int dxr=xr2-xr1;
	 int dyr=yr2-yr1;
	 int x, y;
	 char choque;
	 long xf, cr;
	 if (dxr==0)
	 if (dyr>0)
	 cr=1e8;
	 else if (dyr<0)
	 cr=-1e8;
	 else
	 cr=0;
	 else
	 cr=(long)dyr/dxr;
	 // Calcula o cruzamento entre as trajetorias
	 if (cb-cr!=0) {	// traetorias NAO sao paralelas
	 xf=((yr2 - yb2) + (cb*xb2 - cr*xr2)) / (cb - cr);
	 y=ceil(yr2+cr*(xf-xr2));
	 x=ceil(xf);
	 } else {		// traetorias SAO paralelas
	 x=xb2;		// cruzamento na posicao prevista da bola
	 y=yb2;
	 }
	 choque=0;
	 if (xr1<xr2) {
	 choque=xr1-6<=x && x<=xr2+6;
	 } else {
	 choque=xr2-6<=x && x<=xr1+6;
	 }
	 if (yr1<yr2) {
	 choque=yr1-6<=y && y<=yr2+6;
	 } else {
	 choque=yr2-6<=y && y<=yr1+6;
	 }
	 if (choque) {
	 int m;
	 m=dxr*dxr+dyr*dyr;         // Modulo do vetor direcao


	 if (m>=0 && m<255) {
	 m=Sqrt[m];               // Cabe no vetor Sqrt
	 } else if (m>=0 && m<255*4) {
	 m=Sqrt[m/4]*2;          // Força para caber no vetor Sqrt
	 } else
	 m=dxr>dyr?dxr:dyr;       // Desiste e aproxima o módulo para o maior
	 // a partir da posicão prevista do robo e da direçao, calcula a nova posicao da bola
	 if (m!=0) {
	 xb2=xr2+dxr*12/m;           //12 = (3,5cm do centro do robo + 2,1 do centro da bola)*2
	 yb2=yr2+dyr*12/m;
	 }
	 }
	 }

	 //  BolaParada=abs(estadoPrev[3].Dx)<=1 && abs(estadoPrev[3].Dy)<=1;*/
}

//Retorna:	Numero de quadros ate o choque
//		Numero do elemento no caminho (-1 nenhum)
void calculaChoque(int numRobo, bool consideraBola, float *xObjetivo,
		float *yObjetivo, float deslocXObjetivo, float deslocYObjetivo,
		int *NQuadros, int *NumElemChoque) {
	float xRobo, yRobo, dxRobo, dyRobo;
	float deslocXObjetivoRobo, deslocYObjetivoRobo, deslocXAbs, deslocYAbs;
	int N, nx, ny;

	xRobo = estadoPrev[numRobo].x;
	yRobo = estadoPrev[numRobo].y;
	dxRobo = estadoPrev[numRobo].dx;
	dyRobo = estadoPrev[numRobo].dy;

	deslocXAbs = abs(deslocXObjetivoRobo = *xObjetivo - xRobo); //Distancia entre objetivo e robo
	deslocYAbs = abs(deslocYObjetivoRobo = *yObjetivo - yRobo);

	float ddxObjRobo = deslocXObjetivo - dxRobo;
	float ddyObjRobo = deslocYObjetivo - dyRobo;

	if (ddxObjRobo != 0) {
		nx = deslocXObjetivoRobo / ddxObjRobo;
	} else
		nx = 30;
	if (ddyObjRobo != 0) {
		ny = deslocYObjetivoRobo / ddyObjRobo;
	} else
		ny = 30;

	N = max(nx, ny);
	if (N < 0 || N > 30) { //Se mais que 30 quadros (1s), desconsidera.
		N = 30; //Limita em um segundo
	}
	*NQuadros = N;

	if (deslocYAbs + deslocXAbs <= TAM_ROBO) { //Se distancia e' menor que TAM_ROBO, nao cabe elem. entre robo e objetivo
		*NumElemChoque = -1; //Nao ocorrera choque
	} else {
		float a = deslocYObjetivoRobo / deslocXObjetivoRobo; // reta entre o robo e o objetivo
		float b = yRobo - a * xRobo;
		for (int i = 0; i < NUM_ROBOS_TIME * 2 + 1; i++) {
			if (i != numRobo && (consideraBola || i != IND_BOLA)) {
				float xi, yi, dxi, dyi;
				xi = estadoPrev[i].x;
				yi = estadoPrev[i].y;
				dxi = estadoPrev[i].dx;
				dyi = estadoPrev[i].dy;
				int nAteObst = (yi - a * xi - b) / (a * dxi - dyi);
				if (nAteObst > 0) { // se nAteObst > 0, o encontro (choque) pode ocorrer
					if (nAteObst < *NQuadros) { // nAteObst < *NQuadros, o encontro (choque) pode ocorrer (e vai retornar o cheque mais proximo do Robo
						*NumElemChoque = i;
						*NQuadros = nAteObst;
						float xRoboChoque = xRobo + dxRobo * (nAteObst - 1); // -1 pois sabendo um pouco antes fica melhor para desviar
						float yRoboChoque = yRobo + dyRobo * (nAteObst - 1);
						float xiChoque = xi + dxi * (nAteObst - 1); // -1 pois sabendo um pouco antes fica melhor para desviar
						float yiChoque = yi + dyi * (nAteObst - 1);
						if (xRoboChoque > xiChoque) {
							xRoboChoque += TAM_ROBO;
							if (xRoboChoque > TAM_X_CAMPO - TAM_X_DO_GOL
									- TAM_ROBO)
								xRoboChoque -= TAM_ROBO * 3; // se nao da para passar por um lado, desvia bastante pelo outro
						} else {
							xRoboChoque -= TAM_ROBO;
							if (xRoboChoque < TAM_ROBO)
								xRoboChoque += TAM_ROBO * 3;
						}
						if (yRoboChoque > yiChoque) {
							if (yRoboChoque > TAM_Y_CAMPO - TAM_ROBO)
								yRoboChoque -= TAM_ROBO * 3;
							yRoboChoque += TAM_ROBO;
						} else {
							yRoboChoque -= TAM_ROBO;
							if (yRoboChoque < TAM_ROBO)
								yRoboChoque += TAM_ROBO * 3;
						}

						*xObjetivo = yRoboChoque;
						*yObjetivo = yRoboChoque;
					}
				}
			}
		}
	}
}

bool existeAdversarioProximo(float x, float y, float dist) {
	float quadDist = dist * dist;
	for (int i = IND_BOLA + 1; i < NUM_ROBOS_TIME * 2 + 1; i++) {
		float dx = x - estadoPrev[i].x;
		float dobroDx = dx * dx;
		float dy = y - estadoPrev[i].y;
		float dobroDy = dy * dy;
		if (dobroDx + dobroDy < quadDist) {
			return true;
		}
	}
	return false;
}

//==================== G O L E I R O =========================
void EstrGoleiro(void) {
	float xObjetivo, yObjetivo, xGoleiro, yGoleiro, dxGoleiro, dyGoleiro,
			xBola, yBola, dxBola, dyBola, N, yBolaQuandoChegarNoGol;
	xGoleiro = estadoPrev[indGoleiro].x;
	yGoleiro = estadoPrev[indGoleiro].y;
	dxGoleiro = estadoPrev[indGoleiro].dx;
	dyGoleiro = estadoPrev[indGoleiro].dy;
	xBola = estadoPrev[IND_BOLA].x;
	yBola = estadoPrev[IND_BOLA].y;
	dxBola = estadoPrev[IND_BOLA].dx;
	dyBola = estadoPrev[IND_BOLA].dy;

	//Calcula o deslocamento de limite dependendo do angulo
	float d = abs((((int) estadoPrev[indGoleiro].angulo + 45) % 90) - 45) * 2
			/ 45; // no angulos 45, 135, 225 e 315 considera o robo maior 2 cm
	// a linha acima serve para dar mais espaco ao robo quando girando

	if (tiroMeta) {
		xObjetivo = CENTRO_X_GOL;
		yObjetivo = CENTRO_Y_GOL;
		if (xGoleiro > TAM_X_DO_GOL * 3)
			tiroMeta = false;
	} else {
		if (emPenalidade) {
			xObjetivo = TAM_X_DO_GOL + d;
		} else {
			xObjetivo = POS_X_GOLEIRO + d; //Linha do goleiro com 1,5cm da linha de fundo (medido)
		}

		if (dxBola > 2) {
			emPenalidade = false;
		}

		if (xBola < TAM_X_CAMPO * 0.5500 ) {// Bola antes da metade do campo
			if (abs(dxBola) > 1) { // Bola em movimento
				N = (POS_X_GOLEIRO - xBola) / dxBola;
				if (N > 0) {
					yBolaQuandoChegarNoGol = N * dyBola + yBola; // Calcula y da bola quando chegar no gol
					if (yBolaQuandoChegarNoGol < (TAM_Y_CAMPO - TAM_Y_DO_GOL)
							/ 2 - TAM_ROBO) { // Bola indo para linha de fundo
						yObjetivo = (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 - TAM_ROBO;
					} else if (yBolaQuandoChegarNoGol > TAM_Y_CAMPO
							- (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 + TAM_ROBO) {
						yObjetivo = TAM_Y_CAMPO - (TAM_Y_CAMPO - TAM_Y_DO_GOL)
								/ 2 + TAM_ROBO;
					} else {
						yObjetivo = yBolaQuandoChegarNoGol; // Posiciona o robo alinhado com a chegada da bola
					}
				} else { // a bola esta se afastando do gol
					if (xBola > TAM_X_CAMPO * 0.25 ) { // Bola 80-10cm do gol
						yObjetivo = yGoleiro;
					} else {
						if (yBola < (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 - TAM_ROBO) {
							yObjetivo = (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 - TAM_ROBO;
						} else if (yBola > TAM_Y_CAMPO - (TAM_Y_CAMPO
								- TAM_Y_DO_GOL) / 2 + TAM_ROBO) {
							yObjetivo = TAM_Y_CAMPO - (TAM_Y_CAMPO
									- TAM_Y_DO_GOL) / 2 + TAM_ROBO;
						} else {
							yObjetivo = yBola; // Robo acompanha a posicao y da bola
						}
					}
				}
			} else { // bola parada
				if (xBola > TAM_X_CAMPO * 0.25 ) {
					yObjetivo = TAM_Y_CAMPO / 2; // Robo no centro do gol
				} else {
					if (yBola < (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 - TAM_ROBO*2) {
						yObjetivo = (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 - TAM_ROBO*2;
					} else if (yBola > TAM_Y_CAMPO - (TAM_Y_CAMPO
							- TAM_Y_DO_GOL) / 2 + TAM_ROBO*2) {
						yObjetivo = TAM_Y_CAMPO - (TAM_Y_CAMPO - TAM_Y_DO_GOL)
								/ 2 + TAM_ROBO*2;
					} else {
						yObjetivo = yBola; // Robo acompanha a posicao y da bola
					}
				}
			}
		} else {
			yObjetivo = TAM_Y_CAMPO / 2;
		}
	}
	if (xGoleiro < POS_X_GOLEIRO + d && (yGoleiro
			< (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 || yGoleiro > TAM_Y_CAMPO
			- (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2)) {// Fora do gol
		if (abs(dxGoleiro) <= 1 && abs(dyGoleiro) <= 1) { // Esta parado
			xObjetivo = POS_X_GOLEIRO + d; // Afasta robo da parede
		}
	}
	objetivoRobo[indGoleiro].x = xObjetivo;
	objetivoRobo[indGoleiro].y = yObjetivo;
	objetivoRobo[indGoleiro].angulo = 90; // graus
	objetivoRobo[indGoleiro].vel = 0;
	/*
	 objetivoRobo[indGoleiro].x = 20;
	 objetivoRobo[indGoleiro].y = 25;
	 objetivoRobo[indGoleiro].angulo = 90; // graus
	 objetivoRobo[indGoleiro].vel = 0;*/
}

//==================== V O L A N T E =========================
void estrVolante(int indVolante) {
	int xObjetivo, yObjetivo, angRobo, xRobo, yRobo, dxRobo, dyRobo, xBola,
			yBola, dxBola, dyBola, xAtacante, yAtacante, velObjetivo,
			angObjetivo;
	float yAlinhamentoRoboBolaGol;
	angRobo = estadoPrev[indVolante].angulo;
	xRobo = estadoPrev[indVolante].x;
	yRobo = estadoPrev[indVolante].y;
	dxRobo = estadoPrev[indVolante].dx;
	dyRobo = estadoPrev[indVolante].dy;
	xBola = estadoPrev[IND_BOLA].x;
	yBola = estadoPrev[IND_BOLA].y;
	dxBola = estadoPrev[IND_BOLA].dx;
	dyBola = estadoPrev[IND_BOLA].dy;
	xAtacante = estadoPrev[indAtacante].x;
	yAtacante = estadoPrev[indAtacante].y;

	//Calcula o deslocamento de limite dependendo do angulo
	float d = abs((((int) estadoPrev[indVolante].angulo + 45) % 90) - 45) * 2
	/ 45; // nos angulos 45, 135, 225 e 315 considera o robo maior 2 cm
	// a linha acima serve para dar mais espaco ao robo quando girando

	velObjetivo = 0;
	angObjetivo = 90;
	xObjetivo = xRobo; // Por padrao mantem o robo na linha atual
	yObjetivo = yBola; // Por padrao fica alinhado com a bola
	if (xRobo - xBola > TAM_ROBO) { //Se bola esta atras do robo (passou a defesa)
		if (abs(yBola - yAtacante)< TAM_ROBO)//robo e bola alinhado, fica parado em X
			xObjetivo = xRobo;
		else
			xObjetivo = xBola + TAM_ROBO * 2;
		velObjetivo = 0;
		if (dxBola <= 0) { //Se bola parada ou indo para o gol
			if (xBola - xAtacante < TAM_ROBO && abs(yBola - yAtacante)
					< TAM_ROBO / 2) { // se tem atacante posicionado para empurrar a bola, entao retira volante do caminho
				if (yRobo < yBola) {
					yObjetivo = yBola - TAM_ROBO; //Coloca o robo para a direita da bola
					if (yObjetivo < TAM_ROBO / 2 + d) { // se por baixo nao cabe (impedido pela lateral), vai pelo outro lado
						yObjetivo = yBola + TAM_ROBO;
					}
				} else {
					yObjetivo = yBola + TAM_ROBO; //Coloca o robo para a esquerda da bola
					if (yObjetivo > TAM_Y_CAMPO - (TAM_ROBO / 2 + d)) { // se por baixo nao cabe (impedido pela lateral), vai pelo outro lado
						yObjetivo = yBola - TAM_ROBO;
					}
				}
			}
		} else { //Se a bola esta saindo da defesa (indo p/ ataque)
			if (xObjetivo < TAM_X_DO_GOL + TAM_X_AREA +TAM_ROBO/2)	// se o robo está na área de defesa
				xObjetivo = TAM_X_DO_GOL + TAM_X_AREA +TAM_ROBO/2;
			if (!existeAdversarioProximo(xBola + TAM_ROBO, yBola, TAM_ROBO)) {
				if (xRobo - xBola > TAM_ROBO * 2 && abs(yRobo - yBola)
						< TAM_ROBO) { // Se o robo esta na direcao da bola e proximo a ela, retira o robo para deixar a bola passar
					if (yRobo < yBola) {
						yObjetivo = yBola - TAM_ROBO; //Coloca o robo para a direita da bola
						if (yObjetivo < TAM_ROBO / 2 + d) { // se por baixo nao cabe (impedido pela lateral), vai pelo outro lado
							yObjetivo = yBola + TAM_ROBO;
						}
					} else {
						yObjetivo = yBola + TAM_ROBO;
						if (yObjetivo > TAM_Y_CAMPO - (TAM_ROBO / 2 + d)) { // se por baixo nao cabe (impedido pela lateral), vai pelo outro lado
							yObjetivo = yBola - TAM_ROBO;
						}
					}
				}
			} else { // Existe adversario perigoso mas existe atacante posicionado, entao retira volante para permitir passagem bola/atacante
				if (xBola - xAtacante < TAM_ROBO && abs(yBola - yAtacante)
						< TAM_ROBO / 2) {
					if (yRobo < yBola) {
						yObjetivo = yBola - TAM_ROBO; //Coloca o robo para a direita da bola
						if (yObjetivo < TAM_ROBO / 2 + d) { // se por baixo nao cabe (impedido pela lateral), vai pelo outro lado
							yObjetivo = yBola + TAM_ROBO;
						}
					} else {
						yObjetivo = yBola + TAM_ROBO; //Coloca o robo para a esquerda da bola
						if (yObjetivo > TAM_Y_CAMPO - (TAM_ROBO / 2 + d)) { // se por baixo nao cabe (impedido pela lateral), vai pelo outro lado
							yObjetivo = yBola - TAM_ROBO;
						}
					}
				}
			}
		}
	} else if (xBola < xRobo + TAM_ROBO / 2) { //Se bola esta na mesma linha do robo (ninguem esta a frente)
		if (dxBola <= 0) { //Se bola parada ou indo para o gol
			if (abs(yBola - yRobo) > TAM_ROBO * 2) { //Robo nao esta na direcao da bola, evita robo voltar para a bola e empurra-la para o gol fazendo contral
				xObjetivo = xBola - TAM_ROBO * 1.5; //Reposiciona na linha de defesa, tentando recolocar robo entre a bola e o nosso gol
			} else {
				xObjetivo = xBola + TAM_ROBO * 1.5; //Mantem a 1,5 robos a frente da bola
				//Verificar outros robos e tentar se posicionar antes da bola (futuro)
			}
		} else if (!existeAdversarioProximo(xBola + TAM_ROBO, yBola, TAM_ROBO)) {
			if (yRobo < yBola) {
				yObjetivo = yBola - TAM_ROBO; //Coloca o robo para a direita da bola
			} else {
				yObjetivo = yBola + TAM_ROBO; //Coloca o robo para a esquerda da bola
			}
		} else { // Existe adversario perigoso mas existe atacante posicionado, entao retira volante para permitir passagem bola/atacante
			if (xBola - xAtacante < TAM_ROBO && abs(yBola - yAtacante)
					< TAM_ROBO / 2) {
				if (yRobo < yBola) {
					yObjetivo = yBola - TAM_ROBO; //Coloca o robo para a direita da bola
				} else {
					yObjetivo = yBola + TAM_ROBO; //Coloca o robo para a esquerda da bola
				}
			}
		}
	} else { //Se a bola esta aa frente do robo (no ataque)
		if (xBola - xRobo > 0) {
			yAlinhamentoRoboBolaGol = yBola - (yBola - yRobo) * (xBola
					- CENTRO_X_GOL) / (xBola - xRobo); //y do gol no alinhamento robo-bola-gol
		} else {
			yAlinhamentoRoboBolaGol = yBola;
		}
		//if ((TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 <= yAlinhamentoRoboBolaGol
		//		&& yAlinhamentoRoboBolaGol <= TAM_Y_CAMPO - (TAM_Y_CAMPO
		//				- TAM_Y_DO_GOL) / 2 && //Se alinhado robo-bola-gol
		//		xBola - xRobo < TAM_ROBO * 3 && // e robo esta proximo da bola (tamanho de tres robos no maximo)
		//		estadoAtacante != ATACA) { // e atacante nao esta atacando
		if (abs(yRobo - yBola) <= TAM_ROBO/2 && xBola - xRobo < TAM_ROBO * 5 && estadoAtacante != ATACA ) { // SBAI 2009
			trocarVolanteAtacante = indVolante;
			estadoAtacante = ATACA; //Coloca estado atacando
			velObjetivo = 7;
			xObjetivo = xBola; //Objetivo inicial é a bola
			yObjetivo = yBola;
			angObjetivo = 90;
		} else {
			xObjetivo = xBola - TAM_ROBO * 2;//sbai2009
			if (dxBola <= 0) { //Se Bola parada ou vindo para o gol
//				xObjetivo = xRobo; //Robo se mantem na linha sbai2009
				yObjetivo = yBola; //Acompanha a posicao da bola
			} /*else { //Se a bola esta indo
				if (xBola - TAM_ROBO * 3 > xRobo) {
					xObjetivo = xBola - TAM_ROBO * 3; // Avanca linha de defesa com a bola
				} else {
					xObjetivo = xRobo + TAM_ROBO * 3; // #sbai2009
				}
				if (xObjetivo < TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO)
					xObjetivo = TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO; // Nunca dentro da area
			}*/
			if (xBola > TAM_X_CAMPO / 2 ) { //Se a bola passou do meio de campo
				if (CENTRO_X_GOL - xBola != 0) {
					yObjetivo = CENTRO_Y_GOL - (CENTRO_Y_GOL - yBola)
							* (CENTRO_X_GOL - xObjetivo) / (CENTRO_X_GOL
							- xBola); //Posiciona robo alinhado para chutar
					if (yObjetivo < TAM_ROBO / 2 + d)
						yObjetivo = TAM_ROBO / 2 + d;
					else if (yObjetivo > TAM_Y_CAMPO - TAM_ROBO / 2 + d)
						yObjetivo = TAM_Y_CAMPO - TAM_ROBO / 2 + d;
				} else {
					yObjetivo = CENTRO_Y_GOL;
				}
				angObjetivo
						= atang2(CENTRO_Y_GOL - yBola, CENTRO_X_GOL - xBola);
			} else { //Se a bola esta no nosso campo
				yObjetivo = yBola; //Acompanha a posicao da bola
			}
		}
	}

	float dx = xRobo - xAtacante;
	dx *= dx;
	float dy = yRobo - yAtacante;
	dy *= dy;
	float quadDist = TAM_ROBO * 2;
	quadDist *= quadDist;

	if (dx + dy < quadDist) { //Se robos muito proximos
		if (yAtacante > yRobo) { //Repele em Y
			yObjetivo = yAtacante - TAM_ROBO * 2;
		} else {
			yObjetivo = yAtacante + TAM_ROBO * 2;
		}
		if (xAtacante > xRobo) { //Repele em X
			xObjetivo = xAtacante - TAM_ROBO * 2;
		} else {
			xObjetivo = xAtacante + TAM_ROBO * 2;
		}
	}

	if (xRobo> TAM_X_CAMPO-TAM_X_DO_GOL*1.5) {
		xObjetivo = TAM_X_CAMPO*0.8;
		yObjetivo = TAM_Y_CAMPO/2;
	}
/*

	float xGoleiro = estadoPrev[indGoleiro].x;
	float yGoleiro = estadoPrev[indGoleiro].y;
	dx = xRobo - xGoleiro;
	dx *= dx;
	dy = yRobo - yGoleiro;
	dy *= dy;

	if (dx + dy < quadDist) { //Se robos muito proximos
		if (yGoleiro > yRobo) { //Repele em Y
			yObjetivo = yGoleiro - TAM_ROBO * 2;
		} else {
			yObjetivo = yGoleiro + TAM_ROBO * 2;
		}
		if (xGoleiro > xRobo) { //Repele em X
			xObjetivo = xGoleiro - TAM_ROBO * 2;
		} else {
			xObjetivo = xGoleiro + TAM_ROBO * 2;
		}
	}
*/

	if (xObjetivo < TAM_X_DO_GOL + TAM_X_AREA +TAM_ROBO/2)	// se o robo está na área de defesa
		xObjetivo = TAM_X_DO_GOL + TAM_X_AREA +TAM_ROBO/2;
//	if (xObjetivo>TAM_X_CAMPO+15+TAM_ROBO/2)
//		xObjetivo=TAM_X_CAMPO+15+TAM_ROBO/2;

	objetivoRobo[indVolante].x = TAM_X_CAMPO/2;//xObjetivo;
	objetivoRobo[indVolante].y = yObjetivo;
	objetivoRobo[indVolante].angulo = angObjetivo;
	objetivoRobo[indVolante].vel = velObjetivo;

}

//==================== A T A C A N T E =========================
void estrAtacante(void) {
	float xObjetivo, yObjetivo, xRobo, yRobo, dxRobo, dyRobo, xBola, yBola,
			dxBola, dyBola, velObjetivo, angObjetivo;
	float dxBG, dyBG, dxBO, dyBO, m;
	float yAlinhamentoRoboBolaGol;

	xBola = estadoPrev[IND_BOLA].x;
	yBola = estadoPrev[IND_BOLA].y;
	dxBola = estadoPrev[IND_BOLA].dx;
	dyBola = estadoPrev[IND_BOLA].dy;

	xRobo = estadoPrev[indAtacante].x;
	yRobo = estadoPrev[indAtacante].y;
	dxRobo = estadoPrev[indAtacante].dx;
	dyRobo = estadoPrev[indAtacante].dy;

	dxBG = xBola - CENTRO_X_GOL; //Calcula objetivo atras bola, alinhado direcao Bola-Gol
	dyBG = yBola - CENTRO_Y_GOL;

	//Calcula o deslocamento de limite dependendo do angulo
	float d = abs((((int) estadoPrev[indVolante].angulo + 45) % 90) - 45) * 2
			/ 45; // nos angulos 45, 135, 225 e 315 considera o robo maior 2 cm
	// a linha acima serve para dar mais espaco ao robo quando girando

	if (xBola - xRobo > 0) { //Com o alinhamento robo-bola, calcula Y da linha do gol
		yAlinhamentoRoboBolaGol = yBola - (yBola - yRobo) * (xBola
				- CENTRO_X_GOL) / (xBola - xRobo);
	} else {
		yAlinhamentoRoboBolaGol = yBola;
	}

	if (xBola >= xRobo && // a bola esta a frente do robo
			xBola - xRobo < TAM_ROBO && abs(yBola - yRobo) < TAM_ROBO / 2) { // o robo esta proximo e alinhado com a bola
		yObjetivo = CENTRO_Y_GOL; //Vai para o gol, a bola deve estar no meio
		xObjetivo = CENTRO_X_GOL;
		angObjetivo = atang2(-dyBG, -dxBG);
		velObjetivo = 7;
		estadoAtacante = ATACA;
	} else if (xBola >= xRobo && // a bola esta a frente do robo
			yAlinhamentoRoboBolaGol > (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 + 5
			&& yAlinhamentoRoboBolaGol < TAM_Y_CAMPO - (TAM_Y_CAMPO
					- TAM_Y_DO_GOL) / 2 - 5) { //Se existe alinhamento robo-bola-gol
		// o +/- 5cm serve como faixa de histerese para evitar transicoes constantes entre POSICIONA e ATACA)
		// (CENTRO_Y_GOL - yAlinhamentoRoboBolaGol) é o erro, colocando duas vezes o erro forcará o robo a melhor se alinhar
		yObjetivo = CENTRO_Y_GOL;// - (CENTRO_Y_GOL - yAlinhamentoRoboBolaGol) * 2; // Vai para o gol, a bola deve estar no meio
		xObjetivo = CENTRO_X_GOL;
		angObjetivo = atang2(-dyBG, -dxBG);
		velObjetivo = 7;
		estadoAtacante = ATACA;
	} else {
		// o +/- 10cm serve como faixa de histerese para evitar transicoes constantes entre ATACA e POSICIONA)
		if (!(abs(yRobo - yBola) <= TAM_ROBO && xBola - xRobo < TAM_ROBO * 5 && estadoAtacante != ATACA )) {
		/*if (xBola < xRobo || yAlinhamentoRoboBolaGol < (TAM_Y_CAMPO
				- TAM_Y_DO_GOL) / 2 - 10 || yAlinhamentoRoboBolaGol
				> TAM_Y_CAMPO - (TAM_Y_CAMPO - TAM_Y_DO_GOL) / 2 + 10) { //Nao existe mais alinhamento robo-bola-gol*/
			estadoAtacante = POSICIONA;
		}
	}

	// ----------------------  estadoAtacante==POSICIONA ---------------------------
	if (estadoAtacante == POSICIONA) { //Posicionando o Robo
		int N, R, dx, dy;
		static bool voltando = false;

		if (xRobo > xBola) {
			voltando = true;
		} else if (xRobo < xBola - TAM_ROBO * 2) {
			voltando = false;
		}

		if (voltando) {
			xObjetivo = xBola - TAM_ROBO * 2.5; //para sair do voltando
			if (yBola < yRobo) { // escolhe o lado para voltar atras da bola
				yObjetivo = yBola + TAM_ROBO * 2;
				angObjetivo = 240;
				if (yObjetivo > TAM_Y_CAMPO - TAM_ROBO) {
					yObjetivo = yBola - TAM_ROBO * 2;
				}
			} else {
				yObjetivo = yBola - TAM_ROBO * 2;
				angObjetivo = 120;
				if (yObjetivo < TAM_ROBO * 2) {
					yObjetivo = yBola + TAM_ROBO * 2;
				}
			}
			velObjetivo = 7;
		} else {
			m = max(abs(dxBG), abs(dyBG));
			if (xBola < xRobo) {
				dxBO = dxBG * TAM_ROBO * 2 / m; // 15cm atras da bola, posicao para chute
				dyBO = dyBG * TAM_ROBO * 2 / m;
			} else {
				dxBO = dxBG * TAM_ROBO * 2 / m; // 15cm atras da bola, posicao para chute
				dyBO = dyBG * TAM_ROBO * 2 / m;
			}
			xObjetivo = xBola + dxBO;
			yObjetivo = yBola + dyBO;
			velObjetivo = 3;
		}

		calculaChoque(indAtacante, false, &xObjetivo, &yObjetivo, dxBola,
				dyBola, &N, &R);

		if (!voltando) {
			dx = xBola - xObjetivo; //Com a bola para quando desviar ja posicionar p/ direcao bola-gol
			dy = yBola - yObjetivo;
			angObjetivo = atang2(dy, dx);
		}
		// ----------------------  estadoAtacante==ATACA ---------------------------
	} else {
		if (xBola - xRobo < TAM_ROBO && abs(yBola - yRobo) < TAM_ROBO / 2) { // o robo esta proximo e alinhado com a bola
			yObjetivo = CENTRO_Y_GOL; //Vai para o gol, a bola deve estar no meio
		} else {
			// (CENTRO_Y_GOL - yAlinhamentoRoboBolaGol) é o erro, colocando duas vezes o erro forcará o robo a melhor se alinhar
			yObjetivo = CENTRO_Y_GOL;/* - (CENTRO_Y_GOL - yAlinhamentoRoboBolaGol)
					* 2; // Vai para o gol, a bola deve estar no meio*/
		}
		xObjetivo = CENTRO_X_GOL;
		angObjetivo = atang2(-dyBG, -dxBG);
		velObjetivo = 7;
	}

	if (xObjetivo < TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO / 2) { //Evita outro robo no gol defendendo
		if (xBola > xRobo && xBola > TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO / 2) {
			xObjetivo = xBola;
			angObjetivo = 0;
		} else {
			xObjetivo = TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO / 2;
			angObjetivo = 0;
		}
	} else if (xObjetivo > TAM_X_CAMPO - TAM_X_DO_GOL) {
		xObjetivo = TAM_X_CAMPO - TAM_X_DO_GOL;
		//				angObjetivo = 90;
	}

	// Serve para afastar o robo quando estiver muito perto das laterais
	if (yObjetivo < TAM_ROBO * 0.75 + d) {
		yObjetivo = TAM_ROBO * 0.75 + d;
		xObjetivo = xBola;
		angObjetivo = 0;
	} else if (yObjetivo > TAM_Y_CAMPO - TAM_ROBO * 0.75 - d) {
		yObjetivo = TAM_Y_CAMPO - TAM_ROBO * 0.75 - d;
		xObjetivo = xBola;
		angObjetivo = 0;
	}

	if (xBola < TAM_X_CAMPO / 2 && xBola - xRobo < TAM_ROBO) { //tira bola do ataque
		xObjetivo = xBola;
		yObjetivo = yBola;
		angObjetivo = 0;
		velObjetivo = 7;
	} else if (xBola + yBola > TAM_X_CAMPO + TAM_Y_CAMPO - 20) { //nao coloca robo no triangulo inutil
		xObjetivo = TAM_X_CAMPO - 20;
		yObjetivo = TAM_Y_CAMPO - TAM_ROBO / 2 - d;
		angObjetivo = 135;
		velObjetivo = 0;
	} else if (xBola - yBola > TAM_X_CAMPO - 20) {
		xObjetivo = TAM_X_CAMPO - 20;
		yObjetivo = TAM_ROBO + d;
		angObjetivo = 45;
		velObjetivo = 0;
	}
/*

	float xGoleiro = estadoPrev[indGoleiro].x;
	float yGoleiro = estadoPrev[indGoleiro].y;
	float dx = xRobo - xGoleiro;
	dx *= dx;
	float dy = yRobo - yGoleiro;
	dy *= dy;
	float quadDist = TAM_ROBO * 2;
	quadDist *= quadDist;

	if (dx + dy < quadDist) { //Se robos muito proximos
		if (yGoleiro > yRobo) { //Repele em Y
			yObjetivo = yGoleiro - TAM_ROBO * 2;
		} else {
			yObjetivo = yGoleiro + TAM_ROBO * 2;
		}
		if (xGoleiro > xRobo) { //Repele em X
			xObjetivo = xGoleiro - TAM_ROBO * 2;
		} else {
			xObjetivo = xGoleiro + TAM_ROBO * 2;
		}
	}
*/

	if (xObjetivo < TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO)	// se o robo está na área de defesa
		xObjetivo = TAM_X_DO_GOL + TAM_X_AREA + TAM_ROBO;

	objetivoRobo[indAtacante].x = xObjetivo;
	objetivoRobo[indAtacante].y = yObjetivo;
	objetivoRobo[indAtacante].angulo = angObjetivo;
	objetivoRobo[indAtacante].vel = velObjetivo;

}

void verificaPosiconamentos() {
	if (emPosiciona) {
		objetivoRobo[indGoleiro].x = TAM_X_CAMPO- TAM_X_DO_GOL - 20;
		objetivoRobo[indGoleiro].y = TAM_Y_CAMPO/2;
		objetivoRobo[indGoleiro].angulo = 0;
		objetivoRobo[indGoleiro].vel = 0;

		objetivoRobo[indVolante].x = TAM_X_CAMPO/2 ;
		objetivoRobo[indVolante].y = TAM_Y_CAMPO/2;
		objetivoRobo[indVolante].angulo = 0;
		objetivoRobo[indVolante].vel = 0;

		objetivoRobo[indAtacante].x = TAM_X_CAMPO/2 - 20;
		objetivoRobo[indAtacante].y = TAM_Y_CAMPO/2;
		objetivoRobo[indAtacante].angulo = 0;
		objetivoRobo[indAtacante].vel = 0;

		if (abs(estado[indGoleiro].x-objetivoRobo[indGoleiro].x) + abs(estado[indGoleiro].y-objetivoRobo[indGoleiro].y) < TAM_ROBO &&
				abs(estado[indVolante].x-objetivoRobo[indVolante].x) + abs(estado[indVolante].y-objetivoRobo[indVolante].y) < TAM_ROBO &&
				abs(estado[indAtacante].x-objetivoRobo[indAtacante].x) + abs(estado[indAtacante].y-objetivoRobo[indAtacante].y) < TAM_ROBO) {
			emPosiciona = false;
			emJogo=false;
		}
	}
	if (emInicio) {
		objetivoRobo[indGoleiro].x = TAM_X_DO_GOL+TAM_ROBO/2 ;
		objetivoRobo[indGoleiro].y = TAM_Y_CAMPO/2;
		objetivoRobo[indGoleiro].angulo = 0;
		objetivoRobo[indGoleiro].vel = 0;

		objetivoRobo[indVolante].x = TAM_X_CAMPO/2 - 45;
		objetivoRobo[indVolante].y = TAM_Y_CAMPO/2;
		objetivoRobo[indVolante].angulo = 0;
		objetivoRobo[indVolante].vel = 0;

		objetivoRobo[indAtacante].x = TAM_X_CAMPO/2 - 20;
		objetivoRobo[indAtacante].y = TAM_Y_CAMPO/2;
		objetivoRobo[indAtacante].angulo = 0;
		objetivoRobo[indAtacante].vel = 0;

		if (abs(estado[indGoleiro].x-objetivoRobo[indGoleiro].x) + abs(estado[indGoleiro].y-objetivoRobo[indGoleiro].y) < TAM_ROBO &&
				abs(estado[indVolante].x-objetivoRobo[indVolante].x) + abs(estado[indVolante].y-objetivoRobo[indVolante].y) < TAM_ROBO &&
				abs(estado[indAtacante].x-objetivoRobo[indAtacante].x) + abs(estado[indAtacante].y-objetivoRobo[indAtacante].y) < TAM_ROBO) {
			emInicio = false;
			emJogo=false;
		}
	}

}

//------------  E S T R A T E G I A -------------
void estrategia(void) {
	unsigned char cmd[NUM_ROBOS_TIME];
	int i;
	calculaPrevisao();
	EstrGoleiro();
	estrVolante(indVolante);
	estrAtacante();

	if (trocarVolanteAtacante) {
		int tmp = indAtacante; //Robo vira atacante
		indAtacante = indVolante;
		indVolante = tmp;
		trocarVolanteAtacante = 0;
	}

	verificaPosiconamentos();

	for (i = 0; i < 3; i++) {
		int e, d, ea, da;
		calculaCmd(i, objetivoRobo[i].angulo, objetivoRobo[i].x,
				objetivoRobo[i].y, objetivoRobo[i].vel);
		//    CalculaCmd(i, 45, 150, 90, 0);
		ea = abs(e = cmdEnviado[0][i].esq);
		if (e < 0)
			ea |= 0x8;
		da = abs(d = cmdEnviado[0][i].dir);
		if (d < 0)
			da |= 0x8;
		cmd[i] = (ea << 4) + da;
	}
	//	cout << "!";
//		enviaDados(0x11, 0x11, 0x11);
	if (emJogo) {
		enviaDados(cmd[0], cmd[1], cmd[2]);
		//    fprintf(fp, "%3d, %3x, %3x, %3x\n", ContQuadro, cmd[0], cmd[1], cmd[2]);
	} else {
		enviaByteEspera(7);
		printf("G: %3x, V: %3x, A: %3x\n", indGoleiro, indVolante, indAtacante);
		printf("%3x, %3x, %3x\n", cmd[0], cmd[1], cmd[2]);
	}
}

