/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <stdio.h>
#include <math.h>
#include "gpsapp.h"
#include "gps_protocol.h"

#define DLE 0x10
#define ETX 0x03

static int datestamp, timestamp, sat_track_update;
static int initialized, fix;

static short INT16(char *p)
{
    return ((p[0] << 8) | p[1]);
}

static float Single(char *p)
{
    union { char c[4]; float v; } u;
    u.c[3] = p[0]; u.c[2] = p[1]; u.c[1] = p[2]; u.c[0] = p[3];
    return u.v;
}

static double Double(char *p)
{
    union { char c[8]; double v; } u;
#ifdef __arm__ /* arm uses a different representation for doubles */
    u.c[7] = p[4]; u.c[6] = p[5]; u.c[5] = p[6]; u.c[4] = p[7];
    u.c[3] = p[0]; u.c[2] = p[1]; u.c[1] = p[2]; u.c[0] = p[3];
#else
    u.c[7] = p[0]; u.c[6] = p[1]; u.c[5] = p[2]; u.c[4] = p[3];
    u.c[3] = p[4]; u.c[2] = p[5]; u.c[1] = p[6]; u.c[0] = p[7];
#endif
    return u.v;
}

static void tsip_send(unsigned char id, char *buf, unsigned char len)
{
    char cmd[16];
    int i, j = 0;

#ifndef __arm__
    fprintf(stderr, "sending %x\n", id);
#endif

    cmd[j++] = DLE;
    cmd[j++] = id;
    for (i = 0; i < len; i++) {
	cmd[j++] = buf[i];
	if (buf[i] == DLE)
	    cmd[j++] = DLE;
    }
    cmd[j++] = DLE;
    cmd[j++] = ETX;

    serial_send(cmd, j);
}

#if 0 /* a bit harsh, as the receiver needs to reacquire all satellites */
static void tsip_25_softreset(void)
{
    tsip_send(0x25, NULL, 0);
}
#endif

static void tsip_27_req_signal_levels(void)
{
    tsip_send(0x27, NULL, 0);
}

static void tsip_35_req_io_options(void)
{
    tsip_send(0x35, NULL, 0);
}

static void tsip_35_set_io_options(char *settings)
{
    char data[4];

    data[0] = settings[0];
    data[1] = settings[1];
    data[2] = settings[2];
    data[3] = settings[3];

    tsip_send(0x35, data, 4);
}

static void tsip_37_last_fix(void)
{
    tsip_send(0x37, NULL, 0);
}

static void tsip_3C_req_sat_track_status(void)
{
    char data = 0;
    tsip_send(0x3C, &data, 1);
    sat_track_update = datestamp + timestamp + 4;
}

static void tsip_41_time(void)
{
    float time_of_week, utc_offset;
    short week_number;

    if (packet_idx != 11) return;

    time_of_week = Single(&packet[1]);
    week_number  = INT16(&packet[5]);
    utc_offset   = Single(&packet[7]);

#define EPOCHDIFF 315532800 /* difference between GPS and UNIX time */
    datestamp = week_number * 7 * 86400 + EPOCHDIFF;
    timestamp = time_of_week + utc_offset;
}

static void tsip_45_version(void)
{
    if (packet_idx != 11) return;

    /* this packet should only be received when the receiver has lost power or
     * received our soft reset command and is doing an internal self-test */
    initialized = 0;
}

static void tsip_46_health(void)
{
    char status, hwstatus;

    if (packet_idx != 3) return;

    status = packet[1];
    hwstatus = packet[2];

    fix = (status == 0x00);

    if (initialized)
	tsip_27_req_signal_levels();
}

static void tsip_47_sat_signals(struct gps_state *gps)
{
    unsigned char count, svn;
    float tmp;
    int i, used, snr, time;

    if (packet_idx < 2) return;

    count = packet[1];

    if (packet_idx != 2 + 5 * count) return;

    for (i = 0; i < count; i++) {
	svn = packet[2 + 5 * i];
	tmp = Single(&packet[3 + 5 * i]);
	used = (tmp >= 0);
	snr = abs((int)tmp);
	time = datestamp + timestamp;

	new_sat(gps, svn, time, UNKNOWN_ELV, UNKNOWN_AZM, snr, used);
    }
    if (count)
	gps->updated = GPS_STATE_SIGNALS;

    /* update satellite tracking status */
    if (datestamp + timestamp > sat_track_update)
	tsip_3C_req_sat_track_status();

    /* If we don't have a fix yet there isn't that much data coming from the
     * receiver, which is a bit uncomfortable for the end-user (gpsapp looks to
     * be stuck). So we can try to get a continuous stream of signal level
     * requests */
    if (!count)
	tsip_27_req_signal_levels();
}

static void tsip_55_io_options(void)
{
    char out[4];

    if (packet_idx != 5) return;

    /* make sure we leave the reserved bits unharmed*/
    out[0] = (packet[1] & 0xc0) | 0x12; /* double precision LLA wrt. WGS-84 */
    out[1] = (packet[2] & 0xfc) | 0x02; /* ENU velocity */
    out[2] = (packet[3] & 0xfe) | 0x01; /* UTC time */
    out[3] = (packet[4] & 0xf4) | 0x00;

    if (memcmp(&packet[1], out, 4) != 0)
	tsip_35_set_io_options(out);
}

static void tsip_56_velocity(struct gps_state *gps)
{
    float east, north, up, bias, time;

    if (packet_idx != 21) return;

    east  = Single(&packet[1]);
    north = Single(&packet[5]);
    up    = Single(&packet[9]);
    bias  = Single(&packet[13]);
    time  = Single(&packet[17]);

    timestamp = (int)time;

    gps->time = timestamp + datestamp;
    gps->spd_east = east;
    gps->spd_north = north;
    gps->spd_up = up;

    gps->bearing = radtodeg(atan2(east, north));
    if (gps->bearing < 0) gps->bearing += 360;

    gps->updated |= GPS_STATE_SPEED | GPS_STATE_BEARING;
}

static void tsip_5C_sat_track_status(struct gps_state *gps)
{
    float elv, azm;
    int time, snr;
    unsigned char svn;

    if (packet_idx != 25) return;

    svn = packet[1];
    snr = abs((int)Single(&packet[5]));
    time = datestamp + (int)Single(&packet[9]);
    elv = Single(&packet[13]);
    azm = Single(&packet[17]);

    new_sat(gps, svn, time, elv, azm, UNKNOWN_SNR, UNKNOWN_USED);
    gps->updated |= GPS_STATE_SATS;
}

static void tsip_82_dgps_fix(void)
{
    if (!initialized) {
	/* receiver has restarted and is ready to receive commands */
	initialized = 1;

	/* check current settings, if they are off we'll correct them */
	tsip_35_req_io_options();

	/* get the last fix, useful as we can start routing even while we're
	 * waiting for a satellite lock. */
	tsip_37_last_fix();
    }
}

static void tsip_84_position(struct gps_state *gps)
{
    double lat, lon, alt, bias;
    float time;

    if (packet_idx != 37) return;

    lat  = Double(&packet[1]);
    lon  = Double(&packet[9]);
    alt  = Double(&packet[17]);
    bias = Double(&packet[25]);
    time = Single(&packet[33]);

    timestamp = (int)time;

    gps->time = timestamp + datestamp;
    gps->lat = lat;
    gps->lon = lon;
    gps->updated |= GPS_STATE_COORD;

    tsip_27_req_signal_levels();
}

static void tsip_decode(struct gps_state *gps)
{
    if (packet_idx < 1) return;

#ifndef __arm__
    fprintf(stderr, "receiving %x\n", packet[0]);
#endif

    switch(packet[0]) {
    case 0x41: tsip_41_time(); break;
    case 0x45: tsip_45_version(); break;
    case 0x46: tsip_46_health(); break;
    case 0x47: tsip_47_sat_signals(gps); break;
    case 0x55: tsip_55_io_options(); break;
    case 0x56: tsip_56_velocity(gps); break;
    case 0x5C: tsip_5C_sat_track_status(gps); break;
    case 0x82: tsip_82_dgps_fix(); break;
    case 0x84: tsip_84_position(gps); break;
    }

    return;
}

static void tsip_init(void)
{
    initialized = 0;

    /* reset the receiver to get it into a known state */
    /* it has to reacquire all satellites, which gets annoying... */
    //tsip_25_softreset();

    /* normally this packet is received at the end of a reboot/power up cycle
     * and triggers our side of the initialization sequence, so pretend we just
     * saw one and start the initialization */
    tsip_82_dgps_fix();
}

static void tsip_update(char c, struct gps_state *gps)
{
    static int dles, dle_escape;

    if (c == DLE) {
	if (++dles == 1) return; /* start of packet */
	if (!packet_idx && dles == 2)
	    goto restart;

	/* <dle> inside a packet should be doubled so we drop some */
	if (!dle_escape) {
	    dle_escape = 1;
	    return;
	}
    }

    /* still waiting for start of packet... */
    if (!dles) return;

    /* We should never see dle or etx as the id */
    if (!packet_idx && c == ETX)
	goto restart;

    /* end of packet? */
    if (dle_escape && c == ETX) {
	tsip_decode(gps);

restart:
	packet_idx = 0;
	dle_escape = 0;
	dles = 0;
	return;
    }
    dle_escape = 0;

    packet[packet_idx++] = c;

    /* discard long lines */
    if (packet_idx == MAX_PACKET_SIZE)
	goto restart;
}

REGISTER_PROTOCOL("TSIP", 9600, 'O', tsip_init, NULL, tsip_update);

