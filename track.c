/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <stdio.h>
#include <stdlib.h>
#include "gpsapp.h"
#include "vfdlib.h"

#define MAX_TRACK 128
static struct xy tracklog[MAX_TRACK];
static int tracklog_size;
static int tracklog_idx;

void track_init(void)
{
    tracklog_idx = tracklog_size = 0;
}

void track_pos(void)
{
    /* no need to log the same position multiple times */
    if (tracklog[tracklog_idx].x == gps_coord.xy.x && 
	tracklog[tracklog_idx].y == gps_coord.xy.y)
	return;

    tracklog[tracklog_idx++] = gps_coord.xy;
    if (tracklog_size < MAX_TRACK) tracklog_size++;
    if (tracklog_idx == MAX_TRACK) tracklog_idx = 0;
}

void track_draw(void)
{
    int head = tracklog_idx;

    if (tracklog_size == MAX_TRACK)
	draw_lines(&tracklog[head], MAX_TRACK - head, VFDSHADE_DIM);

    draw_lines(tracklog, head, VFDSHADE_DIM);
}

