/**************************************************************************
 * moonpos.cxx
 * Written by Durk Talsma. Originally started October 1997, for distribution  
 * with the FlightGear project. Version 2 was written in August and 
 * September 1998. This code is based upon algorithms and data kindly 
 * provided by Mr. Paul Schlyter. (pausch@saaf.se). 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Id$
 **************************************************************************/


#include <string.h>

#include <simgear/debug/logstream.hxx>

#include <math.h>

// #include <FDM/flight.hxx>

#include "moonpos.hxx"


/*************************************************************************
 * MoonPos::MoonPos(double mjd)
 * Public constructor for class MoonPos. Initializes the orbital elements and 
 * sets up the moon texture.
 * Argument: The current time.
 * the hard coded orbital elements for MoonPos are passed to 
 * CelestialBody::CelestialBody();
 ************************************************************************/
MoonPos::MoonPos(double mjd) :
  CelestialBody(125.1228, -0.0529538083,
		5.1454,    0.00000,
		318.0634,  0.1643573223,
		60.266600, 0.000000,
		0.054900,  0.000000,
		115.3654,  13.0649929509, mjd)
{
}

MoonPos::MoonPos() :
  CelestialBody(125.1228, -0.0529538083,
		5.1454,    0.00000,
		318.0634,  0.1643573223,
		60.266600, 0.000000,
		0.054900,  0.000000,
		115.3654,  13.0649929509)
{
}


MoonPos::~MoonPos()
{
}


/*****************************************************************************
 * void MoonPos::updatePosition(double mjd, Star *ourSun)
 * this member function calculates the actual topocentric position (i.e.) 
 * the position of the moon as seen from the current position on the surface
 * of the moon. 
 ****************************************************************************/
void MoonPos::updatePosition(double mjd, double lst, double lat, Star *ourSun)
{
  double 
    eccAnom, ecl, actTime,
    xv, yv, v, r, xh, yh, zh, xg, yg, zg, xe, ye, ze,
    Ls, Lm, D, F, mpar, gclat, rho, HA, g,
    geoRa, geoDec;
  
  updateOrbElements(mjd);
  actTime = sgCalcActTime(mjd);

  // calculate the angle between ecliptic and equatorial coordinate system
  // in Radians
  ecl = ((SGD_DEGREES_TO_RADIANS * 23.4393) - (SGD_DEGREES_TO_RADIANS * 3.563E-7) * actTime);  
  eccAnom = sgCalcEccAnom(M, e);  // Calculate the eccentric anomaly
  xv = a * (cos(eccAnom) - e);
  yv = a * (sqrt(1.0 - e*e) * sin(eccAnom));
  v = atan2(yv, xv);               // the moon's true anomaly
  r = sqrt (xv*xv + yv*yv);       // and its distance
  
  // estimate the geocentric rectangular coordinates here
  xh = r * (cos(N) * cos (v+w) - sin (N) * sin(v+w) * cos(i));
  yh = r * (sin(N) * cos (v+w) + cos (N) * sin(v+w) * cos(i));
  zh = r * (sin(v+w) * sin(i));

  // calculate the ecliptic latitude and longitude here
  lonEcl = atan2 (yh, xh);
  latEcl = atan2(zh, sqrt(xh*xh + yh*yh));

  /* Calculate a number of perturbatioin, i.e. disturbances caused by the 
   * gravitational infuence of the sun and the other major planets.
   * The largest of these even have a name */
  Ls = ourSun->getM() + ourSun->getw();
  Lm = M + w + N;
  D = Lm - Ls;
  F = Lm - N;
  
  lonEcl += SGD_DEGREES_TO_RADIANS * (-1.274 * sin (M - 2*D)
			  +0.658 * sin (2*D)
			  -0.186 * sin(ourSun->getM())
			  -0.059 * sin(2*M - 2*D)
			  -0.057 * sin(M - 2*D + ourSun->getM())
			  +0.053 * sin(M + 2*D)
			  +0.046 * sin(2*D - ourSun->getM())
			  +0.041 * sin(M - ourSun->getM())
			  -0.035 * sin(D)
			  -0.031 * sin(M + ourSun->getM())
			  -0.015 * sin(2*F - 2*D)
			  +0.011 * sin(M - 4*D)
			  );
  latEcl += SGD_DEGREES_TO_RADIANS * (-0.173 * sin(F-2*D)
			  -0.055 * sin(M - F - 2*D)
			  -0.046 * sin(M + F - 2*D)
			  +0.033 * sin(F + 2*D)
			  +0.017 * sin(2*M + F)
			  );
  r += (-0.58 * cos(M - 2*D)
	-0.46 * cos(2*D)
	);
  // SG_LOG(SG_GENERAL, SG_INFO, "Running moon update");
  xg = r * cos(lonEcl) * cos(latEcl);
  yg = r * sin(lonEcl) * cos(latEcl);
  zg = r *               sin(latEcl);
  
  xe = xg;
  ye = yg * cos(ecl) -zg * sin(ecl);
  ze = yg * sin(ecl) +zg * cos(ecl);

  geoRa  = atan2(ye, xe);
  geoDec = atan2(ze, sqrt(xe*xe + ye*ye));

  /* SG_LOG( SG_GENERAL, SG_INFO, 
	  "(geocentric) geoRa = (" << (SGD_RADIANS_TO_DEGREES * geoRa) 
	  << "), geoDec= (" << (SGD_RADIANS_TO_DEGREES * geoDec) << ")" ); */


  // Given the moon's geocentric ra and dec, calculate its 
  // topocentric ra and dec. i.e. the position as seen from the
  // surface of the earth, instead of the center of the earth

  // First calculate the moon's parrallax, that is, the apparent size of the 
  // (equatorial) radius of the earth, as seen from the moon 
  mpar = asin ( 1 / r);
  // SG_LOG( SG_GENERAL, SG_INFO, "r = " << r << " mpar = " << mpar );
  // SG_LOG( SG_GENERAL, SG_INFO, "lat = " << f->get_Latitude() );

  gclat = lat - 0.003358 * 
      sin (2 * SGD_DEGREES_TO_RADIANS * lat );
  // SG_LOG( SG_GENERAL, SG_INFO, "gclat = " << gclat );

  rho = 0.99883 + 0.00167 * cos(2 * SGD_DEGREES_TO_RADIANS * lat);
  // SG_LOG( SG_GENERAL, SG_INFO, "rho = " << rho );
  
  if (geoRa < 0)
    geoRa += SGD_2PI;
  
  HA = lst - (3.8197186 * geoRa);
  /* SG_LOG( SG_GENERAL, SG_INFO, "t->getLst() = " << t->getLst() 
	  << " HA = " << HA ); */

  g = atan (tan(gclat) / cos ((HA / 3.8197186)));
  // SG_LOG( SG_GENERAL, SG_INFO, "g = " << g );

  rightAscension = geoRa - mpar * rho * cos(gclat) * sin(HA) / cos (geoDec);
  if (fabs(lat) > 0) {
      declination
	  = geoDec - mpar * rho * sin (gclat) * sin (g - geoDec) / sin(g);
  } else {
      declination = geoDec;
      // cerr << "Geocentric vs. Topocentric position" << endl;
      // cerr << "RA (difference)  : "
      //      << SGD_RADIANS_TO_DEGREES * (geoRa - rightAscension) << endl;
      // cerr << "Dec (difference) : "
      //      << SGD_RADIANS_TO_DEGREES * (geoDec - declination) << endl;
  }

  /* SG_LOG( SG_GENERAL, SG_INFO, 
	  "Ra = (" << (SGD_RADIANS_TO_DEGREES *rightAscension) 
	  << "), Dec= (" << (SGD_RADIANS_TO_DEGREES *declination) << ")" ); */
}
