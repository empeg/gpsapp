#include "gps_protocol.h"

void new_sat(struct gps_state *gps, int svn, int time, double elv, double azm, int snr, int used)
{
    int i, old, old_time = 0;

    if (!svn) return;

    for (i = 0; i < MAX_TRACKED_SATS; i++)
	if (gps->sats[i].svn == svn)
	    goto update;

    old = -1;
    /* not previously seen satellite, replace the oldest entry */
    for (i = 0; i < MAX_TRACKED_SATS; i++) {
	if (old == -1 || gps->sats[i].time < old_time) {
	    old = i;
	    old_time = gps->sats[i].time;
	}
    }
    i = old;

update:
    gps->sats[i].svn = svn;
    if (time != UNKNOWN_TIME) gps->sats[i].time = time;
    if (elv != UNKNOWN_ELV)   gps->sats[i].elv  = elv;
    if (azm != UNKNOWN_AZM)   gps->sats[i].azm  = azm;
    if (snr != UNKNOWN_SNR)   gps->sats[i].snr  = snr;
    if (used != UNKNOWN_USED) gps->sats[i].used = used;
}

/* convert year/month/day to unix time */
int conv_date(int year, int mon, int day)
{
#define	GPSEPOCH 1980
#define GPSLEAPS 479
#define	EPOCHDIFF 315532800
    int mds[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int isleap;
    int days;
	
    /* days will be # of days elapsed since the GPS epoch (1/1/1980) */
    days =
	/* days_per_year * years */
	365 * (year - GPSEPOCH) +
	/* add extra days for leap years */
	year / 4 - year / 100 + year / 400 - GPSLEAPS +
	/* days_per_month * months */
	mds[mon-1] +
	/* days into the month*/
	day - 1;

    isleap = !(year % 4) && ((year % 100) || !(year % 400));
    if (mon <= 2 && isleap)
	days--; /* adjust for the first 2 months in a leap year */

    return days * 86400 + EPOCHDIFF;
}

