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

/* results filled in by the decoding protocols */
struct gps_state {
    time_t  time;	/* GPS time translated to unix time */
    double  lat;	/* latitude in radians */
    double  lon;	/* longtitude in radians */
    int	    bearing;	/* current bearing (degrees 0 - 360) (-1 == no fix) */
    double  spd_east;   /* meters per hour */
    double  spd_north;  /* meters per hour */
    double  spd_up;     /* meters per hour */
};

struct gps_protocol {
    struct gps_protocol *next;
    char *name;
    int baud;
    void (*init)(void);
    int (*update)(char c, struct gps_state *state);
};

extern struct gps_protocol *gps_protocols;

#define REGISTER_PROTOCOL(proto_name, serial_baud, initfunc, updatefunc) \
  static struct gps_protocol __this = { .name = proto_name, .baud = serial_baud, .init = initfunc, .update = updatefunc }; \
  static __attribute__((constructor)) void ___init(void) { __this.next = gps_protocols; gps_protocols = &__this; }

/* automatic destructors don't work right on the arm, or did I mess this up?? */
  //static __attribute__((destructor)) void ___fini(void) { struct gps_protocol **p; for (p = &gps_protocols; (*p); p = &(*p)->next) if ((*p) == &proto_name) *p = (*p)->next; }

#endif

