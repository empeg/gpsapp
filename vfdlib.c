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

#include <stdlib.h>
#include <stdio.h>
#include "vfdlib.h"

#define MASK_LO_NYBBLE   0x0F
#define MASK_HI_NYBBLE   0xF0


/* font info */

#define NUM_FONT_SLOTS   5

typedef struct {
  short offset;
  char width;
} CharInfo;

typedef struct {
  CharInfo *cInfo;
  unsigned char *fBitmap;
  int firstIndex;
  int numOfChars;
} FontInfo;


/* global variables */

static int g_clipXLeft = 0;
static int g_clipYTop = 0;
static int g_clipXRight = VFD_WIDTH;
static int g_clipYBottom = VFD_HEIGHT;
static FontInfo g_fontRegistry[NUM_FONT_SLOTS] = {
  {NULL, NULL, 0, 0},
  {NULL, NULL, 0, 0},
  {NULL, NULL, 0, 0},
  {NULL, NULL, 0, 0},
  {NULL, NULL, 0, 0}
};


/* private utility prototypes */

static void drawPixel4WaySymmetricClipped(char *buffer, int cX, int cY,
					  int x, int y, char shade);
static void drawPixel4WaySymmetricUnclipped(char *buffer, int cX, int cY,
					    int x, int y, char shade);
static void drawLineHMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade);
static void drawLineVMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade);
static void drawLineHMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade);
static void drawLineVMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade);
static void drawBitmap1BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent);
static void drawBitmap2BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent);
static void drawBitmap4BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent);


/* public methods */

/*
SETCLIPAREA

Sets the clip rectangle, outside which all clipped drawing operations will be
discarded.

Parameters:
 xLeft - the left hand side of the clip rectangle
 yTop - the top of the clip rectangle
 xRight - the right hand side of the clip rectangle
 yBottom - the bottom of the clip rectangle

Notes:
 Checks are in place to ensure the clip rectangle cannot reach beyond the
 display area.

*/
void vfdlib_setClipArea(int xLeft, int yTop, int xRight, int yBottom) {

  /* sanity checks */
  if (yTop > yBottom || xLeft > xRight) return;
  if (xLeft < 0) xLeft = 0;
  if (yTop < 0) yTop = 0;
  if (xRight > VFD_WIDTH) xRight = VFD_WIDTH;
  if (yBottom > VFD_HEIGHT) yBottom = VFD_HEIGHT;

  /* assign new values */
  g_clipXLeft = xLeft;
  g_clipYTop = yTop;
  g_clipXRight = xRight;
  g_clipYBottom = yBottom;
}

/*
FASTCLEAR

Clears the entire display buffer to black.

Parameters:
 buffer - pointer to the display buffer

Notes:
 This method ignores the clip area.

*/
void vfdlib_fastclear(char *buffer) {

  int i;
  int* intBuffer = (int *) buffer;
  for (i=0; i<(VFD_HEIGHT * VFD_BYTES_PER_SCANLINE) >> 2; i++) *(intBuffer++) = 0;
}

/*
CLEAR

Clears the area inside the clip area to a specified colour.

Parameters:
 buffer - pointer to the display buffer
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

*/
void vfdlib_clear(char *buffer, char shade) {

  vfdlib_drawSolidRectangleUnclipped(buffer, g_clipXLeft, g_clipYTop,
				     g_clipXRight - g_clipXLeft,
				     g_clipYBottom - g_clipYTop,
				     shade);
}

/*
DRAWPOINTCLIPPED

Sets a (clipped) pixel in the display buffer to a specified colour.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the pixel
 yPos - the y-coordinate of the pixel
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawPointClipped(char *buffer, int xPos, int yPos, char shade) {

  if (xPos >= g_clipXLeft && xPos < g_clipXRight &&
      yPos >= g_clipYTop  && yPos < g_clipYBottom) {

    vfdlib_drawPointUnclipped(buffer, xPos, yPos, shade);
  }
}

/*
DRAWPOINTUNCLIPPED

Sets a pixel in the display buffer to a specified colour.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the pixel
 yPos - the y-coordinate of the pixel
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawPointUnclipped(char *buffer, int xPos, int yPos, char shade) {

  int byteOffset = (yPos << 6) + (xPos >> 1);
  if (xPos & 0x1) { /* RHS pixel */
    buffer[byteOffset] &= MASK_LO_NYBBLE;
    buffer[byteOffset] |= (shade << 4);
  } else { /* LHS pixel */
    buffer[byteOffset] &= MASK_HI_NYBBLE;
    buffer[byteOffset] |= shade;
  }
}

/*
DRAWLINEHORIZCLIPPED

Draws a (clipped) horizontal line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 yPos - the y-coordinate of the line
 xLeft - the x-coordinate of the left hand side of the line
 xRight - the x-coordinate of the right hand side of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawLineHorizClipped(char *buffer, int yPos, int xLeft, int xRight,
				 char shade) {

  if (xLeft < xRight &&
      xRight >= g_clipXLeft && xLeft < g_clipXRight &&
      yPos >= g_clipYTop  && yPos < g_clipYBottom) {

    int left = xLeft > g_clipXLeft ? xLeft : g_clipXLeft;
    vfdlib_drawLineHorizUnclipped(buffer, yPos, left,
				  (xRight < g_clipXRight ?
				   xRight : g_clipXRight) - left,
				  shade);
  }
}

/*
DRAWLINEHORIZUNCLIPPED

Draws a horizontal line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 yPos - the y-coordinate of the line
 xLeft - the x-coordinate of the left hand side of the line
 length - the width of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawLineHorizUnclipped(char *buffer, int yPos, int xLeft,
				   int length, char shade) {

  char doubleShade = shade | (shade << 4);
  
  if (length < 1) return;
  
  buffer += (yPos << 6) + (xLeft >> 1);

  /* do left */
  if (xLeft & 0x1) {
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= (shade << 4);
    length--;
    buffer++;
  }

  /* do middle */
  while (length > 1) {
    *(buffer++) = doubleShade;
    length -= 2;
  }

  /* do right */
  if (length > 0) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  }
}

/*
DRAWLINEVERTCLIPPED

Draws a (clipped) vertical line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the line
 yTop - the y-coordinate of the top of the line
 yBottom - the y-coordinate of the bottom of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawLineVertClipped(char *buffer, int xPos, int yTop, int yBottom,
				char shade) {

  if (yTop < yBottom &&
      xPos >= g_clipXLeft && xPos < g_clipXRight &&
      yBottom >= g_clipYTop  && yTop < g_clipYBottom) {

    int top = yTop > g_clipYTop ? yTop : g_clipYTop;
    vfdlib_drawLineVertUnclipped(buffer, xPos,
				 top,
				 (yBottom < g_clipYBottom ?
				  yBottom : g_clipYBottom) - top,
				 shade);
  }
}

/*
DRAWLINEVERTUNCLIPPED

Draws a vertical line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the line
 yTop - the y-coordinate of the top of the line
 length - the height of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawLineVertUnclipped(char *buffer, int xPos, int yTop, int length,
				  char shade) {

  if (length < 1) return;

  buffer += (yTop << 6) + (xPos >> 1);

  if (xPos & 0x1) { /* LHS pixel */
    while (length-- > 0) {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= (shade << 4);
      buffer += VFD_BYTES_PER_SCANLINE;
    }
  } else { /* RHS pixel */
    while (length-- > 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
      buffer += VFD_BYTES_PER_SCANLINE;
    }
  }
}

/*
DRAWLINECLIPPED

Draws a (clipped) line between two points in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 x0 - the x-coordinate of the first endpoint
 y0 - the y-coordinate of the first endpoint
 x1 - the x-coordinate of the second endpoint
 y1 - the y-coordinate of the second endpoint
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawLineClipped(char *buffer, int x0, int y0, int x1, int y1,
			    char shade) {

  int dx = x1 - x0;
  int dy = y1 - y0;
  int distToHEntry, distToVEntry, distToHExit, distToVExit, xDelta, yDelta;
  int dyAbs, xMax, yMax;

  /* make sure x is positive */
  if (dx < 0) {
    /* swap points */ 
    x0 = x1;
    y0 = y1;
    x1 -= dx;
    y1 -= dy;
    dx = -dx;
    dy = -dy;
  }
  distToHEntry = g_clipXLeft - x0;
  if (dx < distToHEntry) return;
  
  if (dy > 0) {
    distToVEntry = g_clipYTop - y0;
    if (dy < distToVEntry) return;
    distToVExit = g_clipYBottom - y0 - 1;
    dyAbs = dy;
  } else {
    distToVEntry = y0 - g_clipYBottom + 1;
    if (-dy < distToVEntry) return;
    distToVExit = y0 - g_clipYTop;
    dyAbs = -dy;
  }
  if (distToVExit < 0) return;
  distToHExit = g_clipXRight - x0 - 1;
  if (distToHExit < 0) return;

  xDelta = 0; yDelta = 0;

  if (dx > dyAbs) { /* hmax line */
    if (distToHEntry > 0) {
      /* intersect LH edge */ 
      int temp = distToHEntry * dyAbs;
      yDelta += temp / dx;
      if (temp % dx > (dx >> 1)) yDelta++; /* round up */
      xDelta += distToHEntry;
      if (distToVExit - yDelta < 0) return;
    }
    if (distToVEntry - yDelta > 0) {
      /* intersect top/bottom - step back 1/2 a pixel */
      xDelta = (((distToVEntry << 1) - 1) * dx) / (dyAbs << 1) + 1;
      yDelta = distToVEntry;

      /* reject if intersection past RH edge */
      if (xDelta > distToHExit) return;
    }
    if (dy > 0) y0 += yDelta;
    else y0 -= yDelta;

    distToHExit -= xDelta;
    xMax = dx - xDelta;
    if (distToHExit < xMax) xMax = distToHExit;

    distToVExit -= yDelta;    
    yMax = dyAbs - yDelta;
    if (distToVExit < yMax) yMax = distToVExit;

    drawLineHMaxClipped(buffer, x0 + xDelta, y0, dx, dy,
			((2 + (xDelta << 1)) * dyAbs) -
			((1 + (yDelta << 1)) * dx),
			xMax, yMax, shade);
  } else { /* vmax line */
    if (distToVEntry > 0) {
      /* intersect top/bottom */
      int temp = distToVEntry * dx;
      xDelta += temp / dyAbs;
      if (temp % dyAbs > (dyAbs >> 1)) xDelta++; /* round up */
      yDelta += distToVEntry;
      if (distToHExit - xDelta < 0) return;
    }
    if (distToHEntry - xDelta > 0) {
      /* intersect LH edge - step back 1/2 a pixel */
      yDelta = ((distToHEntry << 1) - 1) * dyAbs / (dx << 1) + 1;
      xDelta = distToHEntry;

      /* reject if intersection not on left edge */
      if (yDelta > distToVExit) return;
    }
    if (dy > 0) y0 += yDelta;
    else y0 -= yDelta;

    distToHExit -= xDelta;
    xMax = dx - xDelta;
    if (distToHExit < xMax) xMax = distToHExit;

    distToVExit -= yDelta;    
    yMax = dyAbs - yDelta;
    if (distToVExit < yMax) yMax = distToVExit;

    drawLineVMaxClipped(buffer, x0 + xDelta, y0, dx, dy,
			((2 + (yDelta << 1)) * dx) -
			((1 + (xDelta << 1)) * dyAbs),
			xMax, yMax, shade);
  }
}

/*
DRAWLINEUNCLIPPED

Draws a line between two points in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 x0 - the x-coordinate of the first endpoint
 y0 - the y-coordinate of the first endpoint
 x1 - the x-coordinate of the second endpoint
 y1 - the y-coordinate of the second endpoint
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawLineUnclipped(char *buffer, int x0, int y0, int x1, int y1,
			      char shade) {

  int dx = x1 - x0;
  int dy = y1 - y0;

  if (dx == 0) {
    /* vertical line */
    if (dy >= 0)
      vfdlib_drawLineVertUnclipped(buffer, x0, y0, dy+1, shade);
    else vfdlib_drawLineVertUnclipped(buffer, x0, y1, -dy+1, shade);
    return;
  }
  if (dy == 0) {
    /* horizontal line */
    if (dx >= 0)
      vfdlib_drawLineHorizUnclipped(buffer, y0, x0, dx+1, shade);
    else vfdlib_drawLineHorizUnclipped(buffer, y0, x1, -dx+1, shade);
    return;
  }
  if (dx < 0) {
    /* swap points */
    x0 = x1;
    y0 = y1;
    dy = -dy;
    dx = -dx;
  }
  if (dy < 0) {
    if (dx > -dy) drawLineHMaxUnclipped(buffer, x0, y0, dx, dy, shade);
    else drawLineVMaxUnclipped(buffer, x0, y0, dx, dy, shade);
  } else {
    if (dx > dy) drawLineHMaxUnclipped(buffer, x0, y0, dx, dy, shade);
    else drawLineVMaxUnclipped(buffer, x0, y0, dx, dy, shade);
  }
}

/*
DRAWOUTLINERECTANGLECLIPPED

Draws the (clipped) outline of a rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xRight - the x-coordinate of the righthand side of the rectangle
 yBottom - the y-coordinate of the bottom of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawOutlineRectangleClipped(char *buffer, int xLeft, int yTop,
					int xRight, int yBottom, char shade) {

  vfdlib_drawLineHorizClipped(buffer, yTop, xLeft, xRight, shade);/* top */
  vfdlib_drawLineVertClipped(buffer, xLeft, yTop, yBottom, shade);/* left */
  vfdlib_drawLineHorizClipped(buffer, yBottom-1, xLeft, xRight,
			      shade);/* bottom */
  vfdlib_drawLineVertClipped(buffer, xRight-1, yTop, yBottom,
			     shade);/* right */  
}

/*
DRAWOUTLINERECTANGLEUNCLIPPED

Draws the outline of a rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xWidth - the width of the rectangle
 yHeight - the height of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawOutlineRectangleUnclipped(char *buffer, int xLeft, int yTop,
					  int xWidth, int yHeight, char shade)
{

  vfdlib_drawLineHorizUnclipped(buffer, yTop, xLeft, xWidth, shade);/* top */
  vfdlib_drawLineVertUnclipped(buffer, xLeft, yTop, yHeight, shade);/* left */
  vfdlib_drawLineHorizUnclipped(buffer, yTop+yHeight-1, xLeft, xWidth,
				shade);/* bottom */
  vfdlib_drawLineVertUnclipped(buffer, xLeft+xWidth-1, yTop, yHeight,
			       shade);/* right */  
}

/*
DRAWSOLIDRECTANGLECLIPPED

Draws a (clipped) solid rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xRight - the x-coordinate of the righthand side of the rectangle
 yBottom - the y-coordinate of the bottom of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawSolidRectangleClipped(char *buffer, int xLeft, int yTop,
				      int xRight, int yBottom, char shade) {

  int width, height;
  if (xLeft < g_clipXLeft) xLeft = g_clipXLeft;
  if (yTop < g_clipYTop) yTop = g_clipYTop;
  if (xRight > g_clipXRight) xRight = g_clipXRight;
  if (yBottom > g_clipYBottom) yBottom = g_clipYBottom;
  width = xRight - xLeft;
  height = yBottom - yTop;
  if (width < 1 || height < 1) return;
  vfdlib_drawSolidRectangleUnclipped(buffer, xLeft, yTop, width, height,
				     shade);
}

/*
DRAWSOLIDRECTANGLEUNCLIPPED

Draws a solid rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xWidth - the width of the rectangle
 yHeight - the height of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawSolidRectangleUnclipped(char *buffer, int xLeft, int yTop,
					int xWidth, int yHeight, char shade) {

  int widthRemaining;
  char doubleShade = shade | (shade << 4);
  char *lineStart;
  lineStart = buffer + (yTop << 6) + (xLeft >> 1);

  while (yHeight-- > 0) {

    buffer = lineStart;
    widthRemaining = xWidth;

    /* do left */
    if (xLeft & 0x1) {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= (shade << 4);
      widthRemaining--;
      buffer++;
    }
    
    /* do middle */
    while (widthRemaining > 1) {
      *(buffer++) = doubleShade;
      widthRemaining -= 2;
    }
    
    /* do right */
    if (widthRemaining > 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
	  buffer++;
    }

    lineStart += VFD_BYTES_PER_SCANLINE;
  }
}

/*
INVERTRECTANGLECLIPPED

Inverts a (clipped) solid rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xRight - the x-coordinate of the righthand side of the rectangle
 yBottom - the y-coordinate of the bottom of the rectangle

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_invertRectangleClipped(char *buffer, int xLeft, int yTop,
				   int xRight, int yBottom)
{

  int width, height;
  if (xLeft < g_clipXLeft) xLeft = g_clipXLeft;
  if (yTop < g_clipYTop) yTop = g_clipYTop;
  if (xRight > g_clipXRight) xRight = g_clipXRight;
  if (yBottom > g_clipYBottom) yBottom = g_clipYBottom;
  width = xRight - xLeft;
  height = yBottom - yTop;
  if (width < 1 || height < 1) return;
  vfdlib_invertRectangleUnclipped(buffer, xLeft, yTop, width, height);
}

/*
INVERTRECTANGLEUNCLIPPED

Inverts a solid rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xWidth - the width of the rectangle
 yHeight - the height of the rectangle

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_invertRectangleUnclipped(char *buffer, int xLeft, int yTop,
				     int xWidth, int yHeight)
{

  int widthRemaining, pixelPos;
  char *lineStart, oldShade, invertShade;
  static char invertTable[] = {VFDSHADE_BRIGHT, VFDSHADE_MEDIUM,
			       VFDSHADE_DIM, VFDSHADE_BLACK};
  lineStart = buffer + (yTop << 6) + (xLeft >> 1);
  
  while (yHeight-- > 0) {
    
    buffer = lineStart;
    widthRemaining = xWidth;
    pixelPos = xLeft & 0x1;

    while (widthRemaining > 0) {
      if (pixelPos == 0) {
	oldShade = *buffer & MASK_LO_NYBBLE;

	/* determine inverted colour */
	invertShade = invertTable[oldShade];

	*buffer &= MASK_HI_NYBBLE;
	*buffer |= invertShade;
      } else {
	oldShade = (*buffer & MASK_HI_NYBBLE) >> 4;
	
	/* determine inverted colour */
	invertShade = invertTable[oldShade];
	
	*buffer &= MASK_LO_NYBBLE;
	*buffer |= invertShade << 4;
      }
        
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }      
      widthRemaining--;
    }
    lineStart += VFD_BYTES_PER_SCANLINE;
  }
}

/*
DRAWOUTLINEELLIPSECLIPPED

Draws the (clipped) outline of an ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawOutlineEllipseClipped(char *buffer, int cX, int cY,
				      int a, int b, char shade) {

  int d2, deltaS, x, y, a_2, b_2, a2_2, b2_2, a4_2, b4_2;
  int a8_2, b8_2, a8_2plusb8_2, d1, deltaE, deltaSE;
  
  if ((cX - a) > g_clipXLeft && (cX + a) < g_clipXRight &&
      (cY - b) > g_clipYTop && (cY + b) < g_clipYBottom) {

    vfdlib_drawOutlineEllipseUnclipped(buffer, cX, cY, a, b, shade);
    return;
  }

  x=0;
  y=b;
  
  /* precalculate values for speed */
  a_2 = a * a;
  b_2 = b * b;
  a2_2 = a_2 << 1;
  b2_2 = b_2 << 1;
  a4_2 = a2_2 << 1;
  b4_2 = b2_2 << 1;
  a8_2 = a4_2 << 1;
  b8_2 = b4_2 << 1;
  a8_2plusb8_2 = a8_2 + b8_2;
  
  /* region 1 */
  d1 = b4_2 - (a4_2 * b) + a_2;
  deltaE = 12 * b_2;
  deltaSE = deltaE - a8_2 * b + a8_2;
  
  drawPixel4WaySymmetricClipped(buffer, cX, cY, x, y, shade);
  
  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      y--;
    }
    x++;
    drawPixel4WaySymmetricClipped(buffer, cX, cY, x, y, shade);
  }
  
  /* region 2 */
  d2 =  (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) -
    (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
  
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    y--;
    drawPixel4WaySymmetricClipped(buffer, cX, cY, x, y, shade);
  }
}

/*
DRAWOUTLINEELLIPSEUNCLIPPED

Draws the outline of an ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawOutlineEllipseUnclipped(char *buffer, int cX, int cY,
					int a, int b, char shade) {

  int d2, deltaS;
  
  int x=0;
  int y=b;
  
  /* precalculate values for speed */
  int a_2 = a * a;
  int b_2 = b * b;
  int a2_2 = a_2 << 1;
  int b2_2 = b_2 << 1;
  int a4_2 = a2_2 << 1;
  int b4_2 = b2_2 << 1;
  int a8_2 = a4_2 << 1;
  int b8_2 = b4_2 << 1;
  int a8_2plusb8_2 = a8_2 + b8_2;
  
  /* region 1 */
  int d1 = b4_2 - (a4_2 * b) + a_2;
  int deltaE = 12 * b_2;
  int deltaSE = deltaE - a8_2 * b + a8_2;
  
  drawPixel4WaySymmetricUnclipped(buffer, cX, cY, x, y, shade);
  
  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      y--;
    }
    x++;
    drawPixel4WaySymmetricUnclipped(buffer, cX, cY, x, y, shade);
  }
  
  /* region 2 */
  d2 =  (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) - (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
  
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    y--;
    drawPixel4WaySymmetricUnclipped(buffer, cX, cY, x, y, shade);
  }
}

/*
DRAWSOLIDELLIPSECLIPPED

Draws a (clipped) solid ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawSolidEllipseClipped(char *buffer, int cX, int cY,
				    int a, int b, char shade) {

  int d2, deltaS, x, y, a_2, b_2, a2_2, b2_2, a4_2, b4_2;
  int a8_2, b8_2, a8_2plusb8_2, d1, deltaE, deltaSE;
  
  if ((cX - a) > g_clipXLeft && (cX + a) < g_clipXRight &&
      (cY - b) > g_clipYTop && (cY + b) < g_clipYBottom) {

    vfdlib_drawSolidEllipseUnclipped(buffer, cX, cY, a, b, shade);
    return;
  }

  x=0;
  y=b;

  /* precalculate values for speed */
  a_2 = a * a;
  b_2 = b * b;
  a2_2 = a_2 << 1;
  b2_2 = b_2 << 1;
  a4_2 = a2_2 << 1;
  b4_2 = b2_2 << 1;
  a8_2 = a4_2 << 1;
  b8_2 = b4_2 << 1;
  a8_2plusb8_2 = a8_2 + b8_2;
  
  /* region 1 */
  d1 = b4_2 - (a4_2 * b) + a_2;
  deltaE = 12 * b_2;
  deltaSE = deltaE - a8_2 * b + a8_2;

  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      vfdlib_drawLineHorizClipped(buffer, cY + y, cX - x, cX + x, shade);
      vfdlib_drawLineHorizClipped(buffer, cY - y, cX - x, cX + x, shade);
      y--;
    }
    x++;
  }

  /* region 2 */
  d2 = (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) - (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
	
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    vfdlib_drawLineHorizClipped(buffer, cY + y, cX - x, cX + x, shade);
    vfdlib_drawLineHorizClipped(buffer, cY - y, cX - x, cX + x, shade);
    y--;
  }
  vfdlib_drawLineHorizClipped(buffer, cY, cX - x, cX + x, shade);
}

/*
DRAWSOLIDELLIPSEUNCLIPPED

Draws a solid ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawSolidEllipseUnclipped(char *buffer, int cX, int cY,
				      int a, int b, char shade) {

  int d2, deltaS;

  int x=0;
  int y=b;

  /* precalculate values for speed */
  int a_2 = a * a;
  int b_2 = b * b;
  int a2_2 = a_2 << 1;
  int b2_2 = b_2 << 1;
  int a4_2 = a2_2 << 1;
  int b4_2 = b2_2 << 1;
  int a8_2 = a4_2 << 1;
  int b8_2 = b4_2 << 1;
  int a8_2plusb8_2 = a8_2 + b8_2;

  /* region 1 */
  int d1 = b4_2 - (a4_2 * b) + a_2;
  int deltaE = 12 * b_2;
  int deltaSE = deltaE - a8_2 * b + a8_2;

  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      vfdlib_drawLineHorizUnclipped(buffer, cY + y, cX - x, x << 1, shade);
      vfdlib_drawLineHorizUnclipped(buffer, cY - y, cX - x, x << 1, shade);
      y--;
    }
    x++;
  }

  /* region 2 */
  d2 =  (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) - (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
	
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    vfdlib_drawLineHorizUnclipped(buffer, cY + y, cX - x, x << 1, shade);
    vfdlib_drawLineHorizUnclipped(buffer, cY - y, cX - x, x << 1, shade);
    y--;
  }
  vfdlib_drawLineHorizUnclipped(buffer, cY, cX - x, x << 1, shade);
}

/*
DRAWOUTLINEPOLYGONCLIPPED

Draws the (clipped) outline of a polygon in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 coordsData - pointer to an array of numPoints pairs of integers;
              x0, y0, x1, y1...
 numPoints - the number of vertices in the polygon
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Polygons are closed automatically - the first point is drawn connected to
 the last point.

*/
void vfdlib_drawOutlinePolygonClipped(char *buffer, int *coordsData,
				      int numPoints, char shade) {

  int x0, y0, x1, y1;
  int lastIndex = (numPoints-1) << 1; 
  int currIndex;
  int prevX = coordsData[lastIndex];
  int prevY = coordsData[lastIndex+1];

  for (currIndex = 0; currIndex<=lastIndex;) {
    x0 = prevX;
    y0 = prevY;
    prevX = x1 = coordsData[currIndex++];
    prevY = y1 = coordsData[currIndex++];
    vfdlib_drawLineClipped(buffer, x0, y0, x1, y1, shade);
  }
}

/*
DRAWOUTLINEPOLYGONUNCLIPPED

Draws the outline of a polygon in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 coordsData - pointer to an array of numPoints pairs of integers;
              x0, y0, x1, y1...
 numPoints - the number of vertices in the polygon
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.
 Polygons are closed automatically - the first point is drawn connected to
 the last point.

*/
void vfdlib_drawOutlinePolygonUnclipped(char *buffer, int *coordsData,
					int numPoints, char shade) {

  int x0, y0, x1, y1;
  int lastIndex = (numPoints-1) << 1; 
  int currIndex;
  int prevX = coordsData[lastIndex];
  int prevY = coordsData[lastIndex+1];

  for (currIndex = 0; currIndex<=lastIndex;) {
    x0 = prevX;
    y0 = prevY;
    prevX = x1 = coordsData[currIndex++];
    prevY = y1 = coordsData[currIndex++];
    vfdlib_drawLineUnclipped(buffer, x0, y0, x1, y1, shade);
  }
}

/*
DRAWSOLIDPOLYGON

Draws a (clipped) solid polygon in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 coordsData - pointer to an array of numPoints pairs of integers;
              x0, y0, x1, y1...
 numPoints - the number of vertices in the polygon
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Polygons are closed automatically - the first point is drawn connected to
 the last point.

*/
void vfdlib_drawSolidPolygon(char *buffer, int *coordsData, int numPoints,
			     char shade) {

  struct sETentry {
    int ymax;
    int x;
    int invm_numerator;
    int invm_denominator;
    int increment;
    struct sETentry *next;
  };
  typedef struct sETentry ETentry;
  static ETentry *edgeTable[VFD_HEIGHT];
  int i, y, x0, y0, x1, y1, tx, ty, sorted;
  int lastIndex = (numPoints-1) << 1; 
  int currIndex;
  int prevX = coordsData[lastIndex];
  int prevY = coordsData[lastIndex+1];
  int polyMinY = g_clipYBottom;
  int polyMaxY = g_clipYTop;
  ETentry *prev, *curr, *next, *newEdge, *dead, *AEtable;
	
  /* sanity check */
  if (numPoints < 3) return;

  /* clear the edge table */
  for (i=0; i<VFD_HEIGHT; i++) edgeTable[i] = NULL;

  /* add edges to edge table */
  for (currIndex = 0; currIndex<=lastIndex;) {

    /* get endpoints of edge */
    x0 = prevX;
    y0 = prevY;
    prevX = x1 = coordsData[currIndex++];
    prevY = y1 = coordsData[currIndex++];

    /* add a new entry to the edge table, clipping if necessary */
    if (y0 == y1) continue; /* skip horizontal spans */
    
    /* create new entry for table */
    newEdge = (ETentry *) malloc(sizeof(ETentry));
    if (y0 > y1) {
      /* swap points so first coord is topmost */
      tx = x0; ty = y0;
      x0 = x1; y0 = y1;
      x1 = tx; y1 = ty;
    }
    
    /* do clipping - only needed for top parts of lines */
    if (y1 < g_clipYTop || y0 >= g_clipYBottom) continue;

    newEdge->invm_numerator = x1 - x0;
    newEdge->invm_denominator = y1 - y0;
    
    if (y0 < g_clipYTop) {
      ty = g_clipYTop - y0;
      /* jump start by ty */
      if (newEdge->invm_numerator < 0) {
	newEdge->increment =
	  (ty * -newEdge->invm_numerator) % newEdge->invm_denominator;
	newEdge->x = x0 -
	   (ty * -newEdge->invm_numerator) / newEdge->invm_denominator;
      }
      else { 
	newEdge->increment =
	   (ty * newEdge->invm_numerator) % newEdge->invm_denominator;
	newEdge->x = x0 +
	   (ty * newEdge->invm_numerator) / newEdge->invm_denominator;
      }
      y0 = g_clipYTop;
    }
    else {
      newEdge->increment = 0;
      newEdge->x = x0;
    }
    newEdge->ymax = y1;
    /* add entry to head of linked list */
    newEdge->next = edgeTable[y0];
    edgeTable[y0] = newEdge;
    
    /* update polyMinY & polyMaxY */
    if (y0 < polyMinY) polyMinY = y0;
    if (y1 > polyMaxY) polyMaxY = y1;
  }

  AEtable = NULL;
  if (polyMaxY > g_clipYBottom-1) polyMaxY = g_clipYBottom-1;

  /* scan conversion loop */
  for (y = polyMinY; y <= polyMaxY; y++) {
    /* add new edges to active edge table */
    newEdge = edgeTable[y];
    while (newEdge != NULL) {
      ETentry *nextNewEdge = newEdge->next;
      /* insert new edge into AET */
      prev = NULL;
      curr = AEtable;
      /* find x-sorted insertion point */
      while (curr != NULL && curr->x < newEdge->x) {
	prev = curr;
	curr = curr->next;
      }
      if (prev == NULL) {
	/* add to head of list */
	AEtable = newEdge;
      } else {
	prev->next = newEdge;
      }
      newEdge->next = curr;
      newEdge = nextNewEdge;
    }
    
    /* delete ETentrys as they expire */
    prev = NULL;
    curr = AEtable;
    while (curr != NULL) {
      if (curr->ymax == y) {
	if (prev == NULL) {
	  /* delete from head */
	  AEtable = curr->next;
	  dead = curr;
	  prev = NULL;
	  curr = curr->next;
	  free(dead);
	} else {
	  dead = curr;
	  prev->next = curr->next;
	  curr = curr->next;
	  free(dead);
	}
      } else {
	prev = curr;
	curr = curr->next;
      }
    }
    
    /* draw scanlines */
    curr = AEtable;
    while (curr != NULL) {
      if (curr->next != NULL) {
	/* draw scissored scanline */
	tx = curr->x > g_clipXLeft ? curr->x : g_clipXLeft;
	vfdlib_drawLineHorizUnclipped(buffer, y,
				      tx,
				      (curr->next->x < g_clipXRight-1 ?
				       curr->next->x : g_clipXRight-1) -
				      tx + 1,
				      shade);
	curr = curr->next->next;
      } else curr = NULL;
    }
    
    /* update x values */
    curr = AEtable;
    while (curr != NULL) {
      if (curr->invm_numerator == 0) ; /* vertical - do nothing */
      else if (curr->invm_numerator > 0) {
	if (curr->invm_numerator < curr->invm_denominator) {
	  /* high slope right */
	  curr->increment += curr->invm_numerator;
	  if (curr->increment >= curr->invm_denominator) {
	    curr->x++;
	    curr->increment -= curr->invm_denominator;
	  }
	} else {
	  /* low slope right */
	  curr->increment += curr->invm_numerator;
	  tx = curr->increment / curr->invm_denominator;
	  curr->x += tx;
	  curr->increment -= curr->invm_denominator * tx;
	}
      } else {
	if (-curr->invm_numerator < curr->invm_denominator) {
	  /* high slope left */
	  curr->increment -= curr->invm_numerator;
	  if (curr->increment >= curr->invm_denominator) {
	    curr->x--;
	    curr->increment -= curr->invm_denominator;
	  }
	} else {
	  /* low slope left */
	  curr->increment -= curr->invm_numerator;
	  tx = curr->increment / curr->invm_denominator;
	  curr->x -= tx;
	  curr->increment -= curr->invm_denominator * tx;
	}
      }
      curr = curr->next;
    }
    
    /* bubble sort active edge table */
    sorted = 0;
    while (!sorted) {
      sorted = 1;
      prev = NULL;
      curr = AEtable;
      while (curr != NULL) {
	if (curr->next != NULL && curr->x > curr->next->x) {
	  /* found unsorted */
	  sorted = 0;
	  /* swap the entries */
	  if (prev == NULL) {
	    /* beginning of list */
	    prev = curr->next;
	    next = curr->next->next;
	    AEtable = curr->next;
	    curr->next->next = curr;
	    curr->next = next;
	  } else {
	    ETentry *newPrev = curr->next;
	    next = curr->next->next;
	    prev->next = curr->next;
	    curr->next->next = curr;
	    curr->next = next;
	    prev = newPrev;
	  }
	} else {
	  prev = curr;
	  curr = curr->next;
	}
      }
    }
  }
  
  /* delete any edges left in active edge table */
  curr = AEtable;
  while (curr != NULL) {
    dead = curr;
    curr = curr->next;
    free(dead);
  }
}

/*
REGISTERFONT

Loads a font into one of the 5 available slots so that it may be used
to draw text.

Parameters:
 bfFileName - the name of a file in the same format as used by the player
              software
 fontSlot - the slot number (0-4) to load the font into

Returns:
  0 on success
 -1 if the file could not be read successfully
 -2 if the font slot is not valid

Notes:
 It is safe to register over the top of another font, as the previously
 registered font will be unregistered first.

*/
int vfdlib_registerFont(char *bfFileName, int fontSlot) {

  static struct {
    char identifierString[4]; /* always reads 'EFNT' */
    int fileSize;
    int unknown1;             /* always == 1. version perhaps? */
    int maxWidth;
    int unknown2;             /* always == 32. bits per scanline perhaps? */
    int height;
    int firstIndex;
    int numOfCharacters;
  } bfHeader;

  int i, x, y, width, scanline, totalWidth = 0;
  int bitmapScanlineBytes, scanLineSkip, fBitmapSize;
  FILE *fp;
  unsigned char *fBitmap;
  CharInfo *cInfo;

  if (fontSlot < 0 || fontSlot >= NUM_FONT_SLOTS) return -2; /* invalid slot */

  /* unregister any existing font information */
  vfdlib_unregisterFont(fontSlot);

  if ((fp = fopen(bfFileName,"rb")) == NULL) {
    /* unable to open the file */ 
    return -1;
  }

  if (fread(&bfHeader, 1, sizeof(bfHeader), fp) < 1) {
    /* error reading header */
    fclose(fp);
    return -1;
  }

  /* check header for valid file */
  /* is correct identifier? */
  if (bfHeader.identifierString[0] != 'E' ||
      bfHeader.identifierString[1] != 'F' ||
      bfHeader.identifierString[2] != 'N' ||
      bfHeader.identifierString[3] != 'T') {

    /* file not recognised */
    fclose(fp);
    return -1;
  }
  
  /* allocate memory for charInfo */
  cInfo = (CharInfo *) malloc(sizeof(CharInfo) * bfHeader.numOfCharacters);

  for (i=0; i<bfHeader.numOfCharacters; i++) {
    fseek(fp, sizeof(bfHeader) + (4 * (bfHeader.height+1) * i), SEEK_SET);
    if (fread(&width, 1, sizeof(width), fp) < 1) {
      /* error reading width */
      free(cInfo);
      fclose(fp);
      return -1;
    }
    cInfo[i].offset = totalWidth;
    cInfo[i].width = width;
    totalWidth += width;
  }

  /* allocate memory for fontBitmap */
  bitmapScanlineBytes = ((totalWidth-1) >> 2) + 1;
  fBitmapSize = 5 + (bitmapScanlineBytes * bfHeader.height);
  fBitmap = (unsigned char *) malloc(fBitmapSize);
  for (i=0; i<fBitmapSize; i++) fBitmap[i] = 0; /* clear memory */

  fBitmap[0] = BITMAP_2BPP;
  fBitmap[1] = totalWidth >> 8;
  fBitmap[2] = totalWidth & 0xFF;
  fBitmap[3] = bfHeader.height >> 8;
  fBitmap[4] = bfHeader.height & 0xFF;

  /* load in the character bitmaps */
  for (i=0; i<bfHeader.numOfCharacters; i++) {
    fseek(fp, sizeof(bfHeader) + (((bfHeader.height+1) << 2) * i) + 4,
	  SEEK_SET);
    scanLineSkip = 0;
    for (y=0; y<bfHeader.height; y++) {
      if (fread(&scanline, 1, sizeof(scanline), fp) < 1) {
	/* error reading scanline */
	free(cInfo);
	free(fBitmap);
	fclose(fp);
	return -1;
      }
      for (x=0; x<cInfo[i].width; x++) {
	/* convert scanline to internal format */
	*(fBitmap + 5 + scanLineSkip + ((cInfo[i].offset + x) >> 2)) |=
	  (((unsigned char) scanline & 0x03) <<
	   (((cInfo[i].offset + x) & 0x03) << 1));
	scanline >>= 2;
      }
      scanLineSkip += bitmapScanlineBytes;
    }
  }

  /* add font to registry */
  g_fontRegistry[fontSlot].cInfo = cInfo;
  g_fontRegistry[fontSlot].fBitmap = fBitmap;
  g_fontRegistry[fontSlot].firstIndex = bfHeader.firstIndex;  
  g_fontRegistry[fontSlot].numOfChars = bfHeader.numOfCharacters;
  
  fclose(fp);
  return 0; /* success */
}

/*
UNREGISTERFONT

Frees up resources for a font that is no longer needed.

Parameters:
 fontSlot - the slot number of a font that has been registered 

Notes:
 There are no adverse effects if you try to unregister a slot that has not
 been registered.

*/
void vfdlib_unregisterFont(int fontSlot) {

  if (fontSlot < 0 || fontSlot >= NUM_FONT_SLOTS) return; /* invalid slot */
  if (g_fontRegistry[fontSlot].cInfo != NULL) {
    free(g_fontRegistry[fontSlot].cInfo);
    g_fontRegistry[fontSlot].cInfo = NULL;
  }
  if (g_fontRegistry[fontSlot].fBitmap != NULL) {
    free(g_fontRegistry[fontSlot].fBitmap);
    g_fontRegistry[fontSlot].fBitmap = NULL;
  }
  g_fontRegistry[fontSlot].firstIndex =
    g_fontRegistry[fontSlot].numOfChars = 0; 
}

/*
UNREGISTERALLFONTS

Frees up resources used by all of the fonts.

Notes:
 It is advisable to call this function before your program exits.

*/
void vfdlib_unregisterAllFonts() {

  int i;
  for (i=0; i<NUM_FONT_SLOTS; i++) vfdlib_unregisterFont(i);
}

/*
DRAWTEXT

Draws a (clipped) string of text in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 string - the string of text to display
 xLeft - the x left-hand position of the string
 yTop - the y topmost position of the string
 fontSlot - the slot number of a font that has been registered
 shade - the shade to draw with, or -1 to retain original shades

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

 Use the get functions to determine the pixel size required for a string.

*/
void vfdlib_drawText(char *buffer, char *string, int xLeft, int yTop,
		     int fontSlot, char shade) {

  int charIndex;

  /* check for valid slot */
  if (fontSlot < 0 || fontSlot >= NUM_FONT_SLOTS ||
      g_fontRegistry[fontSlot].cInfo == NULL)
    return;

  while (*string != '\0' && xLeft < g_clipXRight) {
    charIndex = ((unsigned char)*string)-g_fontRegistry[fontSlot].firstIndex;
    if (charIndex >= 0 && charIndex < g_fontRegistry[fontSlot].numOfChars) {
      CharInfo ci = g_fontRegistry[fontSlot].cInfo[charIndex];
      if (ci.width > 0) { 
	vfdlib_drawBitmap(buffer, g_fontRegistry[fontSlot].fBitmap,
			  xLeft, yTop, ci.offset, 0,
			  ci.width, -1, shade, 1);
      }
      xLeft += ci.width;
    }
    string++;
  }
}

/*
GETTEXTHEIGHT

Gets the pixel height of a string.

Parameters:
 fontSlot - the slot number of a font that has been registered

*/
int vfdlib_getTextHeight(int fontSlot) {

  unsigned char *fBitmap;

  /* check for valid slot */
  if (fontSlot < 0 || fontSlot >= NUM_FONT_SLOTS ||
      g_fontRegistry[fontSlot].cInfo == NULL)
    return 0;

  fBitmap = g_fontRegistry[fontSlot].fBitmap;

  return ((int) fBitmap[4] | ((int) fBitmap[3] << 8));
}

/*
GETTEXTWIDTH

Gets the pixel width of a string.

Parameters:
 string - the string of text to be displayed
 fontSlot - the slot number of a font that has been registered 

*/
int vfdlib_getTextWidth(char *string, int fontSlot) {

  int charIndex, width = 0;

  /* check for valid slot */
  if (fontSlot < 0 || fontSlot >= NUM_FONT_SLOTS ||
      g_fontRegistry[fontSlot].cInfo == NULL)
    return 0;

  while (*string != '\0') {
    charIndex = ((unsigned char)*string)-g_fontRegistry[fontSlot].firstIndex;
    if (charIndex >= 0 && charIndex < g_fontRegistry[fontSlot].numOfChars) {
      CharInfo ci = g_fontRegistry[fontSlot].cInfo[charIndex];
      width += ci.width;
    }
    string++;
  }

  return width;
}

/*
GETSTRINGINDEXOFCUTOFFWIDTH

Gets the index of the character within a string that will be cut off if
the space available for displaying the string were only _width_ pixels.

Parameters:
 string - the string of text to be displayed
 fontSlot - the slot number of a font that has been registered
 width - the available horizontal space for displaying the string

Returns:
 The index of the character that will be cut off. If the string will not be
 cut it will be the index of the termination character (\0).

*/
int vfdlib_getStringIndexOfCutoffWidth(char *string, int fontSlot, int width) {

  int charIndex, currWidth = 0, index = 0;
  
  /* check for valid slot */
  if (fontSlot < 0 || fontSlot >= NUM_FONT_SLOTS ||
      g_fontRegistry[fontSlot].cInfo == NULL)
    return 0;

  while (*string != '\0') {
    charIndex = ((unsigned char)*string)-g_fontRegistry[fontSlot].firstIndex;
    if (charIndex >= 0 && charIndex < g_fontRegistry[fontSlot].numOfChars) {
      CharInfo ci = g_fontRegistry[fontSlot].cInfo[charIndex];
      currWidth += ci.width;
      if (currWidth > width) return index; /* found cutoff character */
    }
    string++;
    index++;
  }
  
  return index;
}

/*
DRAWBITMAP

Draws a (clipped) bitmap in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 bitmap - pointer to the bitmap to display
 destX - the left hand side x-coordinate of the destination in the display
 destY - the top y-coordinate of the destination in the display
 sourceX - the left hand side x-coordinate of the source in the bitmap
 sourceY - the top y-coordinate of the source in the bitmap
 width - the width of the area to copy, if <1 then the entire width of the
         bitmap will be copied
 height - the height of the area to copy, if <1 then the entire height of the
          bitmap will be copied 
 shade - the shade to override with, if <0 then the original shades of the
         bitmap will be used
 isTransparent - if nonzero:
                 pixels of shade 0 will be treated as transparent
                 - unless the bitmap is 4BPP in which case the extra
                 transparency information that is encoded will be obeyed.

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 See the header file (vfdlib.h) for a description of the bitmap formats.

*/
void vfdlib_drawBitmap(char *buffer, unsigned char *bitmap,
		       int destX, int destY,
		       int sourceX, int sourceY,
		       int width, int height,
		       signed char shade, int isTransparent) {

  int bitmapWidth = (int) bitmap[2] | ((int) bitmap[1] << 8);
  int bitmapHeight = (int) bitmap[4] | ((int) bitmap[3] << 8);

  /* clip LHS */
  if (width < 1) width = bitmapWidth; 
  if (destX < g_clipXLeft) { 
    int diff = g_clipXLeft - destX;
    sourceX += diff;
    width -= diff;
    destX = g_clipXLeft;
  }

  if (sourceX < 0) {
    destX -= sourceX;
    sourceX = 0;
  }

  /* clip top */
  if (height < 1) height = bitmapHeight;
  if (destY < g_clipYTop) {
    int diff = g_clipYTop - destY;
    sourceY += diff;
    height -= diff;
    destY = g_clipYTop;
  }

  if (sourceY < 0) {
    destY -= sourceY;
    sourceY = 0;
  }

  /* clip RHS */
  if (sourceX + width > bitmapWidth) width = bitmapWidth - sourceX;
  if (destX + width > g_clipXRight) width = g_clipXRight - destX;

  /* clip bottom */
  if (sourceY + height > bitmapHeight) height = bitmapHeight - sourceY;
  if (destY + height > g_clipYBottom) height = g_clipYBottom - destY;

  if (width > 0 && height > 0) {
    switch (bitmap[0]) {
    case BITMAP_1BPP:
      drawBitmap1BPP(buffer, bitmap+5, bitmapWidth, bitmapHeight, destX, destY,
		     sourceX, sourceY, width, height, shade, isTransparent);
      break;
    case BITMAP_2BPP:
      drawBitmap2BPP(buffer, bitmap+5, bitmapWidth, bitmapHeight, destX, destY,
		     sourceX, sourceY, width, height, shade, isTransparent);
      break;
    case BITMAP_4BPP:
      drawBitmap4BPP(buffer, bitmap+5, bitmapWidth, bitmapHeight, destX, destY,
		     sourceX, sourceY, width, height, shade, isTransparent);
      break;
    }
  }
}


/* private utility functions */

static void drawPixel4WaySymmetricClipped(char *buffer, int cX, int cY,
					  int x, int y, char shade) {

  vfdlib_drawPointClipped(buffer, cX+x,cY+y, shade);
  vfdlib_drawPointClipped(buffer, cX+x,cY-y, shade);
  vfdlib_drawPointClipped(buffer, cX-x,cY+y, shade);
  vfdlib_drawPointClipped(buffer, cX-x,cY-y, shade);
}

static void drawPixel4WaySymmetricUnclipped(char *buffer, int cX, int cY,
					    int x, int y, char shade) {

  vfdlib_drawPointUnclipped(buffer, cX+x,cY+y, shade);
  vfdlib_drawPointUnclipped(buffer, cX+x,cY-y, shade);
  vfdlib_drawPointUnclipped(buffer, cX-x,cY+y, shade);
  vfdlib_drawPointUnclipped(buffer, cX-x,cY-y, shade);
}

static void drawLineHMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade) {

  int incrH, incrHV, addV;
  int pixelPos = x0 & 0x1;
  int loPixel = 0;
  int hiPixel = 0;
  char hiShade = shade << 4;
  char biShade = shade | hiShade;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }

  incrH = 2 * dy;
  incrHV = 2 * (dy - dx);

  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) loPixel = 1;
  else hiPixel = 1;

  while (xMax-- > 0) {
    if (d <= 0) {
      d += incrH;
      pixelPos++;
      if (pixelPos == 2) {
	if (loPixel && hiPixel) {
	  *buffer = biShade;
	} else if (loPixel) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= shade;
	} else { /* hiPixel */
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= hiShade;
	}
	buffer++;
	pixelPos = 0;
	hiPixel = 0;
	loPixel = 1;
      } else { /* pixelPos == 1 */
	hiPixel = 1;
      }
    } else {
      d += incrHV;
      
      /* draw outstanding pixels */
      if (loPixel && hiPixel) {
	*buffer = biShade;
      } else if (loPixel) {
	*buffer &= MASK_HI_NYBBLE;
	*buffer |= shade;
      } else { /* hiPixel */
	*buffer &= MASK_LO_NYBBLE;
	*buffer |= hiShade;
      }
      
      /* h++ */
      pixelPos++;
      if (pixelPos == 2) {
	buffer++;
	pixelPos = 0;
	loPixel = 1;
	hiPixel = 0;
      } else { /* pixelPos == 1 */
	loPixel = 0;
	hiPixel = 1;
      }

      /* v++ */
      if (!yMax--) return;
      buffer += addV;
    }
  }
  
  /* draw outstanding pixels */
  if (loPixel && hiPixel) {
    *buffer = biShade;
  } else if (loPixel) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else { /* hiPixel */
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }
}

static void drawLineVMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade) {

  int incrV, incrHV, addV;
  int pixelPos = x0 & 0x1;
  char hiShade = shade << 4;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }

  incrV = 2 * dx;
  incrHV = 2 * (dx - dy);
  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else {
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }

  while (yMax-- > 0) {
    if (d <= 0) {
      d += incrV;
    } else {
      d += incrHV;   

      /* h++ */
      if (!xMax--) return;
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }
    }

    /* v++ */
    buffer += addV;

    /* write pixel */
    if (pixelPos == 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
    } else {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= hiShade;
    }
  }
}

static void drawLineHMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade) {

  int d, incrH, incrHV, addV;
  int pixelPos = x0 & 0x1;
  int loPixel = 0;
  int hiPixel = 0;
  char hiShade = shade << 4;
  char biShade = shade | hiShade;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }

  d = 2 * dy - dx;
  incrH = 2 * dy;
  incrHV = 2 * (dy - dx);

  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) loPixel = 1;
  else hiPixel = 1;

  while (dx-- > 0)
    if (d <= 0) {
      d += incrH;
      pixelPos++;
      if (pixelPos == 2) {
	if (loPixel && hiPixel) {
	  *buffer = biShade;
	} else if (loPixel) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= shade;
	} else { /* hiPixel */
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= hiShade;
	}
	buffer++;
	pixelPos = 0;
	hiPixel = 0;
	loPixel = 1;
      } else { /* pixelPos == 1 */
	hiPixel = 1;
      }
    } else {
      d += incrHV;
      
      /* draw outstanding pixels */
      if (loPixel && hiPixel) {
	*buffer = biShade;
      } else if (loPixel) {
	*buffer &= MASK_HI_NYBBLE;
	*buffer |= shade;
      } else { /* hiPixel */
	*buffer &= MASK_LO_NYBBLE;
	*buffer |= hiShade;
      }
      
      /* h++ */
      pixelPos++;
      if (pixelPos == 2) {
	buffer++;
	pixelPos = 0;
	loPixel = 1;
	hiPixel = 0;
      } else { /* pixelPos == 1 */
	loPixel = 0;
	hiPixel = 1;
      }

      /* v++ */
      buffer += addV;
    }
  
  /* draw outstanding pixels */
  if (loPixel && hiPixel) {
    *buffer = biShade;
  } else if (loPixel) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else { /* hiPixel */
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }
}

static void drawLineVMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade) {

  int d, incrV, incrHV, addV;
  int pixelPos = x0 & 0x1;
  char hiShade = shade << 4;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }
  
  d = 2 * dx - dy;
  incrV = 2 * dx;
  incrHV = 2 * (dx - dy);
  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else {
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }

  while (dy-- > 0) {
    if (d <= 0) {
      d += incrV;
    } else {
      d += incrHV;   

      /* h++ */
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }
    }

    /* v++ */
    buffer += addV;

    /* write pixel */
    if (pixelPos == 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
    } else {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= hiShade;
    }
  }
}

static void drawBitmap1BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent) {

  int startPos, widthLeft;
  char *startBitmap, *startBuffer;
  unsigned char startMask;
  int bitmapScanlineSkip = ((bitmapWidth-1) >> 3) + 1;
  unsigned char bitmask = 0x80 >> (sourceX & 0x07);
  int pixelPos = destX & 0x1;
  char hiShade;
  if (shade < 0) shade = VFDSHADE_BRIGHT;
  hiShade = shade << 4;
  bitmap += (sourceY * bitmapScanlineSkip) + (sourceX >> 3);
  buffer += (destY << 6) + (destX >> 1);
  
  startMask = bitmask;
  startPos = pixelPos;
  startBitmap = bitmap;
  startBuffer = buffer;

  while (height-- > 0) {
    widthLeft = width;
    while (widthLeft-- > 0) {

      /* set pixel */
      if (*bitmap & bitmask) {
	if (pixelPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= shade;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= hiShade;
	}
      } else if (!isTransparent) {
	if (pixelPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	}
      }
      
      /* sourceX++ */
      bitmask >>= 1;
      if (bitmask == 0) {
	bitmask = 0x80;
	bitmap++;
      }
      /* destX++ */
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }      
    }
    /* sourceY++ */
    bitmask = startMask;
    startBitmap += bitmapScanlineSkip;
    bitmap = startBitmap;
    /* destY++ */
    pixelPos = startPos;
    startBuffer += VFD_BYTES_PER_SCANLINE;
    buffer = startBuffer;
  }
}

static void drawBitmap2BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent) {

  int startPos, startShift, widthLeft;
  char currentBitmap, currentShade;
  char *startBitmap, *startBuffer;
  int bitmapScanlineSkip = ((bitmapWidth-1) >> 2) + 1;
  int bitmapShift = (sourceX & 0x03) << 1;
  int pixelPos = destX & 0x1;
  bitmap += (sourceY * bitmapScanlineSkip) + (sourceX >> 2);
  buffer += (destY << 6) + (destX >> 1);
  
  startShift = bitmapShift;
  startPos = pixelPos;
  startBitmap = bitmap;
  startBuffer = buffer;

  while (height-- > 0) {
    widthLeft = width;
    currentBitmap = *bitmap >> bitmapShift;
    while (widthLeft-- > 0) {

      currentShade = currentBitmap & 0x03;

      if (currentShade != 0 || !isTransparent) {
	 /* set pixel */
	if (shade >= 0 && currentShade > 0) currentShade = shade;
	if (pixelPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= currentShade;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= currentShade << 4;
	}
      }
      
      /* sourceX++ */
      bitmapShift += 2;
      if (bitmapShift > 6) {
	bitmapShift = 0;
	currentBitmap = *(++bitmap);
      } else currentBitmap >>= 2;

      /* destX++ */
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }      
    }
    /* sourceY++ */
    bitmapShift = startShift;
    startBitmap += bitmapScanlineSkip;
    bitmap = startBitmap;
    /* destY++ */
    pixelPos = startPos;
    startBuffer += VFD_BYTES_PER_SCANLINE;
    buffer = startBuffer;
  }
}

static void drawBitmap4BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent) {

  int startSourcePos, startDestPos, widthLeft;
  char currentShade, currentTrans;
  char *startBitmap, *startBuffer;
  int bitmapScanlineSkip = ((bitmapWidth-1) >> 1) + 1;
  int pixelSourcePos = sourceX & 0x1;
  int pixelDestPos = destX & 0x1;
  bitmap += (sourceY * bitmapScanlineSkip) + (sourceX >> 1);
  buffer += (destY << 6) + (destX >> 1);
  
  startSourcePos = pixelSourcePos;
  startDestPos = pixelDestPos;
  startBitmap = bitmap;
  startBuffer = buffer;

  if (isTransparent) {
    while (height-- > 0) {
      widthLeft = width;
      while (widthLeft-- > 0) {
	
	if (pixelSourcePos == 0) {
	  currentShade = *bitmap & 0x03;
	  currentTrans = *bitmap & 0x0C;
	}
	else {
	  currentShade = (*bitmap >> 4) & 0x03;
	  currentTrans = *bitmap & 0xC0;
	}
	
	if (!currentTrans) {
	  /* set pixel */
	  if (shade >= 0) currentShade = shade;
	  if (pixelDestPos == 0) {
	    *buffer &= MASK_HI_NYBBLE;
	    *buffer |= currentShade;
	  } else {
	    *buffer &= MASK_LO_NYBBLE;
	    *buffer |= currentShade << 4;
	  }
	}
	
	/* sourceX++ */
	if (++pixelSourcePos > 1) {
	  bitmap++;
	  pixelSourcePos = 0;
	}   
	/* destX++ */
	if (++pixelDestPos > 1) {
	  buffer++;
	  pixelDestPos = 0;
	}      
      }
      /* sourceY++ */
      pixelSourcePos = startSourcePos;;
      startBitmap += bitmapScanlineSkip;
      bitmap = startBitmap;
      /* destY++ */
      pixelDestPos = startDestPos;
      startBuffer += VFD_BYTES_PER_SCANLINE;
      buffer = startBuffer;
    }
  } else {
    while (height-- > 0) {
      widthLeft = width;
      while (widthLeft-- > 0) {
	
	if (pixelSourcePos == 0) currentShade = *bitmap & 0x03;
	else currentShade = (*bitmap >> 4) & 0x03;
	
	/* set pixel */
	if (pixelDestPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= currentShade;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= currentShade << 4;
	}
      
	/* sourceX++ */
	if (++pixelSourcePos > 1) {
	  bitmap++;
	  pixelSourcePos = 0;
	}   
	/* destX++ */
	if (++pixelDestPos > 1) {
	  buffer++;
	  pixelDestPos = 0;
	}      
      }
      /* sourceY++ */
      pixelSourcePos = startSourcePos;;
      startBitmap += bitmapScanlineSkip;
      bitmap = startBitmap;
      /* destY++ */
      pixelDestPos = startDestPos;
      startBuffer += VFD_BYTES_PER_SCANLINE;
      buffer = startBuffer;
    }
  }
}
