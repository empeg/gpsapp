/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <math.h>
#include "gpsapp.h"
#include "gps_protocol.h"

#define DLE 0x10
#define ETX 0x03

static int datestamp, timestamp;
static double latitude, longtitude;
static float spd_e, spd_n, spd_u;

static int initialized, fix;

static int update(struct gps_state *gps)
{
    gps->time      = datestamp + timestamp;
    gps->lat       = latitude;
    gps->lon       = longtitude;
    gps->spd_east  = spd_e;
    gps->spd_north = spd_n;
    gps->spd_up    = spd_u;

    if (!fix)
	gps->bearing = -1;
    else {
	gps->bearing = radtodeg(atan2(spd_e, spd_n));
	if (gps->bearing < 0) gps->bearing += 360;
    }

    return 1;
}

static short INT16(char *p)
{
    return ((p[0] << 8) | p[1]);
}

static float Single(char *p)
{
    int tmp = ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
    return *(float *)&tmp;
}

static double Double(char *p)
{
    long tmp1 = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    long tmp2 = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
    long long tmp = ((long long)tmp1 << 32) | (long long)tmp2;
    return *(double *)&tmp;
}

static void tsip_c25softreset(void)
{
    char cmd[4];

    cmd[0] = DLE;
    cmd[1] = 0x25;
    cmd[2] = DLE;
    cmd[3] = ETX;

    serial_send(cmd, sizeof(cmd));
}

static void tsip_c35get_io_options(void)
{
    char cmd[4];

    cmd[0] = DLE;
    cmd[1] = 0x35;
    cmd[2] = DLE;
    cmd[3] = ETX;

    serial_send(cmd, sizeof(cmd));
}

static void tsip_c35set_io_options(char *settings)
{
    char cmd[8];

    cmd[0] = DLE;
    cmd[1] = 0x35;
    cmd[2] = settings[0];
    cmd[3] = settings[1];
    cmd[4] = settings[2];
    cmd[5] = settings[3];
    cmd[6] = DLE;
    cmd[7] = ETX;

    serial_send(cmd, sizeof(cmd));
}

static void tsip_r41time(void)
{
    float time_of_week, utc_offset;
    short week_number;

    if (packet_idx != 11) return;

    time_of_week = Single(&packet[1]);
    week_number  = INT16(&packet[5]);
    utc_offset   = Single(&packet[7]);

#define EPOCHDIFF 315532800 /* difference between GPS and UNIX time */
    datestamp = week_number * 7 * 86400 + EPOCHDIFF;
}

static void tsip_r45version(void)
{
    if (packet_idx != 11) return;

    /* this packet should only be received when the receiver has lost power or
     * received our soft reset command and is doing an internal self-test */
    initialized = 0;
}

static void tsip_r46health(void)
{
    char status, hwstatus;

    if (packet_idx != 3) return;

    status = packet[1];
    hwstatus = packet[2];

    fix = (status == 0x00);
}

static void tsip_r55io_options(void)
{
    char out[4];

    if (packet_idx != 5) return;

    /* make sure we leave the reserved bits unharmed*/
    out[0] = (packet[1] & 0xc0) | 0x12;
    out[1] = (packet[2] & 0xfc) | 0x02;
    out[2] = (packet[3] & 0xfe) | 0x01;
    out[3] = (packet[4] & 0xf4) | 0x00;

    if (memcmp(&packet[1], out, 4) != 0)
	tsip_c35set_io_options(out);
}

static int tsip_r56velocity(struct gps_state *gps)
{
    float east, north, up, bias, time;
    int upd = 0;

    if (packet_idx != 21) return 0;

    east  = Single(&packet[1]);
    north = Single(&packet[5]);
    up    = Single(&packet[9]);
    bias  = Single(&packet[13]);
    time  = Single(&packet[17]);

    if (time != timestamp) {
	upd = update(gps);
	timestamp = time;
    }

    spd_e = east;
    spd_n = north;
    spd_u = up;

    return upd;
}

static void tsip_r82dgps_fix(void)
{
    if (packet_idx != 2) return;

    if (!initialized) {
	/* receiver has restarted and is ready to receive commands */
	initialized = 1;

	/* check current settings, if they are off we'll correct them */
	tsip_c35get_io_options();
    }
}

static int tsip_r84position(struct gps_state *gps)
{
    double lat, lon, alt, bias;
    float time;
    int upd = 0;

    if (packet_idx != 37) return 0;

    lat  = Double(&packet[1]);
    lon  = Double(&packet[9]);
    alt  = Double(&packet[17]);
    bias = Double(&packet[25]);
    time = Single(&packet[33]);

    if (time != timestamp) {
	upd = update(gps);
	timestamp = time;
    }

    latitude   = lat;
    longtitude = lon;

    return upd;
}

static int tsip_decode(struct gps_state *gps)
{
    int upd = 0;

    if (packet_idx < 1) return 0;

    switch(packet[0]) {
    case 0x41: tsip_r41time(); break;
    case 0x45: tsip_r45version(); break;
    case 0x46: tsip_r46health(); break;
    case 0x55: tsip_r55io_options(); break;
    case 0x56: upd = tsip_r56velocity(gps); break;
    case 0x82: tsip_r82dgps_fix(); break;
    case 0x84: upd = tsip_r84position(gps); break;
    }
    return upd;
}

static void tsip_init(void)
{
    initialized = 0;
    tsip_c25softreset();
}

static int tsip_update(char c, struct gps_state *gps)
{
    static int dles, dle_escape, upd = 0;

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
	upd = tsip_decode(gps);

restart:
	packet_idx = 0;
	dle_escape = 0;
	dles = 0;
	return upd;
    }
    dle_escape = 0;

    packet[packet_idx++] = c;

    /* discard long lines */
    if (packet_idx == MAX_PACKET_SIZE)
	goto restart;
    return 0;
}

REGISTER_PROTOCOL("TSIP", 9600, 'O', tsip_init, tsip_update);

