#
# Datum conversion routines gratefully taken from 'gpsman' a GPL licensed
# GPS mapping application written in TCL/TK.
#	http://www.ncc.up.pt/~mig/gpsman.html
#
import math

def degtorad(deg):
    return deg * 2.0 * math.pi / 360.0

def radtodeg(rad):
    return rad * 360 / (2.0 * math.pi)

class Coord:
    def __init__(self, lat, long):
	self.lat   = lat
	self.long  = long
	self.latr  = degtorad(lat)
	self.longr = degtorad(long)

    def __eq__(self, coord):
	return self.lat == coord.lat and self.long == coord.long

# Ellipse: (a) (semi-major axis in meters) + (invf) (inverse of flattening)
Ellipse_Clarke1866 = (6378206.4, 294.9786982)
Ellipse_WGS84      = (6378137.0, 298.257223563)

# Datum: Ellipse + (dx, dy, dz) offset of center
Datum_NAD27_CONUS = Ellipse_Clarke1866 + (-8, 160, 176)
Datum_WGS84       = Ellipse_WGS84 + (0, 0, 0)

UTM_k0 = 0.9996

def ConvertDatum(coord, fromdatum, todatum):
    if fromdatum == todatum:
	return coord

    phi   = coord.latr
    lmbda = coord.longr
    a0, invf0, x0, y0, z0 = fromdatum
    a1, invf1, x1, y1, z1 = todatum
    f0 = 1 / invf0
    f1 = 1 / invf1
    dx = x1 - x0
    dy = y1 - y0
    dz = z1 - z0
    b0  = a0 * (1.0 - f0)
    b1  = a1 * (1.0 - f1)
    es0 = f0 * (2.0 - f0)
    es1 = f1 * (2.0 - f1)

    # convert geodetic latitude to geocentric latitude
    if coord.lat == 0.0 or coord.lat == 90.0 or coord.lat == -90.0:
    	psi = phi
    else:
	psi = math.atan((1 - es0) * math.tan(phi))

    # x and y axis coordinates with respect to original ellipsoid
    t1 = math.tan(psi)
    jh0 = b0 * b0 + a0 * a0 * t1 * t1
    if coord.long == 90.0 or coord.long == -90.0:
    	x = 0.0
    	y = abs((a0 * b0) / math.sqrt(jh0))
    else:
	t2 = math.tan(lmbda)
	x = abs((a0 * b0) / math.sqrt((1 + t2**2) * jh0))
	y = abs(x * t2)

    if coord.long < -90.0 or coord.long > 90.0:
	x = -x
    if coord.long < 0.0:
	y = -y

    # z axis coordinate with respect to the original ellipsoid
    if coord.lat == 90.0:    z = b0
    elif coord.lat == -90.0: z = -b0
    else:
	z = t1 * math.sqrt((a0 * a0 * b0 * b0) / jh0)

    # geocentric latitude with respect to the new ellipsoid
    ddx = x - dx
    ddy = y - dy
    ddz = z - dz
    psi1 = math.atan(ddz / math.sqrt(ddx * ddx + ddy * ddy))
    lat = radtodeg(math.atan(math.tan(psi1) / (1 - es1)))
    long = radtodeg(math.atan(ddy / ddx))
    if ddx < 0.0:
    	if ddy > 0.0:
	    long = long + 180
	else:
	    long = long - 180
    return Coord(lat, long)

def Quick_Distance(coord1, coord2, datum):
    if coord1 == coord2:
	return 0

    la1 = coord1.latr
    lo1 = coord1.longr
    la2 = coord2.latr
    lo2 = coord2.longr

    dist = datum[0] * math.acos(math.cos(lo1 - lo2) * \
	    math.cos(la1) * math.cos(la2) + math.sin(la1) * math.sin(la2))
    
    return dist

def Distance(coord1, coord2, datum):
    if coord1 == coord2:
	return 0

    la1 = coord1.latr
    la2 = coord2.latr
    lo1 = coord1.longr
    lo2 = coord2.longr

    a = datum[0]
    f = 1.0 / datum[1]
    if abs(math.cos(la1)) < 1.0e-20 or abs(math.cos(la2)) < 1.0e-20:
	return Quick_Distance(coord1, coord2, datum)

    r = 1.0 - f
    tu1 = r * math.tan(la1)
    tu2 = r * math.tan(la2)
    cu1 = 1.0 / math.sqrt(tu1 * tu1 + 1.0)
    su1 = cu1 * tu1
    cu2 = 1.0 / math.sqrt(tu2 * tu2 + 1.0)
    s = cu1 * cu2
    baz = s * tu2
    faz = baz * tu1
    czc = faz + faz
    x = lo2 - lo1
    d = x + 1.0
    while abs(d - x) > 5.0e-12:
	sx = math.sin(x)
	cx = math.cos(x)
	tu1 = cu2 * sx
	tu2 = baz - su1 * cu2 * cx
	sy = math.sqrt(tu1 * tu1 + tu2 * tu2)
	cy = s * cx + faz
	y = math.atan2(sy, cy)
	sa = s * sx / sy
	c2a = -sa * sa + 1.0
	cz = czc
	if czc > 0: cz = -cz / c2a + cy
	e = cz * cz * 2.0 - 1.0
	c = ((-3.0 * c2a + 4.0) * f + 4.0) * c2a * f / 16.0
	d = x
	x = (1.0 - c) * ((e * cy * c + cz) * sy * c + y) * sa * f + lo2 - lo1
    #faz = math.atan2(tu1, tu2)
    baz = math.atan2(cu1 * sx, baz * cx - su1 * cu2) + math.pi
    x = math.sqrt((1.0 / r / r - 1.0) * c2a + 1.0) + 1.0
    x = (x - 2.0) / x
    x2 = x * x
    d = (0.375 * x2 - 1.0) * x
    s = ((((sy * sy * 4.0 - 3.0) * (1.0 - e - e) * \
		    cz * d / 6.0 - e * cy) * d / 4.0 + cz) * \
		    sy * d + y) * (x2 / 4.0 + 1.0) / (1.0 - x) * a * r
    return s


def M(phi, a, es):
    if phi == 0.0:
	return 0.0

    es2 = es * es
    es3 = es * es * es

    return a * ((1.0 - es / 4.0 - 3.0 * es2 / 64.0 - 5.0 * es3 / 256.0) * phi -
	(3.0 * es / 8.0 + 3.0 * es2 / 32.0 + 45.0 * es3 / 1024.0) *
	math.sin(2.0 * phi) + (15.0 * es2 / 256.0 + 45.0 * es3 / 1024.0) *
	math.sin(4.0 * phi) - (35.0 * es3 / 3072.0) * math.sin(6.0 * phi))

def toTM(point, center, k0, datum):
    phi = point.latr
    lmb = point.longr
    phi0 = center.latr
    lmb0 = center.longr

    a = datum[0]
    f = 1.0 / datum[1]
    es = f * (2.0 - f)
    et2 = es / (1.0 - es)

    m = M(phi, a, es)
    m0 = M(phi0, a, es)

    sin_phi = math.sin(phi)
    cos_phi = math.cos(phi)
    tan_phi = math.tan(phi)

    n = a / math.sqrt(1.0 - es * sin_phi * sin_phi);
    t = tan_phi * tan_phi
    c = et2 * cos_phi * cos_phi
    A = (lmb - lmb0) * cos_phi

    x = k0 * n * (A + (1.0 - t + c) * A * A * A / 6.0 + (5.0 - 18.0 * t +
	t * t + 72.0 * c - 58.0 * et2) * A * A * A * A * A / 120.0)

    y = k0 * (m - m0 + n * tan_phi * (A * A / 2.0 + (5.0 - t + 9.0 * c +
	4.0 * c * c) * A * A * A * A / 24.0 + (61.0 - 58.0 * t + t * t +
	600.0 * c - 330.0 * et2) * A * A * A * A * A * A  / 720.0))

    return (x, y)

