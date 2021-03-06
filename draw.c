/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "vfdlib.h"
#include "empeg_ui.h"
#include "gps_protocol.h"
#include "gpsapp.h"

#define MAX_X (VFD_WIDTH - VFD_HEIGHT)
#define MAX_Y (VFD_HEIGHT)

static unsigned char screen[VFD_HEIGHT * VFD_BYTES_PER_SCANLINE];
static int map_scale = 4;

void draw_activity(int refresh)
{
    static int lastshade = 1;
    
    if (get_visual() == 2)
        return;

    vfdlib_drawSolidRectangleClipped(screen, 2, 2, 4, 4, 
                                     (lastshade < 4) ? lastshade : 
                                     (6 - lastshade));

    /* if we're called from the main screen refresh, we should not update the
     * counter or don't redraw the display here */
    if (refresh)
	return;

    if (++lastshade > 5)
	lastshade = 1;
    empeg_updatedisplay(screen);
}

void draw_clear(void)
{
    vfdlib_fastclear(screen);
}

void draw_zoom(int inout)
{
    if (inout) {
	if (map_scale > 0)
	    map_scale--;
    } else {
	if (map_scale < 14)
	    map_scale++;
    }
}

static inline int project(const struct xy *pos, struct xy *xy)
{
    int clip = 0;

    xy->x = (MAX_X / 2) + ((pos->x - gps_coord.xy.x) >> map_scale);
    xy->y = (MAX_Y / 2) - ((pos->y - gps_coord.xy.y) >> map_scale);

    if (xy->x < 0 || xy->x >= MAX_X || xy->y < 0 || xy->y >= MAX_Y) {
	clip = 0x4;
	if (xy->y >= MAX_Y/2) clip |= 0x1;
	if (xy->x < MAX_X/2)  clip |= 0x2;
    }

    return clip;
}

void draw_line(const struct xy *from, const struct xy *to, const int shade)
{
    struct xy fxy, txy;
    int cf, ct;

    cf = project(from, &fxy);
    ct = project(to, &txy);
    if (!cf || !ct || cf != ct)
	vfdlib_drawLineClipped(screen, fxy.x, fxy.y, txy.x, txy.y, shade);
}

void draw_point(const struct xy *coord, const int shade)
{
    struct xy xy;
    int c;

    c = project(coord, &xy);
    if (!c)
	vfdlib_drawPointClipped(screen, xy.x, xy.y, shade);
}

void draw_lines(const struct xy *pts, const int npts, const int shade)
{
    int i;
    struct xy last_xy, cur_xy;
    int cf, ct;

    if (npts <= 1) return;

    cf = project(&pts[0], &last_xy);
    for (i = 1; i < npts; i++) {
	if (pts[i-1].x == pts[i].x && pts[i-1].y == pts[i].y)
	    continue;

	ct = project(&pts[i], &cur_xy);

	if (!cf || !ct || cf != cf)
	    vfdlib_drawLineClipped(screen, last_xy.x, last_xy.y,
				   cur_xy.x, cur_xy.y, shade);
	last_xy = cur_xy;
	cf = ct;
    }
}

#if 0 /* small 7x7 cursor */
static unsigned char *cursors[] = {
    /*?*/  "\x00\x00\x07\x00\x07\x00\x10\x38\x6c\x38\x10\x00",
    /*N*/  "\x00\x00\x07\x00\x07\x10\x38\x38\x6c\x7c\x00\x00",
    /*NNE*/"\x00\x00\x07\x00\x07\x08\x18\x38\x68\x78\x18\x00",
    /*NE*/ "\x00\x00\x07\x00\x07\x02\x0c\x3c\x68\x38\x10\x00",
    /*ENE*/"\x00\x00\x07\x00\x07\x00\x00\x7e\x6c\x38\x30\x00",
    /*E*/  "\x00\x00\x07\x00\x07\x00\x30\x3c\x2e\x3c\x30\x00",
    /*ESE*/"\x00\x00\x07\x00\x07\x00\x30\x38\x6c\x7e\x00\x00",
    /*SE*/ "\x00\x00\x07\x00\x07\x00\x10\x38\x68\x3c\x0c\x02",
    /*SSE*/"\x00\x00\x07\x00\x07\x00\x18\x78\x68\x38\x18\x08",
    /*S*/  "\x00\x00\x07\x00\x07\x00\x00\x7c\x6c\x38\x38\x10",
    /*SSW*/"\x00\x00\x07\x00\x07\x00\x30\x3c\x2c\x38\x30\x20",
    /*SW*/ "\x00\x00\x07\x00\x07\x00\x10\x38\x2c\x78\x60\x80",
    /*WSW*/"\x00\x00\x07\x00\x07\x00\x18\x38\x6c\xfc\x00\x00",
    /*W*/  "\x00\x00\x07\x00\x07\x00\x18\x78\xe8\x78\x18\x00",
    /*WNW*/"\x00\x00\x07\x00\x07\x00\x00\xfc\x6c\x38\x18\x00",
    /*NW*/ "\x00\x00\x07\x00\x07\x80\x60\x78\x2c\x38\x10\x00",
    /*NNW*/"\x00\x00\x07\x00\x07\x20\x30\x38\x2c\x3c\x30\x00",
    /*N*/  "\x00\x00\x07\x00\x07\x10\x38\x38\x6c\x7c\x00\x00",
};
#else /* larger 9x9 cursor */
static unsigned char *cursors[] = {
    /*?*/  "\x00\x00\x09\x00\x09\x00\x00\x00\x00\x08\x00\x1c\x00\x36\x00\x1c\x00\x08\x00\x00\x00\x00\x00",
    /*N*/  "\x00\x00\x09\x00\x09\x08\x00\x08\x00\x1c\x00\x1c\x00\x36\x00\x3e\x00\x00\x00\x00\x00\x00\x00",
    /*NNE*/"\x00\x00\x09\x00\x09\x00\x00\x04\x00\x0c\x00\x1c\x00\x34\x00\x3c\x00\x0c\x00\x00\x00\x00\x00",
    /*NE*/ "\x00\x00\x09\x00\x09\x00\x00\x01\x00\x06\x00\x1e\x00\x34\x00\x1c\x00\x08\x00\x00\x00\x00\x00",
    /*ENE*/"\x00\x00\x09\x00\x09\x00\x00\x00\x00\x00\x00\x3f\x00\x36\x00\x1c\x00\x18\x00\x00\x00\x00\x00",
    /*E*/  "\x00\x00\x09\x00\x09\x00\x00\x00\x00\x18\x00\x1e\x00\x17\x10\x1e\x00\x18\x00\x00\x00\x00\x00",
    /*ESE*/"\x00\x00\x09\x00\x09\x00\x00\x00\x00\x18\x00\x1c\x00\x36\x00\x3f\x00\x00\x00\x00\x00\x00\x00",
    /*SE*/ "\x00\x00\x09\x00\x09\x00\x00\x00\x00\x08\x00\x1c\x00\x34\x00\x1e\x00\x06\x00\x01\x00\x00\x00",
    /*SSE*/"\x00\x00\x09\x00\x09\x00\x00\x00\x00\x0c\x00\x3c\x00\x34\x00\x1c\x00\x0c\x00\x04\x00\x00\x00",
    /*S*/  "\x00\x00\x09\x00\x09\x00\x00\x00\x00\x00\x00\x3e\x00\x36\x00\x1c\x00\x1c\x00\x08\x00\x08\x00",
    /*SSW*/"\x00\x00\x09\x00\x09\x00\x00\x00\x00\x18\x00\x1e\x00\x16\x00\x1c\x00\x18\x00\x10\x00\x00\x00",
    /*SW*/ "\x00\x00\x09\x00\x09\x00\x00\x00\x00\x08\x00\x1c\x00\x16\x00\x3c\x00\x30\x00\x40\x00\x00\x00",
    /*WSW*/"\x00\x00\x09\x00\x09\x00\x00\x00\x00\x0c\x00\x1c\x00\x36\x00\x7f\x00\x00\x00\x00\x00\x00\x00",
    /*W*/  "\x00\x00\x09\x00\x09\x00\x00\x00\x00\x0c\x00\x3c\x00\xf4\x00\x3c\x00\x0c\x00\x00\x00\x00\x00",
    /*WNW*/"\x00\x00\x09\x00\x09\x00\x00\x00\x00\x00\x00\x7e\x00\x36\x00\x1c\x00\x0c\x00\x00\x00\x00\x00",
    /*NW*/ "\x00\x00\x09\x00\x09\x00\x00\x40\x00\x30\x00\x3c\x00\x16\x00\x1c\x00\x08\x00\x00\x00\x00\x00",
    /*NNW*/"\x00\x00\x09\x00\x09\x00\x00\x10\x00\x18\x00\x1c\x00\x16\x00\x1e\x00\x18\x00\x00\x00\x00\x00",
    /*N*/  "\x00\x00\x09\x00\x09\x00\x00\x08\x00\x1c\x00\x1c\x00\x36\x00\x3e\x00\x00\x00\x00\x00\x00\x00",
};
#endif

void draw_mark(const struct xy *pos, const int dir, const int shade)
{
    struct xy xy;
    int c;

    c = project(pos, &xy);
    if (!c)
	_draw_mark(xy.x, xy.y, dir, shade);
}

void _draw_mark(const int x, const int y, const int dir, const int shade)
{
    int idx, dx, dy;

    /* basically: index = (dir + 11.25) / (360 / 16) */
    idx = ((dir << 2) + 45) / 90;
    idx++; /* first entry is for 'unknown direction' */

    if (dir < 0 || dir >= 360)
	idx = 0;

    dx = cursors[idx][2];
    dy = cursors[idx][4];
    vfdlib_drawBitmap(screen, cursors[idx], x - dx / 2, y - dy / 2,
		      0, 0, dx, dy, shade, 1);
}

void draw_msg(const char *msg)
{
    int w0 = vfdlib_getTextWidth((char *)msg, 0);

    vfdlib_drawOutlineRectangleClipped(screen, (VFD_WIDTH - w0 - 4) >> 1,
					       (VFD_HEIGHT - h0 - 4) >> 1,
					       (VFD_WIDTH + w0 + 4) >> 1,
					       (VFD_HEIGHT + h0 + 4) >> 1,
					       VFDSHADE_BRIGHT);
    vfdlib_drawSolidRectangleClipped(screen, (VFD_WIDTH - w0 - 2) >> 1,
					     (VFD_HEIGHT - h0 - 2) >> 1,
					     (VFD_WIDTH + w0 + 2) >> 1,
					     (VFD_HEIGHT + h0 + 2) >> 1,
					     VFDSHADE_BLACK);
    vfdlib_drawText(screen, (char *)msg, (VFD_WIDTH - w0) >> 1,
		    (VFD_HEIGHT - h0) >> 1, 0, -1);
}

void draw_scale(void)
{
    char buf[10];

    formatdist(buf, 8 << map_scale);
    vfdlib_drawText(screen, buf, MAX_X - vfdlib_getTextWidth(buf, 0), 1, 0,
		    VFDSHADE_BRIGHT);
}

void draw_gpscoords(void)
{
    char line[13];

    format_coord(line, gps_coord.lat, "NS");
    vfdlib_drawText(screen, line, 0, 1, 0, VFDSHADE_DIM);

    format_coord(line, gps_coord.lon, "EW");
    vfdlib_drawText(screen, line, 0, h0+1, 0, VFDSHADE_DIM);
}

void draw_info(void)
{
    char *desc, buf[60];
    unsigned int dist;
    int tip_x, tip_y, b;
    struct xy pos;
#if 0
    int center_x, center_y;
    double b2;
#endif

    vfdlib_drawLineVertUnclipped(screen, MAX_X, 0, 8, VFDSHADE_BRIGHT);
    vfdlib_drawLineVertUnclipped(screen, MAX_X, 8, 8, VFDSHADE_MEDIUM);
    vfdlib_drawLineVertUnclipped(screen, MAX_X, 16, 8, VFDSHADE_BRIGHT);
    vfdlib_drawLineVertUnclipped(screen, MAX_X, 24, 8, VFDSHADE_MEDIUM);

    if (route_getwp(-1, NULL, &dist, NULL)) {
	if (show_time) time_estimate(buf, dist);
	else           formatdist(buf, dist);
	vfdlib_drawText(screen, buf, VFD_WIDTH - vfdlib_getTextWidth(buf, 0)+1,
			0, 0, -1);
    }

    formatspeed(buf, gps_speed);
    vfdlib_drawText(screen, buf, VFD_WIDTH - vfdlib_getTextWidth(buf, 1) + 1,
		    2 * h0, 1, -1);

    if (!route_getwp(nextwp, &pos, &dist, &desc)) {
	draw_popup(NULL);
	return;
    }

    if (show_time) time_estimate(buf, dist);
    else	   formatdist(buf, dist);
    vfdlib_drawText(screen, buf, VFD_WIDTH - vfdlib_getTextWidth(buf, 0) + 1,
		    VFD_HEIGHT + 1 - vfdlib_getTextHeight(0), 0, -1);

    draw_popup(show_popups && (dist < 1000 || show_popups == 2) ? desc : NULL);

    /* draw pointer */
    b = radtodeg(bearing(&gps_coord.xy, &pos)) - gps_bearing;
    while (b < 0) b += 360;
#if 0
    center_x = VFD_WIDTH - VFD_HEIGHT / 2;
    center_y = VFD_HEIGHT / 2;
    b2 = degtorad(b);
    tip_x = center_x + (int)(6.0 * sin(b2));
    tip_y = center_y - (int)(6.0 * cos(b2));
    vfdlib_drawLineUnclipped(screen, center_x, center_y,
			     tip_x, tip_y, VFDSHADE_BRIGHT);
#else
    tip_x = VFD_WIDTH - VFD_HEIGHT + h0;
    tip_y = h0 + 4;
#endif
    if (gps_bearing == -1)
	b = -1;
    _draw_mark(tip_x, tip_y, b, VFDSHADE_BRIGHT);
}

void draw_wpstext(void)
{
    static int topwp = 0;
    static int hoff, voff, lost, dir = 1;
    int i, n = 4, offset = 0;
    char *desc;
#define SHIFT 27

    if (topwp < nextwp) { // scroll towards next entry
	voff++;
	if (voff >= h0+1) {
	    topwp++;
	    voff = 0;
	} else {
	    n = 5;
	    do_refresh = 1;
	}
    }
    else if (!voff && topwp > nextwp) { // scroll towards previous entry
	topwp--;
	voff = h0+1;
	n = 5;
	do_refresh = 1;
    } else if (voff) {
	voff--;
	if (voff > 0) {
	    n = 5;
	    do_refresh = 1;
	}
    }

    lost = 0;
    if (route_getwp(topwp, NULL, NULL, &desc)) {
	int tmp = SHIFT + vfdlib_getTextWidth(desc, 0) - VFD_WIDTH;
	if (tmp > lost) lost = tmp;
    }

    // swap scroll direction
    if (hoff >= lost + 5) dir = -1;
    else if (hoff <= -5)  dir = 1;

    if (hoff > 0 || lost) {
	hoff += dir;
	if (hoff > 0) offset = hoff;
	do_refresh = 1;
    }
    for (i = 0; i < n; i++) {
	unsigned int dist, last_dist = 0;
	int vert;
	char buf[10];

	if (!route_getwp(topwp + i, NULL, &dist, &desc))
	    break;

	vert = i * (h0 + 1) + 3 - voff;

	if (show_time) time_estimate(buf, dist);
	else {
	    if (i > 1 || (i == 1 && !voff)) {
		vfdlib_drawText(screen, "+", 0, vert, 0, -1);
		route_getwp(topwp + i - 1, NULL, &last_dist, NULL);
	    }
	    formatdist(buf, abs(dist - last_dist));
	}
	vfdlib_drawText(screen, buf, SHIFT - 1 - vfdlib_getTextWidth(buf, 0),
			vert, 0, -1);

	vfdlib_setClipArea(SHIFT, 0, VFD_WIDTH, VFD_HEIGHT);
	vfdlib_drawText(screen, desc, SHIFT - offset, vert, 0, -1);
	vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
    }
    if (i == 0)
	draw_msg("No route loaded");
}

void draw_popup(char *text)
{
    static char *current;
    static int hoff, voff, lost, dir;
    int offset = 0, streq;

    if (!current && !text) return;
    streq = current && text && strcmp(current, text) == 0;
    if (!streq) { // scroll-out existing message
	voff--;
	if (voff > 0) do_refresh = 1;
	else {
	    free(current);
	    current = NULL;
	}
    }
    if (!current) {
	if (!text) return;
	current = strdup(text);
	lost = vfdlib_getTextWidth(current, 0) - MAX_X;
	if (lost < 0) lost = 0;
	hoff = 0;
	voff = 1;
	dir = 2;
	do_refresh = 1;
    }
    else if (streq && voff < h0) { // scroll-in
	voff++;
	do_refresh = 1;
    } else {
	// swap scroll direction
	if (hoff >= lost + 5) dir = -2;
	else if (hoff <= -5)  dir = 2;
	hoff += dir;
    }

    if (current) {
	if (lost) {
	    if (hoff > lost) offset = lost;
	    else if (hoff > 0) offset = hoff;
	    do_refresh = 1;
	}
	vfdlib_setClipArea(0, 0, MAX_X, VFD_HEIGHT);
	vfdlib_drawText(screen, current, -offset, VFD_HEIGHT - voff, 0, -1);
	vfdlib_setClipArea(0, 0, VFD_WIDTH, VFD_HEIGHT);
    }
}

void draw_display(void)
{
    empeg_updatedisplay(screen);
}

void err(char *msg)
{
    draw_msg(msg);
    draw_display();
    sleep(1);
    draw_clear();
}

void draw_sats(struct gps_state *gps)
{
    char line[26];
    int i, height, x, y, data = 0;
    int gshade, tshade, nsvs = 0;
    int w0;

    /* Draw a center point and circle for the satellite position display */
    vfdlib_drawOutlineEllipseClipped(screen, 112, 15, 15, 15, VFDSHADE_DIM);
    vfdlib_drawPointUnclipped(screen, 112, 15, VFDSHADE_DIM);

    /* draw lines at approximately 20db intervals. */
    vfdlib_drawLineHorizUnclipped(screen, VFD_HEIGHT-h0-1, 0, 64, VFDSHADE_DIM);
    vfdlib_drawLineHorizUnclipped(screen, VFD_HEIGHT-h0-6, 0, 64, VFDSHADE_DIM);
    vfdlib_drawLineHorizUnclipped(screen, VFD_HEIGHT-h0-11,0, 64, VFDSHADE_DIM);
    vfdlib_drawLineHorizUnclipped(screen, VFD_HEIGHT-h0-16,0, 64, VFDSHADE_DIM);

    for (i = 0; i < MAX_TRACKED_SATS; i++) {
	if (!gps->sats[i].svn) continue;

	data = 1;
	sprintf(line, "%02d", gps->sats[i].svn);
	height = 24 - (int)gps->sats[i].snr;

	if (gps->sats[i].used) {
	    gshade = VFDSHADE_BRIGHT;
	    tshade = -1;
	    nsvs++;
	}
	else
	    gshade = tshade = VFDSHADE_MEDIUM;

	vfdlib_drawText(screen, line, i * 8, VFD_HEIGHT - h0, 0, tshade);

	if (gps->time - gps->sats[i].time < 5)
	    vfdlib_drawSolidRectangleClipped(screen, i * 8 + 1, height,
					     i * 8 + 7, VFD_HEIGHT - h0 - 1,
					     gshade);
	else
	    vfdlib_drawOutlineRectangleClipped(screen, i * 8 + 1, height,
					       i * 8 + 7, VFD_HEIGHT - h0 - 1,
					       gshade);

	x = 108 + (12 * sin(gps->sats[i].azm) * cos(gps->sats[i].elv));
	y = 12  + (12 * -cos(gps->sats[i].azm) * cos(gps->sats[i].elv));
	vfdlib_drawText(screen, line, x, y, 0, tshade);
    }

    /* show coordinates? */
    format_coord(line, gps_state.lat, "NS");
    w0 = vfdlib_getTextWidth(line, 0);
    vfdlib_drawText(screen, line, 8, 0, 0, -1);

    format_coord(line, gps_state.lon, "EW");
    vfdlib_drawText(screen, line, 12+w0, 0, 0, -1);

    /* show altitude */
    formatalt(line, gps_state.alt);
    w0 = vfdlib_getTextWidth(line, 0);
    vfdlib_drawText(screen, line, 96-w0, h0, 0,
		    gps_state.fix == 0x3 ? -1 : VFDSHADE_MEDIUM);

    /* show speed */
    formatdist(line, gps_speed);
    strcat(line, "/h");
    w0 = vfdlib_getTextWidth(line, 0);
    vfdlib_drawText(screen, line, 96-w0, 2*h0, 0,
		    gps_state.fix & 0x1 ? -1 : VFDSHADE_MEDIUM);

    /* show HDOP */
    if (gps_state.hdop < 100.0)
	 sprintf(line, "%4.2f HD", gps_state.hdop);
    else strcpy(line, "--.-- HD");
    w0 = vfdlib_getTextWidth(line, 0);
    vfdlib_drawText(screen, line, 96-w0, 3*h0, 0,
		    gps_state.fix & 0x1 ? -1 : VFDSHADE_MEDIUM);

    /* show type of fix and # of svs used */
    {
	char *fixes[] = {" -", "2D", "--", "3D" };
	sprintf(line, "%s %2d SV", fixes[gps_state.fix], nsvs);
	w0 = vfdlib_getTextWidth(line, 0);
	vfdlib_drawText(screen, line, 96-w0, 4*h0, 0, -1);
    }

    /* fix, bearing, coordinates? */
    if (!data)
	draw_msg("Looking for satellites");
}

