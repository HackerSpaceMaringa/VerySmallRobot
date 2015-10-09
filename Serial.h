/*
 * Serial.h
 *
 *  Created on: Sep 9, 2009
 *      Author: futebol
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <fcntl.h>
#include <termios.h>
#include "TiposClasses.h"

void iniciaComunicacao(void);

void enviaByteEspera(int n);

void enviaDados(unsigned char b1, unsigned char b2, unsigned char b3);

void terminaComunicacao(void);

#endif /* SERIAL_H_ */
