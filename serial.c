/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "gpsapp.h"

static int serialfd = -1;

#ifdef __arm__
#define SERIALDEV "/dev/ttyS1"
#else
#define SERIALDEV "/dev/ttyUSB0"
#endif

void serial_open(void)
{
    int ret = -1;
    struct termios termios;

    if (serialfd != -1)
	serial_close();

    serialfd = open(SERIALDEV, O_RDONLY | O_NONBLOCK);
    if (serialfd == -1) goto exit;

    ret = tcgetattr(serialfd, &termios);
    if (ret == -1) goto exit;

    termios.c_iflag = 0;
    termios.c_oflag = ONLRET;
    termios.c_cflag = (CSIZE & CS8) | CREAD | CLOCAL;
    termios.c_lflag = 0;
    cfsetispeed(&termios, B4800);
    cfsetospeed(&termios, B4800);

    ret = tcsetattr(serialfd, TCSAFLUSH, &termios);
    if (ret == -1) goto exit;

exit:
    if (ret == -1) {
	if (serialfd != -1) {
	    close(serialfd);
	    serialfd = -1;
	}
	//err("Failed to set up serial port");
    }
    return;
}

void serial_close(void)
{
    if (serialfd == -1)
	return;

    close(serialfd);
    serialfd = -1;
}

void serial_poll()
{
    static char line[128], xor;
    static int idx;
    int n, i;
    char c, buf[16];

    if (serialfd == -1)
	return;

    while (1) {
	n = read(serialfd, buf, sizeof(buf));
	if (n <= 0) break;

	for (i = 0; i < n; i++) {
	    c = buf[i]; 

	    if (c == '\r')
		continue;

	    if (c == '\n') {
		line[idx] = '\0';

		nmea_decode(line, xor);

		idx = 0;
		xor = '\0';
		continue;
	    }

	    line[idx++] = c;
	    xor ^= c;

	    /* discard long lines */
	    if (idx == sizeof(line)) {
		idx = 0;
		xor = '\0';
	    }
	}
    }
}

