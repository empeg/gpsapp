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

/* If the protocol has a polling function, we call it once every 5 seconds */
#define POLL_INTERVAL 5

#ifdef __arm__
#define SERIALDEV "/dev/ttyS1"
#else
#define SERIALDEV "/dev/ttyUSB0"
#endif

#define GPSD_PORT 2947

int serialfd = -1;

/* this is a buffer that can be shared by all protocols because only one will
 * be active at a time anyways */
unsigned char packet[MAX_PACKET_SIZE];
int packet_idx;

/* this variable will be updated by the decoding protocol */
struct gps_state gps_state;

/* we update these whenever the location is updated */
struct coord gps_coord;
int	     gps_speed;
int	     gps_avgvmg;
int	     gps_bearing;

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
    //fcntl(serialfd, F_SETFL, O_NONBLOCK);

    /* drop back to NMEA, because that is what gpsd is actually spitting out */
    serial_protocol("NMEA");

    return 0;
}
#endif

void serial_open(void)
{
    int ret = -1, parity;
    speed_t spd;
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

    if (protocol->baud == 0)
	goto tracklog;

    serialfd = open(SERIALDEV, O_NOCTTY | O_RDWR | O_NONBLOCK);
    if (serialfd == -1) goto exit;

    ret = tcgetattr(serialfd, &termios);
    if (ret == -1) goto exit;

    switch(protocol->baud) {
    case 9600: spd = B9600; break;
    case 4800:
    default:   spd = B4800; break;
    }
    switch(protocol->parity) {
    case 'O': parity = PARENB | PARODD; break;
    case 'E': parity = PARENB; break;
    case 'N': 
    default:  parity = 0; break;
    }

    termios.c_iflag = 0;
    termios.c_oflag = 0; // ONLRET;
    termios.c_cflag = (CSIZE & CS8) | CREAD | CLOCAL | parity;
    termios.c_lflag = 0;
    cfsetispeed(&termios, spd);
    cfsetospeed(&termios, spd);

    ret = tcsetattr(serialfd, TCSANOW, &termios);
    if (ret == -1) goto exit;

    fcntl(serialfd, F_SETFL, O_RDWR | O_NOCTTY);

exit:
    if (ret == -1) {
	serial_close();
	err("Failed to set up serial port");
	return;
    }

tracklog:
    if (protocol->init)
	protocol->init();

    /* This a bit of a weird solution. While we're waiting for a fix we don't
     * have gps time. But if the time is 0 all new satellites will be assigned
     * to the the first available slot. So we either have to initialize all
     * empty slots to -1, or... */
    gps_state.time = 1;
}

void serial_close(void)
{
    if (serialfd == -1)
	return;

    tcflush(serialfd, TCIOFLUSH);
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
    static time_t poll_stamp;
    time_t old_gps_time;
    char buf[16];
    int n, i;

    if (serialfd == -1)
	return;

    old_gps_time = gps_state.time;
    while (serial_avail()) {
	n = read(serialfd, buf, sizeof(buf));
	if (n <= 0) break;

	for (i = 0; i < n; i++)
	    protocol->update(buf[i], &gps_state);
    }

    /* only updated once a second except when we have no fix, as the time isn't
     * updated in that case. And then only when we actually have received
     * something from the receiver */
    if ((!gps_state.fix || old_gps_time != gps_state.time) && gps_state.updated)
    {
	gps_coord.lat = gps_state.lat;
	gps_coord.lon = gps_state.lon;

	route_recenter();

	toTM(&gps_coord);
	track_pos();

	gps_speed = sqrt(gps_state.spd_east * gps_state.spd_east +
			 gps_state.spd_north * gps_state.spd_north +
			 gps_state.spd_up * gps_state.spd_up) * 3600.0;

	/* According to ellweber, bearing measurements are pretty flaky when
	 * we're moving slower than 1-2 km/h */
	if (gps_speed >= 2000)
	    gps_bearing = gps_state.bearing;

	route_locate();

	if (gps_speed >= 2000)
	    route_update_vmg();

	gps_state.updated = 0;
	do_refresh = 1;
    }

    if (protocol->poll && poll_stamp + POLL_INTERVAL < time(NULL)) {
	protocol->poll();
	poll_stamp = time(NULL);
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

