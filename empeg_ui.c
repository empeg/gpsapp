/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "empeg_ui.h"

#ifdef __arm__ /* assume we're cross-compiled for an empeg */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

static int hijack_fd = -1;

int empeg_init(void)
{
    /* move away from partitions that might need to be remounted during sync */
    chdir("/");

    /* detach from console */
    if (fork()) return -1;

    hijack_fd = open("/dev/display", O_RDWR);
    if (hijack_fd == -1) {
	fprintf(stderr, "Failed to open /dev/display\n");
	return -1;
    }
    return 0;
}

int empeg_waitmenu(void)
{
    const unsigned long buttons[] = { 8,
	/* front panel buttons (8) */
	IR_TOP_BUTTON_PRESSED, IR_RIGHT_BUTTON_PRESSED, IR_LEFT_BUTTON_PRESSED,
	IR_BOTTOM_BUTTON_PRESSED, IR_BOTTOM_BUTTON_RELEASED,
	IR_KNOB_PRESSED, IR_KNOB_RIGHT, IR_KNOB_LEFT
    };
    const struct hijack_geom_s fullscreen = {
	0, EMPEG_SCREEN_ROWS - 1,
	0, EMPEG_SCREEN_COLS - 1
    };
    const char *menu[] = { "GPS", NULL };
    int rc;

    if (hijack_fd == -1)
	return -1;

    rc = ioctl(hijack_fd, EMPEG_HIJACK_WAITMENU, menu);
    if (rc < 0) {
	fprintf(stderr, "Couldn't register Hijack menu\n");
	close(hijack_fd);
	hijack_fd = -1;
	return -1;
    }

    /* Ok, we got selected, now hijack buttons and screen */
    ioctl(hijack_fd, EMPEG_HIJACK_BINDBUTTONS, buttons);
    ioctl(hijack_fd, EMPEG_HIJACK_SETGEOM, &fullscreen);

    return 0;
}

void empeg_free(void)
{
    if (hijack_fd == -1)
	return;

    ioctl(hijack_fd, EMPEG_HIJACK_WAITMENU, NULL);
    close(hijack_fd);
    hijack_fd = -1;
}

int empeg_getkey(unsigned long *key)
{
    int rc;

    if (hijack_fd == -1)
	return -1;

    rc = ioctl(hijack_fd, EMPEG_HIJACK_POLLBUTTONS, key);

    if (rc == -1 && errno != EBUSY && errno != EAGAIN)
	return -1;

    return (rc != -1);
}

void empeg_updatedisplay(const unsigned char *screen)
{
    if (hijack_fd == -1)
	return;

    ioctl(hijack_fd, EMPEG_HIJACK_DISPWRITE, screen);
}

#else /* !defined(__arm__) native compilation for debugging purposes */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

static Display *display;
static Window   window;
static GC       gc;

#define XSCALE  3
#define X_WIDTH  (EMPEG_SCREEN_COLS * XSCALE)
#define X_HEIGHT (EMPEG_SCREEN_ROWS * XSCALE)

int empeg_init(void)
{
    display = XOpenDisplay(NULL);
    if (!display) {
	fprintf(stderr, "couldn't open display\n");
	return -1;
    }

    window = XCreateSimpleWindow(display, DefaultRootWindow(display),
				 0, 0, X_WIDTH+1, X_HEIGHT+1, 0, 0, 0);
    XSelectInput(display, window,
		 StructureNotifyMask | KeyPressMask | KeyReleaseMask);

    XMapWindow(display, window);

    gc = XCreateGC(display, window, 0, NULL);

    for (;;) {
	XEvent e;
	XNextEvent(display, &e);
	if (e.type == MapNotify)
	    break;
    }
    return 0;
}

void empeg_free(void)
{
    XCloseDisplay(display);
}

int empeg_waitmenu(void)
{
    return 0;
}

int empeg_getkey(unsigned long *key)
{
    *key = -1;

    while(*key == -1) {
	XEvent e, next;
	KeySym keysym;

	if (!XPending(display))
	    return 0;

	XNextEvent(display, &e);

	if (e.type == KeyPress) {
	    keysym = XLookupKeysym(&e.xkey, 0);
	    switch (keysym) {
	    case XK_Up:		*key = IR_TOP_BUTTON_PRESSED; break;
	    case XK_Right:	*key = IR_RIGHT_BUTTON_PRESSED; break;
	    case XK_Left:	*key = IR_LEFT_BUTTON_PRESSED; break;
	    case XK_Down:	*key = IR_BOTTOM_BUTTON_PRESSED; break;
	    case XK_Home:       *key = IR_KNOB_PRESSED; break;
	    case XK_Page_Up:    *key = IR_KNOB_RIGHT; break;
	    case XK_Page_Down:  *key = IR_KNOB_LEFT; break;
	    }
	}
	else if (e.type == KeyRelease) {
	    keysym = XLookupKeysym(&e.xkey, 0);
	    /* ignore autorepeat keys (except for "rotate knob") */
	    if (keysym != XK_Page_Up && keysym != XK_Page_Down &&
		XPending(display)) {
		XPeekEvent(display, &next);
		if (next.type == KeyPress && next.xkey.time == e.xkey.time &&
		    next.xkey.keycode == e.xkey.keycode)
		{
		    XNextEvent(display, &next);
		    continue;
		}
	    }
	    switch (keysym) {
	    case XK_Up:		*key = IR_TOP_BUTTON_RELEASED; break;
	    case XK_Right:	*key = IR_RIGHT_BUTTON_RELEASED; break;
	    case XK_Left:	*key = IR_LEFT_BUTTON_RELEASED; break;
	    case XK_Down:	*key = IR_BOTTOM_BUTTON_RELEASED; break;
	    case XK_Home:       *key = IR_KNOB_RELEASED; break;
	    }
	}
    }
    return 1;
}

void empeg_updatedisplay(const unsigned char *screen)
{
    const unsigned int colors[4] =
	//{ 0x00000000, 0x00210000, 0x00300000, 0x00ff0000 };
	  { 0x00000000, 0x0000003f, 0x0000007f, 0x000000ff };
    int x, y, idx, pix;

    for (y = 0; y < EMPEG_SCREEN_ROWS; y++) {
	for (x = 0; x < EMPEG_SCREEN_COLS/2; x++) {
	    idx = y * (EMPEG_SCREEN_COLS/2) + x;

	    pix = screen[idx] & 0x3;
	    XSetForeground(display, gc, colors[pix]);
	    XFillRectangle(display, window, gc, x * XSCALE * 2, y * XSCALE,
			   XSCALE-1, XSCALE-1);

	    pix = (screen[idx] >> 4) & 0x3;
	    XSetForeground(display, gc, colors[pix]);
	    XFillRectangle(display, window, gc, x * XSCALE * 2 + XSCALE,
			   y * XSCALE, XSCALE-1, XSCALE-1);
	}
    }
    XFlush(display);
}
#endif

