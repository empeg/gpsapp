/*

vfdlib - a graphics api library for empeg userland apps
Copyright (C) 2002  Richard Kirkby

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Send comments, suggestions, bug reports to rkirkby (at) cs.waikato.ac.nz

*/

#ifndef VFDLIB_H
#define VFDLIB_H

/* useful constants */

#define VFD_WIDTH               128
#define VFD_HEIGHT              32
#define VFD_BYTES_PER_SCANLINE  64

#define VFDSHADE_BLACK          0
#define VFDSHADE_DIM            1
#define VFDSHADE_MEDIUM         2
#define VFDSHADE_BRIGHT         3

#define BITMAP_1BPP             0
#define BITMAP_2BPP             1
#define BITMAP_4BPP             2

/*

NOTES

The functions and parameters should be fairly self-explanatory. For more
detailed notes about the parameters consult the function headers in the source
file (vfdlib.c).

Coordinates for drawing operations are in pixels. In terms of x-coordinates,
the display spans from position 0 (the leftmost column) to position 127 (the
rightmost column). In terms of y-coordinates, the display spans from position
0 (the top row) to position 31 (the bottom row).

All functions in the library have a vfdlib_ prefix. Every function that draws
to a screen buffer requires a pointer to the buffer (a 2048 area of memory
where each nybble represents a pixel), and a shade to draw with
(VFDSHADE_BLACK, VFDSHADE_DIM, VFDSHADE_MEDIUM or VFDSHADE_BRIGHT).

Several of the shape functions come in two versions - outline or solid.

The clip area is a user-definable rectangle. Most drawing functions come in 
two flavours - clipped and unclipped. Text, bitmaps and solid polygons are
always clipped. Clipped functions are guaranteed to be safe but incur extra 
overhead. If you are certain that a drawing operation will always be contained
within the clip area you can instead opt for the unclipped version to gain
extra speed. Use at your own risk! 

Notice that the some of clipped shape drawing methods ask for the extremities
of the shapes, whereas their unclipped conterparts ask for lengths.

*/

/*** CLIP AREA

By default, the clip area will be set to the full display
(0, 0, VFD_WIDTH, VFD_HEIGHT) and will protect memory outside of the display
buffer.

After setting the clip area, any pixels that fall outside during a clipped
drawing operation will not be drawn. 

*/
void vfdlib_setClipArea(int xLeft, int yTop, int xRight, int yBottom);

/*** CLEAR */
void vfdlib_fastclear(char *buffer);
void vfdlib_clear(char *buffer, char shade);

/*** POINTS */
void vfdlib_drawPointClipped(char *buffer, int xPos, int yPos, char shade);
void vfdlib_drawPointUnclipped(char *buffer, int xPos, int yPos, char shade);

/*** LINES */
void vfdlib_drawLineHorizClipped(char *buffer, int yPos,
				 int xLeft, int xRight, char shade);
void vfdlib_drawLineHorizUnclipped(char *buffer, int yPos, int xLeft,
				   int length, char shade);
void vfdlib_drawLineVertClipped(char *buffer, int xPos,
				int yTop, int yBottom, char shade);
void vfdlib_drawLineVertUnclipped(char *buffer, int xPos, int yTop,
				  int length, char shade);
void vfdlib_drawLineClipped(char *buffer, int x1, int y1, int x2, int y2,
			    char shade);
void vfdlib_drawLineUnclipped(char *buffer, int x1, int y1, int x2, int y2,
			      char shade);

/*** RECTANGLES 

The invert methods allow you to invert the pixel intensities in a rectangular
area of the display.

*/
void vfdlib_drawOutlineRectangleClipped(char *buffer, int xLeft, int yTop,
					int xRight, int yBottom, char shade);
void vfdlib_drawOutlineRectangleUnclipped(char *buffer, int xLeft, int yTop,
					  int xWidth, int yHeight, char shade);
void vfdlib_drawSolidRectangleClipped(char *buffer, int xLeft, int yTop,
				      int xRight, int yBottom, char shade);
void vfdlib_drawSolidRectangleUnclipped(char *buffer, int xLeft, int yTop,
					int xWidth, int yHeight, char shade);
void vfdlib_invertRectangleClipped(char *buffer, int xLeft, int yTop,
				   int xRight, int yBottom);
void vfdlib_invertRectangleUnclipped(char *buffer, int xLeft, int yTop,
				     int xWidth, int yHeight);


/*** ELLIPSES

Parameters cX and cY are the centre coordinates of the ellipse.

*/
void vfdlib_drawOutlineEllipseClipped(char *buffer, int cX, int cY,
				      int halfWidth, int halfHeight,
				      char shade);
void vfdlib_drawOutlineEllipseUnclipped(char *buffer, int cX, int cY,
					int halfWidth, int halfHeight,
					char shade);
void vfdlib_drawSolidEllipseClipped(char *buffer, int cX, int cY,
				    int halfWidth, int halfHeight,
				    char shade);
void vfdlib_drawSolidEllipseUnclipped(char *buffer, int cX, int cY,
				      int halfWidth, int halfHeight,
				      char shade);

/*** POLYGONS

Parameter coordsData should point to an array of numPoints pairs of integers -
x0, y0, x1, y1, ...
Polygons are closed automatically - the first point is drawn connected to the
last point.

*/
void vfdlib_drawOutlinePolygonClipped(char *buffer, int *coordsData,
				      int numPoints, char shade);
void vfdlib_drawOutlinePolygonUnclipped(char *buffer, int *coordsData,
					int numPoints, char shade);
void vfdlib_drawSolidPolygon(char *buffer, int *coordsData,
			     int numPoints, char shade);

/*** TEXT

Before fonts can be used they must be registered. To register a font you
must give the filename of a .bf font file (the same type of files used by
the player software).

The font files used by the player software are as follows:

/empeg/lib/fonts/small.bf  -  6 pixels high by  0-9 pixels wide
/empeg/lib/fonts/medium.bf -  9 pixels high by 0-11 pixels wide
/empeg/lib/fonts/large.bf  - 18 pixels high by 0-16 pixels wide

The more fonts you use the more resources they take so only register the
fonts you need. There are five available slots (0-4) to load with fonts.

When fonts are no longer needed, you should unregister them to free up the
memory used. The best way to ensure a nice clean up is to call
unregisterAllFonts before your program exits.

The general process of using fonts goes something like this:

// at beginning of program:
vfdlib_registerFont("/empeg/lib/fonts/small.bf", 0);
vfdlib_registerFont("/empeg/lib/fonts/medium.bf", 1);

// during program:
vfdlib_drawText(buffer, "Small text", 0, 0, 0, -1);
vfdlib_drawText(buffer, "Medium text", 0, vfdlib_getTextHeight(0), 1, -1);

// at exit of program:
vfdlib_unregisterAllFonts();

The shade parameter to drawText behaves the same as it does for drawing
bitmaps. As the fonts are 2BPP, it is best to use -1 for this parameter
to retain the anti-aliased look of the fonts.

The get functions allow you to measure the pixel size required to display a
string. The getStringIndexOfCutoffWidth function allows you to determine
which character within a string will be cut off within limited horizontal
space.

*/
int vfdlib_registerFont(char *bfFileName, int fontSlot);
void vfdlib_unregisterFont(int fontSlot);
void vfdlib_unregisterAllFonts();
int vfdlib_getTextHeight(int fontSlot);
int vfdlib_getTextWidth(char *string, int fontSlot);
int vfdlib_getStringIndexOfCutoffWidth(char *string, int fontSlot, int width);
void vfdlib_drawText(char *buffer, char *string, int xLeft, int yTop,
		     int fontSlot, char shade);

/*** BITMAPS

Format:

The header consists of 5 bytes:
0 - the type of bitmap, either BITMAP_1BPP, BITMAP_2BPP or BITMAP_4BPP
1 - the hi byte of the width
2 - the lo byte of the width
3 - the hi byte of the height
4 - the lo byte of the height

The remaining bytes are the actual bitmap infomation, arranged left to right,
top to bottom. Note that the bitmap scanlines must be byte-aligned. That is,
each scanline must be padded with extra bits so that the next scanline always
begins with a new byte.

The pixel format for each byte depends on the type:
BITMAP_1BPP: 8 pixels per byte. Each bit encodes a pixel, either on or off.
  The highest bit being the leftmost pixel, the lowest being the rightmost
BITMAP_2BPP: 4 pixels per byte. Each consecutive pair of bits encodes a pixel,
  0-3 representing the 4 possible shades: 00 - black, 01 - dim, 10 - medium,
  11 - bright. The lowest pair represents the leftmost pixel, the highest pair
  represents the rightmost pixel.
BITMAP_4BPP: 2 pixels per byte. Each nybble represents a pixel, the lower 2
  bits hold the shade in the same way as BITMAP_2BPP. The upper bits are used
  to encode transpency. If the isTransparent flag is set when calling the draw
  function, any pixel with an upper bit set will not affect the display.

Notes:

Setting the shade will override the shade of any pixels plotted, set the shade
to -1 to preserve the shades encoded in the bitmap. If the isTransparent flag
is not set, then the bitmap rectangle will destroy all pixels it is written
onto. If the isTransparent flag is set, then any zero-valued pixels will not
be drawn (or in the case of 4BPP, any pixels with high bits set).

Set the width or height to -1 to use the maximum dimensions of the bitmap.

*/
void vfdlib_drawBitmap(char *buffer, unsigned char *bitmap,
		       int destX, int destY,
		       int sourceX, int sourceY,
		       int width, int height,
		       signed char shade, int isTransparent);

#endif
