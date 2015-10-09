#include "Auxiliares.h"
#include "Captura.h"
#include "Posicoes.h"
#include "Serial.h"

//#define VIDEO

#ifdef VIDEO	// so tem sentido usao SALVA no ARQ se esta capturando VIDEO
//#define SALVA_ARQ "imgJogoReal5.png"
#endif

#define IMG_INICIAL "imgJogoReal5.png"

// --- VARIAVEIS GLOBAIS ---

Cor coresRepresenta[MAX_IND_CORES]; //cores padroes de cada etiqueta

// Indice de cada etiqueta
enum IndCores indCores;

/** Vetor com todas as combinacoes possíveis. Usado para encontrar a combinacao ótima de etiquetas, robos, entre imagens quadros e posicoes */
guint8 comb[MAX_CONT_COMB][NUM_ROBOS_TIME];

/** Mantem o histograma do canal A, do B e do I(lum) criado pelos cliques do mouse ma imgOrig */
int aHisto[256], bHisto[256], lumHisto[256];

/** se verdadeira apresenta graficos sobre a imagem digital para mostrar como os elementos estao sendo encontrados */
bool graficos = true;

bool emJogo = false;

int indGoleiro = 0;
int indVolante = 1;
int indAtacante = 2;
bool emPenalidade = false;
bool emPosiciona = false;
bool emInicio = false;
bool tiroMeta = false;

Objetivo objetivoRobo[NUM_ROBOS_TIME];

int indCamEmUso = 0;

Estado estado[NUM_ROBOS_TIME * 2 + 1], estadoAnt[NUM_ROBOS_TIME * 2 + 1], estadoPrev[NUM_ROBOS_TIME * 2 + 1];

class FutCamera: public FutebolCamera {
public:
	FutCamera(int indCamera) :
		FutebolCamera(indCamera) {
	}

	void nucleoFutebol(guint8 *pImgDigi, guint8 rgb_paleta[]) {
		posicoes(pImgDigi, rgb_paleta, indCamera);
	}
};

FutCamera *futCam[NUM_CAMERAS];

class CapVideo
#ifdef VIDEO
: public Captura {
#else
{
public:
	int numErro;
#endif
	Gtk::Window *jan;
	int indCamera;
public:
	CapVideo(char * devName, int videoInput, Gtk::Window *jan, int indCamera)
#ifdef VIDEO
	:
	Captura(devName, videoInput)
#endif
	{
		this->jan = jan;
		this->indCamera = indCamera;

#ifdef VIDEO
		v4l2_control ctrl;
		ctrl.id = V4L2_CID_SATURATION;
		//ctrl.value=64;// saturacao original
		ctrl.value = 127;

		if (-1 == ioctl(getFD(), VIDIOC_S_CTRL, &ctrl))
		cout << "erro v4l2_control" << ctrl.value << "  " << errno << "  "
		<< strerror(errno) << endl;
		else
		cout << "valor " << ctrl.value << endl;
#else
		numErro = 0;
#endif
	}

	virtual void video(const void * p) {
		if (p) {
			memcpy(futCam[indCamera]->pImgCap, p, TAM_IMAGEM_640x480x3);
			if (!emJogo) {

#ifdef SALVA_ARQ
				Cor *p = futCam[indCamera]->pImgCap;
				for (int i = 0; i < TAM_IMAGEM_640x480x1; i++, p++) {
					guint8 t = p->r;
					p->r = p->b;
					p->b = t;
				}
				futCam[indCamera]->imgCap->save(SALVA_ARQ, "png");
#endif

				Glib::RefPtr<Gdk::Window> win = jan->get_window();
				if (win) {
					win->invalidate(true);
				}

				if (futCam[indCamera]->contImgsCampoVazio > 0) { // se valor que zero, captura imagens e calcula o campo vazio
					futCam[indCamera]->preparaCampoVazio(); // captura images para calcular a media
				}
			}
			futCam[indCamera]->processa();
		}
	}
};

class SelecionaCor: public Gtk::VBox {
public:
	int *aHisto, *bHisto, *lumHisto;
	Gtk::RadioButton rbCorBola, rbCorTime, rbCorContra, rbCorAux1, rbCorAux2, rbCorAux3;
	SelecionaCor(int *aHisto, int *bHisto, int *lumHisto) :
		rbCorBola("Bola"), rbCorTime("Time"), rbCorContra("Contra"), rbCorAux1("Aux1"), rbCorAux2("Aux2"), rbCorAux3("Aux3") {
		this->aHisto = aHisto;
		this->bHisto = bHisto;
		this->lumHisto = lumHisto;
		Gtk::RadioButton::Group grupo = rbCorBola.get_group();
		rbCorTime.set_group(grupo);
		rbCorContra.set_group(grupo);
		rbCorAux1.set_group(grupo);
		rbCorAux2.set_group(grupo);
		rbCorAux3.set_group(grupo);
		add(rbCorBola);
		add(rbCorTime);
		add(rbCorContra);
		add(rbCorAux1);
		add(rbCorAux2);
		add(rbCorAux3);
		rbCorBola.signal_clicked().connect(sigc::mem_fun(*this, &SelecionaCor::on_clicked));
		rbCorTime.signal_clicked().connect(sigc::mem_fun(*this, &SelecionaCor::on_clicked));
		rbCorContra.signal_clicked().connect(sigc::mem_fun(*this, &SelecionaCor::on_clicked));
		rbCorAux1.signal_clicked().connect(sigc::mem_fun(*this, &SelecionaCor::on_clicked));
		rbCorAux2.signal_clicked().connect(sigc::mem_fun(*this, &SelecionaCor::on_clicked));
		rbCorAux3.signal_clicked().connect(sigc::mem_fun(*this, &SelecionaCor::on_clicked));
	}

	virtual void on_clicked() {
		for (int i = 0; i < 256; i++) {
			aHisto[i] = bHisto[i] = lumHisto[i] = 0;
		}
		Glib::RefPtr<Gdk::Window> win = get_window();
		if (win) {
			win->invalidate(true);
		}

	}

	IndCores estado() {
		if (rbCorBola.get_active())
			return COR_BOLA;
		if (rbCorTime.get_active())
			return COR_TIME;
		if (rbCorContra.get_active())
			return COR_CONTRA;
		if (rbCorAux1.get_active())
			return COR_AUX1;
		if (rbCorAux2.get_active())
			return COR_AUX2;
		if (rbCorAux3.get_active())
			return COR_AUX3;
		else {
			erro("sem cor definica na interface com usuário");
			return SEM_COR;
		}
	}

};

class Imagem: public Gtk::DrawingArea {
public:
	int larg, altu;
	Imagem(int larg, int alt) {
		this->altu = alt;
		this->larg = larg;
		set_size_request(larg, alt);
	}
protected:
	virtual bool on_expose_event(GdkEventExpose* event) {
		//cout << "Imagem" << endl;
		Glib::RefPtr<Gdk::Window> win = get_window();
		if (win) {
			Cairo::RefPtr<Cairo::Context> cr = win->create_cairo_context();
			cr->set_source_rgb(0.0, 0.0, 0.0);
			cr->rectangle(event->area.x, event->area.y, event->area.width, event->area.height);
			cr->stroke();
		}
		return true;
	}
};

class ImagemOriginal: public Imagem {
public:
	int *aHisto, *bHisto, *lumHisto;
	int posX, posY, origX, origY;
	double escala;
	Glib::RefPtr<Gdk::Pixbuf> imgOrig;

	ImagemOriginal(int larg, int alt, int *aHisto, int *bHisto, int *lumHisto) :
		Imagem(larg, alt), posX(0), posY(0), escala(0.5), origX(0), origY(0) {
		this->aHisto = aHisto;
		this->bHisto = bHisto;
		this->lumHisto = lumHisto;
		imgOrig = Gdk::Pixbuf::create_from_file(IMG_INICIAL, larg, altu);
		set_events(Gdk::BUTTON_PRESS_MASK);
	}

	virtual bool on_expose_event(GdkEventExpose* ev) {
		futCam[indCamEmUso]->imgCap->scale(imgOrig, 0, 0, larg, altu, posX, posY, escala, escala, (Gdk::InterpType) 1);
		Cor *pImgOrig = (Cor *) imgOrig->get_pixels();
		for (int i = 0; i < larg * altu; i++, pImgOrig++) {
			guint8 t = pImgOrig->r;
			pImgOrig->r = pImgOrig->b;
			pImgOrig->b = t;
		}
		imgOrig->render_to_drawable(get_window(), get_style()->get_black_gc(), 0, 0, 0, 0, larg, altu, // draw the whole image (from 0,0 t
				Gdk::RGB_DITHER_NONE, 0, 0);

		Glib::RefPtr<Gdk::Window> win = get_window();
		if (win) {
			LimitesCampo limitesCampo = futCam[indCamEmUso]->limitesCampo;
			Cairo::RefPtr<Cairo::Context> cr = win->create_cairo_context();
			cr->set_source_rgb(1.0, 0.0, 0.0);
			vector<double> dash(1);
			dash[0] = 2.0;
			cr->set_dash(dash, 0.0);
			cr->rectangle(limitesCampo.minXSemGol >> 1, limitesCampo.minY >> 1, (limitesCampo.maxXSemGol - limitesCampo.minXSemGol) >> 1, (limitesCampo.maxY - limitesCampo.minY) >> 1);
			cr->stroke();
		}
		return true;
	}

	virtual bool on_button_press_event(GdkEventButton *event) {
		if (event->button == 1) {//botao esquerdo
			int x = event->x * 2;
			int y = event->y * 2;
			if ((event->state & GDK_CONTROL_MASK) != 0) { // com control pressionado marca limites do campo
				int xe, ye;
				if (escala != 0.5) {
					xe = (x - larg) / 2 / escala + origX;
					ye = (y - altu) / 2 / escala + origY;
				} else {
					xe = x;
					ye = y;
				}
				cout << xe << endl;
				if (xe < larg) {
					futCam[indCamEmUso]->limitesCampo.minXSemGol = xe;
				} else {
					futCam[indCamEmUso]->limitesCampo.maxXSemGol = xe;
				}
				float fatorX = (futCam[indCamEmUso]->limitesCampo.maxXSemGol - futCam[indCamEmUso]->limitesCampo.minXSemGol) / TAM_X_CAMPO_SEM_GOL;
				futCam[indCamEmUso]->limitesCampo.minX = futCam[indCamEmUso]->limitesCampo.minXSemGol - TAM_X_DO_GOL * fatorX;
				futCam[indCamEmUso]->limitesCampo.maxX = futCam[indCamEmUso]->limitesCampo.maxXSemGol + TAM_X_DO_GOL * fatorX;
				if (futCam[indCamEmUso]->limitesCampo.maxX > 640 - GAP / 2 + 1)
					futCam[indCamEmUso]->limitesCampo.maxX = 640 - GAP / 2 + 1;
				if (futCam[indCamEmUso]->limitesCampo.minX < GAP / 2 + 1)
					futCam[indCamEmUso]->limitesCampo.minX = GAP / 2 + 1;
				if (ye < altu)
					futCam[indCamEmUso]->limitesCampo.minY = ye;
				else
					futCam[indCamEmUso]->limitesCampo.maxY = ye;

				futCam[indCamEmUso]->recalculaPixelsPosCentimetros();
			} else {
				if (escala != 0.5) {
					x = (x - larg) / 2 / escala + origX;
					y = (y - altu) / 2 / escala + origY;
				}
				//				cout << "x: " << x << " y: " << y << endl;
				futCam[indCamEmUso]->processa();
				Cor *p = futCam[indCamEmUso]->pImgCap + x + y * 640;
				int ax, bx, lum;
				if (p->r != 0 || p->g != 0 || p->b != 0) {
					RGB_SCT(p->r & 0xf8, p->g & 0xf8, p->b & 0xf8, ax, bx, lum);
					cout << "cor " << ax << "," << bx << "," << lum << endl;
					//			p->r=0;p->g=0;p->b=0;
					aHisto[ax]++;
					bHisto[bx]++;
					lumHisto[lum]++;
				}
			}
		} else if (event->button == 3) {//botao direito
			if (escala == 0.5) {
				escala = 2.0;
				origX = event->x * 2;
				posX = -origX * escala + larg / 2;
				origY = event->y * 2;
				posY = -origY * escala + altu / 2;
			} else {
				escala = 0.5;
				posX = origX = 0;
				posY = origY = 0;
			}
		}
		Glib::RefPtr<Gdk::Window> win = get_parent_window();
		if (win) {
			win->invalidate(true);
		}
		return true;
	}
};

class ImagemDigital: public Imagem {
public:
	int *aHisto, *bHisto, *lumHisto;

	ImagemDigital(int larg, int alt, int *aHisto, int *bHisto, int *lumHisto) :
		Imagem(larg, alt) {
		this->aHisto = aHisto;
		this->bHisto = bHisto;
		this->lumHisto = lumHisto;
	}

	virtual bool on_expose_event(GdkEventExpose* ev) {
		guint8 *p = futCam[indCamEmUso]->pImgDigi;
		Cor *pImg = (Cor *) futCam[indCamEmUso]->imgDigi->get_pixels();
		/**copia os pixel da imagem pImgDigi (dados da imagem digitalizada de 8 bits), convertendo a imagem que será representada de BGR para RGB com 24 bits */
		for (int i = 0; i < TAM_IMAGEM_640x480x1; i++) {
			//			cout << (int)*p;
			if (*p < MAX_IND_CORES) {
				pImg->r = coresRepresenta[*p].b;
				pImg->g = coresRepresenta[*p].g;
				pImg++->b = coresRepresenta[*p++].r;
			} else {
				int cor = *p++;
				if (cor != SENTINELA) {
					pImg->r = (cor & 0x07) << 2;//5;
					pImg->g = (cor & 0x18);// << 3;
					pImg++->b = cor & 0xe0 >> 3;
				} else {
					pImg->r = 0;
					pImg->g = 0;
					pImg++->b = 0;
				}
			}

		}
		futCam[indCamEmUso]->imgDigi->render_to_drawable(get_window(), get_style()->get_black_gc(), 0, 0, 0, 0, larg, altu, // draw the whole image (from 0,0 t
				Gdk::RGB_DITHER_NONE, 0, 0);
		desenhaElementos();
		return Imagem::on_expose_event(ev);
	}

	virtual bool on_button_press_event(GdkEventButton *event) {
		return true;
	}

	int escalaX(float x) {
		LimitesCampo limitesCampo = futCam[indCamEmUso]->limitesCampo;
		float fatorX = (limitesCampo.maxXSemGol - limitesCampo.minXSemGol) / (float) TAM_X_CAMPO_SEM_GOL;
		if (futCam[indCamEmUso]->nossoLado == ESQUERDA)
			return (x - TAM_X_DO_GOL) * fatorX + limitesCampo.minXSemGol;
		else {
			return limitesCampo.minXSemGol - (x - (TAM_X_DO_GOL + TAM_X_CAMPO_SEM_GOL)) * fatorX;
		}
	}

	int escalaY(float y) {
		LimitesCampo limitesCampo = futCam[indCamEmUso]->limitesCampo;
		float fatorY = (limitesCampo.maxY - limitesCampo.minY) / (float) TAM_Y_CAMPO;
		if (futCam[indCamEmUso]->nossoLado == ESQUERDA)
			return limitesCampo.minY - (y - TAM_Y_CAMPO) * fatorY;
		else {
			return y * fatorY + limitesCampo.minY;
		}
	}

	void desenhaElementos() {
		Glib::RefPtr<Gdk::Window> win = get_window();
		if (win && graficos) {

			Cairo::RefPtr<Cairo::Context> cr = win->create_cairo_context();
			//			cr->set_line_width((float) peso);
			for (int i = 0; i < NUM_ROBOS_TIME * 2 + 1; i++) {
				cr->set_line_width(1.0);
				cr->set_source_rgb(1.0, 1.0, 0.0);
				cr->move_to(escalaX(estado[i].x), escalaY(estado[i].y));
				cr->rel_line_to(estado[i].dx * 4, -estado[i].dy * 4);
				cr->stroke();

				cr->set_line_width(1.0);
				cr->set_source_rgb(1-(float)i/3, 0, (float)i/3);
				cr->move_to(escalaX(estado[i].x)+5, escalaY(estado[i].y));
				cr->arc(escalaX(estado[i].x), escalaY(estado[i].y), 10, 0, 2 * M_PI);
				cr->stroke();
			}
			for (int i = 0; i < NUM_ROBOS_TIME; i++) {
				cr->set_source_rgb(0.8, 0.8, 0.8);
				cr->move_to(escalaX(estado[i].x), escalaY(estado[i].y));
				int dx = 30 * cos(estado[i].angulo * M_PI / 180);
				int dy = -30 * sin(estado[i].angulo * M_PI / 180);
				if (futCam[indCamEmUso]->nossoLado == DIREITA) {
					dx = -dx;
					dy = -dy;
				}
				//				cout << " a: " << estado[i].angulo << " dx: " << dx << " dy: " << dy << " x: " << escalaX(estado[i].x) << " y: " << escalaY(estado[i].y);
				cr->rel_line_to(dx, dy);
				cr->stroke();

				// Objetivos
				cr->set_line_width(1.0);
				cr->set_source_rgb(1-(float)i/3, 0, (float)i/3);
				cr->move_to(escalaX(objetivoRobo[i].x)+5, escalaY(objetivoRobo[i].y));
				cr->arc(escalaX(objetivoRobo[i].x), escalaY(objetivoRobo[i].y), 10, 0, 2 * M_PI);
				cr->stroke();

				cr->set_source_rgb(0, 0.8, 0.8);
				cr->move_to(escalaX(objetivoRobo[i].x), escalaY(objetivoRobo[i].y));
				dx = 30 * cos(objetivoRobo[i].angulo * M_PI / 180);
				dy = -30 * sin(objetivoRobo[i].angulo * M_PI / 180);
				if (futCam[indCamEmUso]->nossoLado == DIREITA) {
					dx = -dx;
					dy = -dy;
				}
				//				cout << " a: " << estado[i].angulo << " dx: " << dx << " dy: " << dy << " x: " << escalaX(estado[i].x) << " y: " << escalaY(estado[i].y);
				cr->rel_line_to(dx, dy);
				cr->stroke();
			}
			//			cout << endl;
			//			cr->set_line_width(1.0);
			//			cr->set_source_rgb(0.0, 0.0, 0.0);
			//			cr->rectangle(0, 0, larg - 1, altu - 1);
			//			cr->stroke();
		}
	}
};

class BarraCores: public Imagem {
public:
	static const int peso = 4;

	BarraCores(int larg) :
		Imagem(larg + 4, MAX_IND_CORES * peso + 2) {
	}

	virtual int inicioCor(int indCor) {
		return 0;
	}

	virtual int finalCor(int indCor) {
		return 255;
	}

	virtual bool on_expose_event(GdkEventExpose* event) {
		//		if (emJogo)
		//			return true;
		//cout << "BarraCores" << endl;
		Glib::RefPtr<Gdk::Window> win = get_window();
		if (win) {

			Cairo::RefPtr<Cairo::Context> cr = win->create_cairo_context();
			cr->set_line_width((float) peso);
			for (int indCor = COR_BOLA; indCor < MAX_IND_CORES; indCor++) {
				cr->set_source_rgb(coresRepresenta[indCor].r / 255.0, coresRepresenta[indCor].g / 255.0, coresRepresenta[indCor].b / 255.0);
				int ini = inicioCor(indCor), fin = finalCor(indCor);
				if (ini < fin) {
					cr->move_to(ini + 2, indCor * peso + 2);
					cr->line_to(fin + 2, indCor * peso + 2);
				} else {
					cr->move_to(2, indCor * peso + 2);
					cr->line_to(fin + 2, indCor * peso + 2);
					cr->move_to(ini + 2, indCor * peso + 2);
					cr->line_to(255 + 2, indCor * peso + 2);
				}
				cr->stroke();
			}
			cr->set_line_width(1.0);
			cr->set_source_rgb(0.0, 0.0, 0.0);
			cr->rectangle(0, 0, larg - 1, altu - 1);
			cr->stroke();

		}
		return true;
	}
};

class BarraCoresCanalA: public BarraCores {
public:
	BarraCoresCanalA(int larg) :
		BarraCores(larg) {
	}

	virtual int inicioCor(int indCor) {
		return futCam[indCamEmUso]->aInicio[indCor];
	}

	virtual int finalCor(int indCor) {
		return futCam[indCamEmUso]->aFinal[indCor];
	}
};

class BarraCoresCanalB: public BarraCores {
public:
	BarraCoresCanalB(int larg) :
		BarraCores(larg) {
	}

	virtual int inicioCor(int indCor) {
		return futCam[indCamEmUso]->bInicio[indCor];
	}

	virtual int finalCor(int indCor) {
		return futCam[indCamEmUso]->bFinal[indCor];
	}
};

class BarraCoresCanalI: public BarraCores {
public:
	BarraCoresCanalI(int larg) :
		BarraCores(larg) {
	}

	virtual int inicioCor(int indCor) {
		return futCam[indCamEmUso]->lumInicio[indCor];
	}

	virtual int finalCor(int indCor) {
		return futCam[indCamEmUso]->lumFinal[indCor];
	}
};

class Histograma: public Imagem {
	GdkWindow *winEvent;
public:
	int *hist;
	SelecionaCor *corSel;
	int ini;

	Histograma(int *hist, int larg, int altu, SelecionaCor *corSel) :
		Imagem(larg + 4, altu) {
		this->hist = hist;
		this->corSel = corSel;
		for (int i = 0; i < 256; i++)
			hist[i] = 0;
		set_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	}

	virtual bool on_button_press_event(GdkEventButton *event) {
		ini = satura(event->x - 2);
		return true;
	}

	virtual void atribui(int ini, int fin, int indCor) {
	}

	virtual bool on_button_release_event(GdkEventButton *event) {
		if (event->y > 0 && event->y < altu) {
			int indCor = corSel->estado();
			atribui(ini, satura(event->x - 2), indCor);
			futCam[indCamEmUso]->iniciaVetoresRGBPaleta();
		}

		Glib::RefPtr<Gdk::Window> win = get_parent_window();
		if (win) {
			win->invalidate(true);
		}
		return true;
	}

	virtual bool on_expose_event(GdkEventExpose* event) {
		//		if (emJogo)
		//			return true;
		//cout << "Histograma" << endl;
		Glib::RefPtr<Gdk::Window> window = get_window();
		if (window) {
			Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();

			cr->set_line_width(1.0);
			int maior = *max_element(hist, hist + larg - 4);
			if (maior > 0) {
				cr->set_source_rgb(0.0, 0.0, 1.0);
				for (int i = 0; i < 256; i++) {
					cr->move_to(i + 2, altu - 2);
					cr->line_to(i + 2, altu - hist[i] * altu / maior);
				}
			}
			cr->stroke();
			cr->set_source_rgb(0.0, 0.0, 0.0);
			cr->rectangle(0, 0, larg - 1, altu - 1);
			cr->stroke();
		}
		return true;
	}
};

class HistogramaCanalA: public Histograma {
public:
	HistogramaCanalA(int *hist, int larg, int altu, SelecionaCor *corSel) :
		Histograma(hist, larg, altu, corSel) {
	}
	virtual void atribui(int ini, int fin, int indCor) {
		futCam[indCamEmUso]->aInicio[indCor] = ini;
		futCam[indCamEmUso]->aFinal[indCor] = fin;
	}
};

class HistogramaCanalB: public Histograma {
public:
	HistogramaCanalB(int *hist, int larg, int altu, SelecionaCor *corSel) :
		Histograma(hist, larg, altu, corSel) {
	}
	virtual void atribui(int ini, int fin, int indCor) {
		futCam[indCamEmUso]->bInicio[indCor] = ini;
		futCam[indCamEmUso]->bFinal[indCor] = fin;
	}
};

class HistogramaCanalI: public Histograma {
public:
	HistogramaCanalI(int *hist, int larg, int altu, SelecionaCor *corSel) :
		Histograma(hist, larg, altu, corSel) {
	}
	virtual void atribui(int ini, int fin, int indCor) {
		futCam[indCamEmUso]->lumInicio[indCor] = ini;
		futCam[indCamEmUso]->lumFinal[indCor] = fin;
	}
};

template<class Histograma, class BarraCores>
class Limites: public Gtk::HBox {
	Gtk::VBox vboxAux;
	Histograma histo;
	BarraCores barrasCores;
	int *hist;
public:
	Limites(int *hist, SelecionaCor *corSel) :
		barrasCores(256), histo(hist, 256, 30, corSel) {
		this->hist = hist;
		vboxAux.add(histo);
		vboxAux.add(barrasCores);
		add(vboxAux);
	}
};

/**
 * +--------------------------JanPrincipal--------------------------------------
 * | +--vBoxImgOrig
 * | |
 * | |
 * | |
 * | |
 * |
 * |
 * |
 * |
 * |
 * |
 * |
 * |
 * |
 * |
 *
 *
 */

class JanPrincipal: public Gtk::Window {
	Gtk::Button btSalva;
	Gtk::VBox vBoxImgOrig, vBoxImgDigi, vBoxControlesCores, vBoxGeral;
	Gtk::HBox hBoxImgs, hBoxSelCor, hBoxCampoVazioLado, hBoxLado, hBoxBotoes, hBoxBotoesJogo;
	SelecionaCor corSel;
	Limites<HistogramaCanalA, BarraCoresCanalA> canalA;
	Limites<HistogramaCanalB, BarraCoresCanalB> canalB;
	Limites<HistogramaCanalI, BarraCoresCanalI> canalI;
	ImagemOriginal imgOrig;
	ImagemDigital imgDig;
	Gtk::Button btCampoVazio;
	Gtk::HScale nivelVazio;
	Gtk::RadioButton rbEsquerda, rbDireita;
	Gtk::ToggleButton tbtGraficos;
	Gtk::Button btJogo;
	Gtk::Button btPenalidade;
	Gtk::Button btPosiciona;
	Gtk::Button btPartida;
public:
	JanPrincipal() :
		nivelVazio(0, 200, 1), btCampoVazio("Vazio"), btSalva("Salva"), btJogo("Jogo"), tbtGraficos("Graficos"), corSel(aHisto, bHisto, lumHisto), imgOrig(320, 240, aHisto, bHisto, lumHisto), imgDig(640, 480, aHisto, bHisto, lumHisto),
		// chama aos contrutores da representacao grafica (histograma x barra de cores)
				canalA(aHisto, &corSel), canalB(bHisto, &corSel), canalI(lumHisto, &corSel), rbEsquerda("Esquerda"), rbDireita("Direita"), btPenalidade("Penalidade"), btPosiciona("Posiciona"), btPartida("Partida") {

		set_title("Futebol - Departamento de Computacao - UNESP - Bauru");
		set_border_width(10);

		Gtk::RadioButton::Group grupoRb = rbEsquerda.get_group();
		rbDireita.set_group(grupoRb);
		hBoxLado.add(rbEsquerda);

		leParametros();

		add(vBoxGeral);
		vBoxGeral.add(hBoxImgs);
		vBoxGeral.add(hBoxBotoes);

		hBoxImgs.add(vBoxImgOrig);

		vBoxImgOrig.add(imgOrig);
		vBoxImgOrig.add(hBoxSelCor);

		hBoxSelCor.add(vBoxControlesCores);
		hBoxSelCor.add(corSel);

		hBoxLado.add(rbDireita);
		hBoxLado.add(btSalva);

		vBoxControlesCores.add(hBoxLado);
		vBoxControlesCores.add(canalA);
		vBoxControlesCores.add(canalB);
		vBoxControlesCores.add(canalI);
		vBoxControlesCores.add(tbtGraficos);
		vBoxControlesCores.add(hBoxCampoVazioLado);
		vBoxControlesCores.add(hBoxBotoesJogo);

		hBoxBotoesJogo.add(btJogo);
		hBoxBotoesJogo.add(btPenalidade);
		hBoxBotoesJogo.add(btPosiciona);
		hBoxBotoesJogo.add(btPartida);

		hBoxImgs.add(vBoxImgDigi);

		vBoxImgDigi.add(imgDig);

		hBoxCampoVazioLado.add(nivelVazio);
		hBoxCampoVazioLado.add(btCampoVazio);


		hBoxImgs.set_border_width(1);

		btSalva.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::btSalvaParams));
		btCampoVazio.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::btCapturaCampoVazio));
		nivelVazio.signal_change_value().connect(sigc::mem_fun(*this, &JanPrincipal::alterouNivelCampoVazio));
		rbEsquerda.signal_toggled().connect(sigc::mem_fun(*this, &JanPrincipal::alterouLado));
		btJogo.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::btColocaEmJogo));
		btPenalidade.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::btPosicionaPenalidade));
		btPosiciona.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::btPosicionaRobos));
		btPosiciona.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::btPreparaPartida));
		tbtGraficos.signal_clicked().connect(sigc::mem_fun(*this, &JanPrincipal::tbtApresentaGraficos));

		tbtGraficos.set_active(graficos);

		Glib::RefPtr<Gdk::Window> win = get_window();
		if (win) {
			win->invalidate(true);
		}

	}

	virtual bool on_key_press_event(GdkEventKey *event) {
		if (event->keyval == GDK_Escape/* && (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == (GDK_SHIFT_MASK | GDK_CONTROL_MASK)*/)
			emJogo = false;
		return true;
	}

	void alterouLado() {
		//				cout << rbEsquerda.get_active()<< endl;
		if (rbEsquerda.get_active() == 0) // desligado
			futCam[indCamEmUso]->nossoLado = DIREITA;
		else
			futCam[indCamEmUso]->nossoLado = ESQUERDA;
	}

	void tbtApresentaGraficos() {
		graficos = tbtGraficos.get_active();
	}

	void btColocaEmJogo() {
		emJogo = true;
		emPenalidade = false;
		emPosiciona = false;
		emInicio = false;
	}

	void btPosicionaPenalidade() {
		emPenalidade = true;
	}

	void btPosicionaRobos() {
		emPosiciona = true;
	}

	void btPreparaPartida() {
		emJogo=true;
		emInicio = true;
	}

	bool alterouNivelCampoVazio(Gtk::ScrollType scroll, double valor) {
		futCam[indCamEmUso]->nivelCampoVazio = valor;
		return true;
	}

	void btCapturaCampoVazio() {
		futCam[indCamEmUso]->contImgsCampoVazio = NUM_IMGS_CAMPO_VAZIO;
	}

	void btSalvaParams() {
		futCam[indCamEmUso]->salvaParametros();
	}

	void leParametros() {
		futCam[indCamEmUso]->leParametros();
		if (futCam[indCamEmUso]->nossoLado == ESQUERDA) {
			rbEsquerda.set_active(true);
			rbDireita.set_active(false);
		} else {
			rbEsquerda.set_active(false);
			rbDireita.set_active(true);
		}
		nivelVazio.set_value(futCam[indCamEmUso]->nivelCampoVazio);
	}

	virtual bool on_expose_event(GdkEventExpose* event) {
		static int valAntigo = 0;
		if (futCam[indCamEmUso]->contImgsCampoVazio != valAntigo) {
			/** o valAntigo serve para limitar a alteracao do fundo do botao
			 * a cada vez que muda o fundo esta rotina on_expose_event é novamente chamada
			 * se mudar todas as vezes, fica em loop
			 */
			valAntigo = futCam[indCamEmUso]->contImgsCampoVazio;
			if (futCam[indCamEmUso]->contImgsCampoVazio != 0) {
				btCampoVazio.modify_bg(Gtk::STATE_NORMAL, Gdk::Color("red"));
				btCampoVazio.modify_bg(Gtk::STATE_PRELIGHT, Gdk::Color("red"));
			} else {
				btCampoVazio.unset_bg(Gtk::STATE_NORMAL);
				btCampoVazio.unset_bg(Gtk::STATE_PRELIGHT);
			}
		}
		//		cout << "---------------" << endl;
		for (int indCor = COR_BOLA; indCor < MAX_IND_CORES; indCor++) {
			int ax = (futCam[indCamEmUso]->aInicio[indCor] + futCam[indCamEmUso]->aFinal[indCor]) / 2;
			int bx = (futCam[indCamEmUso]->bInicio[indCor] + futCam[indCamEmUso]->bFinal[indCor]) / 2;
			int lum = (int) ((futCam[indCamEmUso]->lumInicio[indCor] + futCam[indCamEmUso]->lumFinal[indCor]) / 2);
			SCT_RGB(ax, bx, lum, coresRepresenta[indCor].r, coresRepresenta[indCor].g, coresRepresenta[indCor].b);
			//			show_all_children();
			//			cout << "(" << (int) coresRepresenta[indCor].r << ", " << (int) coresRepresenta[indCor].b << ", " << (int) coresRepresenta[indCor].b << ")" << endl;
		}
		coresRepresenta[MAX_IND_CORES].r = coresRepresenta[MAX_IND_CORES].g = coresRepresenta[MAX_IND_CORES].b = 64;
		show_all_children();
		return Gtk::Window::on_expose_event(event);
	}

};

CapVideo *cap = NULL;

static gboolean idle(gpointer data) {
	// medidor de quadros por segundo
	static int cont = 0;
	static time_t t1 = 0;
	if (time(0) - t1 > 1) {
		cout << (float) cont / (time(0) - t1) << endl;
		cout.flush();
		cont = 0;
		t1 = time(0);
	} else
		cont++;


	if (cap != NULL && cap->numErro != 0) {
		delete cap; // implementado para recuperar o erro "VIDIOC_DQBUF error 5, Input/output error"
		cap = NULL;
	}
	if (cap == NULL) {
		gravaLog("reiniciando o V4L2\n");
		cap = new CapVideo("/dev/video0", 2, (JanPrincipal*) data, 0);
	}
#ifdef VIDEO
	cap->queryFrames();
#else
	cap->video(futCam[0]->pImgCap);
#endif
	return true;
}

int main(int argc, char *argv[]) {
	Gtk::Main kit(argc, argv);

	iniciaComunicacao();

	geraMatrizComb(comb);

	futCam[0] = new FutCamera(0);

	futCam[0]->imgCap = Gdk::Pixbuf::create_from_file(IMG_INICIAL, 640, 480);
	futCam[0]->pImgCap = (Cor *) futCam[0]->imgCap->get_pixels();
#ifndef VIDEO
	Cor *p = futCam[0]->pImgCap;
	for (int i = 0; i < 640 * 480; i++, p++) {
		guint8 t = p->r;
		p->r = p->b;
		p->b = t;
	}
#endif
	futCam[0]->imgDigi = Gdk::Pixbuf::create((Gdk::Colorspace) 0, false, 8, 640, 480);

	JanPrincipal janela;

	g_idle_add(idle, &janela);

	Gtk::Main::run(janela);

	delete cap;

	delete futCam[0];

	terminaComunicacao();

	return 0;
}
