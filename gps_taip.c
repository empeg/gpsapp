/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <stdlib.h>
#include <math.h>
#include "gpsapp.h"
#include "gps_protocol.h"

static int taip_decode(struct gps_state *gps)
{
    char buf[10];
    double speed, b;

    if (strcmp(packet, "RTM") == 0) {
	// datestamp = ???
    } else if (strcmp(packet, "RPV") == 0) {
	if (packet[32] == '0') return 0;

	memcpy(buf, &packet[3], 5); buf[5] = '\0';
	gps->time = strtol(buf, NULL, 10); // + datestamp;

	memcpy(buf, &packet[8], 8); buf[8] = '\0';
	gps->lat = degtorad((double)strtol(buf, NULL, 10) / 100000.0);

	memcpy(buf, &packet[16], 9); buf[9] = '\0';
	gps->lon = degtorad((double)strtol(buf, NULL, 10) / 100000.0);

	memcpy(buf, &packet[28], 3); buf[3] = '\0';
	gps->bearing = strtol(buf, NULL, 10);

	memcpy(buf, &packet[25], 3); buf[3] = '\0';
	speed = (double)strtol(buf, NULL, 10) * 0.44704; // * 1609.344 / 3600

	b = degtorad(gps->bearing);
	gps->spd_east  = sin(b) * speed;
	gps->spd_north = cos(b) * speed;
	gps->spd_up    = 0.0;

	if (packet[31] == '9')
	    gps->bearing = -1;

	return 1;
    }
    return 0;
}

static void taip_init(void)
{
    char *cmd;
    
    /* Report position every second */
    cmd = ">FPV00010000<";
    serial_send(cmd, sizeof(cmd));

    /* Report time every 15 seconds */
    cmd = ">FTM00150000<";
    serial_send(cmd, sizeof(cmd));

    /* Get current time */
    cmd = ">QTM<";
    serial_send(cmd, sizeof(cmd));
}

static int taip_update(char c, struct gps_state *gps)
{
    static int start;
    int upd = 0;

    if (c == '>') {
	start = 1;
	return 0;
    }

    if (!start) return 0;

    /* end of packet? */
    if (c == '<') {
	upd = taip_decode(gps);

restart:
	packet_idx = 0;
	start = 0;
	return upd;
    }

    packet[packet_idx++] = c;

    /* discard long lines */
    if (packet_idx == MAX_PACKET_SIZE)
	goto restart;
    return 0;
}

REGISTER_PROTOCOL("TAIP", 4800, taip_init, taip_update);

