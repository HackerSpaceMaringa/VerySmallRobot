/*
 * Posicao.cpp
 *
 *  Created on: Aug 24, 2009
 *      Author: rene
 */

#include "TiposClasses.h"
#include "Auxiliares.h"
#include "Estrategia.h"

#define MAX_ELEMS 100

extern FutebolCamera *futCam[NUM_CAMERAS];

extern Estado estado[NUM_ROBOS_TIME * 2 + 1], estadoAnt[NUM_ROBOS_TIME * 2 + 1], estadoPrev[NUM_ROBOS_TIME * 2 + 1];
extern Lado nossoLado;
extern bool emJogo;
extern guint8 comb[MAX_CONT_COMB][NUM_ROBOS_TIME];

struct Elem {
	unsigned num;
	struct Conj {
		int n;
		float x, y;
		int propTamOtimo; // proporcao do tamanho ótimo quanto maior mais proximo do blob otimo */
	} e[MAX_ELEMS];
} elem[2][MAX_IND_CORES]; //[2] para as linhas pares [0] e para as linha impares [1]

int campo;
PosXYAux posNossosRobosCampo1[NUM_ROBOS_TIME], posNossosRobosCampo2[NUM_ROBOS_TIME];
PosXY posBolaCampo1, posAdversariosCampo1[NUM_ROBOS_TIME], posBolaCampo2, posAdversariosCampo2[NUM_ROBOS_TIME];

void separaEtiquetas(guint8 *pImgDigi, guint8 rgb_paleta[], int indCamera) {
	int contElem = 0;
	const int DUAS_LINHAS = 640 * 2;
	guint8 *pVaiProcessar = pImgDigi, *pIni, *pFin;

	const int minLimiteXBlob = futCam[indCamera]->minLimiteXBlob;
	const int maxLimiteXBlob= futCam[indCamera]->maxLimiteXBlob;
	const int minLimiteYBlob= futCam[indCamera]->minLimiteYBlob;
	const int maxLimiteYBlob= futCam[indCamera]->maxLimiteYBlob;
	const int numMinPixelsBlob= futCam[indCamera]->numMinPixelsBlob;
	const int numMaxPixelsBlob= futCam[indCamera]->numMaxPixelsBlob;

	const int tamIdealXBlob= futCam[indCamera]->tamIdealXBlob;
	const int tamIdealYBlob= futCam[indCamera]->tamIdealYBlob;
	const int numIdealPixelsBlob= futCam[indCamera]->numIdealPixelsBlob;

	int linInicio = max(2, futCam[indCamera]->limitesCampo.minY);
	int posLinInicio = linInicio * 640;

	/** Inicia vetor de elementos */
	for (int j = 0; j < 2; j++)
		// j=[0..1], para linhas pares (0) e linhas impares (1)
		for (int i = 0; i < MAX_IND_CORES; i++)
			elem[j][i].num = 0;

	pVaiProcessar += posLinInicio; // salta linhas iniciais de sentinela

	while (*pVaiProcessar != SENTINELA) {
		while (*pVaiProcessar == SEM_COR)
			pVaiProcessar++;
		if (*pVaiProcessar == SENTINELA)
			break;
		int cor = *pVaiProcessar, somaX = 0, somaY = 0, xIni, xFin, y;
		pIni = pVaiProcessar;
		pFin = pIni + 1;

		/* numero de pixels no blob */
		int nPelBlog = 0;
		/** limites do blob */
		int minX = INT_MAX, minY = (pIni - pImgDigi) / 640, maxX = 0, maxY = 0;

		while (*pIni == cor) {
			guint8 *pAux = pFin;

			/** verifica se existem pixels apos (a direita) de pFin que tambem sao da cor*/

			int contGAP = 0;
			do {
				while (*pFin++ == cor) {
					contGAP = 0;
				}
				contGAP++;
			} while (contGAP < GAP);
			pFin -= 1 + contGAP;

			if (pFin <= pAux) { // nao tem pixel da cor a direita do final, vamos procurar a esquerda
				/** verifica se existem pixels antes (a esquerda) de pFin que tambem sao da cor*/
				while (*pFin != cor && pIni < pFin)
					pFin--;
			}

			/** Ja temos o pIni e o pFin indicando o inicio e o fim do blob na linha */

			div_t d = div(pIni - pImgDigi, 640);
			xIni = d.rem;
			y = d.quot;
			int cont = pFin - pIni + 1;
			xFin = xIni + cont;
			somaY += y * cont;
			somaX += (xFin + xIni) * cont / 2; // formula da PA
			nPelBlog += cont;
			minX = MIN(minX, xIni);
			maxX = MAX(maxX, xFin);
			maxY = y;

			/** limpa os pixel de cores já considerados */

			memset(pIni, SEM_COR, cont);

			/** Agora na proxima linha ... */

			pIni += DUAS_LINHAS; // salta para a proxima linha do QUADRO
			pFin += DUAS_LINHAS; // salta para a proxima linha do QUADRO

			/** verifica se existem pixels antes (a esquerda) de pIni que tambem sao da cor*/

			pAux = pIni;

			contGAP = 0;
			do {
				while (*pIni-- == cor) {
					contGAP = 0;
				}
				contGAP++;
			} while (contGAP < GAP);
			pIni += 1 + contGAP;

			if (pIni >= pAux) { // nao tem pixel da cor a esquerda do inicio, vamos procurar a direita
				/** verifica se existem pixels depois (a direta) de pIni que tambem sao da cor*/
				while (*pIni != cor && pIni < pFin)
					pIni++;
			}
		}

		// o blob de uma cor acaba de ser encontrado conpletamente, os dados obtidos serao analizados abaixo
		int dx = maxX - minX + 1, dy = maxY - minY + 1;
		//300,147 209,182; 276,214
		//		if (cor == 0)
		//		if (dx >= 6 && minY > 150 && minX > 250)// para teste no debuguer
		//			pIni++;
		if (minLimiteXBlob <= dx && dx <= maxLimiteXBlob) {
			if (minLimiteYBlob <= dy && dy <= maxLimiteYBlob && numMinPixelsBlob <= nPelBlog && nPelBlog <= numMaxPixelsBlob) {
				int campoImpar = minY & 1;
				int ind = elem[campoImpar][cor].num++;
				if (ind < MAX_ELEMS) {
					elem[campoImpar][cor].e[ind].n = nPelBlog;
					elem[campoImpar][cor].e[ind].x = somaX / (float) nPelBlog;
					elem[campoImpar][cor].e[ind].y = somaY / (float) nPelBlog;
					// calculo da proporcao do tamanho ótimo (diferencas entre as medidas e os valores ideais)
					dx -= tamIdealXBlob;
					dx *= dx;
					dy -= tamIdealYBlob;
					dy *= dy;
					elem[campoImpar][cor].e[ind].propTamOtimo = dx + dy + abs(nPelBlog - numIdealPixelsBlob);
					contElem++;
				}
			}
		}
	}
	//	cout << "Elems: " << contElem << endl;
}

/** encontra os objetos a partir das etiquetas previamnte encontradas
 * o parametro campoImpar deve ser [0] para linhas pares e [1] para linhas impares
 */
void encontraObjetos(int campo, PosXYAux posNossosRobos[], PosXY &posBola, PosXY posAdversarios[], int indCamera) {
	int num, numTime;
	// Encontra os NUM_ROBOS_TIME*2 grupos de cor do nosso time com numero de pixel mais proximos com o ideal
	num = elem[campo][COR_TIME].num;
	numTime = min(num, NUM_ROBOS_TIME * 2);
	Elem::Conj *cTime = elem[campo][COR_TIME].e;
	for (int i = 0; i < numTime; i++) {
		for (int j = i + 1; j < num; j++) { //repete para os num primeiros
			if (cTime[j].propTamOtimo < cTime[i].propTamOtimo) {
				Elem::Conj tmp = cTime[j];
				cTime[j] = cTime[i];
				cTime[i] = tmp;
			}
		}
	}

	//	for (int i = 0; i < numTime; i++)
	//		cout << cTime[i].x << "," << cTime[i].y << ":" << cTime[i].n << " ";
	//	cout << endl;

	const int maxDistEtiquetas= futCam[indCamera]->maxDistEtiquetas;
	const int distIdealEtiquetas= futCam[indCamera]->distIdealEtiquetas;
	const int quadradoDistMax = maxDistEtiquetas * maxDistEtiquetas;
	const int quadradoDistIdeal = distIdealEtiquetas * distIdealEtiquetas;
	struct Rel {
		/** indAux é o indice da etiqueta encontrada com uma com auxiliar */
		int indAux;
		/** qualidade da relacao, incluindo a qualidade da etiqueta e a qualidade da
		 * distancia entre as etiquetas (time x Aux).
		 * Valor mantido ao quadrado. Quanto menor melhor */
		int prop;
	} relAux[numTime][NUM_CORES_AUX];

	/** Procura as possiveis combinacoes entre etiquetas auxiliares e etiqueta do time.
	 * Para cada etiqueta aux (indEtiqAux) verifica todas as etiquetas do time, procurando
	 * combinacoes possiveis.
	 * Apenas as melhores candidatas indEtiqAux, dentre as encontradas, sao
	 * relacionada com uma etiqueta do time.
	 * relAux[i][j].indAux indica o indice da etiqueta da j-ésima cor auxilar que pode
	 * estar relacionada à i-ésima etiqueta do time.
	 */
	for (int indEtiqAux = 0; indEtiqAux < NUM_CORES_AUX; indEtiqAux++) {
		// inicia com NAO_EXISTE
		for (int i = 0; i < numTime; i++) {
			relAux[i][indEtiqAux].prop = NAO_EXISTE;
		}
		int numAux = elem[campo][COR_AUX1 + indEtiqAux].num;
		Elem::Conj *cAux = elem[campo][COR_AUX1 + indEtiqAux].e;
		for (int i = 0; i < numTime; i++) { //repete para as numTime (dobro das etiquetas encontradas) primeiros
			for (int j = 0; j < numAux; j++) {
				int dx = cTime[i].x - cAux[j].x;
				int dy = cTime[i].y - cAux[j].y;
				int d = dx * dx + dy * dy; //calcula a distancia, deixa ao quadrado pois todas as distancias estao ao quadrado
				if (d < quadradoDistMax) {
					int prop = abs(d - quadradoDistIdeal) + cAux[j].propTamOtimo;
					if (prop < relAux[i][indEtiqAux].prop) {
						relAux[i][indEtiqAux].prop = prop;
						relAux[i][indEtiqAux].indAux = j;
					}
				}
			}
		}
	}

	/** Remove as etiquetas do time sem etiqueta auxiliar proxima */
	int indNaoVazio = 0;
	for (int i = 0; i < numTime; i++) {
		bool vazio = true;
		for (int indEtiqAux = 0; indEtiqAux < NUM_CORES_AUX; indEtiqAux++) {
			if (relAux[i][indEtiqAux].prop != NAO_EXISTE) {
				vazio = false; //etiqueta auxiliar encontrada junta a etiqueta time
				break;
			}
		}
		if (!vazio) {
			for (int indEtiqAux = 0; indEtiqAux < NUM_CORES_AUX; indEtiqAux++) {
				relAux[indNaoVazio][indEtiqAux] = relAux[i][indEtiqAux];
				cTime[indNaoVazio] = cTime[i];
			}
			indNaoVazio++;
		}
	}

	/* Possibilidade de melhorar a visao:
	 *  possibilidade 1: ordena relAux e cTime de acordo com o custo minimo encontrado (distancia entre etiquetas)
	 *  possibilidade 2: usar dados das etiquetas encontradas anteriormente
	 */

	//	cout << numTime << "|" << indNaoVazio << "]";

	/** agora numTime pode ser apenas o numero de robos; indNaoVazio é o numero de etiquetas da
	 * cor do time com uma etiqueta de cor proxima */
	numTime = min(indNaoVazio, NUM_ROBOS_TIME);

	/** combina todos as etiquetas do time com as possiveis etiquetas auxiliares relacionadas,
	 * otimizando para a melhor relacao
	 */

	int melhorCusto = NAO_EXISTE;
	int indMelhorCusto = 0;
	for (int i = 0; i < MAX_CONT_COMB; i++) {
		int custo = 0;
		for (int j = 0; j < numTime; j++) {
			int k = comb[i][j]; // numero cor da etiqueta auxiliar
			custo += relAux[j][k].prop;
			if (custo >= melhorCusto) // se o custo que esta sendo calculado já é maior, tenta outra combinacao
				break;
		}
		if (custo < melhorCusto) {
			melhorCusto = custo;
			indMelhorCusto = i;
		}
	}

	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		posNossosRobos[i].x = NAO_EXISTE;
	}

	for (int j = 0; j < numTime; j++) {
		//		cout << comb[indMelhorCusto][j] << "!";
		if (relAux[j][comb[indMelhorCusto][j]].prop < NAO_EXISTE) {
			posNossosRobos[comb[indMelhorCusto][j]].x = cTime[j].x;
			posNossosRobos[comb[indMelhorCusto][j]].y = cTime[j].y;
			posNossosRobos[comb[indMelhorCusto][j]].xAux = elem[campo][comb[indMelhorCusto][j] + COR_AUX1].e[relAux[j][comb[indMelhorCusto][j]].indAux].x;
			posNossosRobos[comb[indMelhorCusto][j]].yAux = elem[campo][comb[indMelhorCusto][j] + COR_AUX1].e[relAux[j][comb[indMelhorCusto][j]].indAux].y;
		}
	}

	// encontrando a melhor bola

	int numBola = elem[campo][COR_BOLA].num;
	Elem::Conj *cBola = elem[campo][COR_BOLA].e;
	int custo = cBola[0].propTamOtimo;
	int indBlobBola = 0;
	for (int i = 1; i < numBola; i++) {
		if (cBola[i].propTamOtimo < custo) {
			custo = cBola[i].propTamOtimo;
			indBlobBola = i;
		}
	}
	if (numBola > 0) {
		posBola.x = cBola[indBlobBola].x;
		posBola.y = cBola[indBlobBola].y;
	} else {
		posBola.x = NAO_EXISTE; // bola nao encontrada
	}

	// encontra adversários

	// Encontra os NUM_ROBOS_TIME grupos de cor do time adversario com numero de pixel mais proximos do ideal
	num = elem[campo][COR_CONTRA].num;
	int numAdve = min(num, NUM_ROBOS_TIME);
	cTime = elem[campo][COR_CONTRA].e;
	for (int i = 0; i < numAdve; i++) {
		for (int j = i + 1; j < num; j++) { //repete para todos os elementos desta cor
			if (cTime[j].propTamOtimo < cTime[i].propTamOtimo) {
				Elem::Conj tmp = cTime[j];
				cTime[j] = cTime[i];
				cTime[i] = tmp;
			}
		}
	}

	for (int j = 0; j < numAdve; j++) {
		posAdversarios[j].x = cTime[j].x;
		posAdversarios[j].y = cTime[j].y;
	}
	for (int j = numAdve; j < NUM_ROBOS_TIME; j++) {
		posAdversarios[j].x = NAO_EXISTE;
	}

	// ESCALA - coloca na escala do campo em centimetros

	LimitesCampo limitesCampo = futCam[indCamera]->limitesCampo;
	Lado nossoLado = futCam[indCamera]->nossoLado;
	float fatorX = (float) TAM_X_CAMPO_SEM_GOL / (limitesCampo.maxXSemGol - limitesCampo.minXSemGol);
	float fatorY = (float) TAM_Y_CAMPO / (limitesCampo.maxY - limitesCampo.minY);
	// nosso time
	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		if (posNossosRobos[i].x != NAO_EXISTE) {
			if (nossoLado == ESQUERDA) {
				posNossosRobos[i].x = TAM_X_DO_GOL + (posNossosRobos[i].x - limitesCampo.minXSemGol) * fatorX;
				posNossosRobos[i].y = TAM_Y_CAMPO - (posNossosRobos[i].y - limitesCampo.minY) * fatorY;
				posNossosRobos[i].xAux = TAM_X_DO_GOL + (posNossosRobos[i].xAux - limitesCampo.minXSemGol) * fatorX;
				posNossosRobos[i].yAux = TAM_Y_CAMPO - (posNossosRobos[i].yAux - limitesCampo.minY) * fatorY;
			} else {
				posNossosRobos[i].x = TAM_X_DO_GOL + TAM_X_CAMPO_SEM_GOL - (posNossosRobos[i].x - limitesCampo.minXSemGol) * fatorX;
				posNossosRobos[i].y = (posNossosRobos[i].y - limitesCampo.minY) * fatorY;
				posNossosRobos[i].xAux = TAM_X_DO_GOL + TAM_X_CAMPO_SEM_GOL - (posNossosRobos[i].xAux - limitesCampo.minXSemGol) * fatorX;
				posNossosRobos[i].yAux = (posNossosRobos[i].yAux - limitesCampo.minY) * fatorY;
			}
		} else {
			posNossosRobos[i].x = NAO_EXISTE;
		}
	}
	// bola
	if (posBola.x != NAO_EXISTE) {
		if (nossoLado == ESQUERDA) {
			posBola.x = TAM_X_DO_GOL + (posBola.x - limitesCampo.minXSemGol) * fatorX;
			posBola.y = TAM_Y_CAMPO - (posBola.y - limitesCampo.minY) * fatorY;
		} else {
			posBola.x = TAM_X_DO_GOL + TAM_X_CAMPO_SEM_GOL - (posBola.x - limitesCampo.minXSemGol) * fatorX;
			posBola.y = (posBola.y - limitesCampo.minY) * fatorY;
		}
	} else {
		posBola.x = NAO_EXISTE;
	}

	// adversarios
	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		if (posAdversarios[i].x != NAO_EXISTE) {
			if (nossoLado == ESQUERDA) {
				posAdversarios[i].x = TAM_X_DO_GOL + (posAdversarios[i].x - limitesCampo.minXSemGol) * fatorX;
				posAdversarios[i].y = TAM_Y_CAMPO - (posAdversarios[i].y - limitesCampo.minY) * fatorY;
			} else {
				posAdversarios[i].x = TAM_X_DO_GOL + TAM_X_CAMPO_SEM_GOL - (posAdversarios[i].x - limitesCampo.minXSemGol) * fatorX;
				posAdversarios[i].y = (posAdversarios[i].y - limitesCampo.minY) * fatorY;
			}
		} else {
			posAdversarios[i].x = NAO_EXISTE;
		}
	}

}



/** as estiquetas dos adversarios podem chegar aqui fora de ordem.
 * Esta rotina procura os pares de etiquetas, sendo uma em cada campo da imagem
 */

/** as estiquetas dos adversarios podem chegar aqui fora de ordem.
 * Esta rotina procura os pares de etiquetas, sendo uma em cada campo da imagem
 */
void combinaAdvesariosEntreCamposEAnterior(PosXY posAdversariosCampo1[NUM_ROBOS_TIME], PosXY posAdversariosCampo2[NUM_ROBOS_TIME], Estado estadoAnt[]) {
	/** combina todos as etiquetas do time com as possiveis etiquetas auxiliares relacionadas,
	 * otimizando para a melhor relacao
	 */

	int menorDesloc1 = NAO_EXISTE;
	int indMenorDesloc1 = 0;
	for (int i = 0; i < MAX_CONT_COMB; i++) {
		int desloc = 1;
		for (int j = 0; j < NUM_ROBOS_TIME; j++) {
			/* NUM_ROBOS_TIME + 1 é a posicao do primeiro dos adversarios apos rodos do time [0..NUM_ROBOS_TIME-1] e
			 * bola [NUM_ROBOS_TIME] no estadoAnt */
			int ji=j+ + NUM_ROBOS_TIME+1;
			int k = comb[i][j];
			if (posAdversariosCampo1[k].x == NAO_EXISTE || estadoAnt[ji].x == NAO_EXISTE)
				desloc *= 100; // penalidade, mas nao descarta pois o resto da combinacao pode ser a melhor
			else
				desloc *= abs(posAdversariosCampo1[k].x - estadoAnt[ji].x) + abs(posAdversariosCampo1[k].y - estadoAnt[ji].y) + 1;
			if (desloc >= menorDesloc1) // se o custo que esta sendo calculado já é maior, tenta outra combinacao
				break;
		}
		if (desloc < menorDesloc1) {
			menorDesloc1 = desloc;
			indMenorDesloc1 = i;
		}
	}

	int menorDesloc2 = NAO_EXISTE;
	int indMenorDesloc2 = 0;
	for (int i = 0; i < MAX_CONT_COMB; i++) {
		int desloc = 1;
		for (int j = 0; j < NUM_ROBOS_TIME; j++) {
			/* NUM_ROBOS_TIME + 1 é a posicao do primeiro dos adversarios apos rodos do time [0..NUM_ROBOS_TIME-1] e
			 * bola [NUM_ROBOS_TIME] no estadoAnt */
			int ji=j+ + NUM_ROBOS_TIME+1;
			int k = comb[i][j];
			if (posAdversariosCampo2[k].x == NAO_EXISTE || estadoAnt[ji].x == NAO_EXISTE)
				desloc *= 100; // penalidade, mas nao descarta pois o resto da combinacao pode ser a melhor
			else
				desloc *= abs(posAdversariosCampo2[k].x - estadoAnt[ji].x) + abs(posAdversariosCampo2[k].y - estadoAnt[ji].y) + 1;
			if (desloc >= menorDesloc2) // se o custo que esta sendo calculado já é maior, tenta outra combinacao
				break;
		}
		if (desloc < menorDesloc2) {
			menorDesloc2 = desloc;
			indMenorDesloc2 = i;
		}
	}

	//
	//	cout<<"--------------"<<endl;
	//	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
	//		cout << (int)comb[indMenorDesloc][j]<<","<< " " ;
	//	}

//		cout<<endl;
//		for (int j = 0; j < NUM_ROBOS_TIME; j++) {
//			cout << "("<< posAdversariosCampo1[j].x<<","<< posAdversariosCampo1[j].y<<")" ;
//		}

	//	cout<<endl;
	//	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
	//		cout << "("<< posAdversariosCampo2[j].x<<","<< posAdversariosCampo2[j].y<<")" ;
	//	}

	// recoloca na melhor ordem em posAdversariosCampo1

	PosXY posAdversarios[NUM_ROBOS_TIME];
	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
		posAdversarios[j] = posAdversariosCampo1[comb[indMenorDesloc1][j]];
	}
	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
		posAdversariosCampo1[j] = posAdversarios[j];
	}

	// recoloca na melhor ordem em posAdversariosCampo2

	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
		posAdversarios[j] = posAdversariosCampo2[comb[indMenorDesloc2][j]];
	}
	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
		posAdversariosCampo2[j] = posAdversarios[j];
	}
	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		if (posAdversariosCampo1[i].x!=NAO_EXISTE &&
				(abs(posAdversariosCampo1[i].x - posAdversariosCampo2[i].x) > VELOCIDADE_MAXIMA / 60.0 || abs(posAdversariosCampo1[i].y - posAdversariosCampo2[i].y) > VELOCIDADE_MAXIMA / 60.0))
			posAdversariosCampo2[i].x = NAO_EXISTE;
	}
}
/*
void combinaAdvesariosEntreCampos(PosXY posAdversariosCampo1[NUM_ROBOS_TIME], PosXY posAdversariosCampo2[NUM_ROBOS_TIME]) {
	* combina todos as etiquetas do time com as possiveis etiquetas auxiliares relacionadas,
	 * otimizando para a melhor relacao


	int menorDesloc = NAO_EXISTE;
	int indMenorDesloc = 0;
	for (int i = 0; i < MAX_CONT_COMB; i++) {
		int desloc = 1;
		for (int j = 0; j < NUM_ROBOS_TIME; j++) {
			int k = comb[i][j];
			if (posAdversariosCampo1[j].x == NAO_EXISTE || posAdversariosCampo2[k].x == NAO_EXISTE)
				desloc *= 100; // penalidade, mas nao descarta pois o resto da combinacao pode ser a melhor
			else
				desloc *= abs(posAdversariosCampo1[j].x - posAdversariosCampo2[k].x) + abs(posAdversariosCampo1[j].y - posAdversariosCampo2[k].y) + 1;
			if (desloc >= menorDesloc) // se o custo que esta sendo calculado já é maior, tenta outra combinacao
				break;
		}
		if (desloc < menorDesloc) {
			menorDesloc = desloc;
			indMenorDesloc = i;
		}
	}
	//
	//	cout<<"--------------"<<endl;
	//	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
	//		cout << (int)comb[indMenorDesloc][j]<<","<< " " ;
	//	}

//		cout<<endl;
//		for (int j = 0; j < NUM_ROBOS_TIME; j++) {
//			cout << "("<< posAdversariosCampo1[j].x<<","<< posAdversariosCampo1[j].y<<")" ;
//		}

	//	cout<<endl;
	//	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
	//		cout << "("<< posAdversariosCampo2[j].x<<","<< posAdversariosCampo2[j].y<<")" ;
	//	}

	// recoloca na melhor ordem
	PosXY posAdversarios[NUM_ROBOS_TIME];
	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
		posAdversarios[j] = posAdversariosCampo2[comb[indMenorDesloc][j]];
	}
	for (int j = 0; j < NUM_ROBOS_TIME; j++) {
		posAdversariosCampo2[j] = posAdversarios[j];
	}
	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		if (abs(posAdversariosCampo1[i].x - posAdversariosCampo2[i].x) > VELOCIDADE_MAXIMA / 60.0 || abs(posAdversariosCampo1[i].y - posAdversariosCampo2[i].y) > VELOCIDADE_MAXIMA / 60.0)
			posAdversariosCampo2[i].x = NAO_EXISTE;
	}
}

*/

void posicoes(guint8 *pImgDigi, guint8 rgb_paleta[], int indCamera) {
	LimitesCampo limitesCampo = futCam[indCamera]->limitesCampo;
	separaEtiquetas(pImgDigi, rgb_paleta, indCamera);

	//	cout << "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii" << endl;
	encontraObjetos(0, posNossosRobosCampo1, posBolaCampo1, posAdversariosCampo1, indCamera); // 0 indica linhas impares
	//
	//	for (int i = 0; i < NUM_ROBOS_TIME; i++)
	//		cout << "time (" << posNossosRobosCampo1[i].x << ","
	//				<< posNossosRobosCampo1[i].y << ")-("
	//				<< posNossosRobosCampo1[i].xAux << " "
	//				<< posNossosRobosCampo1[i].yAux << ") \t ";
	//
	//	cout << "\nbola (" << posBolaCampo1.x << "," << posBolaCampo1.y << ")\n";
	//	for (int i = 0; i < NUM_ROBOS_TIME; i++)
	//		cout << "adve (" << posAdversariosCampo1[i].x << ","
	//				<< posAdversariosCampo1[i].y << ") \t ";
	//
	//	cout << endl;

	//	cout << "ppppppppppppppppppppppppppppppppppp" << endl;
	encontraObjetos(1, posNossosRobosCampo2, posBolaCampo2, posAdversariosCampo2, indCamera); // 0 indica linhas pares
	//
	//	for (int i = 0; i < NUM_ROBOS_TIME; i++)
	//		cout << "time (" << posNossosRobosCampo2[i].x << ","
	//				<< posNossosRobosCampo2[i].y << ")-("
	//				<< posNossosRobosCampo2[i].xAux << " "
	//				<< posNossosRobosCampo2[i].yAux << ") \t ";
	//
	//	cout << "\nbola (" << posBolaCampo2.x << "," << posBolaCampo2.y << ")\n";
	//	for (int i = 0; i < NUM_ROBOS_TIME; i++)
	//		cout << "adve (" << posAdversariosCampo2[i].x << ","
	//				<< posAdversariosCampo2[i].y << ") \t ";
	//
	//	cout << endl;

	for (int i = 0; i < NUM_ROBOS_TIME * 2 + 1; i++) {
		estadoAnt[i] = estado[i];
	}

	/** calcula o vetor de estados dos objetos no jogo */

	for (int i = 0; i < NUM_ROBOS_TIME; i++) {
		float anguloCampo1, anguloCampo2;
		if (posNossosRobosCampo1[i].x != NAO_EXISTE) {
			anguloCampo1 = atang2(posNossosRobosCampo1[i].y - posNossosRobosCampo1[i].yAux, posNossosRobosCampo1[i].x - posNossosRobosCampo1[i].xAux) - 45;
			if (anguloCampo1<0)
				anguloCampo1+=360;
//			if (i==0)
//			cout<< "{"<<anguloCampo1;
			if (posNossosRobosCampo2[i].x != NAO_EXISTE) {
				anguloCampo2 = atang2(posNossosRobosCampo2[i].y - posNossosRobosCampo2[i].yAux, posNossosRobosCampo2[i].x - posNossosRobosCampo2[i].xAux) - 45;
				if (anguloCampo2<0)
					anguloCampo2+=360;
//				if (i==0)
//				cout<< ";"<<anguloCampo2<< "}"<<endl;
				/* calcula angulo e dAngulo */
				int dAng;
				estado[i].dAngulo = dAng = difAngulos(anguloCampo2,anguloCampo1);
				if (dAng > 180)
					dAng %= 180;
				if (dAng < 12) { //32 graus é a velocidade máxima de rotacao do robo, mas nunca será usada
					/* media ponderada, 0,75 campo 2 + 0,25 campo 1 */
					estado[i].angulo = anguloCampo2 - estado[i].dAngulo / 4;
				} else { // se maior, indica que algum valor foi medido errado
					// usa o valor anterior para tentar escolher o angulo menos errado
					int angAnt = estadoAnt[i].angulo;
					if (difAngulosAbs(angAnt, anguloCampo1) > difAngulosAbs(angAnt, anguloCampo2)) {
						estado[i].dAngulo = difAngulos(anguloCampo2, angAnt);
						estado[i].angulo = anguloCampo2;
					} else {
						estado[i].dAngulo = difAngulos(anguloCampo1, angAnt) * 2;// *2 pois o campo1 esta a 1/60 s do angulo anterior
						estado[i].angulo = anguloCampo1;
					}
				}
				/* calcula posicao e deslocamento */
				/* media ponderada, 0,75 campo 2 + 0,25 campo 1 */
				float xCampo1 = (posNossosRobosCampo1[i].x + posNossosRobosCampo1[i].xAux) / 2;
				float xCampo2 = (posNossosRobosCampo2[i].x + posNossosRobosCampo2[i].xAux) / 2;
				estado[i].x = (xCampo2 * 3 + xCampo1) / 4;
				estado[i].dx = (xCampo2 - xCampo1) * 2; //*2 para colocar em 1/30 s
				float yCampo1 = (posNossosRobosCampo1[i].y + posNossosRobosCampo1[i].yAux) / 2;
				float yCampo2 = (posNossosRobosCampo2[i].y + posNossosRobosCampo2[i].yAux) / 2;
				estado[i].y = (yCampo2 * 3 + yCampo1) / 4;
				estado[i].dy = (yCampo2 - yCampo1) * 2; //*2 para colocar em 1/30 s
			} else {// nao foi encontrado no segundo campo da imagem
				int angAnt = estadoAnt[i].angulo;
				estado[i].dAngulo = difAngulos(anguloCampo1, angAnt);
				estado[i].angulo = anguloCampo1;
				estado[i].x = (posNossosRobosCampo1[i].x + posNossosRobosCampo1[i].xAux) / 2;
				estado[i].y = (posNossosRobosCampo1[i].y + posNossosRobosCampo1[i].yAux) / 2;
				estado[i].dx = estado[i].x - estadoAnt[i].x;
				estado[i].dy = estado[i].y - estadoAnt[i].y;
			}
		} else { // nao foi encontrado no primeiro campo da imagem
			if (posNossosRobosCampo2[i].x  != NAO_EXISTE) {
				anguloCampo2 = atang2(posNossosRobosCampo2[i].y - posNossosRobosCampo2[i].yAux, posNossosRobosCampo2[i].x - posNossosRobosCampo2[i].xAux) - 45;
				if (anguloCampo1<0)
					anguloCampo1+=360;
				int angAnt = estadoAnt[i].angulo;
				estado[i].dAngulo = difAngulos(anguloCampo2, angAnt);
				estado[i].angulo = anguloCampo2;
				estado[i].x = (posNossosRobosCampo2[i].x + posNossosRobosCampo2[i].xAux) / 2;
				estado[i].y = (posNossosRobosCampo2[i].y + posNossosRobosCampo2[i].yAux) / 2;
				estado[i].dx = estado[i].x - estadoAnt[i].x;
				estado[i].dy = estado[i].y - estadoAnt[i].y;
			} else { // este robo nao foi encontrado em nenhum dos campos da imagem
				estado[i].dAngulo = estadoAnt[i].dAngulo;
				estado[i].angulo = estadoAnt[i].angulo;
				estado[i].x = estadoAnt[i].x;
				estado[i].y = estado[i].y;
				estado[i].dx = estadoAnt[i].dx;
				estado[i].dy = estadoAnt[i].dy;
			}
		}
	}

	if (posBolaCampo1.x  != NAO_EXISTE) { // a bola pode ser vista no campo 1 da imagem?
		if (posBolaCampo2.x  != NAO_EXISTE) { // a bola pode ser vista no campo 2 da imagem?
			estado[IND_BOLA].x = (posBolaCampo2.x * 3 + posBolaCampo1.x) / 4;
			estado[IND_BOLA].y = (posBolaCampo2.y * 3 + posBolaCampo1.y) / 4;
			estado[IND_BOLA].dx = (posBolaCampo2.x - posBolaCampo1.x) * 2; // 2* para transformas de 1/60 para 1/30 s
			estado[IND_BOLA].dy = (posBolaCampo2.y - posBolaCampo1.y) * 2;
			estado[IND_BOLA].angulo = atang2(estado[IND_BOLA].dy, estado[IND_BOLA].dx);
			estado[IND_BOLA].dAngulo = difAngulos(estado[IND_BOLA].angulo, estadoAnt[IND_BOLA].angulo);
		} else {// a bola nao pode ser vista no campo 2 da imagem, só no campo 1
			estado[IND_BOLA].x = posBolaCampo1.x;
			estado[IND_BOLA].y = posBolaCampo1.y;
			estado[IND_BOLA].dx = (estadoAnt[IND_BOLA].x - posBolaCampo1.x) * 2; // 2* para transformas de 1/60 para 1/30 s
			estado[IND_BOLA].dy = (estadoAnt[IND_BOLA].y - posBolaCampo1.y) * 2;
			estado[IND_BOLA].angulo = atang2(estado[IND_BOLA].dy, estado[IND_BOLA].dx);
			estado[IND_BOLA].dAngulo = difAngulos(estado[IND_BOLA].angulo, estadoAnt[IND_BOLA].angulo);
		}
	} else { // a bola nao pode ser vista no campo 1 da imagem
		if (posBolaCampo2.x  != NAO_EXISTE) { // mas pode ser vista no campo 2
			estado[IND_BOLA].x = posBolaCampo2.x;
			estado[IND_BOLA].y = posBolaCampo2.y;
			estado[IND_BOLA].dx = (estadoAnt[IND_BOLA].x - posBolaCampo2.x); // 2* para transformas de 1/60 para 1/30 s
			estado[IND_BOLA].dy = (estadoAnt[IND_BOLA].y - posBolaCampo2.y);
			estado[IND_BOLA].angulo = atang2(estado[IND_BOLA].dy, estado[IND_BOLA].dx);
			estado[IND_BOLA].dAngulo = difAngulos(estado[IND_BOLA].angulo, estadoAnt[IND_BOLA].angulo);
		} else { // a bola nao foi vista neta imagem (nem no campo 1 nem no campo 2)
			estado[IND_BOLA].x = estadoAnt[IND_BOLA].x;
			estado[IND_BOLA].y = estadoAnt[IND_BOLA].y;
			estado[IND_BOLA].dx = estadoAnt[IND_BOLA].dx;
			estado[IND_BOLA].dy = estadoAnt[IND_BOLA].dy;
			estado[IND_BOLA].angulo = estadoAnt[IND_BOLA].angulo;
			estado[IND_BOLA].dAngulo = estadoAnt[IND_BOLA].dAngulo;
		}
	}

	combinaAdvesariosEntreCamposEAnterior(posAdversariosCampo1, posAdversariosCampo2, estadoAnt);

	int j = 0;
	for (int i = IND_BOLA + 1; i < NUM_ROBOS_TIME * 2 + 1; i++, j++) {
		if (posAdversariosCampo1[j].x  != NAO_EXISTE) { // o adversario i pode ser vista no campo 1 da imagem?
			if (posAdversariosCampo2[j].x  != NAO_EXISTE) { // o adversario i pode ser vista no campo 2 da imagem?
				estado[i].x = (posAdversariosCampo2[j].x * 3 + posAdversariosCampo1[j].x) / 4;
				estado[i].y = (posAdversariosCampo2[j].y * 3 + posAdversariosCampo1[j].y) / 4;
				estado[i].dx = (posAdversariosCampo2[j].x - posAdversariosCampo1[j].x) * 2; // 2* para transformas de 1/60 para 1/30 s
				estado[i].dy = (posAdversariosCampo2[j].y - posAdversariosCampo1[j].y) * 2;
				estado[i].angulo = atang2(estado[i].dy, estado[i].dx);
				estado[i].dAngulo = difAngulos(estado[i].angulo, estadoAnt[i].angulo);
			} else {// o adversario i nao pode ser vista no campo 2 da imagem, só no campo 1
				estado[i].x = posAdversariosCampo1[j].x;
				estado[i].y = posAdversariosCampo1[j].y;
				estado[i].dx = (estadoAnt[i].x - posAdversariosCampo1[j].x) * 2; // 2* para transformas de 1/60 para 1/30 s
				estado[i].dy = (estadoAnt[i].y - posAdversariosCampo1[j].y) * 2;
				estado[i].angulo = atang2(estado[i].dy, estado[i].dx);
				estado[i].dAngulo = difAngulos(estado[i].angulo, estadoAnt[i].angulo);
			}
		} else { // o adversario i nao pode ser vista no campo 1 da imagem
			if (posAdversariosCampo2[j].x  != NAO_EXISTE) { // mas pode ser vista no campo 2
				estado[i].x = posAdversariosCampo2[j].x;
				estado[i].y = posAdversariosCampo2[j].y;
				estado[i].dx = (estadoAnt[i].x - posAdversariosCampo2[j].x); // 2* para transformas de 1/60 para 1/30 s
				estado[i].dy = (estadoAnt[i].y - posAdversariosCampo2[j].y);
				estado[i].angulo = atang2(estado[i].dy, estado[i].dx);
				estado[i].dAngulo = difAngulos(estado[i].angulo, estadoAnt[i].angulo);
			} else { // o adversario i nao foi vista neta imagem (nem no campo 1 nem no campo 2)
				estado[i].x = estadoAnt[i].x;
				estado[i].y = estadoAnt[i].y;
				estado[i].dx = estadoAnt[i].dx;
				estado[i].dy = estadoAnt[i].dy;
				estado[i].angulo = estadoAnt[i].angulo;
				estado[i].dAngulo = estadoAnt[i].dAngulo;
			}
		}
	}

	if (!emJogo) {
		for (int i = 0; i < NUM_ROBOS_TIME * 2 + 1; i++) {
			printf("(%2d,%2d,%2d) ", (int)estado[i].x, (int)estado[i].y, (int)estado[i].angulo);
		}
		cout << endl;
	}

	estrategia();
}
