/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "gpsapp.h"

static struct {
    int		 timestamp;
    int		 coord_set;
    struct coord coord;
    int		 speed_set;
    double	 speed;
    int		 bearing_set;
    int		 bearing;
    int		 fix;
} gps_tmp;

struct coord gps_coord;
int	     gps_bearing;

static int hex(char c)
{
    char r = c - '0';
    if (r > 9) r -= 'A' - '0' - 10;
    if (r > 15) r -= 'a' - 'A';
    if (r > 15) r = 0;
    return r;
}

static char *field(char *line, int pos)
{
    int i;
    char *p = &line[7];

    for (i = 0; i < pos; i++, p++)
	p = strchr(p, ',');
    return p;
}

static void nmea_time(char *f)
{
    int hour, min, sec, timestamp;
    // time 23 59 59

    if (*f == ',') return;

    if (*f < '0' && *f > '2') return;
    hour = (*(f++) - '0') * 10;
    if (*f < '0' && *f > '9') return;
    hour += *(f++) - '0';
    if (hour > 23) return;

    if (*f < '0' && *f > '5') return;
    min = (*(f++) - '0') * 10;
    if (*f < '0' && *f > '9') return;
    min += *(f++) - '0';

    if (*f < '0' && *f > '5') return;
    sec = (*(f++) - '0') * 10;
    if (*f < '0' && *f > '9') return;
    sec += *(f++) - '0';

    timestamp = ((hour * 60) + min) * 60 + sec;
    if (timestamp != gps_tmp.timestamp) {
	gps_bearing = -1;
	if (gps_tmp.fix && gps_tmp.bearing_set)
	    gps_bearing = gps_tmp.bearing;

	if (gps_tmp.coord_set) {
	    gps_coord.lat = degtorad(gps_tmp.coord.lat);
	    gps_coord.lon = degtorad(gps_tmp.coord.lon);
	    toTM(&gps_coord, &coord_center, &gps_coord.xy);

	    track_pos();
	    route_locate();

	    do_refresh = 1;
	}

	memset(&gps_tmp, 0, sizeof(gps_tmp));
	gps_tmp.timestamp = timestamp;
    }
    return;
}

static void nmea_latitude(char *f)
{
    double dmm, min;
    int deg;
    char *p;

    if (*f == ',') return;
    dmm = strtod(f, &p);
    if (*(p++) != ',') return;
    if (*p != 'N' && *p != 'S') return;

    deg = dmm / 100;
    min = dmm - (deg * 100.0);
    gps_tmp.coord.lat = deg + (min / 60.0);

    if (*p == 'S')
	gps_tmp.coord.lat = -gps_tmp.coord.lat;
}

static void nmea_longtitude(char *f)
{
    double dmm, min;
    int deg;
    char *p;

    if (*f == ',') return;
    dmm = strtod(f, &p);
    if (*(p++) != ',') return;
    if (*p != 'E' && *p != 'W') return;

    deg = dmm / 100;
    min = dmm - (deg * 100.0);
    gps_tmp.coord.lon = deg + (min / 60.0);

    if (*p == 'W')
	gps_tmp.coord.lon = -gps_tmp.coord.lon;
    gps_tmp.coord_set = 1;
}

static void nmea_fix(char *f)
{
    if (*f == 'A' || *f == '1' || *f == '2')
	gps_tmp.fix = 1;
}

static void nmea_speed(char *f)
{
    double speed;
    char *p;

    if (*f == ',') return;
    speed = strtod(f, &p);
    if (*p != ',') return;

    gps_tmp.speed = speed;
    gps_tmp.speed_set = 1;
}

static void nmea_speed_knots(char *f)
{
    double speed;
    char *p;

    if (*f == ',') return;
    speed = strtod(f, &p);
    if (*p != ',') return;

    gps_tmp.speed = speed / 539.9568e-3;
    gps_tmp.speed_set = 1;
}

static void nmea_bearing(char *f)
{
    double bearing;
    char *p;

    if (*f == ',') return;
    bearing = strtod(f, &p);
    if (*p != ',') return;

    gps_tmp.bearing = bearing;
    gps_tmp.bearing_set = 1;
}

void nmea_decode(char *line, char xor)
{
    int n, csum;

    /* NMEA lines should start with a '$' */
    if (line[0] != '$') return;

    /* and end with '*XX' */
    n = strlen(line);
    if (n < 9 || line[n-3] != '*') return;

    /* fix up the xor and check the checksum */
    xor ^= '$' ^ '*' ^ line[n-2] ^ line[n-1];
    csum = hex(line[n-2]) << 4 | hex(line[n-1]);
    if (xor != csum) return;

    if (memcmp(&line[1], "GPGGA", 5) == 0) {
	nmea_time(field(line, 0));
	nmea_latitude(field(line, 1));
	nmea_longtitude(field(line, 3));
	nmea_fix(field(line, 5));
    } else if (memcmp(&line[1], "GPGLL", 5) == 0) {
	nmea_time(field(line, 4));
	nmea_latitude(field(line, 0));
	nmea_longtitude(field(line, 2));
	nmea_fix(field(line, 5));
    } else if (memcmp(&line[1], "GPRMC", 5) == 0) {
	nmea_time(field(line, 0));
	nmea_fix(field(line, 1));
	nmea_latitude(field(line, 2));
	nmea_longtitude(field(line, 4));
	nmea_bearing(field(line, 7));
	if (!gps_tmp.speed_set)
	    nmea_speed_knots(field(line, 6));
    } else if (memcmp(&line[1], "GPVTG", 5) == 0) {
	nmea_bearing(field(line, 0));
	nmea_speed(field(line, 3));
    }
}

