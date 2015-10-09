/*
 * Controle.cpp
 *
 *  Created on: Sep 8, 2009
 *      Author: rene
 */

#include "TiposClasses.h"
#include "Auxiliares.h"
#include "Controle.h"

#define CteEstCmd 0.5
#define CteEstVel 0.5
#define CTE_PARADA 4

//#define SO_UM
//#define ATEH_DOIS
//#define ATEH_TRES
#define ATEH_QUATRO
//#define ATEH_CINCO

extern FutebolCamera *futCam[NUM_CAMERAS];

extern Estado estado[NUM_ROBOS_TIME * 2 + 1], estadoAnt[NUM_ROBOS_TIME * 2 + 1], estadoPrev[NUM_ROBOS_TIME * 2 + 1];

extern CmdEnviado cmdEnviado[10][NUM_ROBOS_TIME]; //comando enviado aos robos

void calculaVelMotores(int NumRobo, int *me, int *md) {
	float dx, dy, ang, v_cm, v_unid, ve, vd, aux, difVelRodas;
	//Calcula abaixo a velocidade dos motores do robo
	ang = estadoPrev[NumRobo].angulo;
	dx = estadoPrev[NumRobo].dx;
	dy = estadoPrev[NumRobo].dy;

	v_cm = sqrt(dx * dx + dy * dy); //Calcula a velocidade do Robo em cm
	//Calcula as velocidade dos motores em unidades
	v_unid = v_cm * FATOR_VEL_ROBO_UNID_POR_CM;
	if (v_unid <= VEL_MAXIMA_ROBO_UNID) {
		if (abs(difAngMenos180a180(atang2(dy, dx), ang)) > 90) {
			//Robo andando de re';
			v_unid = -v_unid; //frente (etiquetas) para um lado, deslocamento para o outro
		}

		aux = difAngMenos180a180(ang, estadoAnt[NumRobo].angulo);
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
		ve = (cmdEnviado[0][NumRobo].esq + cmdEnviado[1][NumRobo].esq + cmdEnviado[2][NumRobo].esq + cmdEnviado[3][NumRobo].esq) / 4;
		vd = (cmdEnviado[0][NumRobo].dir + cmdEnviado[1][NumRobo].dir + cmdEnviado[2][NumRobo].dir + cmdEnviado[3][NumRobo].dir) / 4;
	}
	*me = ve;
	*md = vd;
}

void calculaCmd(int indJogador, int angObjetivo, int xObjetivo, int yObjetivo, int velObjetivo) {
	// PA e' usado na contagem dos comandos necessarios para acelerar e parar
	char PA[] = { 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10 };
	//PA+1 para diminuir a velocidade angular perto do objetivo (prog. aritmetica)
	float angRobo, xRobo, yRobo, angRoboObjetivo;
	float ang, direc;
	int ve, vd, pe, pd, i;
	float de, dd, parteFixa;
	int DA;
	float dx, dy;
	float d;

	angRobo = estadoPrev[indJogador].angulo;
	xRobo = estadoPrev[indJogador].x;
	yRobo = estadoPrev[indJogador].y;
	dx = xObjetivo - xRobo;
	dy = yObjetivo - yRobo;
	d = sqrt(dx * dx + dy * dy);
	//Tenta realizar uma curva para chegar na bola com angulo correto
//	yObjetivo -= d * velObjetivo / 7 * seno(angObjetivo);//o 7 era 14 @@@1
//	xObjetivo -= d * velObjetivo / 7 * coss(angObjetivo);//o 7 era 14 @@@1
	if (d >= CTE_PARADA) {
		angRoboObjetivo = atang2(yObjetivo - yRobo, xObjetivo - xRobo);
		ang = (angRoboObjetivo - angRobo);
	} else { //Ja esta no objetivo
		ang = (angObjetivo - angRobo);
	}

	if (ang < 0) {
		ang += 360;
	}
	if (ang <= 90) {
		ang = ang;
		direc = 1;
	} else if (ang <= 180) {
		ang = ang - 180;
		direc = -1;
	} else if (ang <= 270) {
		ang = ang - 180;
		direc = -1;
	} else {
		ang = ang - 360;
		direc = 1;
	}

	//Calcula abaixo o comando maximo para chegar ao objetivo (comando ideal)
	if (d <= (7 - velObjetivo) * CTE_PARADA) {
		velObjetivo = d / CTE_PARADA;
	} else {
		velObjetivo = 7;
	}

	if (direc == -1) {
		velObjetivo = -velObjetivo;
	}

	// Parte Fixa da velocidade, igual para as duas rodas
	parteFixa = (velObjetivo * (90 - abs(ang))) / 90; //Quando maior o angulo de giro, menor a velocidade
	if (parteFixa == 0 && d >= CTE_PARADA) { // PF==0 significa que Vel==0 ou giro=90 graus
		if (velObjetivo > 0) {
			parteFixa = 1;
		} else if (velObjetivo < 0) {
			parteFixa = -1;
		}
	}

	//8 e' chute valores de 5 a 15, quize e' mais rapido, mas vira menos
	DA = abs(ang / 40); //necessario repetir 3x cada comando (5 graus => 1 unid x 3 comandos) ,	era 8, agora é 8*2
	if (DA > 6)//6)
		DA = 6;//6; //36*5 = 180 => giro de 360 graus   , era 6
	DA = PA[DA]; //Usando a PA dos comandos ate' equilibrar (andar pulsos desenjados)

	if (ang > 0) {//Se motor dir. deve ser MAIOR que motor esq. (curva p/esq.)
		pe = parteFixa - DA / 2;
		pd = parteFixa + DA / 2;
		if (DA & 1) {//DA/2 tem resto?
			if (parteFixa > 0) {
				pe--; // Reduz a roda, mais facil que acelerar a mais lenta
			} else {
				pd++;
			}
		}
		if (pe < -7) { //Verifica limites -7 e 7
			pe = -7;
			pd = -7 + DA;
		} else if (pd > 7) {
			pd = 7;
			pe = 7 - DA;
		}
	} else { //Se motor dir. deve ser MENOR que motor esq. (curva p/dir.)
		pd = parteFixa - DA / 2;
		pe = parteFixa + DA / 2;
		if (DA & 1) { //DA/2 tem resto?
			if (parteFixa > 0) {
				pd--; // Reduz a roda, mais facil que acelerar a mais lenta
			} else {
				pe++;
			}
		}
		if (pd < -7) {
			pd = -7;
			pe = -7 + DA;
		} else if (pe > 7) {
			pe = 7;
			pd = 7 - DA;
		}
	}

	calculaVelMotores(indJogador, &ve, &vd);

	//  ve=EstVel[Jogador].e=ve*CteEstVel+EstVel[Jogador].e*(1-CteEstVel);
	//  vd=EstVel[Jogador].d=vd*CteEstVel+EstVel[Jogador].d*(1-CteEstVel);

	for (i = 9; i > 0; i--) {
		cmdEnviado[i][indJogador] = cmdEnviado[i - 1][indJogador];
	}

	//O robo esta' mudando de direcao, para o robo primeiro antes de fazer curvas
	if (d < 10 && ((pd + pe) * (ve + vd)) < 0 && //Sinais diferentes (sentidos diferentes)
			abs((pd + pe) - (ve + vd)) > 11 && abs(ve - vd) < 4) {
		if (ve > 0) {
			pd = pe = -7;
		} else {
			pd = pe = 7;
		}
	} else {
		int Passoe, Passod;
		Passoe = abs(pe - ve); //Calcula o erro entre a velocidade desejada e a real
		Passoe = Passoe > 3 ? 3 : Passoe; //Limita a variacao da velocidade em 3
		if (abs(ve) < Passoe) {
			Passoe -= abs(ve) / 2;
		} else {
			Passoe = 1;
		}
		Passod = abs(pd - vd);
		Passod = Passod > 3 ? 3 : Passod;
		if (abs(vd) < Passod) {
			Passod -= abs(vd) / 2;
		} else {
			Passod = 1;
		}
		if (ve == vd && pe == pd) { //Robo anda reto
			if (ve < pe) {
				pd = pe = ve + Passoe;
			} else if (ve > pe) {
				pd = pe = ve - Passoe;
			} else {
				pd = pe = ve;
			}
		} else {
			if (abs(de = ve - pe) > abs(dd = vd - pd)) {//Se motor esq. mais longe do comando ideal
				pd = vd;
				if (de > 0) { //Se o motor esta alem do comando ideal
					pe = ve - Passoe;
				} else if (de < 0) { //Se o motor esta aquem do comando ideal
					pe = ve + Passoe;
				} else {
					pe = ve;
				}
			} else {
				pe = ve;
				if (dd > 0) { //Se o motor esta alem do comando ideal
					pd = vd - Passod;
				} else if (dd < 0) { //Se o motor esta aquem do comando ideal
					pd = vd + Passod;
				} else {
					pd = vd;
				}
			}
		}
	}
	if (pe > 7)
		pe = 7;
	else if (pe < -7)
		pe = -7;
	if (pd > 7)
		pd = 7;
	else if (pd < -7)
		pd = -7;

#ifdef SO_UM
	if (pe<0)
	pe=-1;
	else if (pe>0)
	pe=1;
	if (pd<0)
	pd=-1;
	else if (pd>0)
	pd=1;
#endif
#ifdef ATEH_DOIS
	if (pe > 2)
		pe = 2;
	else if (pe < -2)
		pe = -2;
	if (pd > 2)
		pd = 2;
	else if (pd < -2)
		pd = -2;
#endif
#ifdef ATEH_TRES
	if (pe>3)
	pe=3;
	else if (pe<-3)
	pe=-3;
	if (pd>3)
	pd=3;
	else if (pd<-3)
	pd=-3;
#endif
#ifdef ATEH_QUATRO
	if (pe>4)
	pe=4;
	else if (pe<-4)
	pe=-4;
	if (pd>4)
	pd=4;
	else if (pd<-4)
	pd=-4;
#endif
#ifdef ATEH_CINCO
	if (pe>5)
	pe=5;
	else if (pe<-5)
	pe=-5;
	if (pd>5)
	pd=5;
	else if (pd<-5)
	pd=-5;
#endif

	//  pe=EstCmd[Jogador].e=pe*CteEstCmd+EstCmd[Jogador].e*(1-CteEstCmd);
	//  pd=EstCmd[Jogador].d=pd*CteEstCmd+EstCmd[Jogador].d*(1-CteEstCmd);

	cmdEnviado[0][indJogador].esq = pe;
	cmdEnviado[0][indJogador].dir = pd;
}

