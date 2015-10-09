/*
 * Serial.cpp
 *
 *  Created on: Sep 9, 2009
 *      Author: futebol
 */

#include "Serial.h"

const char *device = "/dev/ttyS0";

struct termios cfg;

int fd = 0;

void iniciaComunicacao(void) {
#ifdef SERIAL
	fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1) {
		erro("erro na abertura da interface serial\n");
	}

	if (!isatty(fd)) {
		erro("A interface serial aberta nao é realmente uma interface serial!\n");
	}

	if (tcgetattr(fd, &cfg) < 0) {
		erro("A configuracao da interface serial nao pode ser lida!\n");
	}

	cfg.c_iflag = IGNBRK | IGNPAR;
	cfg.c_oflag = 0;
	cfg.c_lflag = 0;
	cfg.c_cflag = B2400 | CS8;// | CSTOPB;
	cfg.c_ispeed = 2400;
	cfg.c_ospeed = 2400;

	cfg.c_cc[VMIN] = 1;
	cfg.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSAFLUSH, &cfg) < 0) {
			erro("A configuracao da interface serial nao pode ser alterada!\n");
		}

	if (cfsetispeed(&cfg, B2400) < 0 || cfsetospeed(&cfg, B2400) < 0) {
		erro("A interface serial nao pode ser configurada!\n");
	}
#endif
}

void enviaByteEspera(int n) {
#ifdef SERIAL
	tcflush(fd, TCIOFLUSH);
	for (int i = 0; i < n; i++) {
		char b = 0x88;
		int resp = write(fd, &b, 1);
		if (resp < 0) {
			erro("Interface serial - nao pode enviar byte.");
			break;
		}
	}
#endif
}

void enviaDados(unsigned char b1, unsigned char b2, unsigned char b3) {
#ifdef SERIAL
	tcflush(fd, TCIOFLUSH);
	unsigned char b[NUM_ROBOS_TIME + 1];
	b[0] = b1;
	b[1] = b2;
	b[2] = b3;
	b[3] = ~(b1 + b2 + b3);
	int resp = write(fd, b, 4);
	if (resp < 0) {
		erro("Interface serial - nao pode enviar comandos aos robos.");
	}
#endif
}

void terminaComunicacao(void) {
#ifdef SERIAL
	close(fd);
#endif
}
