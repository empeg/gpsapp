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
    double elv;  /* elevation above horizon in radians */
    double azm;  /* azimuth from true north in radians */
    int    snr;  /* signal to noise ratio, scaled to [0, 24] */
    int    used; /* set when satellite is used in the fix */
};

struct gps_state {
    time_t  time;	/* GPS time translated to unix time */
    double  lat;	/* latitude in radians */
    double  lon;	/* longtitude in radians */
    int	    bearing;	/* current bearing (degrees 0 - 360) (-1 == no fix) */
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
    void (*init)(void);
    int (*update)(char c, struct gps_state *state);
};

#define UNKNOWN_TIME -1
#define UNKNOWN_ELV -1.0
#define UNKNOWN_AZM -1.0
#define UNKNOWN_SNR -1
#define UNKNOWN_USED -1
void new_sat(struct gps_state *gps, int svn, int time, double elv, double azm,
	     int snr, int used);
int conv_date(int year, int mon, int day);

extern struct gps_protocol *gps_protocols;

#define REGISTER_PROTOCOL(proto_name, serial_baud, serial_parity, initfunc, updatefunc) \
  static struct gps_protocol __this = { .name = proto_name, .baud = serial_baud, .parity = serial_parity, .init = initfunc, .update = updatefunc }; \
  static __attribute__((constructor)) void ___init(void) { __this.next = gps_protocols; gps_protocols = &__this; }

/* automatic destructors don't work right on the arm, or did I mess this up?? */
  //static __attribute__((destructor)) void ___fini(void) { struct gps_protocol **p; for (p = &gps_protocols; (*p); p = &(*p)->next) if ((*p) == &proto_name) *p = (*p)->next; }

#endif

