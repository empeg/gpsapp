CFLAGS := -Wall -g -O2
LDLIBS := -lm

CC     := arm-linux-gcc
HOSTCC := gcc
STRIP  := arm-linux-strip

gpsapp_SRCS := gpsapp.c convert_empeg.c draw.c route.c track.c \
    serial.c gps_nmea.c gps_tsip.c gps_earthmate.c gps_protocol.c \
    empeg_ui.c vfdlib.c config.c
mini_ifconfig_SRCS := mini_ifconfig.c

gpsapp_OBJS := $(gpsapp_SRCS:.c=.o)
gpsapp_host_OBJS := $(gpsapp_SRCS:.c=_host.o) gps_tracklog_host.o
mini_ifconfig_OBJS := $(mini_ifconfig_SRCS:.c=.o)

all: gpsapp gpsapp_host mini_ifconfig

gpsapp: ${gpsapp_OBJS}
	$(CC) -o $@ $^ $(LDLIBS)
	-$(STRIP) $@

%_host.o : %.c
	$(HOSTCC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

gpsapp_host: ${gpsapp_host_OBJS}
	$(HOSTCC) $(CFLAGS) -o $@ $^ $(LDLIBS) -L/usr/X11R6/lib -lX11

mini_ifconfig: ${mini_ifconfig_OBJS}
	$(CC) -o $@ $^ $(LDLIBS)
	-$(STRIP) $@

clean: dist
	-rm -f gpsapp hack_init mini_ifconfig

dist:
	-rm -f ${gpsapp_host_OBJS} ${gpsapp_OBJS} ${mini_ifconfig_OBJS}
	-rm -f gpsapp_host *.orig

