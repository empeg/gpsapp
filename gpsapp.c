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
#include <fcntl.h>
#include "empeg_ui.h"
#include "vfdlib.h"
#include "gpsapp.h"

enum {
    VIEW_SATS = 0,
    VIEW_MAP,
    VIEW_ROUTE,
} visual;
int show_metric     = 0;
int show_gpscoords  = 0;
int show_rubberband = 1;
int show_track      = 1;

int show_scale      = 1;
int show_popups     = 1;
int show_time	    = 0;

/* height of font 0, used a lot, so looking it up once might be useful */
int h0;

/* force a screen refresh */
int do_refresh = 0;

static int menu;
static int lastmenu;
static int load_route;
static int menu_pos;
static int lastmenu_pos;
static char *menu_msg[] = {
    "Load Route",
    "Toggle Text/Map/Sats",
    "Toggle Popups",
    "Toggle Miles/Meters",
    "Toggle Coordinates",
    "Toggle Rubberband",
    "Toggle Distance/Time",
    "Toggle Track",
};
#define MENU_ENTRIES 8

void timesub(struct timeval *res, struct timeval *from, struct timeval *val)
{
    res->tv_sec  = from->tv_sec  - val->tv_sec;
    res->tv_usec = from->tv_usec - val->tv_usec;
    if (res->tv_usec < 0) {
	res->tv_sec--;
	res->tv_usec += 1000000;
    }
}

static void refresh_display(void)
{
    struct xy pos;
    int i;

    do_refresh = 0;

    draw_clear();
    switch (visual) {
    case VIEW_SATS:
	draw_sats(&gps_state);
	break;

    case VIEW_MAP:
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
#if 0
	    if (show_rubberband)
	    	draw_line(&gps_coord.xy, &pos, VFDSHADE_BRIGHT);
#endif
	}

	/* draw our own location */
	draw_mark(&gps_coord.xy, gps_bearing, VFDSHADE_BRIGHT);

	vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
	draw_info();
	break;

    case VIEW_ROUTE:
	draw_wpstext();
	break;
    }

    if (load_route)
	routes_list();
    else if (menu) 
	draw_msg(menu_msg[menu_pos]);

    else if (lastmenu) {
	char *msg=NULL;
	lastmenu--;
	switch(menu_pos) {
	case 2: switch (show_popups) {
		case 0: msg = "No Popups"; break;
		case 1: msg = "Popups"; break;
		case 2: msg = "Permanent Popups"; break;
		}
		break;
	case 3: msg = (show_metric?"Meters":"Miles"); break;
	case 4: msg = (show_gpscoords?"Coordinates":"No Coordinates"); break;
	case 5: msg = (show_rubberband?"Rubberband":"No Rubberband"); break;
	case 6: msg = (show_time?"Time":"Distance"); break;
	case 7: msg = (show_track?"Track":"No Track"); break;
	}
	if (msg)
	    draw_msg(msg);
    }

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
    static int long_press;
    struct timeval now, diff;
    unsigned long key;
    int rc;

    serial_poll();

    if (pressed.tv_sec && pressed.tv_usec) {
	gettimeofday(&now, NULL);
	timesub(&diff, &now, &pressed);
	/* LONG_PRESS? */
	if (diff.tv_sec >= 1) {
	    switch (visual) {
	    case VIEW_SATS:  visual = VIEW_MAP; break;
	    case VIEW_MAP:   visual = VIEW_ROUTE; break;
	    case VIEW_ROUTE: visual = VIEW_SATS; break;
	    }
	    /* allow for cycling by keeping the button pressed */
	    pressed.tv_sec  = now.tv_sec;
	    pressed.tv_usec = now.tv_usec;
	    long_press = 1;
	    do_refresh = 1;
	    return 0;
	}
    }

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
	pressed.tv_sec = pressed.tv_usec = 0;
	if (long_press) {
	    long_press = 0;
	    break;
	}
	if (load_route) {
	    route_load();
	    load_route = 0;
	    menu = 0;
	}
	else if (menu) {
	    switch(menu_pos) {
	    case 0: load_route = routes_init(); break;
	    case 1:
		switch (visual) {
		case VIEW_SATS:  visual = VIEW_MAP; break;
		case VIEW_MAP:   visual = VIEW_ROUTE; break;
		case VIEW_ROUTE: visual = VIEW_SATS; break;
		}
		break;
	    case 2:
		if (++show_popups == 3)
		    show_popups = 0;
		break;
	    case 3: show_metric = 1 - show_metric; break;
	    case 4: show_gpscoords = 1 - show_gpscoords;  break;
	    case 5: show_rubberband = 1 - show_rubberband; break;
	    case 6: show_time = 1 - show_time; break;
	    case 7: show_track = 1 - show_track; break;
	    }
	    menu = 0; lastmenu = 3; lastmenu_pos = menu_pos;
	}
	else
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
	else {
	    switch (visual) {
	    case VIEW_SATS:  break;
	    case VIEW_MAP:   draw_zoom(0); break;
	    case VIEW_ROUTE: route_skipwp(-1); break;
	    }
	}
	do_refresh = 1;
	break;

    case IR_RIGHT_BUTTON_PRESSED:
	if (load_route)
	    route_select(1);
	else if (menu) {
	    if (++menu_pos >= MENU_ENTRIES)
		menu_pos = 0;
	}
	else {
	    switch (visual) {
	    case VIEW_SATS:  break;
	    case VIEW_MAP:   draw_zoom(1); break;
	    case VIEW_ROUTE: route_skipwp(1); break;
	    }
	}
	do_refresh = 1;
	break;

    case IR_KNOB_LEFT:
    case IR_KNOB_RIGHT:
	route_skipwp(key == IR_KNOB_LEFT ? -1 : 1);
	route_locate();
	do_refresh = 1;
	break;

    case IR_KNOB_PRESSED:
    default:
	break;
    }

    return 0;
}

static void
init_gpsapp()
{
    char buf[512];
    ssize_t bytes = 0, done = 0;
    int ret = 0;
    int offset = 0;
    int fd = open("empeg/var/config.ini", O_RDONLY);
    int inside = 0;

    while ((bytes=read(fd, buf, 512))>CONFIG_HDRLEN) {
	buf[bytes]='\0';
	offset=CONFIG_HDRLEN; 
	done+=bytes;
	ret = config_ini_option (buf, "visual", &inside);
	if (ret > -1 && ret < 3) show_visual = ret;
	ret = config_ini_option (buf, "metric", &inside);
	if (ret > -1 && ret < 2) show_metric = ret;
	ret = config_ini_option (buf, "gpscoords", &inside);
	if (ret > -1 && ret < 2) show_gpscoords = ret;
	ret = config_ini_option (buf, "rubberband", &inside);
	if (ret > -1 && ret < 2) show_rubberband = ret;
	ret = config_ini_option (buf, "track", &inside);
	if (ret > -1 && ret < 2) show_track = ret;
	ret = config_ini_option (buf, "scale", &inside);
	if (ret > -1 && ret < 2) show_scale = ret;
	ret = config_ini_option (buf, "popups", &inside);
	if (ret > -1 && ret < 3) show_popups = ret;
	ret = config_ini_option (buf, "time", &inside);
	if (ret > -1 && ret < 2) show_time = ret;
	if (lseek(fd, -CONFIG_HDRLEN, SEEK_CUR) != done- CONFIG_HDRLEN) {
	    // bomb out?
	}
	done-=CONFIG_HDRLEN;
    }
    close(fd);
}

int main(int argc, char **argv)
{
    const char *menu[] = { "GPSapp", NULL };
    struct timeval timeout;
    int rc = 0;

    if (empeg_init() == -1)
	exit(-1);

    init_gpsapp();

    printf("GPS app started\n");

    vfdlib_registerFont("empeg/lib/fonts/small.bf", 0);
    h0 = vfdlib_getTextHeight(0);

    route_init();

    /* we need a way to look the protocol up in a config file? */
    if (argc > 1)
	serial_protocol(argv[1]);

    while (rc != -1) {
	if (empeg_waitmenu(menu) == -1)
	    break;

	serial_open();

	draw_msg("Waiting for data from GPS receiver");
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

