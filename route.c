/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gpsapp.h"
#include "vfdlib.h"

/* coord_center is needed to project GPS coords into map coords */ 
struct coord coord_center;
int nextwp;

/* saved state variables after a successful locate */
static struct route route;
static int total_dist;
static int minidx;

#define ROUTE_DIR "programs0/routes"
static int selected_route;
static char **routes;
static int nroutes;

int routes_init(void)
{
    DIR *dir;
    struct dirent *entry;
    int i;

    for (i = 0; i < nroutes; i++) free(routes[i]);
    if (routes) free(routes);
    routes = NULL;
    nroutes = 0;

    dir = opendir(ROUTE_DIR);
    if (!dir) return 0;

    while (1) {
	entry = readdir(dir);
	if (!entry) break;
	if (entry->d_name[0] == '.') continue;
	nroutes++;
    }
    routes = malloc(nroutes * sizeof(char *));
    if (!routes) {
	closedir(dir);
	nroutes = 0;
	return 0;
    }
    memset(routes, 0, nroutes * sizeof(char *));

    rewinddir(dir);
    i = 0;
    while (1) {
	entry = readdir(dir);
	if (!entry) break;
	if (entry->d_name[0] == '.') continue;
	routes[i++] = strdup(entry->d_name);
    }
    closedir(dir);
    return 1;
}

void routes_list(void)
{
    if (!routes || !nroutes) {
	draw_msg("No routes found");
	return;
    }
    draw_msg(routes[selected_route]);
}

void route_select(int updown)
{
    if (updown) selected_route++;
    else        selected_route--;

    if (selected_route >= nroutes) selected_route = 0;
    else if (selected_route < 0)   selected_route = nroutes-1;
}

void route_load(void)
{
    char file[PATH_MAX];

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
    int fd = -1, i, len;
    
    if (!routes || selected_route < 0 || selected_route >= nroutes)
	return;

    strcpy(file, ROUTE_DIR);
    strcat(file, "/");
    strcat(file, routes[selected_route]);

    for (i = 0; i < nroutes; i++)
	free(routes[i]);
    free(routes);
    routes = NULL;
    nroutes = 0;

    fd = open(file, O_RDONLY);
    if (fd == -1) return;

    route_init();

    read(fd, &hdr, sizeof(hdr));
    coord_center.lat = intdegtorad((double)hdr.center_lat);
    coord_center.lon = intdegtorad((double)hdr.center_lon);
    route.npts = hdr.npts;
    route.nwps = hdr.nwps;

    /* any logged points are in the wrong coordinate system. */
    track_init();

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

    /* recenter around current GPS position? */
}

static void route_update_gps_speed(void)
{
    double vmg_east, vmg_north, b;
    int vmg = 0;
    
    if (nextwp < route.nwps) {
	int idx = route.wps[nextwp].idx;
	/* calculate actual velocity towards the target wp */
	b = bearing(&gps_coord.xy, &route.pts[idx]);

	vmg_east = sin(b) * gps_state.spd_east;
	vmg_north = cos(b) * gps_state.spd_north;

	if (vmg_east && vmg_north) {
	    vmg = sqrt(vmg_east * vmg_east + vmg_north * vmg_north) * 3600.0;
	    gps_avgvmg += vmg - (gps_avgvmg >> AVGVMG_SHIFT);
	}
    }

#ifndef __arm__
    fprintf(stderr, "vmg %f\n", (gps_avgvmg >> AVGVMG_SHIFT) / 1609.344);
#endif

    /* we don't use actual gps_speed right now, so no need to do the math */
    //gps_speed = sqrt(gps_spd_e * gps_speed_e + gps_speed_n * gps_speed_n +
    //		       gps_spd_u * gps_speed_u);
}

void route_locate(void)
{
    int minwp, idx;
    long long mindist, dist;
    int i, incr;

    minidx = minwp = 0;
    if (!route.wps) return;

    minidx = route.wps[0].idx;
    mindist = distance2(&gps_coord.xy, &route.pts[minidx]);
    for (i = 1; i < route.nwps; i++) {
	idx = route.wps[i].idx;
	dist = distance2(&gps_coord.xy, &route.pts[idx]);

	if (dist > mindist)
	    continue;

	minwp = i;
	minidx = idx;
	mindist = dist;
    }

    /* If we are travelling towards this waypoint check all points before this
     * one. Otherwise check all points towards the next wp */
    idx = minidx;
    incr = 1;
    if (towards(&gps_coord.xy, &route.pts[minidx], gps_state.bearing))
	 incr = -1;

    while(1) {
	idx += incr;
	if (idx < 0 || idx >= route.npts)
	    break;

	dist = distance2(&gps_coord.xy, &route.pts[idx]);
	if (dist > 2 * mindist)
	    break;

	if (dist > mindist)
	    continue;

	minidx = idx;
	mindist = dist;
    }

    /* ok we're really close now */
    /* did we pass the point we just found? */
    if (minidx+1 < route.npts-1) {
	if (!towards(&gps_coord.xy, &route.pts[minidx], gps_state.bearing) &&
	    towards(&gps_coord.xy, &route.pts[minidx+1], gps_state.bearing)) {
	    minidx++;
	    mindist = distance2(&gps_coord.xy, &route.pts[minidx]);
	}
    }

    while (minidx > route.wps[minwp].idx)
	minwp++;

    /* got it! */
    total_dist = sqrt((double)mindist) + route.dists[minidx];
    nextwp = minwp;

    route_update_gps_speed();
}

void route_draw(struct xy *cur_pos)
{
    draw_lines(route.pts, route.npts, VFDSHADE_MEDIUM);

#if 0
    /* draw line to chosen closest point, probably too much clutter */
    if (minidx < route.npts)
	draw_line(cur_pos, &route.pts[minidx], VFDSHADE_MEDIUM);
#endif
}

int route_getwp(const int wp, struct xy *pos, unsigned int *dist, char **desc)
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
	*dist = abs(total_dist - route.dists[idx]);
    }

    if (desc)
	*desc = route.wps[wpidx].desc;

    return 1;
}

