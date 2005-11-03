#!/usr/bin/python

import xml.sax, re, sys, os
from convert import *

discard="((Start|End)\sPoint|follow|Continue onto|Continue on|continue|Continue|bears?|Bear|bear|turn|TURN|Turn |Go |into the |Take the|Take |Merge into|into|to |Set by click|sharply|left at|right at|left|right|straight|onto|as road goes into|as it|Head\s(north|west|east|south)\s(on|from))\s?"

class Wpoint:
    def __init__(self, coord, desc = None):
	if not desc: desc = ""
	self.coord = coord
	self.xy = (0,0)
	self.desc = re.sub(discard, "", desc).strip()
    def __repr__(self):
	return "%f %f %s" % (self.coord.lat, self.coord.long, self.desc)
	

def project(coord, center):
    return toTM(coord, center, UTM_k0, Datum_WGS84)


# google maps specific parser

SCALE = 1.0E-5
def getCoord(vc, i):
    Pc = Ua = 0
    while 1:
      Xb = ord(vc[i]) - ord('?')
      i += 1
      Ua |= (Xb & 0x1f) << Pc
      Pc += 5
      if (Xb < 32):
	break

    delta = Ua >> 1
    if Ua & 0x1:
	delta = ~delta
    return i, delta

def decodePolyline(vc):
    points = []
    lat = lon = 0
    i = 0
    while i < len(vc):
	i, dlat = getCoord(vc, i)
	lat += dlat

	i, dlon = getCoord(vc, i)
	lon += dlon

	coord = Coord(lat * SCALE, lon * SCALE)
	points.append(coord)
    return points

class gmapshandler(xml.sax.ContentHandler):
    def __init__(self):
	xml.sax.ContentHandler.__init__(self)
	self.content = ''
	self.pointIndex = 0
	self.destination = ''
	self.coords  = ''
	self.segments = {}

    def startElement(self, name, attrs):
	if name == 'destinationAddress':
	    self.content = ''
	elif name == 'points':
	    self.content = ''
	elif name == 'segment':
	    self.content = ''
	    self.pointIndex = int(attrs['pointIndex'])

    def endElement(self, name):
	if name == "destinationAddress":
	    self.destination = self.content
	elif name == "line":
	    self.content += ' '
	elif name == "points":
	    self.coords = decodePolyline(self.content)
	elif name == "segment":
	    self.segments[self.pointIndex] = self.content

    def characters(self, content):
	self.content += content

def parse(googlemap):
    content = open(googlemap).read()

    # dig the chunk of XML data out of the html page
    start = content.find('<?xml version="1.0"?><page>')
    end = content.find('</page>', start)
    content = content[start:end+7]

    h = gmapshandler()
    xml.sax.parseString(content, h)

    h.segments[len(h.coords)-1] = h.destination
    wpoints = []
    for i in range(len(h.coords)):
	wpoints.append(Wpoint(h.coords[i], h.segments.get(i)))
    return wpoints

# common with parse_mapsonus

if not sys.argv[1:]:
    print "Usage: %s <raw route>\n\tWhere raw route is the saved 'link to this page' page\n\tfrom maps.google.com in html format" % sys.argv[0]
    sys.exit(-1)

inname = sys.argv[1]
outname = os.path.basename(os.path.splitext(inname)[0])
wpoints = parse(inname)

# filter out duplicates
tmp = []
for wp in wpoints:
  if tmp and tmp[-1].coord == wp.coord:
    tmp[-1].desc = tmp[-1].desc + wp.desc
  else:
    tmp.append(wp)
wpoints = tmp

# find center point
min = Coord(wpoints[0].coord.lat, wpoints[0].coord.long)
max = Coord(wpoints[0].coord.lat, wpoints[0].coord.long)
for wp in wpoints:
    if wp.coord.lat < min.lat:   min.lat  = wp.coord.lat
    if wp.coord.long < min.long: min.long = wp.coord.long
    if wp.coord.lat > max.lat:   max.lat  = wp.coord.lat
    if wp.coord.long > max.long: max.long = wp.coord.long
center = Coord((min.lat + max.lat) / 2, (min.long + max.long) / 2)

# project all points to a rectangular grid around the center
wps = 0
for wp in wpoints:
    wp.xy = project(wp.coord, center)
    if wp.desc: wps = wps + 1

# and write out what we have
out = open(outname, "w")

buf = "%.6f %.6f %d %d\n" % (center.lat, center.long, len(wpoints), wps)
out.write(buf)
for wp in wpoints:
    buf = "%d %d %s\n" % (wp.xy[0], wp.xy[1], wp.desc)
    out.write(buf)

out.close()

