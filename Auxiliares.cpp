/*
 * Auxiliares.cpp
 *
 *  Created on: Aug 21, 2009
 *      Author: futebol
 */

#include "Auxiliares.h"
extern bool emJogo;

Props::Props(string nomeArq) {
	this->nomeArq = nomeArq;
	carrega();
}

void Props::carrega() {
	clear();
	ifstream fs(nomeArq.c_str());
	if (fs.fail())
		return;
	string linha;
	getline(fs, linha);
	while (!fs.eof()) {
		getline(fs, linha);
		int pos = linha.find('=');
		if (pos == 0)
			continue;
		(*this)[linha.substr(0, pos)] = linha.substr(pos + 1);
		;
	}
}

string Props::get(string chave, string padrao) {
	if (find(chave) == end())
		return padrao;
	return (*this)[chave];
}

int Props::getInt(string chave, int padrao) {
	if (find(chave) == end())
		return padrao;
	return atoi((*this)[chave].c_str());
}

int Props::getFloat(string chave, float padrao) {
	if (find(chave) == end())
		return padrao;
	return atof((*this)[chave].c_str());
}

void Props::put(string chave, string valor) {
	(*this)[chave] = valor;
}

void Props::put(string chave, int valor) {
	char valStr[20];
	sprintf(valStr, "%d", valor);
	string s = valStr;
	(*this)[chave] = s;
}

void Props::put(string chave, float valor) {
	char valStr[20];
	sprintf(valStr, "%g", valor);
	string s = valStr;
	(*this)[chave] = s;
}

void Props::salva(string header) {
	ofstream fs(nomeArq.c_str());
	if (fs.fail()) {
		cout << "Os parametros nao podem ser salvos" << endl;
		return;
	}
	fs << '#' << header << endl;
	for (map<string, string>::iterator it = begin(); it != end(); it++) {
		fs << it->first << "=" << it->second << endl;
	}
}

Props::~Props() {
	//		salva("Futebol de Robos");
}

/** ang1 - ang2 */
float difAngulos(float ang1, float ang2) {
	float dAng = ang1 - ang2;
	while (dAng < 0)
		dAng += 360;
	while (dAng >= 360)
		dAng -= 360;
	return dAng;
}

/** ang2 - ang1 pelo lado mais curto da circunferencia */
float difAngulosAbs(float ang1, float ang2) {
	float dAng = abs(ang1 - ang2);
	while (dAng >= 180)
		dAng -= 180;
	return dAng;
}

void geraMatrizComb(guint8 comb[MAX_CONT_COMB][NUM_ROBOS_TIME]) {
	int indsComb[NUM_ROBOS_TIME];
	int contComb = 0;
	for (int i = 0; i < NUM_ROBOS_TIME; i++)
		indsComb[i] = 0;
	int indComb = 0;
	do {
		continue1:
		// o "for" abaixo cria combinacao possiveis
		/** 			indComb[0]	indComb[1]	indComb[2]	...
		 * 1a combinacao:		0				0				0
		 * 2a combinacao		1				0				0
		 * 3a combinacao		2				0				0
		 */
		for (indComb = 0; ++indsComb[indComb] == NUM_CORES_AUX && indComb
				< NUM_ROBOS_TIME; indComb++) {
			indsComb[indComb] = 0;
		}

		//		cout << "\n->" << indEtiqAux[0] << " " << indEtiqAux[1] << " " << indEtiqAux[2] << endl;

		if (indComb == NUM_ROBOS_TIME)
			break; // já foi a ultima combinacao, termina

		for (int i1 = 0; i1 < NUM_ROBOS_TIME; i1++) {
			for (int i2 = i1 + 1; i2 < NUM_ROBOS_TIME; i2++)
				if (indsComb[i1] == indsComb[i2])
					// existem etiquetas duplicadas na combinacao, calcula proxima combinacao
					goto continue1;
		}

		for (int i = 0; i < NUM_ROBOS_TIME; i++) {
			comb[MAX_CONT_COMB - contComb - 1][i] = indsComb[i];
		}

		contComb++;
	} while (indComb != NUM_CORES_AUX);

}

void erro(string msg) {
	cout << "ERRO: " << msg << endl;
	cin.get();
}

void erro(string msg, int errno) {
	cout << "ERRO: " << msg << errno << " " << strerror(errno) << endl;
	cin.get();
}

void gravaLog(char *s) {
	if (emJogo)
		return;
	FILE *fp = fopen("debug.txt", "at");
	fprintf(fp, s);
	fflush(fp);
	fclose(fp);
}

/** classe que repreesenta uma camera com todas as variaveis necessárias para obter as informacoes de uma imagem da camera.
 esta classe foi criada para simplificar a utilizacao de duas cameras  */
FutebolCamera::FutebolCamera(int indCamera) {
	contImgsCampoVazio = 0;
	contImgsIntervalo = 0;
	nivelCampoVazio = 0;
	this->indCamera = indCamera;
	nossoLado = ESQUERDA;
	props = new Props("futebol.parametros");
}

void FutebolCamera::recalculaPixelsPosCentimetros() {
	tamIdealXBlob = 4 * (limitesCampo.maxXSemGol - limitesCampo.minXSemGol)
			/ TAM_X_CAMPO_SEM_GOL - 1; // (4cm) ideal (-2 pois os limites (contorno) nao sao vistos
	tamIdealYBlob = 4 * (limitesCampo.maxY - limitesCampo.minY) / TAM_Y_CAMPO
			- 1; // ideal;
	minLimiteXBlob = tamIdealXBlob / 2; // 50% do ideal
	minLimiteYBlob = tamIdealYBlob / 2;
	maxLimiteXBlob = tamIdealXBlob * 3; // 3 vezes pois quando a etiqueta esta se movimentando o rastro aul]menta a etiqueta
	maxLimiteYBlob = tamIdealYBlob * 3;

	numIdealPixelsBlob = (tamIdealXBlob + tamIdealYBlob) / 4; // (media)/2
	numIdealPixelsBlob = numIdealPixelsBlob * numIdealPixelsBlob * 3.14 / 2; // (quadrado de R) * PI (/2 é pois apenas as linhas de um campo sao contadas por vez)
	numMinPixelsBlob = numIdealPixelsBlob / 4; //como a area é o quadrado, divide 2 ao quadrado)
	numMaxPixelsBlob = numIdealPixelsBlob * (3 * 3);
	distIdealEtiquetas = 4
			* (limitesCampo.maxXSemGol - limitesCampo.minXSemGol)
			/ TAM_X_CAMPO_SEM_GOL;
	// considerando que apenas as partes mais internas dos circulos (etiquetas) sao visiveis
	minDistEtiquetas = distIdealEtiquetas / 2;
	// considerando que apenas as partes mais externas dos circulos (etiquetas) sao visiveis
	maxDistEtiquetas = distIdealEtiquetas * 1.5;
	maxPixelsPorSegundo = (limitesCampo.maxXSemGol - limitesCampo.minXSemGol)
			* VELOCIDADE_MAXIMA / TAM_X_CAMPO_SEM_GOL; // 200cm/s
}

void FutebolCamera::converteRGB24Paleta(guint8 *pImgDigi, Cor *pImgCap,
		Cor *pCampoVazio) {
	int linInicio = 2, linFinal = 480 - linInicio;

	guint8 *pAux = pImgDigi;

	linInicio = max(linInicio, limitesCampo.minY - 15);
	linFinal = min(linFinal, limitesCampo.maxY+15);
	int posLinInicio = linInicio * 640;
	int posLinFinal = linFinal * 640;

	pImgCap += posLinInicio;
	pImgDigi += posLinInicio;
	pCampoVazio += posLinInicio;

	for (int i = posLinInicio; i < posLinFinal; i++, pImgCap++, pImgDigi++, pCampoVazio++) {
		int r = pImgCap->r;
		int g = pImgCap->g;
		int b = pImgCap->b;
		if (abs(r - pCampoVazio->r) > nivelCampoVazio
				|| abs(g - pCampoVazio->g) > nivelCampoVazio || abs(b
				- pCampoVazio->b) > nivelCampoVazio) {
			int j = ((r & 0xf8) << 7) + ((g & 0xf8) << 2) + ((b & 0xf8) >> 3);
			*pImgDigi = rgb_paleta[j];
		} else {
			*pImgDigi = SEM_COR;
		}

	}
	pImgDigi = pAux;

	/* coloca cor sentinela nas extremidades da imagem */
	memset(pImgDigi, SENTINELA, posLinInicio);
	memset(pImgDigi + posLinFinal, SENTINELA, TAM_IMAGEM_640x480x1
			- posLinFinal);
	pImgDigi += posLinInicio; //salta as duas primeiras linhas
	guint8 *p = pImgDigi;

	//15 é para ver o robonas laterais e dentro do gol
	int linInicioGol = linInicio + (linFinal - linInicio) * (TAM_Y_CAMPO
			- TAM_Y_DO_GOL) / 2 / TAM_Y_CAMPO -15;
	int linFinalGol = linInicioGol + (linFinal - linInicio) * TAM_Y_DO_GOL
			/ TAM_Y_CAMPO+15;
	int minXSemGol = max(limitesCampo.minXSemGol-15, GAP);
	int maxXSemGol = min(limitesCampo.maxXSemGol+15, 640-GAP);
	int minX = max(limitesCampo.minX-15, GAP);
	int maxX = min(limitesCampo.maxX+15, 640-GAP);
	for (int i = linInicio; i < linInicioGol; i++) {
		memset(p, SEM_COR, minXSemGol);
		p += maxXSemGol;
		memset(p, SEM_COR, 640 - maxXSemGol);
		p += 640 - maxXSemGol;
	}
	for (int i = linInicioGol; i < linFinalGol; i++) {
		memset(p, SEM_COR, minX);
		p += maxX;
		memset(p, SEM_COR, 640 - maxX);
		p += 640 - maxX;
	}
	for (int i = linFinalGol; i < linFinal; i++) {
		memset(p, SEM_COR, minXSemGol);
		p += maxXSemGol;
		memset(p, SEM_COR, 640 - maxXSemGol);
		p += 640 - maxXSemGol;
	}
}

void FutebolCamera::processa() {

	converteRGB24Paleta(pImgDigi, pImgCap, pCampoVazio);

	if (!emJogo) { //	salva a imagem gerada por converteRGB24Paleta
		memcpy(pImgAux, pImgDigi, TAM_IMAGEM_640x480x1);
	}

	nucleoFutebol(pImgDigi, rgb_paleta);

	if (!emJogo) { //	restaura a imagem com cores tranformadas, como antes do processamento da segmentacao
		memcpy(pImgDigi, pImgAux, TAM_IMAGEM_640x480x1);
	}
}

void FutebolCamera::preparaCampoVazio() {
	if (++contImgsIntervalo >= NUM_IMGS_INTERVALO_CAMPO_VAZIO) {
		contImgsIntervalo = 0;
		memcpy(imgs[contImgsCampoVazio], pImgCap, TAM_IMAGEM_640x480x3);
		if (contImgsCampoVazio == 1) {
			int somaR, somaB, somaG;
			for (int i = 0; i < TAM_IMAGEM_640x480x1; i++) {
				somaR = 0;
				somaG = 0;
				somaB = 0;
				for (int j = 0; j < NUM_IMGS_CAMPO_VAZIO; j++) {
					somaR += imgs[j][i].r;
					somaG += imgs[j][i].g;
					somaB += imgs[j][i].b;
				}
				pCampoVazio[i].r = somaR / NUM_IMGS_CAMPO_VAZIO;
				pCampoVazio[i].g = somaG / NUM_IMGS_CAMPO_VAZIO;
				pCampoVazio[i].b = somaB / NUM_IMGS_CAMPO_VAZIO;
			}
		}
		contImgsCampoVazio--;
	}
}

void FutebolCamera::iniciaVetoresRGBPaleta() {
	int ax, bx, lum;
	for (int i = 0; i <= 0x7fff; i++) {
		int r = (i & 0x7C00) >> 7, g = (i & 0x3E0) >> 2, b = (i & 0x01f) << 3;
		RGB_SCT(r, g, b, ax, bx, lum);

		int cor = SEM_COR; //sem cor conhecida
		for (int indCor = COR_BOLA; indCor < MAX_IND_CORES; indCor++) {
			if (aInicio[indCor] <= ax && ax <= aFinal[indCor]
					&& bInicio[indCor] <= bx && bx <= bFinal[indCor]
					&& lumInicio[indCor] <= lum && lum <= lumFinal[indCor]) {
				cor = indCor;
				break;
			}
		}
		rgb_paleta[i] = cor;
	}
}

void FutebolCamera::leParametros() {
	leParametros(aInicio, aFinal, "A");
	leParametros(bInicio, bFinal, "B");
	leParametros(lumInicio, lumFinal, "I");

	string strIndCam = "[ ]";
	strIndCam[1] = (char) (indCamera + '0');

	nivelCampoVazio = props->getInt(strIndCam + "NivelCampoVazio", 0);
	limitesCampo.minXSemGol = props->getInt(strIndCam
			+ "limitesCampo.minXSemGol", 10);
	limitesCampo.maxXSemGol = props->getInt(strIndCam
			+ "limitesCampo.maxXSemGol", 620);
	limitesCampo.minX = props->getInt(strIndCam + "limitesCampo.minX", 72);
	limitesCampo.maxX = props->getInt(strIndCam + "limitesCampo.maxX", 550);
	limitesCampo.minY = props->getInt(strIndCam + "limitesCampo.minY", 10);
	limitesCampo.maxY = props->getInt(strIndCam + "limitesCampo.maxY", 460);
	recalculaPixelsPosCentimetros();
	nossoLado = (Lado) props->getInt(strIndCam + "nossoLado", ESQUERDA);
	nivelCampoVazio = props->getInt(strIndCam + "nivelCampoVazio", 0);

	leImgCampoVazio();
}


void FutebolCamera::leImgCampoVazio() {
	string strIndCam = " .png";
	strIndCam[0] = (char) (indCamera + '0');
	strIndCam = "CampoVazioCamera"+strIndCam;
	cout<<strIndCam<<"}"<<endl;

	try {
	Glib::RefPtr<Gdk::Pixbuf> imgCampVazio = Gdk::Pixbuf::create_from_file(
			strIndCam, 640, 480);
	memcpy(pCampoVazio, imgCampVazio->get_pixels(), TAM_IMAGEM_640x480x3);
	} catch (Glib::FileError e) {
		memset(pCampoVazio, 0, TAM_IMAGEM_640x480x3);
	}
}

void FutebolCamera::leParametros(int inicio[], int final[], string canal) {
	char conv[MAX_IND_CORES] = { '0', '1', '2', '3', '4', '5' };

	string strIndCam = "[ ]";
	strIndCam[1] = conv[indCamera];

	for (int i = 0; i < MAX_IND_CORES; i++) {
		inicio[i] = props->getInt(strIndCam + "InicioFaixaCanal" + canal
				+ "Cor" + conv[i]);
		final[i] = props->getInt(strIndCam + "FinalFaixaCanal" + canal + "Cor"
				+ conv[i]);
	}
	iniciaVetoresRGBPaleta();
}

void FutebolCamera::salvaParametros() {
	salvaParametros(aInicio, aFinal, "A");
	salvaParametros(bInicio, bFinal, "B");
	salvaParametros(lumInicio, lumFinal, "I");

	string strIndCam = "[ ]";
	strIndCam[1] = (char) (indCamera + '0');

	props->put(strIndCam + "NivelCampoVazio", nivelCampoVazio);
	props->put(strIndCam + "limitesCampo.minXSemGol", limitesCampo.minXSemGol);
	props->put(strIndCam + "limitesCampo.maxXSemGol", limitesCampo.maxXSemGol);
	props->put(strIndCam + "limitesCampo.minX", limitesCampo.minX);
	props->put(strIndCam + "limitesCampo.maxX", limitesCampo.maxX);
	props->put(strIndCam + "limitesCampo.minY", limitesCampo.minY);
	props->put(strIndCam + "limitesCampo.maxY", limitesCampo.maxY);
	props->put(strIndCam + "nivelCampoVazio", nivelCampoVazio);
	props->put(strIndCam + "nossoLado", nossoLado);

	props->salva("Futebol de Robos");
	salvaImgCampoVazio();
}

void FutebolCamera::salvaImgCampoVazio() {
	Cor pImgAux[TAM_IMAGEM_640x480x3];

	string strIndCam = " .png";
	strIndCam[0] = (char) (indCamera + '0');
	strIndCam = "CampoVazioCamera"+strIndCam;

	memcpy(pImgAux, pImgCap, TAM_IMAGEM_640x480x3); //necessario somente para desenvolvimento (sem video)
	memcpy(pImgCap, pCampoVazio, TAM_IMAGEM_640x480x3);
	imgCap->save(strIndCam, "png");
	memcpy(pImgCap, pImgAux, TAM_IMAGEM_640x480x3); //necessario somente para desenvolvimento (sem video)
}

void FutebolCamera::salvaParametros(int inicio[], int final[], string canal) {
	char conv[MAX_IND_CORES] = { '0', '1', '2', '3', '4', '5' };

	string strIndCam = "[ ]";
	strIndCam[1] = conv[indCamera];

	for (int i = 0; i < MAX_IND_CORES; i++) {
		props->put(strIndCam + "InicioFaixaCanal" + canal + "Cor" + conv[i],
				inicio[i]);
		props->put(strIndCam + "FinalFaixaCanal" + canal + "Cor" + conv[i],
				final[i]);
	}
}

FutebolCamera::~FutebolCamera() {
	delete props;
}
