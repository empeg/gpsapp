/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include "gpsapp.h"

/* If the protocol has a polling function, we call it once every 5 seconds */
#define POLL_INTERVAL 5
#define UPDATE_INTERVAL 1

#define SERIALDEV "/dev/ttyS1"
#define GPSD_PORT 2947

int serialfd = -1;
char *serport = NULL;

/* this is a buffer that can be shared by all protocols because only one will
 * be active at a time anyways */
unsigned char packet[MAX_PACKET_SIZE];
int packet_idx;

/* this variable will be updated by the decoding protocol */
struct gps_state gps_state;

/* we update these whenever the location is updated */
struct coord gps_coord;
unsigned int gps_speed;
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

static int gpsd_open(void)
{
    struct ifreq ifr;
    struct sockaddr_in sin;
    int fd, ret, flag = 1;
    
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1)
	return -1;

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /* make sure that at the loopback network device is up.
     * This code is taken from dbrashear's mini_ifconfig */
    strcpy(ifr.ifr_name, "lo");
    memcpy(&ifr.ifr_addr, &sin, sizeof(struct sockaddr_in));
    /* set the inet address for 'lo' */
    if (ioctl(fd, SIOCSIFADDR, &ifr) < 0) {
	close(fd);
	return -1;
    }
    /* bring the interface up */
    strcpy(ifr.ifr_name, "lo");
    ifr.ifr_flags = (IFF_UP | IFF_RUNNING);
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
	close(fd);
	return -1;
    }

    /* and now we can try to connect to the gpsd without having to worry about
     * getting stuck in connect() */
    sin.sin_port = htons(GPSD_PORT);
    ret = connect(fd, (struct sockaddr *)&sin, sizeof(sin));
    if (ret == -1) {
	close(fd);
	return -1;
    }

    setsockopt(fd, SOL_TCP, TCP_NODELAY, &flag, sizeof(flag));
    //fcntl(fd, F_SETFL, O_NONBLOCK);

    /* Tell gpsd to output raw NMEA sentences */
    write(fd, "R", 1);

    /* drop back to NMEA, because that is what gpsd is actually spitting out */
    serialfd = fd;
    serial_protocol("NMEA");

    return 0;
}

void serial_open(void)
{
    int ret = -1, parity;
    speed_t spd;
    struct termios termios;

    if (serialfd != -1)
	serial_close();

    /* try to open a socket to the gpsd daemon */
    ret = gpsd_open();
    if (ret != -1) goto exit;

    /* if that failed, try to open the serial port direcly */
    if (!protocol) {
	err("Unknown protocol, using NMEA");
	serial_protocol("NMEA");
    }

    if (protocol->baud == 0)
	goto tracklog;

    serialfd = open(serport?serport:SERIALDEV, O_NOCTTY | O_RDWR | O_NONBLOCK);
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
    static time_t poll_stamp, update_stamp;
    time_t now;
    char buf[16];
    int n, i;

    if (serialfd == -1)
	return;

    while (serial_avail()) {
	n = read(serialfd, buf, sizeof(buf));
	if (n <= 0) break;

	for (i = 0; i < n; i++)
	    protocol->update(buf[i], &gps_state);
    }

    now = time(NULL);
    /* only updated once a second except when we have no fix, as the time isn't
     * updated in that case. And then only when we actually have received
     * something from the receiver */
    if ((!(gps_state.fix & 0x1) || (update_stamp + UPDATE_INTERVAL <= now)) &&
	gps_state.updated)
    {
	gps_coord.lat = gps_state.lat;
	gps_coord.lon = gps_state.lon;

	route_recenter();

	toTM(&gps_coord);

	gps_speed = sqrt(gps_state.spd_east * gps_state.spd_east +
			 gps_state.spd_north * gps_state.spd_north +
			 gps_state.spd_up * gps_state.spd_up) * 3600.0;

	/* According to ellweber, bearing measurements are pretty flaky when
	 * we're moving slower than 1-2 km/h, but the speed reported by my
	 * receiver easily wanders up to 5 km/h when I'm standing still */
	if (gps_speed >= 5000) {
	    track_pos();
	    gps_bearing = gps_state.bearing;
	}

	route_locate();

	if (gps_speed >= 5000)
	    route_update_vmg();

	update_stamp = now;
	gps_state.updated = 0;
	do_refresh = 1;
    }

    if (protocol->poll && poll_stamp + POLL_INTERVAL <= now) {
	protocol->poll();
	poll_stamp = now;
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

