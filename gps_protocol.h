/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#ifndef _GPS_PROTOCOL_H_
#define _GPS_PROTOCOL_H_

#include <time.h>

/* shared scratch buffer that can be used by the various decoding protocols
 * as only one is active at a time anyways. */
#define MAX_PACKET_SIZE 128
extern unsigned char packet[MAX_PACKET_SIZE];
extern int packet_idx;
void serial_send(char *buf, int len);

/* structure to be filled in by the decoding protocols */
#define MAX_TRACKED_SATS 8
struct gps_sat {
    int    svn;  /* satellite identifier (set to 0 when this slot is unused) */
    int	   time; /* appx time of last measurement */
    double elv;  /* elevation above horizon in radians [0, PI/2] */
    double azm;  /* azimuth from true north in radians [0, PI*2] */
    int    snr;  /* signal to noise ratio, scaled to [0, 24] */
    int    used; /* set when satellite is used in the fix */
};

struct gps_state {
    int	    updated;    /* bitmask, which fields have been updated */
#define GPS_STATE_COORD   0x01
#define GPS_STATE_BEARING 0x02
#define GPS_STATE_SPEED   0x04
#define GPS_STATE_SIGNALS 0x08
#define GPS_STATE_SATS    0x10
#define GPS_STATE_FIX     0x20

    time_t  time;	/* GPS time, most likely of last fix, translated to
			   unix time */

    int	    fix;	/* type of last fix (0 = No fix, 2 = 2D, 3 = 3D) */
    double  hdop;	/* horizontal dillution of precision */

    double  lat;	/* latitude in degrees [-180, 180] */
    double  lon;	/* longtitude in degrees [-90, 90] */
    double  alt;	/* altitude */

    int	    bearing;	/* current bearing degrees [0, 360] */
    double  spd_east;   /* meters per second */
    double  spd_north;  /* meters per second */
    double  spd_up;     /* meters per second */

    struct gps_sat sats[MAX_TRACKED_SATS];
};

struct gps_protocol {
    struct gps_protocol *next;
    char *name;
    int baud;
    char parity;
    void (*init)(void); /* protocol initializer */
    void (*poll)(void); /* every 5 seconds to poll non-automatic updates */
    void (*update)(char c, struct gps_state *state); /* serial input */
};

/* helper functions in gps_protocol.c */
#define UNKNOWN_TIME -1
#define UNKNOWN_ELV -1.0
#define UNKNOWN_AZM -1.0
#define UNKNOWN_SNR -1
#define UNKNOWN_USED -1
void new_sat(struct gps_state *gps, int svn, int time, double elv, double azm,
	     int snr, int used);
int conv_date(int year, int mon, int day);

extern struct gps_protocol *gps_protocols;

#define REGISTER_PROTOCOL(proto_name, serial_baud, serial_parity, initfunc, pollfunc, updatefunc) \
  static struct gps_protocol __this = { .name = proto_name, .baud = serial_baud, .parity = serial_parity, .init = initfunc, .poll = pollfunc, .update = updatefunc }; \
  static __attribute__((constructor)) void ___init(void) { __this.next = gps_protocols; gps_protocols = &__this; }

/* automatic destructors don't work right on the arm, or did I mess this up?? */
  //static __attribute__((destructor)) void ___fini(void) { struct gps_protocol **p; for (p = &gps_protocols; (*p); p = &(*p)->next) if ((*p) == &proto_name) *p = (*p)->next; }

#endif

