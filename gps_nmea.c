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

struct gps_info {
    int		 timestamp;
    int		 datestamp;
    int		 coord_set;
    double	 lat;
    double	 lon;
    int		 speed_set;
    double	 speed;
    int		 bearing_set;
    double	 bearing;
    int		 fix;
};

static int timestamp, got_sats;

static int new_measurement(const struct gps_info *tmp, struct gps_state *gps)
{
    static struct gps_info cur;
    struct xy last_coord;
    double b;
    int update = 0;

    if (tmp->timestamp && tmp->timestamp != cur.timestamp) {
	last_coord = gps_coord.xy;

	if (cur.coord_set) {
	    gps->lat = degtorad(cur.lat);
	    gps->lon = degtorad(cur.lon);
	}

        if (cur.bearing_set) b = cur.bearing;
	else                 b = bearing(&last_coord, &gps_coord.xy);

	if (cur.speed_set) {
	    double speed = cur.speed / 3.6; // * 1000 / 3600
	    gps->spd_east  = sin(b) * speed;
	    gps->spd_north = cos(b) * speed;
	    gps->spd_up    = 0.0;
	}

	if (!cur.fix)
	    gps->bearing = -1;
	else
	    gps->bearing = radtodeg(b);

	/* Some gps's don't give us any NMEA sentences with a date, we can try
	 * to compensate by pulling the date off of the local clock... */
	if (!cur.datestamp) {
	    time_t now = time(NULL);
	    struct tm *tm = gmtime(&now);
	    cur.datestamp = conv_date(tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
	}
	gps->time = timestamp = cur.timestamp + cur.datestamp;

	memset(&cur, 0, sizeof(cur));

	update = 1;
    }

    if (tmp->timestamp)
	cur.timestamp = tmp->timestamp;
    if (tmp->datestamp)
	cur.datestamp = tmp->datestamp;

    if (tmp->coord_set == 2) {
	cur.lat = tmp->lat;
	cur.lon = tmp->lon;
	cur.coord_set = 2;
    }

    if (tmp->fix)
	cur.fix = 1;

    if (tmp->bearing_set) {
	cur.bearing = tmp->bearing;
	cur.bearing_set = 1;
    }

    if (tmp->speed_set) {
	cur.speed = tmp->speed;
	cur.speed_set = 1;
    }
#if 0
    printf("ts %d fix %d lat %.6f lon %.6f spd %.2f%s bear %.2f%s\n",
	   cur.timestamp, cur.fix, cur.lat, cur.lon,
	   cur.speed, cur.speed_set ? "" : "?",
	   cur.bearing, cur.bearing_set ? "" : "?");
#endif
    if (got_sats) {
	update = 1;
	got_sats = 0;
    }
    return update;
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
	mon <= 12 && year >= 00 && year <= 99)
	    datestamp = conv_date(year + 2000, mon, day);

    return datestamp;
}

static int nmea_latlong(char **p, double *latlong, char pos, char neg)
{
    double min;
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
    *latlong = deg + (min / 60.0);

    if (inv)
	*latlong = -*latlong;

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

int nmea_decode(char xor, struct gps_state *gps)
{
    double b;
    int n, csum;
    struct gps_info tmp;
    char *p;

    /* NMEA lines should start with a '$' */
    if (packet[0] != '$') {
	/* recognize tripmate's 'ASTRAL' message */
	if (packet_idx >= 6 && memcmp(packet, "ASTRAL", 6) == 0)
	    serial_send("$IIGPQ,ASTRAL*73\r\n", 18);
	return 0;
    }

    /* and end with '*XX' */
    n = strlen(packet);
    if (n < 9 || packet[n-3] != '*') return 0;

    /* fix up the xor and check the trailing checksum */
    xor ^= '$' ^ '*' ^ packet[n-2] ^ packet[n-1];
    csum = hex(packet[n-2]) << 4 | hex(packet[n-1]);
    if (xor != csum) return 0;

#ifndef __arm__
    fprintf(stderr, "%s\n", packet);
#endif

    p = &packet[6];
    memset(&tmp, 0, sizeof(tmp));

    if (memcmp(&packet[1], "GPGGA", 5) == 0) {
	/* $GPGGA,time,lat,N/S,long,E/W,fix(0/1/2),nsat,HDOP,alt,gheight,
	 * dgpsdt,dgpsid* */
	tmp.timestamp      = nmea_time(&p);
	tmp.coord_set      = nmea_latlong(&p, &tmp.lat, 'N', 'S');
	tmp.coord_set     += nmea_latlong(&p, &tmp.lon, 'E', 'W');
	tmp.fix            = nmea_fix(&p);
    } else if (memcmp(&packet[1], "GPGLL", 5) == 0) {
	/* $GPGLL,lat,N/S,long,E/W,time,fix(?/A)* */
	tmp.coord_set      = nmea_latlong(&p, &tmp.lat, 'N', 'S');
	tmp.coord_set     += nmea_latlong(&p, &tmp.lon, 'E', 'W');
	tmp.timestamp      = nmea_time(&p);
	tmp.fix            = nmea_fix(&p);
    } else if (memcmp(&packet[1], "GPRMC", 5) == 0) {
	/* $GPRMC,time,fix(V/A),lat,N/S,long,E/W,knot-speed,bear,date,magnvar**/
	tmp.timestamp      = nmea_time(&p);
	tmp.fix            = nmea_fix(&p);
	tmp.coord_set      = nmea_latlong(&p, &tmp.lat, 'N', 'S');
	tmp.coord_set     += nmea_latlong(&p, &tmp.lon, 'E', 'W');
	if (tmp.speed_set) {
	    p++; skip(&p);
	}
	else {
	    tmp.speed_set  = nmea_float(&p, &tmp.speed);
	    tmp.speed = tmp.speed / 539.9568e-3;
	}
	tmp.bearing_set    = nmea_float(&p, &b);
	tmp.datestamp	   = nmea_date(&p);

	if (tmp.bearing_set)
		tmp.bearing = degtorad(b);
    } else if (memcmp(&packet[1], "GPVTG", 5) == 0) {
	/* $GPVTG,bear,T,magnbear,M,knot-speed,N,kph-speed,K* */
	tmp.bearing_set = nmea_float(&p, &b); skip(&p);
	p++; skip(&p); p++; skip(&p);
	p++; skip(&p); p++; skip(&p);
	tmp.speed_set = nmea_float(&p, &tmp.speed);

	if (tmp.bearing_set)
		tmp.bearing = degtorad(b);
    } else if (memcmp(&packet[1], "GPGSV", 5) == 0) {
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
	    snr = (nmea_int(&p) - 20) / 2;

	    new_sat(gps, svn, timestamp, elv, azm, snr, UNKNOWN_USED);
	}
	got_sats = (nmsg == msg);
    } else if (memcmp(&packet[1], "GPGSA", 5) == 0) {
	/* $GPGSA,mode,fix(0/1/2D/3D),sat1,...,sat12,pdop,hdop,vdop* */
	int i, svn;
	p++; skip(&p); p++; skip(&p);
	for (i = 0; i < MAX_TRACKED_SATS; i++)
	    gps->sats[i].used = 0;
	for (i = 0; i < 12; i++)
	    svn = nmea_int(&p);
	    new_sat(gps, svn, timestamp, UNKNOWN_ELV, UNKNOWN_AZM, UNKNOWN_SNR, 1);
    }
    return new_measurement(&tmp, gps);
}
   
static int nmea_update(char c, struct gps_state *gps)
{
    static char xor;
    int update;

    if (c == '\r') return 0;

    if (packet_idx == MAX_PACKET_SIZE) {
	/* discard long lines */
	packet_idx = 0;
	xor = '\0';
    }

    if (c == '\n') {
	packet[packet_idx] = '\0';

	update = nmea_decode(xor, gps);

	packet_idx = 0;
	xor = '\0';

	return update;
    }

    packet[packet_idx++] = c;
    xor ^= c;

    return 0;
}

static void nmea_init(void)
{
  serial_send("$PMOTG,VTG,0001\r\n", 17);
}

REGISTER_PROTOCOL("NMEA", 4800, 'N', nmea_init, nmea_update);

