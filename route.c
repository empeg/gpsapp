#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gpsapp.h"
#include "vfdlib.h"

/* center is needed to project GPS coords into map coords */ 
struct coord center;
struct route route;

/* saved state variables after a successful locate */
static int total_dist;
static int minidx;

void route_load(const char *file)
{
    struct disk_hdr {
	int center_lat;
	int center_lon;
	unsigned int npts;
	unsigned int nwps;
    } hdr;
    struct disk_pnt {
	int x;
	int y;
	unsigned int dist;
    } pnt;
    struct disk_wp {
	unsigned int idx;
	unsigned int len;
    } wp;
    int fd, i, len;
    
    route_init();

    fd = open(file, O_RDONLY);
    if (fd == -1) return;

    read(fd, &hdr, sizeof(hdr));
    center.lat = intdegtorad((double)hdr.center_lat);
    center.lon = intdegtorad((double)hdr.center_lon);
    route.npts = hdr.npts;
    route.nwps = hdr.nwps;

    track_init(&center);

    route.pts = malloc(route.npts * sizeof(struct coord));
    route.dists = malloc(route.npts * sizeof(int));

    if (!route.npts) goto done;
    for (i = 0; i < route.npts; i++) {
	read(fd, &pnt, sizeof(pnt));
	route.pts[i].x = pnt.x;
	route.pts[i].y = pnt.y;
	route.dists[i] = pnt.dist;
    }

    if (!route.nwps) goto done;
    route.wps = malloc(route.nwps * sizeof(struct wp));
    for (i = 0; i < route.nwps; i++) {
	read(fd, &wp, sizeof(wp));
	route.wps[i].idx = wp.idx;
	len = wp.len;
	route.wps[i].desc = malloc(len + 1);
	read(fd, route.wps[i].desc, len);
	route.wps[i].desc[len] = '\0';
    }
done:
    close(fd);
}

void route_init(void)
{
    int i;

    for (i = 0; i < route.nwps; i++)
	if (route.wps[i].desc)
	    free(route.wps[i].desc);

    if (route.nwps)
	free(route.wps);

    if (route.pts) {
	free(route.pts);
	free(route.dists);
    }

    route.npts = route.nwps = 0;

    /* recenter around current GPS position */
    track_init(NULL);
}

void route_locate(const struct coord *here, const int dir, int *nextwp)
{
    int minwp, idx;
    long long mindist, dist;
    int low, high, i;

    minidx = minwp = 0;
    if (!route.wps) return;

    minidx = route.wps[0].idx;
    mindist = distance2(&here->xy, &route.pts[minidx]);
    for (i = 1; i < route.nwps; i++) {
	idx = route.wps[i].idx;
	dist = distance2(&here->xy, &route.pts[idx]);

	if (dist > mindist)
	    continue;

	minwp = i;
	minidx = idx;
	mindist = dist;
    }

    /* now check all points around this waypoint */
    low = high = minidx;
    if (minwp > 0)
	low = route.wps[minwp-1].idx;
    if (minwp < route.nwps - 1)
	high = route.wps[minwp+1].idx;

    for (idx = low; idx < high; idx++)
    {
	dist = distance2(&here->xy, &route.pts[idx]);
	if (dist > mindist)
	    continue;

	minidx = idx;
	mindist = dist;
    }

    /* ok we're pretty close, go for the heavier stuff */
    /* did we already pass this losest point we just found? */
    if (minidx + 1 < route.npts) {
	if (!towards(&here->xy, &route.pts[minidx], dir) &&
	    towards(&here->xy, &route.pts[minidx+1], dir)) {
	    minidx++;
	    mindist = distance2(&here->xy, &route.pts[minidx]);
	}
    }
    if (minidx > route.wps[minwp].idx)
	minwp++;

    /* got it! */
    total_dist = sqrt((double)mindist) + route.dists[minidx];
    *nextwp = minwp;
}

void route_draw(unsigned char *screen, struct xy *cur_pos)
{
    draw_route(screen, route.pts, route.npts, VFDSHADE_MEDIUM);

    /* draw line to chosen closest point, probably too cluttering */
    //draw_line(screen, &cur_location, &route.pts[minidx], VFDSHADE_MEDIUM);
}

int route_getwp(const int wp, struct xy *pos, int *dist, char **desc)
{
    int idx, wpidx;

    if (wp == -1)
	wpidx = route.nwps - 1;
    else
	wpidx = wp;

    if (wpidx < 0 || wpidx >= route.nwps)
	return 0;

    if (pos) {
	idx = route.wps[wpidx].idx;
	*pos = route.pts[idx];
    }

    if (dist) {
	idx = route.wps[wpidx].idx;
	*dist = total_dist - route.dists[idx];
    }

    if (desc)
	*desc = route.wps[wpidx].desc;

    return 1;
}

