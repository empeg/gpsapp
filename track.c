#include <stdio.h>
#include <stdlib.h>
#include "gpsapp.h"
#include "vfdlib.h"

static FILE *track;

static struct coord prev, next;
static int pts, nts, cts;

#define MAX_TRACK 500
static struct xy *tracklog;
static int tracklog_size;
static int tracklog_idx;

static int read_pos(struct coord *coord, int *timestamp)
{
    char line[255], time[10], *p;
    double lat, lon;
    int n;

    do {
	if (!track || feof(track))
	    return -1;

	fgets(line, sizeof(line), track);
	n = sscanf(line, "T %*s %s %lf %lf", time, &lat, &lon);
    } while (n != 3);

    *timestamp = strtol(time, &p, 10); *timestamp *= 60; p++;
    *timestamp += strtol(p, &p, 10);   *timestamp *= 60; p++;
    *timestamp += strtol(p, &p, 10);

    coord->lat = degtorad(lat);
    coord->lon = degtorad(lon);

    return 0;
}

struct coord center;

void track_init(struct coord *ctr)
{
    if (!tracklog)
	tracklog = malloc(MAX_TRACK * sizeof(struct xy));
    tracklog_size = tracklog_idx = 0;

    track = fopen("programs0/track", "r");
    read_pos(&prev, &pts);
    read_pos(&next, &nts);

    if (ctr) center = *ctr;
    else     center = prev;

    toTM(&prev, &center, &prev.xy);
    toTM(&next, &center, &next.xy);

    tracklog[0] = prev.xy;
    cts = pts;
}

void track_jump(const int fwrev)
{
    struct coord pnt;
    int ts;

    if (fwrev) {
	if (read_pos(&pnt, &ts) == -1)
	    return;
	prev = next; next = pnt;
	pts = nts; nts = ts;
    }
    else {
	rewind(track);
	read_pos(&prev, &pts);
	read_pos(&next, &nts);
	while (nts < cts) {
	    if (read_pos(&pnt, &ts) == -1)
		return;
	    prev = next; next = pnt;
	    pts = nts; nts = ts;
	}
    }
    cts = pts;
}

int track_pos(struct coord *coord, int *dir)
{
    static struct coord cur_coord;
    static int cur_dir;
    int updated = 0;
    double ratio;
    struct coord pnt;
    int ts;

    static int slowdown;
    if ((slowdown++ % 10) != 0)
	goto done;

    while (cts >= nts) {
	if (read_pos(&pnt, &ts) == -1)
	    goto done;
	prev = next; next = pnt;
	pts = nts; nts = ts;
    }

    cts += 2; // faster than a speeding bullet?
    cur_coord = prev;

    ratio = (double)(cts - pts) / (double)(nts - pts);
    cur_coord.lat += (next.lat - prev.lat) * ratio;
    cur_coord.lon += (next.lon - prev.lon) * ratio;

    toTM(&cur_coord, &center, &cur_coord.xy);
    cur_dir = bearing(&tracklog[tracklog_idx], &cur_coord.xy);

    if (tracklog_size < MAX_TRACK) tracklog_size++;
    if (++tracklog_idx == MAX_TRACK) tracklog_idx = 0;
    tracklog[tracklog_idx] = cur_coord.xy;

    updated = 1;
done:
    if (coord)  *coord  = cur_coord;
    if (dir)    *dir    = cur_dir;
    return updated;
}

void track_draw(unsigned char *screen)
{
    int head = tracklog_idx + 1;	

    if (tracklog_size == MAX_TRACK)
	draw_route(screen, &tracklog[head], MAX_TRACK - head, VFDSHADE_DIM);

    draw_route(screen, tracklog, head, VFDSHADE_DIM);
}

void track_free(void)
{
    if (track)
	fclose(track);

    free(tracklog);
}

