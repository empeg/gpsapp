#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include "hijack.h"
#include "gpsapp.h"
#include "vfdlib.h"

int show_scale      = 1;
int show_gpscoords  = 0;
int show_rubberband = 0;
int show_metric     = 0;
int show_popups     = 0;
int show_fps        = 0;

/* height of font 0, used a lot, so looking it up once might be useful */
int h0;

static int scale, visual, menu;
static unsigned char screen[EMPEG_SCREEN_BYTES];

static int menu_pos;
static char *menu_msg[] = {
    "Load Route",
    "Toggle Text/Map",
    "Toggle Miles/Meters",
    "Toggle Coordinates",
    "Toggle Rubberband",
    "Toggle Scale",
    "Toggle Popups",
    "Toggle FPS",
};
#define MENU_ENTRIES 8

extern int stats_fromTM;
extern int stats_toTM;
extern int stats_distance;
extern int stats_bearing;

void timesub(struct timeval *from, struct timeval *val)
{
    from->tv_sec -= val->tv_sec;
    from->tv_usec -= val->tv_usec;
    if (from->tv_usec < 0) {
	from->tv_sec--;
	from->tv_usec += 1000000;
    }
}

static int do_refresh = 0;
static void refresh_display(void)
{
    struct timeval start, end;
    struct coord cur_location;
    int cur_bearing, newpos;
    static int nextwp;

    static int shift, shift_dir = 1;
    int	max_shift = 0;

    if (show_fps)
	gettimeofday(&start, NULL);

    newpos = track_pos(&cur_location, &cur_bearing);
    if (!newpos && !do_refresh)
	return;

    if (newpos)
	route_locate(&cur_location, cur_bearing, &nextwp);

    do_refresh = 0;

    vfdlib_fastclear(screen);
    if (visual) {
	struct xy pos;
	int dist, i;
	char *p, *desc;

	/* initialize map center and scale */
#define MAX_X (VFD_WIDTH - VFD_HEIGHT)
	vfdlib_setClipArea(0, 0, MAX_X, VFD_HEIGHT);
	draw_setup(screen, &cur_location, scale);

	/* show map scale */
	if (show_scale) {
	    vfdlib_drawLineHorizUnclipped(screen, 0, 0, 3, VFDSHADE_DIM);
	    vfdlib_drawLineHorizUnclipped(screen, 7, 0, 3, VFDSHADE_DIM);
	    vfdlib_drawLineVertUnclipped(screen, 1, 1, 6, VFDSHADE_DIM);
	    vfdlib_drawText(screen, formatdist(8 << scale), 3, 2, 0, VFDSHADE_DIM);
	}

	/* show gps coordinates */
	if (show_gpscoords) {
	    char line[13];
	    sprintf(line, "%12.6f", radtodeg(cur_location.lat));
	    vfdlib_drawText(screen, line, MAX_X - vfdlib_getTextWidth(line, 0),
			    0, 0, VFDSHADE_DIM);
	    sprintf(line, "%12.6f", radtodeg(cur_location.lon));
	    vfdlib_drawText(screen, line, MAX_X - vfdlib_getTextWidth(line, 0),
			    h0, 0, VFDSHADE_DIM);
	}

	/* draw tracklog */
	track_draw(screen);

	/* draw route we're following */
	route_draw(screen, &cur_location.xy);

	/* highlight waypoints */
	i = 0;
	while (route_getwp(i, &pos, NULL, NULL)) {
	    draw_point(screen, &pos, VFDSHADE_BRIGHT);

	    if (i++ != nextwp)
		continue;

	    /* add focus to next waypoint */
	    draw_mark(screen, &pos, -1, VFDSHADE_MEDIUM);

	    if (show_rubberband)
		draw_line(screen, &cur_location.xy, &pos, VFDSHADE_BRIGHT);
	}

	/* draw our own location */
	draw_mark(screen, &cur_location.xy, cur_bearing, VFDSHADE_BRIGHT);

	/* draw separator between map and info displays */
	vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
	vfdlib_drawLineVertUnclipped(screen, MAX_X, 0, VFD_HEIGHT,VFDSHADE_DIM);

	if (route_getwp(-1, NULL, &dist, NULL)) {
	    p = formatdist(dist);
	    vfdlib_drawText(screen, p, VFD_WIDTH - vfdlib_getTextWidth(p, 0)+1,
			    0, 0, -1);
	}
	if (route_getwp(nextwp, &pos, &dist, &desc)) {
	    p = formatdist(dist);
	    vfdlib_drawText(screen, p, VFD_WIDTH - vfdlib_getTextWidth(p, 0)+1,
			    VFD_HEIGHT + 1 - h0, 0, -1);

	    /* this doesn't work right at all */
	    max_shift = 0;
	    if (show_popups && dist < 1000) {
		int offset = 0, lost;
		lost = vfdlib_getTextWidth(desc, 0) - MAX_X;
		if (lost > 0) {
		    max_shift = lost;
		    if (shift > 0)         offset = shift;
		    if (shift > max_shift) offset = max_shift;
		    do_refresh = 1;
		}
		/* go a bit faster compared to the info screen */
		if (shift_dir == 1) shift_dir = 2;
		vfdlib_setClipArea(0, 0, MAX_X, VFD_HEIGHT);
		vfdlib_drawText(screen, desc, -offset, VFD_HEIGHT - h0, 0, -1);
		vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
	    }

	    /* draw pointer */
	    if (cur_bearing != -1) {
		int center_x, center_y, tip_x, tip_y, b;
		double b2;

		b = bearing(&cur_location.xy, &pos) - cur_bearing;
		if (b < 0) b += 360;
		center_x = VFD_WIDTH - VFD_HEIGHT / 2;
		center_y = VFD_HEIGHT / 2;
		b2 = degtorad(b);
		tip_x = center_x + (int)(6.0 * sin(b2));
		tip_y = center_y - (int)(6.0 * cos(b2));
		vfdlib_drawLineUnclipped(screen, center_x, center_y,
					 tip_x, tip_y, VFDSHADE_BRIGHT);
		_draw_mark(screen, tip_x, tip_y, b, VFDSHADE_BRIGHT);
	    }
	}
    } else {
	int n, last_dist = 0;
	for (n = 0; n < 4; n++) {
	    char *wpdesc, *p;
	    int wpdist, lost, offset = 0;

	    if (!route_getwp(nextwp + n, NULL, &wpdist, &wpdesc))
		break;

	    p = formatdist(wpdist - last_dist);
	    vfdlib_drawText(screen, p, 22 - vfdlib_getTextWidth(p, 0),
			    3 + n * (h0 + 1), 0, -1);

	    lost = vfdlib_getTextWidth(wpdesc, 0) + 23 - VFD_WIDTH;
	    if (lost > 0) {
		if (lost > max_shift)  max_shift = lost;
		if (shift > 0)         offset = shift;
		if (shift > max_shift) offset = max_shift;
		do_refresh = 1;
	    }

	    vfdlib_setClipArea(23, 0, VFD_WIDTH, VFD_HEIGHT);
	    vfdlib_drawText(screen, wpdesc, 23 - offset,
			    3 + n * (h0 + 1), 0, -1);
	    vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
	    last_dist = wpdist;
	}
	if (n == 0)
	    draw_msg(screen, "No route loaded");
    }

    shift += shift_dir;
    if (shift >= max_shift + h0) shift_dir = -2;
    if (shift <= -h0) shift_dir = 1;

    if (menu) 
	draw_msg(screen, menu_msg[menu_pos]);

    if (show_fps) {
	int fps, len;
	char line[25];
	gettimeofday(&end, NULL);
	timesub(&end, &start);
	fps = 10000000 / (end.tv_sec * 1000000 + end.tv_usec);
	sprintf(line, "%d.%dfps", fps / 10, fps % 10);
	len = vfdlib_getTextWidth(line, 0);
	vfdlib_drawSolidRectangleClipped(screen, MAX_X - len -1, 0, MAX_X, h0, VFDSHADE_BLACK);
	vfdlib_drawText(screen, line, MAX_X - len, 0, 0, VFDSHADE_DIM);
    }

    hijack_updatedisplay(screen);

#ifndef __arm__
    fprintf(stderr, "fromTM %d toTM %d distance %d bearing %d\n",
	    stats_fromTM, stats_toTM, stats_distance, stats_bearing);
    stats_fromTM = stats_toTM = stats_distance = stats_bearing = 0;
#endif
}

static int handle_input(void)
{
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
    static struct timeval pressed;
    struct timeval released;
    unsigned long key;
    int rc;

    rc = hijack_getkey(&key);
    if (rc == -1) return rc;

    /* Pause a bit. we don't really want to be blocked in an ioctl as we have
     * to keep track of the serial port as well. But on the other hand, we
     * don't want to eat all CPU time looping */
    if (rc == 0) {
	select(0, NULL, NULL, NULL, &timeout);
	rc = hijack_getkey(&key);
	if (rc != 1) return rc;
    }

    switch(key) {
    case IR_TOP_BUTTON_PRESSED:
	if (!menu)
	    return 1; /* done, go back to waiting for hijack*/

	menu = 0;
	do_refresh = 1;
	break;

    case IR_BOTTOM_BUTTON_PRESSED:
	gettimeofday(&pressed, NULL);
	return 0;

    case IR_BOTTOM_BUTTON_RELEASED:
	gettimeofday(&released, NULL);
	timesub(&released, &pressed);
	/* LONG_PRESS? */
	if (released.tv_sec >= 1 || released.tv_usec > 500000) {
	    visual = 1 - visual;
	    if (visual)
		scale  = 3;
	    do_refresh = 1;
	    break;
	}

	if (menu) {
	    switch(menu_pos) {
	    case 0: route_load("programs0/route"); break;
	    case 1: visual = 1 - visual; break;
	    case 2: show_metric = 1 - show_metric; break;
	    case 3: show_gpscoords = 1 - show_gpscoords;  break;
	    case 4: show_rubberband = 1 - show_rubberband; break;
	    case 5: show_scale = 1 - show_scale; break;
	    case 6: show_popups = 1 - show_popups; break;
	    case 7: show_fps = 1 - show_fps; break;
	    }
	}
	menu = 1 - menu;
	do_refresh = 1;
	break;

    case IR_LEFT_BUTTON_PRESSED:
	if (menu) {
	    if (--menu_pos < 0)
		menu_pos = MENU_ENTRIES - 1;
	}
	else if (visual && scale < 14)
	    scale++;
	do_refresh = 1;
	break;

    case IR_RIGHT_BUTTON_PRESSED:
	if (menu) {
	    if (++menu_pos >= MENU_ENTRIES)
		menu_pos = 0;
	}
	else if (visual && scale > 0)
	    scale--;
	do_refresh = 1;
	break;

    case IR_KNOB_LEFT:
	track_jump(0);
	break;

    case IR_KNOB_RIGHT:
	track_jump(1);
	break;

    case IR_KNOB_PRESSED:
	break;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 0;

    if (hijack_init() == -1)
	exit(-1);

    printf("GPS app started\n");

    vfdlib_registerFont("empeg/lib/fonts/small.bf", 0);
    h0 = vfdlib_getTextHeight(0);

    route_init();

    while (rc != -1) {
	if (hijack_waitmenu() == -1)
	    break;

	vfdlib_fastclear(screen);
	hijack_updatedisplay(screen);

	while(1) {
	    refresh_display();
	    rc = handle_input();
	    if (rc) break;
	}
    }

    route_init();
    track_free();

    draw_msg(screen, "GPS app dying...");
    hijack_updatedisplay(screen);
    sleep(5);

    vfdlib_unregisterAllFonts();
    hijack_exit();
    return 0;
}

