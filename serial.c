/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include "gpsapp.h"

#ifdef __arm__
#define SERIALDEV "/dev/ttyS1"
#else
#define SERIALDEV "/dev/ttyUSB0"
#endif

#define GPSD_PORT 2947

static int serialfd = -1;

/* this is a buffer that can be shared by all protocols because only one will
 * be active at a time anyways */
unsigned char packet[MAX_PACKET_SIZE];
int packet_idx;

/* this variable will be updated by the decoding protocol */
struct gps_state gps_state;

/* we update these whenever the location is updated */
struct coord gps_coord;
int	     gps_avgvmg; /* updated by route_update_gps_speed() */

struct gps_protocol *gps_protocols;       /* list of all protocols. */
static struct gps_protocol *protocol; /* currently selected protocol */

void serial_protocol(char *proto)
{
    for (protocol = gps_protocols; protocol; protocol = protocol->next)
	if (strcasecmp(proto, protocol->name) == 0)
	    break;
}

#ifndef __arm__
/* this doesn't work, the empeg doesn't have a loopback interface */
static int gpsd_open(void)
{
    struct sockaddr_in sin;
    int ret, flag = 1;
    
    serialfd = socket(PF_INET, SOCK_STREAM, 0);
    if (serialfd == -1)
	return -1;

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(GPSD_PORT);

    ret = connect(serialfd, (struct sockaddr *)&sin, sizeof(sin));
    if (ret == -1) {
	close(serialfd);
	serialfd = -1;
	return -1;
    }

    setsockopt(serialfd, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag));
    fcntl(serialfd, F_SETFL, O_NONBLOCK);

    /* drop back to NMEA, because that is what gpsd is actually spitting out */
    serial_protocol("NMEA");

    return 0;
}
#endif

void serial_open(void)
{
    int ret = -1, spd;
    struct termios termios;

    if (serialfd != -1)
	serial_close();

#ifndef __arm__
    ret = gpsd_open();
    if (ret != -1) goto exit;
#endif

    if (!protocol) {
	err("Unknown protocol, using NMEA");
	serial_protocol("NMEA");
    }

    serialfd = open(SERIALDEV, O_RDWR);
    if (serialfd == -1) goto exit;

    ret = tcgetattr(serialfd, &termios);
    if (ret == -1) goto exit;

    switch(protocol->baud) {
    case 9600: spd = B9600; break;
    case 4800:
    default:   spd = B4800; break;
    }

    termios.c_iflag = 0;
    termios.c_oflag = ONLRET;
    termios.c_cflag = (CSIZE & CS8) | CREAD | CLOCAL;
    termios.c_lflag = 0;
    cfsetispeed(&termios, spd);
    cfsetospeed(&termios, spd);

    ret = tcsetattr(serialfd, TCSAFLUSH, &termios);
    if (ret == -1) goto exit;

exit:
    if (ret == -1) {
	if (serialfd != -1) {
	    close(serialfd);
	    serialfd = -1;
	}
	err("Failed to set up serial port");
    }
    if (protocol->init)
	protocol->init();
}

void serial_close(void)
{
    if (serialfd == -1)
	return;

    close(serialfd);
    serialfd = -1;
}

static int serial_avail(void)
{
    int ret, avail = 0;
    ret = ioctl(serialfd, FIONREAD, &avail, sizeof(int));
    if (ret == -1) return 0;
    return avail;
}

void serial_poll()
{
    char buf[16];
    int n, i, new = 0;

    if (serialfd == -1)
	return;

    while (serial_avail()) {
	n = read(serialfd, buf, sizeof(buf));
	if (n <= 0) break;

	for (i = 0; i < n; i++)
	    new += protocol->update(buf[i], &gps_state);
    }

    if (new) {
	gps_coord.lat = gps_state.lat;
	gps_coord.lon = gps_state.lon;

	toTM(&gps_coord);
	track_pos();
	route_locate();
	do_refresh = 1;
    }
}

void serial_send(char *buf, int len)
{
    if (serialfd == -1)
	return;

    while (len) {
	int n = write(serialfd, buf, len);
	if (n < 0) break;
	buf += n;
	len -= n;
    }
}

