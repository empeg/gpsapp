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

static int qcmp(const void *a, const void *b)
{
    return strcasecmp(*(const char **)a, *(const char **)b);
}

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

    while ((entry = readdir(dir)) != NULL)
	nroutes++;

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
	char buf[PATH_MAX];
	struct stat s;

	entry = readdir(dir);
	if (!entry) break;

	strcpy(buf, ROUTE_DIR);
	strcat(buf, "/");
	strcat(buf, entry->d_name);

	if (stat(buf, &s) == 0 && S_ISREG(s.st_mode))
	    routes[i++] = strdup(entry->d_name);
	else
	    nroutes--; // we allocated a couple of pointers more than necessary.
    }
    closedir(dir);

    /* sort the list alphabetically */
    qsort(routes, nroutes, sizeof(char *), qcmp);
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
    char buf[PATH_MAX], *p;
    FILE *f;
    int i, j, len;
    
    if (!routes || selected_route < 0 || selected_route >= nroutes)
	return;

    strcpy(buf, ROUTE_DIR);
    strcat(buf, "/");
    strcat(buf, routes[selected_route]);

    for (i = 0; i < nroutes; i++)
	free(routes[i]);
    free(routes);
    routes = NULL;
    nroutes = 0;

    f = fopen(buf, "r");
    if (!f) return;

    route_init();

    draw_clear();
    draw_msg("Loading");
    draw_display();

    fgets(buf, PATH_MAX, f); p = buf;
    coord_center.lat = degtorad(strtod(p, &p));
    coord_center.lon = degtorad(strtod(p, &p));
    route.npts = strtol(p, &p, 10);
    route.nwps = strtol(p, &p, 10);

    if (!route.npts) goto done;

    route.pts = malloc(route.npts * sizeof(struct coord));
    route.dists = malloc(route.npts * sizeof(int));
    route.wps = malloc(route.nwps * sizeof(struct wp));
    /* malloc's don't fail right :) */
    if (!route.pts || !route.dists || !route.wps) {
	err("Failed allocation for route");
	if (route.pts) free(route.pts);
	if (route.dists) free(route.dists);
	if (route.wps) free(route.wps);
	route.pts = NULL; route.dists = NULL; route.wps = NULL;
	route.npts = route.nwps = 0;
	fclose(f);
	goto done;
    }

    for (i = 0, j = 0; i < route.npts; i++) {
	fgets(buf, PATH_MAX, f); p = buf;
	len = strlen(buf);
	if (buf[len-1] == '\n') buf[--len] = '\0';
	if (buf[len-1] == '\r') buf[--len] = '\0';
	if (buf[len-1] == ' ') buf[--len] = '\0';
	route.pts[i].x = strtol(p, &p, 10);
	route.pts[i].y = strtol(p, &p, 10);
	if (p < buf + len) {
	    if (j == route.nwps) {
		err("Too many waypoints?");
		for (i = 0; i < j; i++)
		    free(route.wps[i].short_desc);
		free(route.pts); free(route.dists); free(route.wps);
		route.pts = NULL; route.dists = NULL; route.wps = NULL;
		route.npts = route.nwps = 0;
		goto done;
	    }
	    route.wps[j].idx = i;
	    route.wps[j++].short_desc = strdup(p+1);
	}
    }

    draw_clear();
    draw_msg("Pre-computing values");
    draw_display();

    /* now calculate the distances */
    route.dists[route.npts-1] = 0;
    for (i = route.npts-2; i >= 0; i--)
	route.dists[i] = route.dists[i+1] + 
	    sqrt(distance2(&route.pts[i], &route.pts[i+1]));

    for (i = 0; i < route.nwps; i++) {
	int idx = route.wps[i].idx;
	if (idx > 0 && idx < route.npts-1) {
	    route.wps[i].inhdg = radtodeg(bearing(&route.pts[idx-1], &route.pts[idx]));
	    route.wps[i].outhdg = radtodeg(bearing(&route.pts[idx], &route.pts[idx+1]));
	}
    }

done:
    fclose(f);
}

void route_init(void)
{
    int i;

    for (i = 0; i < route.nwps; i++)
	if (route.wps[i].short_desc)
	    free(route.wps[i].short_desc);

    if (route.nwps)
	free(route.wps);

    if (route.pts) {
	free(route.pts);
	free(route.dists);
    }

    route.npts = route.nwps = 0;

    /* recenter around current GPS position */
    coord_center.lat = coord_center.lon = 0.0;

    /* Any previously logged points are based on the wrong center
     * coordinate. */
    track_init();
}

void route_recenter(void)
{
    if (!route.npts && coord_center.lat == 0.0 && coord_center.lon == 0.0)
	coord_center = gps_coord;
}


void route_update_vmg(void)
{
    double vmg_east, vmg_north, b;
    int idx, vmg = 0;

    if (nextwp >= route.nwps) return;

    idx = route.wps[nextwp].idx;
    /* calculate actual velocity towards the target wp */
    b = bearing(&gps_coord.xy, &route.pts[idx]);

    vmg_east = sin(b) * gps_state.spd_east;
    vmg_north = cos(b) * gps_state.spd_north;

    vmg = sqrt(vmg_east * vmg_east + vmg_north * vmg_north) * 3600.0;
    gps_avgvmg += vmg - (gps_avgvmg >> AVGVMG_SHIFT);

#ifndef __arm__
    fprintf(stderr, "vmg %f\n", (gps_avgvmg >> AVGVMG_SHIFT) / 1609.344);
#endif
}

void route_skipwp(int dir)
{
    int prev;
    nextwp += dir;

    if (nextwp >= route.nwps)
	nextwp = route.nwps-1;
    if (nextwp < 0)
	nextwp = 0;

    prev = nextwp - 1;
    if (prev >= 0 && prev < route.nwps)
	minidx = route.wps[prev].idx;
}

void route_locate(void)
{
    int idx;
    long long mindist, dist;

    if (nextwp >= route.nwps) return;

    /* don't try to be smart, just search based on the current nextwp */
    if (minidx >= route.npts)
	minidx = 0;
    mindist = distance2(&gps_coord.xy, &route.pts[minidx]);

    for (idx = minidx; idx < route.npts; idx++) {
	if (nextwp < route.nwps-1 && idx >= route.wps[nextwp].idx+1)
	    break;

	dist = distance2(&gps_coord.xy, &route.pts[idx]);
	if (dist > mindist)
	    continue;

	minidx = idx;
	mindist = dist;
    }

    /* ok we're really close now */
    /* did we pass the point we just found? */
    if (minidx+1 < route.npts-1) {
#if 0
	if (!towards(&gps_coord.xy, &route.pts[minidx], gps_bearing) &&
	    towards(&gps_coord.xy, &route.pts[minidx+1], gps_bearing)) {
	    minidx++;
	    mindist = distance2(&gps_coord.xy, &route.pts[minidx]);
	}
#else
	if (!towards(&gps_coord.xy, &route.pts[minidx], gps_bearing) &&
	    minidx + 1 < route.npts) {
	    dist = distance2(&gps_coord.xy, &route.pts[minidx + 1]);
	    if (dist < mindist) {
		minidx++;
		mindist = dist;
	    }
	}
#endif
    }

    while (minidx > route.wps[nextwp].idx)
	nextwp++;

    /* got it! */
    total_dist = sqrt((double)mindist) + route.dists[minidx];
}

void route_draw(struct xy *cur_pos)
{
    if (minidx < route.npts && nextwp < route.nwps) {
	int nextidx = route.wps[nextwp].idx;
	draw_lines(route.pts, minidx+1, VFDSHADE_MEDIUM);
	draw_line(cur_pos, &route.pts[minidx], VFDSHADE_BRIGHT);
	if (nextidx != minidx)
	    draw_lines(&route.pts[minidx], (nextidx-minidx)+1, VFDSHADE_BRIGHT);
	draw_lines(&route.pts[nextidx], route.npts - nextidx, VFDSHADE_MEDIUM);
    } else
	draw_lines(route.pts, route.npts, VFDSHADE_MEDIUM);
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

    if (desc) {
	static char buf[80];
	char *short_desc;

	/* Restore the full description,
	 * '[continue|bear|turn] [sharply] [left|right] onto foo' */
	idx = route.wps[wpidx].idx;
	short_desc = route.wps[wpidx].short_desc;

	if (idx == route.npts-1)
	    sprintf(buf, "End at %s", short_desc);
	else if (idx == 0)
	    sprintf(buf, "Start at %s", short_desc);
	else
	{
	    short inhdg, outhdg, turn;
	    char *heading, *strong;

	    if (idx == minidx)
		//inhdg = bearing(&gps_coord.xy, &route.pts[idx]);
		inhdg = gps_bearing;
	    else
		inhdg = route.wps[wpidx].inhdg;

	    outhdg = route.wps[wpidx].outhdg;
	    turn = outhdg - inhdg;
	    if (turn < -180) turn += 360;
	    if (turn >= 180) turn -= 360;

	    if (turn < 0.0) {
		heading = "LEFT";
		turn = -turn;
	    } else
		heading = "RIGHT";

	    strong = turn > 130 ? "sharply " : "";

	    if (turn <= 5)
		sprintf(buf, "Continue onto %s", short_desc);
	    else if (turn <= 55)
		sprintf(buf, "Bear %s onto %s", heading, short_desc);
	    else
		sprintf(buf, "Turn %s%s onto %s", strong, heading, short_desc);
	}

	*desc = buf;
    }

    return 1;
}

