#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int serialfd = -1;

void serial_open(void)
{
    int serialfd, ret = -1;
    struct termios termios;

    serialfd = open("/dev/ttyS0", O_RDWR);
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
	err("Failed to set up serial port");
	if (serialfd != -1) {
	    close(serialfd);
	    serialfd = -1;
	}
    }

    return ret;
}

void serial_close(void)
{
    if (serialfd != -1) {
	close(serialfd);
	serialfd = -1;
    }
}

int serial_avail(void)
{
    int avail = 0, ret;

    if (serialfd != -1) {
	ret = ioctl(serialfd, FIONREAD, &avail);
	if (ret == -1)
	    avail = 0;
    }
    return avail;
}

