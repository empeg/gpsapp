/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <math.h>
#include "gpsapp.h"
#include "gps_protocol.h"

#define DLE  0x10
#define ETX  0x03
#define ACK  0x06
#define CMD  0x0A
#define NACK 0x15
#define PVT  0x33

static void garmin_send(unsigned char id, char *buf, unsigned char len)
{
    char cmd[261], csum = 0;
    int i, j = 0;

    cmd[j++] = DLE;
    cmd[j++] = id; csum -= id;
    cmd[j++] = len; csum -= len;
    for (i = 0; i < len; i++) {
	cmd[j++] = buf[i]; csum -= buf[i];
	if (buf[i] == DLE)
	    cmd[j++] = DLE;
    }
    cmd[j++] = csum;
    if (csum == DLE)
	cmd[j++] = DLE;
    cmd[j++] = DLE;
    cmd[j++] = ETX;

    serial_send(cmd, j);
}

static void garmin_send_ack(void)
{
    garmin_send(ACK, &packet[0], 1);
}

static void garmin_send_nack(void)
{
    garmin_send(NACK, &packet[0], 1);
}

static double garmin_double(char *p)
{
    union { char c[8]; double v; } u;
#ifdef __arm__ /* arm uses a different representation for doubles */
    u.c[0] = p[4]; u.c[1] = p[5]; u.c[2] = p[6]; u.c[3] = p[7];
    u.c[4] = p[0]; u.c[5] = p[1]; u.c[6] = p[2]; u.c[7] = p[3];
#else
    u.c[0] = p[0]; u.c[1] = p[1]; u.c[2] = p[2]; u.c[3] = p[3];
    u.c[4] = p[4]; u.c[5] = p[5]; u.c[6] = p[6]; u.c[7] = p[7];
#endif
    return u.v;
}

static int garmin_r33pvt_data(struct gps_state *gps)
{
    if (packet_idx != 67)
	return 0;

    /* difference between UNIX and Garmin time which starts 1/1/1990? */
#define EPOCHDIFF 631065600
    gps->time = *(int *)&packet[62] * 86400 + EPOCHDIFF +
	garmin_double(&packet[20]) - *(short *)&packet[60];

    gps->lat       = garmin_double(&packet[28]);
    gps->lon       = garmin_double(&packet[36]);
    gps->spd_east  = *(float *)&packet[44];
    gps->spd_north = *(float *)&packet[48];
    gps->spd_up    = *(float *)&packet[52];

    if (*(short *)&packet[18] < 2)
	gps->bearing = -1;
    else {
	gps->bearing = radtodeg(atan2(gps->spd_east, gps->spd_north));
	if (gps->bearing < 0) gps->bearing += 360;
    }

    return 1;
}

static int garmin_decode(struct gps_state *gps)
{
    if (packet[0] == PVT)
	return garmin_r33pvt_data(gps);
    return 0;
}

static void garmin_init(void)
{
    /* Tell gps to start sending PVT data */
    char *cmd = "\x00\x31";
    garmin_send(CMD, cmd, 2);
}

static int garmin_update(char c, struct gps_state *gps)
{
    static int dles, dle_escape;
    static unsigned char csum;
    int update = 0;

    if (c == DLE) {
	if (++dles == 1) return 0; /* start of packet */
	if (!packet_idx && dles == 2)
	    goto restart;

	/* <dle> inside a packet should be doubled so we drop some */
	if (!dle_escape) {
	    dle_escape = 1;
	    return 0;
	}
    }

    /* still waiting for start of packet... */
    if (!dles) return 0;

    /* We should never see dle or etx as the id */
    if (!packet_idx && c == ETX)
	goto restart;

    /* end of packet? */
    if (dle_escape && c == ETX) {
	if (csum == 0) {
	    update = garmin_decode(gps);
	    if (packet[0] != ACK && packet[0] != NACK)
		garmin_send_ack();
	}
	else if (packet[0] != ACK && packet[0] != NACK)
	    garmin_send_nack();
restart:
	packet_idx = 0;
	dles = 0;
	dle_escape = 0;
	csum = 0;
	return update;
    }

    dle_escape = 0;

    packet[packet_idx++] = c;
    csum += c;

    /* discard long lines */
    if (packet_idx == MAX_PACKET_SIZE)
	goto restart;
    return 0;
}

REGISTER_PROTOCOL("GARMIN", 9600, 'N', garmin_init, garmin_update);

