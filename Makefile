CFLAGS := -Wall -g -O2
LDLIBS := -lm

CC     := arm-linux-gcc
HOSTCC := gcc
STRIP  := arm-linux-strip

gpsapp_SRCS := gpsapp.c convert_empeg.c draw.c route.c track.c \
    serial.c gps_nmea.c gps_garmin.c gps_tsip.c gps_taip.c \
    gps_earthmate.c gps_protocol.c empeg_ui.c vfdlib.c config.c
hack_init_SRCS := hack_init.c
incar_SRCS := incar.c

gpsapp_OBJS := $(gpsapp_SRCS:.c=.o)
gpsapp_host_OBJS := $(gpsapp_SRCS:.c=_host.o) gps_tracklog_host.o
hack_init_OBJS := $(hack_init_SRCS:.c=.o)
incar_OBJS := $(incar_SRCS:.c=.o)

all: gpsapp hack_init incar gpsapp_host

gpsapp: ${gpsapp_OBJS}
	$(CC) -o $@ $^ $(LDLIBS)
	-$(STRIP) $@

hack_init: ${hack_init_OBJS}
	$(CC) -o $@ $^
	-$(STRIP) $@

incar: ${incar_OBJS}
	$(CC) -o $@ $^
	-$(STRIP) $@

%_host.o : %.c
	$(HOSTCC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

gpsapp_host: ${gpsapp_host_OBJS}
	$(HOSTCC) -o $@ $^ $(LDLIBS) -L/usr/X11R6/lib -lX11

clean: dist
	-rm -f gpsapp hack_init incar

dist:
	-rm -f gpsapp_host ${gpsapp_host_OBJS} ${gpsapp_OBJS} ${hack_init_OBJS}
	-rm -f ${incar_OBJS} *.orig

