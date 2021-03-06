/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

/*
 * Reads tracklogs as dumped by gpstrans with lines of the following format,
 * "T	09/14/2002 13:52:20	39.6513319	-83.5433006"
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "gpsapp.h"
#include "gps_protocol.h"

struct state {
    time_t time;
    double lat, lon;
};

static struct state prev;
static FILE *track;

static int tracklog_read(struct state *pos)
{
    int hour, min, sec;

next:
    if (feof(track)) return 0;
    fgets(packet, MAX_PACKET_SIZE, track);
    if (packet[0] == '$') return 2;
    if (packet[0] != 'T') goto next;

    hour = strtol(&packet[13], NULL, 10);
    min = strtol(&packet[16], NULL, 10);
    sec = strtol(&packet[19], NULL, 10);

    pos->time = (hour * 60 + min) * 60 + sec;
    pos->lat = strtod(&packet[21], NULL);
    pos->lon = strtod(&packet[32], NULL);
    return 1;
}

void tracklog_init(void)
{
    extern int serialfd;

    if (track)
	fclose(track);

    track = fopen("track", "r");
    serialfd = fileno(track);
    //if (tracklog_read(&prev) == 2) rewind(track);

    gps_state.time = prev.time;
}

extern void nmea_decode(struct gps_state *gps);
void tracklog_update(char c, struct gps_state *gps)
{
    struct state cur;
    long offset;
    double ratio, dlat, dlon;
    static struct xy last;

    //static int ms;
    //if ((ms++ % 10) != 0) return;

    gps->time++;

next:
    offset = ftell(track);

    switch (tracklog_read(&cur)) {
    case 2:  nmea_decode(gps);
    case 0:  return;
    default: break;
    }

    if (gps->time >= cur.time) {
	prev = cur;
	goto next;
    }

    ratio = ((double)(gps->time - prev.time) / (double)(cur.time - prev.time));
    dlat = cur.lat - prev.lat;
    dlon = cur.lon - prev.lon;

    gps->lat = degtorad(dlat * ratio + prev.lat);
    gps->lon = degtorad(dlon * ratio + prev.lon);
    gps->updated |= GPS_STATE_COORD;

    gps_coord.lat = gps->lat;
    gps_coord.lon = gps->lon;
    toTM(&gps_coord);

    gps->bearing = radtodeg(atan2(dlon, dlat));
    if (gps->bearing < 0) gps->bearing += 360;
    gps->updated |= GPS_STATE_BEARING;

    gps->spd_east  = gps_coord.xy.x - last.x;
    gps->spd_north = gps_coord.xy.y - last.y;
    gps->spd_up    = 0;

    if (abs(gps->spd_east)  > 1e6) gps->spd_east = 0;
    if (abs(gps->spd_north) > 1e6) gps->spd_north = 0;
    gps->updated |= GPS_STATE_SPEED;

    last = gps_coord.xy;
    fseek(track, offset, SEEK_SET);
}

REGISTER_PROTOCOL("TRACKLOG", 0, 'N', tracklog_init, NULL, tracklog_update);

