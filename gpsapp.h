#ifndef _GPSAPP_H_
#define _GPSAPP_H_

#include <sys/time.h>

/* configuration settings */
extern int show_scale;
extern int show_gpscoords;
extern int show_rubberband;
extern int show_metric;

/* screen coordinates */
struct xy { int x, y; };

/* geodetic coordinates (radians, WGS85 datum) */
struct coord {
    double lat, lon;
    struct xy xy;
};

/* route waypoints */
struct wp { int idx; char *desc; };

struct route {
    int	npts;
    int	nwps;
    struct xy *pts;
    struct wp *wps;
    int	*dists;
};

extern int h0;

/* conversion functions (convert.c) */
double degtorad(const double deg);
double intdegtorad(const int deg);
double radtodeg(const double rad);
char *formatdist(const unsigned int distance);

void toTM(const struct coord *point, const struct coord *center, struct xy *xy);
long long distance2(const struct xy *coord1, const struct xy *coord2);
int bearing(const struct xy *coord1, const struct xy *coord2);
int towards(const struct xy *here, const struct xy *coord, const int dir);

/* screen update functions (screen.c) */
void draw_setup(unsigned char *screen, const struct coord *center,
		const int scale);
void draw_line(unsigned char *screen, const struct xy *from,
	       const struct xy *to, const int shade);
void draw_point(unsigned char *screen, const struct xy *coord, const int shade);
void draw_route(unsigned char *screen, const struct xy *pts, const int npts,
		const int shade);
void _draw_mark(unsigned char *screen, const int x, const int y, const int dir,
	       const int shade);
void draw_mark(unsigned char *screen, const struct xy *pnt, const int dir,
	       const int shade);
void draw_msg(unsigned char *screen, const char *msg);

void draw_init(void);
void draw_dump(void);
void draw_exit(void);

/* actual drawing functions (x11.c) */
#ifdef USE_X11
void x11_init(void);
void x11_dump(const char *screenbuf);
void x11_exit(void);
#endif

/* tracking functions (track.c) */
void track_init(struct coord *center);
void track_jump(const int fwrev);
int track_pos(struct coord *coord, int *dir);
void track_draw(unsigned char *screen);
void track_free(void);

/* i/o functions (hijack.c) */
int hijack_init(void);
int hijack_waitmenu(void);
void hijack_exit(void);
int hijack_getkey(unsigned long *key);
void hijack_updatedisplay(const unsigned char *screen);

/* route functions (route.c) */
void route_init(void);
void route_load(const char *file);
void route_locate(const struct coord *pos, const int dir, int *nextwp);
void route_draw(unsigned char *screen, struct xy *cur_pos);
int route_getwp(const int wp, struct xy *pos, int *dist, char **desc);

#endif

