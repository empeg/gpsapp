/*
 * Copyright (c) 2002 Derrick J Brashear shadow@dementia.org
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "gpsapp.h"
#include "gps_protocol.h"

#define DLE 0x81
#define ETX 0xFF
#define WD(x) 2*x

struct zodiac_header {
    unsigned short sync;
    unsigned short id;
    unsigned short ndata;
    unsigned short flags;
    unsigned short csum;
};

static short INT16(unsigned char *p)
{
    return ((p[1] << 8) | p[0]);
}

static int INT32(unsigned char *p)
{
    return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static void em_1000geodpos(struct gps_state *gps)
{
    double speed;
    int bearing, solinv;

    solinv = INT16(&packet[WD(10)]) & 0x5;
    if (solinv) gps->fix &= ~0x3; /* do we know whether it is a 2D or 3D fix? */
    else	gps->fix |=  0x3;

    gps->lat = INT32(&packet[WD(27)]) * 1.0e-8;
    gps->lon = INT32(&packet[WD(29)]) * 1.0e-8;
    gps->alt = INT32(&packet[WD(31)]) * 1.0e-2;
    speed = ((unsigned int)INT32(&packet[WD(34)])) * 1.0e-2;
    bearing = radtodeg(INT16(&packet[WD(36)]) * 1.0e-3);

    if (bearing < 0) bearing += 2 * M_PI;

    gps->bearing = radtodeg(bearing);
    gps->spd_up = 0.0;
    gps->spd_east  = sin(bearing) * speed;
    gps->spd_north = cos(bearing) * speed;
    gps->time = conv_date(INT16(&packet[WD(21)]), 
			  INT16(&packet[WD(20)]), INT16(&packet[WD(19)])) +
	INT16(&packet[WD(24)])+(60*INT16(&packet[WD(23)])) +
	(3600*INT16(&packet[WD(22)]));

    gps->updated |= GPS_STATE_FIX | GPS_STATE_COORD | GPS_STATE_BEARING | GPS_STATE_SPEED;
}

static void em_1002chsum(struct gps_state *gps)
{
    int j, status, svn, snr, used, valid;
    int timestamp, datestamp, time;

#define EPOCHDIFF 315532800 /* difference between GPS and UNIX time */
#define LEAP_SECONDS 13 /* hardcoded, bad me, should get it from the receiver */
    datestamp = INT16(&packet[WD(10)]) * 7 * 86400 + EPOCHDIFF;
    timestamp = INT32(&packet[WD(11)]) + LEAP_SECONDS;
    time = datestamp + timestamp;
    
    for (j = 0; j < 12; j++) {
	status = INT16(&packet[WD(15 + (3 * j))]);
	used  = status & 0x1;
	valid = status & 0x4;
	svn = INT16(&packet[WD(16 + (3 * j))]);
	snr = INT16(&packet[WD(17 + (3 * j))]) / 4; /* scaling snr to 0-16 */

	new_sat(gps, svn, valid ? time : UNKNOWN_TIME, UNKNOWN_ELV, UNKNOWN_AZM,
		snr, used);
    }

    gps->updated = GPS_STATE_SIGNALS;
}

static void em_1003sats(struct gps_state *gps)
{
    double elv, azm;
    int j, svn, nsats = INT16(&packet[WD(14)]);

    gps->hdop = INT32(&packet[WD(11)]) * 1.0e-2;

    for (j = 0; j < nsats; j++) {
	svn = INT16(&packet[WD(15 + (3 * j))]);
	elv = INT16(&packet[WD(17 + (3 * j))]) * 1.0e-4;
	azm = INT16(&packet[WD(16 + (3 * j))]) * 1.0e-4;

	if (elv < 0) elv = 0;
	if (azm < 0) azm += 2 * M_PI;

	new_sat(gps, svn, UNKNOWN_TIME, elv, azm, UNKNOWN_SNR, UNKNOWN_USED);
    }

    if (nsats)
	gps->updated = GPS_STATE_SATS;
}

static unsigned short zodiac_checksum(unsigned short *w, int n)
{
    unsigned short csum = 0;

    while (n--)
        csum += *(w++);
    csum = -csum;
    return csum;
}

static void em_decode(struct gps_state *gps)
{
    if (packet_idx < 1) return;

#ifndef __arm__
    fprintf(stderr, "receiving %x\n", packet[0]);
#endif

    /* NMEA lines should start with a '$' */
    if (packet[0] == 'E') {
        /* recognize earthmate's 'EARTHA' message */
        if (packet_idx >= 6 && memcmp(packet, "EARTHA", 6) == 0)
            serial_send("EARTHA\r\n", 8);
        return;
    }

    draw_activity(0);

    /* verify checksum XXX */

    switch(INT16(&packet[0])) {
    case 1000: em_1000geodpos(gps); break;
    case 1002: em_1002chsum(gps); break;
    case 1003: em_1003sats(gps); break;
    }
}

static void em_update(char c, struct gps_state *gps)
{
    static int dles, eartha = 0;

    if ((unsigned char)c == DLE) {
	eartha = 0; /* should never see this when expecting EARTHA */

	if (++dles == 1 && !packet_idx) /* start of packet */
	    goto restart;
    }

    /* still waiting for start of packet... */
    if (!dles) return;

    /* We should never see dle or etx as the id */
    if (!packet_idx && (unsigned char)c == ETX)
	goto restart;

    /* Deal with EARTHA */
    if (!packet_idx && c == 'E') {
	eartha = 1;
	goto restart;
    }

    /* end of packet? */
    if ((packet_idx && (unsigned char)c == ETX) ||
	(eartha == 1 && c == '\n')) {
	em_decode(gps);

restart:
	packet_idx = 0;
	dles = 0;
	return;
    }

    packet[packet_idx++] = c;

    /* discard long lines */
    if (packet_idx == MAX_PACKET_SIZE) 
	goto restart;
}

REGISTER_PROTOCOL("EARTHMATE", 9600, 'N', NULL, NULL, em_update);

void zodiac_send(int type, unsigned short *dat, int dlen)
{
    struct zodiac_header h;

    h.flags = 0;

    h.sync = 0x81ff;
    h.id = (unsigned short) type;
    h.ndata = dlen - 1; /* data checksum word doesn't count */
    h.csum = zodiac_checksum((unsigned short *) &h, 4);

    /* Add data checksum */
    dat[dlen - 1] = zodiac_checksum(dat, dlen - 1);

    serial_send((char *)&h, sizeof(h));
    serial_send((char *)dat, (sizeof(unsigned short) * dlen));
}

