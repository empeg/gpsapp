#!/usr/bin/python

import string, re, sys, os
from convert import *

# we can relatively easily regenerate all (or most) of this information
discard="((Start|End)\sPoint|FOLLOW|CONTINUE|BEARS?|TURN|SHARPLY|LEFT|RIGHT|onto|as road goes into|as it|Head\s(NORTH|WEST|EAST|SOUTH)\son)\s?"

# words that commonly start a new line
sol="(START|PLAN-DESCR|TURN\s+\d+|TURN-STREET|FORMAT-TYPE|MATCH-LOCAL-MAP|END|KM)\s"

class Wpoint:
    def __init__(self, coord, desc = ""):
	self.coord = coord
	self.desc = string.strip(re.sub(discard, "", desc))

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
    desc = re.sub("(\r|\n)", " ", f.read())
    lines = re.split(sol, desc)
    lines = lines[1:]
    while lines:
	parts = [ lines[0] ] + string.split(lines[1])
	lines = lines[2:]

	if parts[0] in ["START", "INT", "END"]:
	    print parts
	    long = float(parts[1])
	    lat  = float(parts[2])
	    mark = parts[3]
	    desc = string.join(parts[4:])
	    route[next_point-1] = \
		Wpoint(NAD27toWGS84(Coord(lat, long)), desc)
	    continue
	elif parts[0][:5] == "TURN ":
	    #idx = int(parts[0][5:])
	    long = float(parts[1])
	    lat  = float(parts[2])
	    pnt  = int(parts[3])
	    npts = int(parts[4])
	    turn = int(parts[5])
	    #outhdg = int(parts[6])
	    #inhdg  = int(parts[7])
	    #km     = float(parts[8])
	    desc = string.join(parts[9:])
	    if pnt != next_point:
		print "parse error?"
	    route[pnt] = Wpoint(NAD27toWGS84(Coord(lat, long)), desc)
	    next_point = pnt + npts
	    continue
	elif parts[0] == "KM":
	    while parts[0] != "<xmp>":
		parts = parts[1:]
	    parts = parts[1:]
	    try:
		for i in range(0, len(parts), 2):
		    lat  = float(re.sub("</xmp>", "", parts[i+1]))
		    long = float(parts[i])
		    coords.append(NAD27toWGS84(Coord(lat, long)))
	    except (IndexError, ValueError):
		continue

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

inname = sys.argv[1]
outname = os.path.basename(os.path.splitext(inname)[0])
wpoints = parse(inname)

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

