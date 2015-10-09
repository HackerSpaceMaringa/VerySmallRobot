/*
 * Captura.h
 *
 *  Created on: Jun 3, 2009
 *      Author: futebol
 */

#ifndef CAPTURA_H_
#define CAPTURA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>
#include <string.h>
//#include "cxtypes.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

class Captura {
	struct buffer {
		void * start;
		size_t length;
	};

	char devName[20];
	int fd;
	struct buffer * buffers;
	unsigned int n_buffers;
	int videoInput;

	void errnoExit(const char * s);
	int xioctl(int fd, int request, void * arg);
	void stopCapturing(void);
	void startCapturing(void);
	void uninitDevice(void);
	void initMmap(void);
	void initDevice(void);
	void closeDevice(void);
	void openDevice(void);
	bool queryCurrentFrame();

public:
	int numErro;	// se um erro ocorrer o numero sera atribuido aqui; necessario para recuperar do erro "VIDIOC_DQBUF error 5, Input/output error"

	Captura(char * devName = "/dev/video0", int videoInput=0);
	void queryFrames(unsigned int  countFrames = 1);
	int getFD();
	virtual void video(const void * p) {
		printf(".");
//		errnoExit("Esta funcao deve ser substituida na classe descendente!\n");
	}

	~Captura();

	void enumCaptureFormats();
};

#endif /* CAPTURA_H_ */



