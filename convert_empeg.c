/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include "gpsapp.h"

#define WGS84_a    6378137.0
#define WGS84_invf 298.257223563
#define UTM_k0	   0.9996

int stats_fromTM;
int stats_toTM;
int stats_distance;
int stats_bearing;

double degtorad(const double deg)
{
    return deg * ((2.0 * M_PI) / 360.0);
}

double intdegtorad(const int deg)
{
    return (double)deg * (M_PI / (double)0x7fffffff);
}

double radtodeg(const double rad)
{
    return rad * (360.0 / (2.0 * M_PI));
}

char *formatdist(const unsigned int dist)
{
#define meters_per_mile 1609.344
#define meters_per_foot 0.3048
    static char buf[10];

    if (show_metric) {
	if (dist < 1000)
	    sprintf(buf, "%um ", dist);
	else {
	    unsigned int hectameters = dist / 100;
	    if (hectameters < 1000)
		sprintf(buf, "%u.%ukm", hectameters / 10, hectameters % 10);
	    else
		sprintf(buf, "%ukm", hectameters / 10);
	}
    } else {
	if (dist < 305) // ~1000 feet
	    sprintf(buf, "%uft", (dist * 10000) / 3048);
	else {
	    unsigned int decimiles = (dist * 1000) / 160934; // .4
	    if (decimiles < 1000) {
		sprintf(buf, "%u.%umi", decimiles / 10, decimiles % 10);
	    } else
		sprintf(buf, "%umi", decimiles / 10);
	}
    }
    return buf;
}

char *time_estimate(const unsigned int dist)
{
    static char buf[10];
    int hour, min, sec = 0;

    if (dist && gps_speed)
	sec = ((dist * 3600) / gps_speed);

    if (!dist || gps_speed) {
	if (show_abs) {
	    struct tm *tm;
	    time_t s = sec + gps_time;
	    tm = localtime(&s);

	    hour = tm->tm_hour;
	    min  = tm->tm_min;
	    sec  = tm->tm_sec;
	} else {
	    min = sec / 60;
	    sec -= min * 60;
	    hour = min / 60;
	    min -= hour * 60;
	}
	if (hour) sprintf(buf, "%02d:%02d", hour, min);
	else      sprintf(buf, "%02dm%02d", min, sec);
    } else
	sprintf(buf, "--?--");

    return buf;
}

/* geodetic coordinates to cartographic projection (Transverse Mercator) */
static double M(const double phi)
{
    double es, es2, es3;
    
    if (phi == 0.0) return 0.0;

    // f = 1.0 / WGS84_invf;
    es  = 0.0066943799901413165; // f * (2 - f);
    es2 = 4.481472345240445e-05; // es * es;
    es3 = 3.000067879434931e-07; // es * es * es;

    return WGS84_a * ((1.0 - es/4.0 - 3.0*es2/64.0 - 5.0*es3/ 256.0) * phi -
		      (3.0*es/8.0+3.0*es2/32.0+45.0*es3/1024.0)*sin(2.0*phi) + 
		      (15.0 * es2/256.0 + 45.0 * es3/1024.0) * sin(4.0 * phi) -
		      (35.0 * es3 / 3072.0) * sin(6.0 * phi));
}

void toTM(const struct coord *point, const struct coord *center, struct xy *xy)
{
    double phi, phi0, lambda, lambda0;
    double m, m0, sin_phi, cos_phi, tan_phi, es, et2, n, t, c, A;

    phi     = point->lat;
    lambda  = point->lon;
    phi0    = center->lat;
    lambda0 = center->lon;

    m = M(phi);
    m0 = M(phi0);

    sin_phi = sin(phi);
    cos_phi = cos(phi);
    tan_phi = tan(phi);

    /* these three should get collapsed to constants by the compiler */
    // f = 1.0 / WGS84_invf;
    es  = 0.0066943799901413165; // f * (2.0 - f);
    et2 = 0.0067394967422764341; // es / (1.0 - es);

    n = WGS84_a / sqrt(1.0 - es * sin_phi * sin_phi);
    t = tan_phi * tan_phi;
    c = et2 * cos_phi * cos_phi;
    A = (lambda - lambda0) * cos_phi;
    
    xy->x = UTM_k0 * n * (A + (1.0 - t + c) * A * A * A / 6.0 +
			  (5.0 - 18.0 * t + t * t + 72.0 * c - 58.0 * et2) *
			  A * A * A * A * A / 120.0);
    xy->y = UTM_k0 * (m - m0 + n * tan(phi) *
		      (A * A / 2.0 + (5.0 - t + 9.0 * c + 4 * c * c) *
		       A * A * A * A / 24.0 +
		       (61.0 - 58.0 * t + t * t + 600.0 * c - 330.0 * et2) *
		       A * A * A * A * A * A / 720.0));
    stats_toTM++;
}

long long distance2(const struct xy *coord1, const struct xy *coord2)
{
    long long dx, dy, dist;
    dx = coord1->x - coord2->x;
    dy = coord1->y - coord2->y;
    dist = dx * dx + dy * dy;
    stats_distance++;
    return dist;
}

int bearing(const struct xy *coord1, const struct xy *coord2)
{
    int dx, dy, b;
    dx = coord2->x - coord1->x;
    dy = coord2->y - coord1->y;
    if (!dx && !dy) return -1;

    b = radtodeg(atan2(dx, dy));
    if (b < 0) b += 360;
    stats_bearing++;
    return b;
}

int towards(const struct xy *here, const struct xy *coord, const int dir)
{
    int b;
    
    if (dir == -1) return 1;
    b = bearing(here, coord) - dir;
    if (b < 0) b += 360;
    return (b <= 90 || b >= 270);
}

