#
#
#

CFLAGS := -Wall -g -O2
LDLIBS := -lm
gpsapp_OBJS := gpsapp.o convert.empeg.o hijack.o route.o screen.o track.o \
    vfdlib.o

CC := arm-linux-gcc
#LDLIBS += -L/usr/X11R6/lib -lX11

gpsapp: ${gpsapp_OBJS}

clean:
	rm -f gpsapp ${gpsapp_OBJS}

