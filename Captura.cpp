/*
 *  Criado por Rene a partir de um V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

#include <iostream>
using namespace std;

#include "Captura.h"

Captura::Captura(char * devName, int videoInput) {
	this->videoInput = videoInput;
	strcpy(this->devName, devName);
	fd = -1;
	buffers = NULL;
	n_buffers = 0;
	numErro = 0;

	openDevice();
	initDevice();
	startCapturing();
}

int Captura::getFD() {
	return fd;
}

void Captura::errnoExit(const char * s) {
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));

	exit(EXIT_FAILURE);
}

int Captura::xioctl(int fd, int request, void * arg) {
	int r;

	do
		r = ioctl(fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

void Captura::enumCaptureFormats() {
	struct v4l2_fmtdesc fmtDesc;
	printf("Formatos de video para %s\n", devName);
	for (int i = 0;; i++) {
		CLEAR(fmtDesc);
		fmtDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmtDesc.index = i;

		if (-1 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtDesc))
			break;
		printf("\tindice %d;", fmtDesc.index);
		printf("\tdescricao %s;", fmtDesc.description);
		printf("\tformato %c%c%c%c\n", fmtDesc.pixelformat, fmtDesc.pixelformat
				>> 8, fmtDesc.pixelformat >> 16, fmtDesc.pixelformat >> 24);
	}

}

bool Captura::queryCurrentFrame(void) {
	struct v4l2_buffer buf;

	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			cout << "EAGAIN" << endl;
			return false;

		case EIO:
			/* Could ignore EIO, see spec. */
			cout << "EIO" << endl;
			/* fall through */

		default:
			//errnoExit("VIDIOC_DQBUF");
			cout << "VIDIOC_DQBUF error 5, Input/output error" << endl;
			numErro = errno;
			return false;
		}
	}

	assert (buf.index < n_buffers);

	video(buffers[buf.index].start);

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		errnoExit("VIDIOC_QBUF");

	return true;
}

void Captura::queryFrames(unsigned int countFrames) {
	while (countFrames-- > 0) {
		for (;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO (&fds);
			FD_SET (fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r) {
				if (EINTR == errno)
					continue;

				errnoExit("select");
			}

			if (0 == r) {
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}

			if (queryCurrentFrame())
				break;

			/* EAGAIN - continue select loop. */
		}
	}
}

void Captura::stopCapturing(void) {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
		errnoExit("VIDIOC_STREAMOFF");
}

void Captura::startCapturing(void) {
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			errnoExit("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
		errnoExit("VIDIOC_STREAMON");

}

void Captura::uninitDevice(void) {
	unsigned int i;
	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap(buffers[i].start, buffers[i].length))
			errnoExit("munmap");
	free(buffers);
}

void Captura::initMmap(void) {
	struct v4l2_requestbuffers req;

	CLEAR(req);
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
				"memory mapping\n", devName);
			exit(EXIT_FAILURE);
		} else {
			errnoExit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", devName);
		exit(EXIT_FAILURE);
	}

	buffers = (buffer*) calloc(req.count, sizeof(*buffers));

	if (!buffers) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errnoExit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start = mmap(NULL /* start anywhere */, buf.length,
				PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */, fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errnoExit("mmap");
	}
}

void Captura::initDevice(void) {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;

	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", devName);
			exit(EXIT_FAILURE);
		} else {
			errnoExit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", devName);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", devName);
		exit(EXIT_FAILURE);
	}

	/* Select video input, video standard and tune here. */

	CLEAR(cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	int std = V4L2_STD_NTSC_M;
	if (-1 == xioctl(fd, VIDIOC_S_STD, &std))
		printf("Can't set NTSC standard\n");

	if (-1 == xioctl(fd, VIDIOC_S_INPUT, &videoInput))
		printf("Can't set video input\n");

	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
		errnoExit("VIDIOC_G_FMT");

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		errnoExit("VIDIOC_S_FMT");
	//	----
	//	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//	fmt.fmt.pix.width = 640;
	//	fmt.fmt.pix.height = 480;
	////	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	//	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
	//	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	//
	//	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
	//		errnoExit("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	CLEAR(parm);
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = 60;
	parm.parm.capture.capturemode = 0;
	if (ioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
		//		errnoExit("VIDIOC_S_PARM");
	}

	if (ioctl(fd, VIDIOC_G_PARM, &parm) < 0) {
		errnoExit("VIDIOC_G_PARM");
	}
	cout << parm.parm.capture.timeperframe.denominator << endl;

	initMmap();

}

void Captura::closeDevice(void) {
	if (-1 == close(fd))
		errnoExit("close");

	fd = -1;
}

void Captura::openDevice(void) {
	struct stat st;

	if (-1 == stat(devName, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", devName, errno,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf(stderr, "%s is no device\n", devName);
		exit(EXIT_FAILURE);
	}

	fd = open(devName, O_RDWR /* required */| O_NONBLOCK, 0);

	if (-1 == fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n", devName, errno, strerror(
				errno));
		exit(EXIT_FAILURE);
	}
}

Captura::~Captura() {
	stopCapturing();
	uninitDevice();
	closeDevice();
}
