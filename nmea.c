/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "gpsapp.h"

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

struct coord gps_coord;
int	     gps_bearing;
int	     gps_speed;
time_t       gps_time;

static void new_measurement(const struct gps_info *tmp)
{
    static struct gps_info cur;
    struct xy last_coord;

    if (tmp->timestamp && tmp->timestamp != cur.timestamp) {
	last_coord = gps_coord.xy;

	if (cur.coord_set) {
	    gps_coord.lat = degtorad(cur.lat);
	    gps_coord.lon = degtorad(cur.lon);
	    toTM(&gps_coord, &coord_center, &gps_coord.xy);

	    track_pos();
	    route_locate();

	    do_refresh = 1;
	}

	if (!cur.fix)
	    gps_bearing = -1;
	else if (cur.bearing_set)
	    gps_bearing = (int)cur.bearing;
	else
	    gps_bearing = bearing(&last_coord, &gps_coord.xy);

	if (cur.speed_set)
	    gps_speed = (int)(cur.speed * 1000);
	else
	    gps_speed = 0;

	gps_time = cur.timestamp + cur.datestamp;

	memset(&cur, 0, sizeof(cur));
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
    sec = strtol(*p, p, 10);
    if (**p != ',')
	return skip(p);

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

    year += 2000;
    if (day >= 1 && day <= 31 && mon >= 1 &&
	mon <= 12 && year >= 2000 && year <= 2099) {
#define	GPSEPOCH 1980
#define GPSLEAPS 479
#define	EPOCHDIFF 315532800
	int mds[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	int days;
	int isleap = !(year % 4) && ((year % 100) || !(year % 400));
	
	days = 365 * (year - GPSEPOCH) + /* years */
	       year / 4 - year / 100 + year / 400 - GPSLEAPS + /* leap years */
	       mds[mon-1] + /* months */
	       day - 1;     /* days */
	if (mon <= 2 && isleap) days--; /* start of a leap year? */

	datestamp = days * 86400 + EPOCHDIFF;
    }

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
    if (**p != ',')
	return skip(p);

    return fix;
}

static int nmea_float(char **p, double *val)
{
    double tmp;

    if (**p != ',')
	return skip(p);

    (*p)++;
    tmp = strtod(*p, p);
    if (**p != ',')
	return skip(p);

    *val = tmp;
    return 1;
}

void nmea_decode(char *line, char xor)
{
    int n, csum;
    struct gps_info tmp;
    char *p;

    /* NMEA lines should start with a '$' */
    if (line[0] != '$') return;

    /* and end with '*XX' */
    n = strlen(line);
    if (n < 9 || line[n-3] != '*') return;

    /* fix up the xor and check the trailing checksum */
    xor ^= '$' ^ '*' ^ line[n-2] ^ line[n-1];
    csum = hex(line[n-2]) << 4 | hex(line[n-1]);
    if (xor != csum) return;

#ifndef __arm__
    fprintf(stderr, "%s\n", line);
#endif

    p = &line[6];
    memset(&tmp, 0, sizeof(tmp));

    if (memcmp(&line[1], "GPGGA", 5) == 0) {
	tmp.timestamp      = nmea_time(&p);
	tmp.coord_set      = nmea_latlong(&p, &tmp.lat, 'N', 'S');
	tmp.coord_set     += nmea_latlong(&p, &tmp.lon, 'E', 'W');
	tmp.fix            = nmea_fix(&p);
    } else if (memcmp(&line[1], "GPGLL", 5) == 0) {
	tmp.coord_set      = nmea_latlong(&p, &tmp.lat, 'N', 'S');
	tmp.coord_set     += nmea_latlong(&p, &tmp.lon, 'E', 'W');
	tmp.timestamp      = nmea_time(&p);
	tmp.fix            = nmea_fix(&p);
    } else if (memcmp(&line[1], "GPRMC", 5) == 0) {
	tmp.timestamp      = nmea_time(&p);
	tmp.fix            = nmea_fix(&p);
	tmp.coord_set      = nmea_latlong(&p, &tmp.lat, 'N', 'S');
	tmp.coord_set     += nmea_latlong(&p, &tmp.lon, 'E', 'W');
	if (tmp.speed_set) skip(&p);
	else {
	    tmp.speed_set  = nmea_float(&p, &tmp.speed);
	    tmp.speed = tmp.speed / 539.9568e-3;
	}
	tmp.bearing_set    = nmea_float(&p, &tmp.bearing);
	tmp.datestamp	   = nmea_date(&p);
    } else if (memcmp(&line[1], "GPVTG", 5) == 0) {
	tmp.bearing_set    = nmea_float(&p, &tmp.bearing);
	skip(&p);
	skip(&p);
	tmp.speed_set      = nmea_float(&p, &tmp.speed);
    }
    new_measurement(&tmp);
}

