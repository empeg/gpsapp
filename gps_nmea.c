/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "gpsapp.h"
#include "gps_protocol.h"

static int datestamp;

static time_t today(void)
{
    /* Some gps's don't give us any NMEA sentences with a date, we can try
     * to compensate by pulling the date off of the local clock... */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    return conv_date(tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
}

static int hex(char c)
{
    char r = c - '0';
    if (r > 9) r -= 'A' - '0' - 10;
    if (r > 15) r -= 'a' - 'A';
    if (r > 15) r = 0;
    return r;
}

static int skip(char **p)
{
    while (**p && **p != ',') (*p)++;
    return 0;
}

static int nmea_time(char **p)
{
    int hour, min, sec, timestamp = 0;

    if (**p != ',')
	return skip(p);

    (*p)++;
    sec = strtod(*p, p);

    min  = sec / 100;
    sec -= min * 100;
    hour = min / 100;
    min -= hour * 100;

    if (hour >= 0 && hour <= 23 && min >= 0 &&
	min <= 59 && sec >= 0 && sec <= 59)
	timestamp = (hour * 60 + min) * 60 + sec;

    return timestamp;
}

static int nmea_date(char **p)
{
    int day, mon, year, datestamp = 0;

    if (**p != ',')
	return skip(p);

    (*p)++;
    year = strtol(*p, p, 10);
    if (**p != ',')
	return skip(p);

    mon   = year / 100;
    year -= mon * 100;
    day   = mon / 100;
    mon  -= day * 100;

    if (day >= 1 && day <= 31 && mon >= 1 &&
	mon <= 12 && year >= 0 && year <= 99)
	    datestamp = conv_date(year + 2000, mon, day);

    return datestamp;
}

static int nmea_latlong(char **p, double *latlong, char pos, char neg)
{
    double min, tmp;
    int deg, inv;

    if (**p != ',')
	return skip(p);

    (*p)++;
    min = strtod(*p, p);
    if (**p != ',')
	return skip(p);

    (*p)++;
    inv = (**p == neg);
    if (!inv && **p != pos)
	return skip(p);
    (*p)++;

    if (**p != ',')
	return skip(p);

    deg = min / 100;
    min -= deg * 100.0;
    tmp = deg + (min / 60.0);

    if (inv)
	tmp = -tmp;

    *latlong = degtorad(tmp);
    return 1;
}

static int nmea_fix(char **p)
{
    int fix;
    
    if (**p != ',')
	return skip(p);

    (*p)++;
    fix = (**p == 'A' || **p == '1' || **p == '2');
    (*p)++;

    return fix;
}

static int nmea_int(char **p)
{
    int tmp;

    if (**p != ',') {
	skip(p);
	return 0;
    }

    (*p)++;
    tmp = strtol(*p, p, 10);

    return tmp;
}

static int nmea_float(char **p, double *val)
{
    double tmp;

    if (**p != ',')
	return skip(p);

    (*p)++;
    tmp = strtod(*p, p);

    *val = tmp;
    return 1;
}

void nmea_decode(struct gps_state *gps)
{
    char *p;

#ifndef __arm__
    fprintf(stderr, "%s\n", packet);
#endif

    p = &packet[6];

    if (memcmp(&packet[1], "GPGGA", 5) == 0) {
	/* $GPGGA,time,lat,N/S,long,E/W,fix(0/1/2),nsat,HDOP,alt,gheight,
	 * dgpsdt,dgpsid* */
	int timestamp, lat_set, lon_set, fix;
	double lat, lon, tmp;

	timestamp = nmea_time(&p);
	gps->time = timestamp + datestamp;
	if (!datestamp) gps->time += today();

	lat_set = nmea_latlong(&p, &lat, 'N', 'S');
	lon_set = nmea_latlong(&p, &lon, 'E', 'W');

	fix = nmea_fix(&p);
	if (fix && !(gps->fix & 0x1)) {
	    gps->fix |= 0x1;
	    gps->updated |= GPS_STATE_FIX;
	}
	if (!fix && (gps->fix & 0x1)) {
	    gps->fix &= ~0x1;
	    gps->updated |= GPS_STATE_FIX;
	}

	p++; skip(&p); /* nsats */

	if (nmea_float(&p, &tmp)) gps->hdop = tmp;
	if (nmea_float(&p, &tmp)) gps->alt = tmp;

	if (lat_set && lon_set) {
	    gps->lat = lat;
	    gps->lon = lon;
	    gps->updated |= GPS_STATE_COORD;
	}
    }
    else if (memcmp(&packet[1], "GPGLL", 5) == 0) {
	/* $GPGLL,lat,N/S,long,E/W,time,fix(?/A)* */
	int timestamp, lat_set, lon_set, fix;
	double lat, lon;

	lat_set = nmea_latlong(&p, &lat, 'N', 'S');
	lon_set = nmea_latlong(&p, &lon, 'E', 'W');
	timestamp = nmea_time(&p);

	fix = nmea_fix(&p);
	if (fix && !(gps->fix & 0x1)) {
	    gps->fix |= 0x1;
	    gps->updated |= GPS_STATE_FIX;
	}
	if (!fix && (gps->fix & 0x1)) {
	    gps->fix &= ~0x1;
	    gps->updated |= GPS_STATE_FIX;
	}

	gps->time = timestamp + datestamp;
	if (!datestamp) gps->time += today();

	if (lat_set && lon_set) {
	    gps->lat = lat;
	    gps->lon = lon;
	    gps->updated |= GPS_STATE_COORD;
	}

    }
    else if (memcmp(&packet[1], "GPRMC", 5) == 0) {
	/* $GPRMC,time,fix(V/A),lat,N/S,long,E/W,knot-speed,bear,date,magnvar**/
	int timestamp, fix, lat_set, lon_set, spd_set;
	double lat, lon, spd, bearing;

	timestamp = nmea_time(&p);

	fix = nmea_fix(&p);
	if (fix && !(gps->fix & 0x1)) {
	    gps->fix |= 0x1;
	    gps->updated |= GPS_STATE_FIX;
	}
	if (!fix && (gps->fix & 0x1)) {
	    gps->fix &= ~0x1;
	    gps->updated |= GPS_STATE_FIX;
	}

	lat_set = nmea_latlong(&p, &lat, 'N', 'S');
	lon_set = nmea_latlong(&p, &lon, 'E', 'W');
	spd_set = nmea_float(&p, &spd);
	if (nmea_float(&p, &bearing)) {
	    gps->bearing = (int)bearing;
	    gps->updated |= GPS_STATE_BEARING;
	}
	datestamp = nmea_date(&p);

	gps->time = timestamp + datestamp;

	if (lat_set && lon_set) {
	    gps->lat = lat;
	    gps->lon = lon;
	    gps->updated |= GPS_STATE_COORD;
	}

	if (spd_set) {
	    double b = degtorad(gps->bearing);
	    spd /= 539.9568e-3 /* knots to kph */ * 3.6 /* kph to m/s */;
	    gps->spd_east  = sin(b) * spd;
	    gps->spd_north = cos(b) * spd;
	    gps->spd_up    = 0.0;
	    gps->updated |= GPS_STATE_SPEED;
	}
    }
    else if (memcmp(&packet[1], "GPVTG", 5) == 0) {
	/* $GPVTG,bear,T,magnbear,M,knot-speed,N,kph-speed,K* */
	double bearing, spd;

	if (nmea_float(&p, &bearing)) {
		gps->bearing = (int)bearing;
		gps->updated |= GPS_STATE_BEARING;
	}

	p++; skip(&p); // T
	p++; skip(&p); p++; skip(&p); // magnbear, M
	p++; skip(&p); p++; skip(&p); // knot-speed, N

	if (nmea_float(&p, &spd)) {
	    double b = degtorad(gps->bearing);
	    spd /= 3.6 /* kph to m/s */;
	    gps->spd_east  = sin(b) * spd;
	    gps->spd_north = cos(b) * spd;
	    gps->spd_up    = 0.0;
	    gps->updated |= GPS_STATE_SPEED;
	}
    }
    else if (memcmp(&packet[1], "GPGSV", 5) == 0) {
	/* $GPGSV,nmsg,msg,nsat,svn1,elv1,azm1,snr1,...,svn4,elv4,azm4,snr4* */
	double elv, azm;
	int i, svn, snr;
	int nmsg, msg;

	nmsg = nmea_int(&p);
	msg = nmea_int(&p);

	p++; skip(&p);

	for (i = 0; i < 4; i++) {
	    svn = nmea_int(&p);
	    elv = degtorad(nmea_int(&p));
	    azm = degtorad(nmea_int(&p));
	    snr = nmea_int(&p) / 4;

	    new_sat(gps, svn, gps->time, elv, azm, snr, UNKNOWN_USED);
	}
	if (msg == nmsg)
	    gps->updated |= GPS_STATE_SIGNALS | GPS_STATE_SATS;
    }
    else if (memcmp(&packet[1], "GPGSA", 5) == 0) {
	/* $GPGSA,mode,fix(0/1/2D/3D),sat1,...,sat12,pdop,hdop,vdop* */
	int i, fix, svn;
	double tmp;

	p++; skip(&p);

	fix = nmea_int(&p);
	switch (fix) {
	case 3: fix |= 0x2; break;
	case 2: fix &= ~0x2; break;
	case 1: fix = 0; break;
	default: break;
	}

	for (i = 0; i < MAX_TRACKED_SATS; i++)
	    gps->sats[i].used = 0;

	for (i = 0; i < 12; i++) {
	    svn = nmea_int(&p);
	    new_sat(gps, svn, gps->time, UNKNOWN_ELV, UNKNOWN_AZM, UNKNOWN_SNR, 1);
	}
	if (nmea_float(&p, &tmp)) gps->hdop = tmp;
	gps->updated |= GPS_STATE_FIX | GPS_STATE_SATS;
    }
}
   
static void nmea_update(char c, struct gps_state *gps)
{
    static char xor;

    if (c == '\r') return;

    if (packet_idx == MAX_PACKET_SIZE) {
	/* discard long lines */
	packet_idx = 0;
	xor = '\0';
    }

    if (c == '\n') {
	int n, csum;

	packet[packet_idx] = '\0';

	/* NMEA lines should start with a '$' */
	if (packet[0] != '$') {
	    /* recognize tripmate's 'ASTRAL' message */
	    if (packet_idx >= 6 && memcmp(packet, "ASTRAL", 6) == 0)
		serial_send("$IIGPQ,ASTRAL*73\r\n", 18);
	    goto restart;
	}

	/* and end with '*XX' */
	n = strlen(packet);
	if (n < 9 || packet[n-3] != '*') goto restart;

	/* fix up the xor and check the trailing checksum */
	xor ^= '$' ^ '*' ^ packet[n-2] ^ packet[n-1];
	csum = hex(packet[n-2]) << 4 | hex(packet[n-1]);
	if (xor != csum) goto restart;

	nmea_decode(gps);

restart:
	packet_idx = 0;
	xor = '\0';

	return;
    }

    packet[packet_idx++] = c;
    xor ^= c;

    /* discard long lines */
    if (packet_idx == MAX_PACKET_SIZE)
	goto restart;
}

static void nmea_init(void)
{
  char buf[40];
  time_t t;
  struct tm *tm;
  unsigned short *dat;

  t = time(NULL);
  tm = gmtime(&t);

  serial_send("$PMOTG,GGA,0001\r\n", 17);
  serial_send("$PMOTG,RMC,0001\r\n", 17);
  serial_send("$PMOTG,GSA,0001\r\n", 17);
  serial_send("$PMOTG,GSV,0001\r\n", 17);
  snprintf(buf, 40, "$PRWIINIT,V,,,,,,,,,,,,%02d%02d%02d,%02d%02d%02d\r\n",
	  tm->tm_hour, tm->tm_min, tm->tm_sec,
	  tm->tm_mday, tm->tm_mon + 1, tm->tm_year);
  serial_send(buf, 38);

  if (do_coldstart==0) {
    memset(buf, 0, sizeof(buf));
    dat = (unsigned short *)buf;
    dat[0] = 1; /* serial number, should increment XXX */
    dat[1] = (1 << 0); /* disable cold start */
    zodiac_send(1216, dat, 4); /* 9 - 5 */

    memset(buf, 0, sizeof(buf));
    dat = (unsigned short *)buf;
    dat[0] = 2; /* serial number, should increment XXX */
    dat[1] = 5; /* car */
    zodiac_send(1220, dat, 3); /* 8 - 5 */
  }
}

REGISTER_PROTOCOL("NMEA", 4800, 'N', nmea_init, NULL, nmea_update);

