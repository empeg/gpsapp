/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "empeg_ui.h"
#include "vfdlib.h"
#include "gpsapp.h"

int visual	    = 1;
int show_metric     = 0;
int show_gpscoords  = 0;
int show_rubberband = 1;
int show_track      = 1;

int show_scale      = 1;
int show_popups     = 1;
int show_abs	    = 0;
int show_time	    = 0;

/* height of font 0, used a lot, so looking it up once might be useful */
int h0;

/* force a screen refresh */
int do_refresh = 0;

static int menu;
static int load_route;
static int menu_pos;
static char *menu_msg[] = {
    "Load Route",
    "Toggle Text/Map",
    "Toggle Miles/Meters",
    "Toggle Coordinates",
    "Toggle Rubberband",
    "Toggle Distance/Time",
    "Toggle Absolute/Relative",
    "Toggle Track",
};
#define MENU_ENTRIES 8

void timesub(struct timeval *from, struct timeval *val)
{
    from->tv_sec -= val->tv_sec;
    from->tv_usec -= val->tv_usec;
    if (from->tv_usec < 0) {
	from->tv_sec--;
	from->tv_usec += 1000000;
    }
}

static void refresh_display(void)
{
    do_refresh = 0;

    draw_clear();
    if (visual) {
	struct xy pos;
	int i;

	/* show map scale */
	if (show_scale)
	    draw_scale();

	/* show gps coordinates */
	if (show_gpscoords)
	    draw_gpscoords();

	vfdlib_setClipArea(0, 0, VFD_WIDTH - VFD_HEIGHT, VFD_HEIGHT);

	/* draw tracklog */
	if (show_track)
	    track_draw();

	/* draw route we're following */
	route_draw(&gps_coord.xy);

	/* highlight waypoints */
	i = 0;
	while (route_getwp(i, &pos, NULL, NULL)) {
	    draw_point(&pos, VFDSHADE_BRIGHT);
	    if (i++ != nextwp) continue;

	    /* add focus to next waypoint */
	    draw_mark(&pos, -1, VFDSHADE_MEDIUM);
	    if (show_rubberband)
		draw_line(&gps_coord.xy, &pos, VFDSHADE_BRIGHT);
	}

	/* draw our own location */
	draw_mark(&gps_coord.xy, gps_state.bearing, VFDSHADE_BRIGHT);

	vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
	draw_info();
    } else
	draw_wpstext();

    if (load_route)
	routes_list();
    else if (menu) 
	draw_msg(menu_msg[menu_pos]);

    draw_display();

#ifndef __arm__
    {
	extern int stats_toTM;
	extern int stats_distance;
	extern int stats_bearing;

	fprintf(stderr, "conv stats: GPS>TM %d DIST %d HDG %d\n",
		stats_toTM, stats_distance, stats_bearing);
	stats_toTM = stats_distance = stats_bearing = 0;
    }
#endif
}

static int handle_input(void)
{
    static struct timeval pressed;
    struct timeval released;
    unsigned long key;
    int rc;

    serial_poll();
    rc = empeg_getkey(&key);
    if (rc != 1) return rc;

    switch(key) {
    case IR_TOP_BUTTON_PRESSED:
	if (!load_route && !menu)
	    return 1; /* done, wait to be selected again in the hijack menu */

	load_route = 0;
	menu = 0;
	do_refresh = 1;
	break;

    case IR_BOTTOM_BUTTON_PRESSED:
	gettimeofday(&pressed, NULL);
	break;

    case IR_BOTTOM_BUTTON_RELEASED:
	gettimeofday(&released, NULL);
	timesub(&released, &pressed);
	/* LONG_PRESS? */
	if (released.tv_sec >= 1 || released.tv_usec > 500000)
	    visual = 1 - visual;
	else if (load_route) {
	    route_load();
	    load_route = 0;
	    menu = 0;
	} else if (menu) {
	    switch(menu_pos) {
	    case 0: load_route = routes_init(); break;
	    case 1: visual = 1 - visual; break;
	    case 2: show_metric = 1 - show_metric; break;
	    case 3: show_gpscoords = 1 - show_gpscoords;  break;
	    case 4: show_rubberband = 1 - show_rubberband; break;
	    case 5: show_time = 1 - show_time; break;
	    case 6: show_abs = 1 - show_abs; break;
	    case 7: show_track = 1 - show_track; break;
	    }
	    menu = 0;
	} else
	    menu = 1;

	do_refresh = 1;
	break;

    case IR_LEFT_BUTTON_PRESSED:
	if (load_route)
	    route_select(0);
	else if (menu) {
	    if (--menu_pos < 0)
		menu_pos = MENU_ENTRIES - 1;
	}
	else if (visual)
	    draw_zoom(0);
	do_refresh = 1;
	break;

    case IR_RIGHT_BUTTON_PRESSED:
	if (load_route)
	    route_select(1);
	else if (menu) {
	    if (++menu_pos >= MENU_ENTRIES)
		menu_pos = 0;
	}
	else if (visual)
	    draw_zoom(1);
	do_refresh = 1;
	break;

    case IR_KNOB_LEFT:
    case IR_KNOB_RIGHT:
    case IR_KNOB_PRESSED:
    default:
	break;
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct timeval timeout;
    int rc = 0;

    if (empeg_init() == -1)
	exit(-1);

    printf("GPS app started\n");

    vfdlib_registerFont("empeg/lib/fonts/small.bf", 0);
    h0 = vfdlib_getTextHeight(0);

    route_init();

    /* we need a way to look the protocol up in a config file? */
    if (argc > 1)
	serial_protocol(argv[1]);

    while (rc != -1) {
	if (empeg_waitmenu() == -1)
	    break;

	serial_open();

	draw_msg("Waiting for GPS location...");
	draw_display();

	while(1) {
	    rc = handle_input();
	    if (rc) break;

	    if (do_refresh)
		refresh_display();

	    /* pause a bit to avoid burning CPU cycles */
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 100000;
	    select(0, NULL, NULL, NULL, &timeout);
	}
    }

    serial_close();
    route_init();

    draw_msg("GPS app dying...");
    draw_display();
    sleep(5);

    vfdlib_unregisterAllFonts();
    empeg_free();
    return 0;
}

