#!/usr/bin/python

import string, re, sys
from convert import *

class Wpoint:
    def __init__(self, coord, desc = "", dist = 0):
	self.coord = coord
	self.desc = desc
	self.dist = dist

def NAD27toWGS84(coord):
    return ConvertDatum(coord, Datum_NAD27_CONUS, Datum_WGS84)

def project(coord, center):
    return toTM(coord, center, UTM_k0, Datum_WGS84)

def parse(mapsonus_rawroute):
    f = open(mapsonus_rawroute)

    state = ""
    route = {}
    coords = []
    next_point = 0
    while 1:
	line = f.readline()
	if not line: break

	if state == "R":
	    parts = string.split(line)
	    if parts[0] in ["START", "INT", "END"]:
		long = float(parts[1])
		lat  = float(parts[2])
		mark = parts[3]
		desc = string.join(parts[4:])
		route[next_point-1] = \
		    Wpoint(NAD27toWGS84(Coord(lat, long)), desc)
		continue
	    elif parts[0] == "TURN":
		#idx  = int(parts[1])
		long = float(parts[2])
		lat  = float(parts[3])
		pnt  = int(parts[4])
		npts = int(parts[5])
		turn = int(parts[6])
		#ohdg = int(parts[7])
		#ihdg = int(parts[8])
		#km   = float(parts[9])
		desc = string.join(parts[10:])
		if pnt != next_point:
		    print "parse error?"
		route[pnt] = Wpoint(NAD27toWGS84(Coord(lat, long)), desc)
		next_point = pnt + npts
		continue
	elif state == "S":
	    line = re.sub("<.+?>", "", line)
	    parts = string.split(line)
	    try:
		for i in range(0, len(parts), 2):
		    lat  = float(parts[i+1])
		    long = float(parts[i])
		    coords.append(NAD27toWGS84(Coord(lat, long)))
	    except (IndexError, ValueError):
		continue

	if not state and re.search("Route Turn Information", line):
	    state = "R"
	elif state == "R" and re.search("Route Shape Points", line):
	    state = "S"

    # create a route as a combination of 'shape' and 'turn' information
    # make sure we discard duplicate records.
    waypoints = []
    for i in range(-1, len(coords)):
	if route.has_key(i):
	    if len(waypoints) and waypoints[-1].coord == coords[i]:
	    	del waypoints[-1]
	    waypoints.append(route[i])
	else:
	    if waypoints[-1].coord != coords[i]:
		waypoints.append(Wpoint(coords[i]))
    return waypoints
	
if not sys.argv[1:]:
    print "Usage: %s <raw route>\n\tWhere raw route is the saved Raw Route Description from\n\tmapsonus.switchboard.com in html format" % sys.argv[0]
    sys.exit(-1)

wpoints = parse(sys.argv[1])

#find center point
min = Coord(wpoints[0].coord.lat, wpoints[0].coord.long)
max = Coord(wpoints[0].coord.lat, wpoints[0].coord.long)
for wp in wpoints:
    if wp.coord.lat < min.lat:   min.lat  = wp.coord.lat
    if wp.coord.long < min.long: min.long = wp.coord.long
    if wp.coord.lat > max.lat:   max.lat  = wp.coord.lat
    if wp.coord.long > max.long: max.long = wp.coord.long

wps = 0
center = Coord((min.lat + max.lat) / 2, (min.long + max.long) / 2)
for wp in wpoints:
    wp.xy = project(wp.coord, center)
    if wp.desc: wps = wps + 1
    #print "prj %.8f %.8f %.8f %.8f %d %d" % (center.lat, center.long, wp.coord.lat, wp.coord.long, wp.xy[0], wp.xy[1])

# fill in distances to endpoint
dist = 0.0
for i in range(len(wpoints)-1, 0, -1):
    wpoints[i].dist = dist
    dist = dist + Distance(wpoints[i].coord, wpoints[i-1].coord, Datum_WGS84) 
wpoints[0].dist = dist


import struct
out = open("route", "w")

DEGTOINT = ((1L<<31)-1) / 180.0
#print "hdr %f %f %d %d" % (center.lat, center.long, len(wpoints), wps)
#print "hdr %d %d %d %d" % (center.lat * DEGTOINT, center.long * DEGTOINT, len(wpoints), wps)
buf = struct.pack("<iiII", int(center.lat * DEGTOINT), int(center.long * DEGTOINT), len(wpoints), wps)
out.write(buf)
for wp in wpoints:
    #print "pts %.8f %.8f %d" % (wp.coord.lat, wp.coord.long, wp.dist)
    buf = struct.pack("<iiI", wp.xy[0], wp.xy[1], wp.dist)
    out.write(buf)

for i in range(len(wpoints)):
    if not wpoints[i].desc:
	continue
    #print "wps %d %s" % (i, wpoints[i].desc)
    buf = struct.pack("<II", i, len(wpoints[i].desc)) + wpoints[i].desc
    out.write(buf)

out.close()

