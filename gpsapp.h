/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#ifndef _GPSAPP_H_
#define _GPSAPP_H_

#include <sys/time.h>
#include <math.h>
#include "gps_protocol.h"

/* configuration settings */
extern int show_scale;
extern int show_gpscoords;
extern int show_rubberband;
extern int show_metric;
extern int show_popups;
extern int show_time;
extern int coord_format; /* DDD = 0, DMM = 1, DMS = 2 */

/* screen coordinates */
struct xy { int x, y; };

/* geodetic coordinates (radians, WGS85 datum) */
struct coord {
    double lat, lon;
    struct xy xy;
};

/* route waypoints */
struct wp {
    int idx;
    char *short_desc;
    short inhdg, outhdg;
};

struct route {
    int	npts;
    int	nwps;
    struct xy *pts;
    struct wp *wps;
    int *dists;
};

extern int h0;
extern int do_refresh;
extern struct coord coord_center;

/* conversion functions (convert.c) */
#define degtorad(deg) (deg * ((2.0 * M_PI) / 360.0))
#define radtodeg(rad) (rad * (360.0 / (2.0 * M_PI)))

char *formatdist(char *buf, const int distance, const int alt);
char *time_estimate(char *buf, const int distance);
char *format_coord(char *buf, double latr, double lonr);

void toTM(struct coord *point);
long long distance2(const struct xy *coord1, const struct xy *coord2);
double bearing(const struct xy *coord1, const struct xy *coord2);
int towards(const struct xy *here, const struct xy *coord, const int dir);

/* screen update functions (draw.c) */
void draw_clear(void);
void draw_zoom(int inout);
void draw_point(const struct xy *coord, const int shade);
void draw_line(const struct xy *from, const struct xy *to, const int shade);
void draw_lines(const struct xy *pts, const int npts, const int shade);
void draw_msg(const char *msg);
void _draw_mark(const int x, const int y, const int dir, const int shade);
void draw_mark(const struct xy *pnt, const int dir, const int shade);
void draw_info(void);
void draw_scale(void);
void draw_gpscoords(void);
void draw_wpstext(void);
void draw_popup(char *text);
void draw_display(void);
void err(char *msg);
void draw_sats(struct gps_state *gps);

/* tracking functions (track.c) */
void track_init(void);
void track_pos(void);
void track_draw(void);

/* route functions (route.c) */
extern int nextwp;
int routes_init(void);
void routes_list(void);
void route_select(int updown);
void route_load(void);
void route_init(void);
void route_locate(void);
void route_skipwp(int dir);
void route_draw(struct xy *cur_pos);
int route_getwp(const int wp, struct xy *pos, unsigned int *dist, char **desc);
void route_recenter(void);
void route_update_vmg(void);

/* serial port/gps interfacing functions (serial.c) */

extern struct gps_state gps_state;
extern struct coord     gps_coord;
extern int		gps_speed;     /* current speed measurement */
extern int              gps_avgvmg;    /* average velocity made good */
extern int		gps_bearing;   /* last 'stable' bearing measurement */
#define AVGVMG_SHIFT 6

void serial_protocol(char *proto);
void serial_open(void);
void serial_close(void);
void serial_poll(void);

/* config file parser (config.c) */
#define CONFIG_HEADER "[gpsapp]"
#define CONFIG_HDRLEN 8
int config_ini_option (char *s, char *match, int *inside);

#endif

