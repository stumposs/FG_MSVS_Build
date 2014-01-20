// metar interface class
//
// Written by Melchior FRANZ, started December 2003.
//
// Copyright (C) 2003  Melchior FRANZ - mfranz@aon.at
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

/**
 * @file metar.cxx
 * Interface for encoded Meteorological Aerodrome Reports (METAR).
 */
#ifdef HAVE_CONFIG_H
#  include <simgear_config.h>
#endif

#include <string>
#include <time.h>
#include <cstring>

#include <simgear/debug/logstream.hxx>
#include <simgear/structure/exception.hxx>

#include "metar.hxx"

#define NaN SGMetarNaN

using std::string;
using std::map;
using std::vector;

/**
 * The constructor takes a Metar string
 * The constructor throws sg_io_exceptions on failure. The "METAR"
 * keyword has no effect (apart from incrementing the group counter
 * @a grpcount) and can be left away. A keyword "SPECI" is
 * likewise accepted.
 *
 * @param m     ICAO station id or metar string
 *
 * @par Examples:
 * @code
 * SGMetar *m = new SGMetar("METAR KSFO 061656Z 19004KT 9SM SCT100 OVC200 08/03 A3013");
 * double t = m->getTemperature_F();
 * delete m;

 * @endcode
 */
SGMetar::SGMetar(const string& m) :
	_grpcount(0),
	_x_proxy(false),
	_year(-1),
	_month(-1),
	_day(-1),
	_hour(-1),
	_minute(-1),
	_report_type(-1),
	_wind_dir(-1),
	_wind_speed(NaN),
	_gust_speed(NaN),
	_wind_range_from(-1),
	_wind_range_to(-1),
	_temp(NaN),
	_dewp(NaN),
	_pressure(NaN),
	_rain(false),
	_hail(false),
	_snow(false),
	_cavok(false)
{
	_data = new char[m.length() + 2];	// make room for " \0"
	strcpy(_data, m.c_str());
	_url = _data;
		
	normalizeData();

	_m = _data;
	_icao[0] = '\0';

	// NOAA preample
	if (!scanPreambleDate())
		useCurrentDate();
	scanPreambleTime();

	// METAR header
	scanType();
	if (!scanId() || !scanDate()) {
		delete[] _data;
		throw sg_io_exception("metar data bogus ", sg_location(_url));
	}
	scanModifier();

	// base set
	scanWind();
	scanVariability();
	while (scanVisibility()) ;
	while (scanRwyVisRange()) ;
	while (scanWeather()) ;
	while (scanSkyCondition()) ;
	scanTemperature();
	scanPressure();
	while (scanSkyCondition()) ;
	while (scanRunwayReport()) ;
	scanWindShear();

	// appendix
	while (scanColorState()) ;
	scanTrendForecast();
	while (scanRunwayReport()) ;
	scanRemainder();
	scanRemark();

	if (_grpcount < 4) {
		delete[] _data;
		throw sg_io_exception("metar data incomplete ", sg_location(_url));
	}

	_url = "";
}


/**
  * Clears lists and maps to discourage access after destruction.
  */
SGMetar::~SGMetar()
{
	_clouds.clear();
	_runways.clear();
	_weather.clear();
	delete[] _data;
}


void SGMetar::useCurrentDate()
{
	struct tm now;
	time_t now_sec = time(0);
#ifdef _WIN32
	now = *gmtime(&now_sec);
#else
	gmtime_r(&now_sec, &now);
#endif
	_year = now.tm_year + 1900;
	_month = now.tm_mon + 1;
}

/**
  * Replace any number of subsequent spaces by just one space, and add
  * a trailing space. This makes scanning for things like "ALL RWY" easier.
  */
void SGMetar::normalizeData()
{
	char *src, *dest;
	for (src = dest = _data; (*dest++ = *src++); )
		while (*src == ' ' && src[1] == ' ')
			src++;
	for (dest--; isspace(*--dest); ) ;
	*++dest = ' ';
	*++dest = '\0';
}


// \d\d\d\d/\d\d/\d\d
bool SGMetar::scanPreambleDate()
{
	char *m = _m;
	int year, month, day;
	if (!scanNumber(&m, &year, 4))
		return false;
	if (*m++ != '/')
		return false;
	if (!scanNumber(&m, &month, 2))
		return false;
	if (*m++ != '/')
		return false;
	if (!scanNumber(&m, &day, 2))
		return false;
	if (!scanBoundary(&m))
		return false;
	_year = year;
	_month = month;
	_day = day;
	_m = m;
	return true;
}


// \d\d:\d\d
bool SGMetar::scanPreambleTime()
{
	char *m = _m;
	int hour, minute;
	if (!scanNumber(&m, &hour, 2))
		return false;
	if (*m++ != ':')
		return false;
	if (!scanNumber(&m, &minute, 2))
		return false;
	if (!scanBoundary(&m))
		return false;
	_hour = hour;
	_minute = minute;
	_m = m;
	return true;
}


// (METAR|SPECI)
bool SGMetar::scanType()
{
	if (strncmp(_m, "METAR ", 6) && strncmp(_m, "SPECI ", 6))
		return false;
	_m += 6;
	_grpcount++;
	return true;
}


// [A-Z]{4}
bool SGMetar::scanId()
{
	char *m = _m;
	for (int i = 0; i < 4; m++, i++)
		if (!(isalpha(*m) || isdigit(*m)))
			return false;
	if (!scanBoundary(&m))
		return false;
	strncpy(_icao, _m, 4);
	_icao[4] = '\0';
	_m = m;
	_grpcount++;
	return true;
}


// \d{6}Z
bool SGMetar::scanDate()
{
	char *m = _m;
	int day, hour, minute;
	if (!scanNumber(&m, &day, 2))
		return false;
	if (!scanNumber(&m, &hour, 2))
		return false;
	if (!scanNumber(&m, &minute, 2))
		return false;
	if (*m++ != 'Z')
		return false;
	if (!scanBoundary(&m))
		return false;
	_day = day;
	_hour = hour;
	_minute = minute;
	_m = m;
	_grpcount++;
	return true;
}


// (NIL|AUTO|COR|RTD)
bool SGMetar::scanModifier()
{
	char *m = _m;
	int type;
	if (!strncmp(m, "NIL", 3)) {
		_m += strlen(_m);
		return true;
	}
	if (!strncmp(m, "AUTO", 4))			// automatically generated
		m += 4, type = AUTO;
	else if (!strncmp(m, "COR", 3))			// manually corrected
		m += 3, type = COR;
	else if (!strncmp(m, "RTD", 3))			// routine delayed
		m += 3, type = RTD;
	else
		return false;
	if (!scanBoundary(&m))
		return false;
	_report_type = type;
	_m = m;
	_grpcount++;
	return true;
}


// (\d{3}|VRB)\d{1,3}(G\d{2,3})?(KT|KMH|MPS)
bool SGMetar::scanWind()
{
	char *m = _m;
	int dir;
	if (!strncmp(m, "VRB", 3))
		m += 3, dir = -1;
	else if (!scanNumber(&m, &dir, 3))
		return false;

	int i;
	if (!scanNumber(&m, &i, 2, 3))
		return false;
	double speed = i;

	double gust = NaN;
	if (*m == 'G') {
		m++;
		if (!scanNumber(&m, &i, 2, 3))
			return false;
		gust = i;
	}
	double factor;
	if (!strncmp(m, "KT", 2))
		m += 2, factor = SG_KT_TO_MPS;
	else if (!strncmp(m, "KMH", 3))
		m += 3, factor = SG_KMH_TO_MPS;
	else if (!strncmp(m, "KPH", 3))		// ??
		m += 3, factor = SG_KMH_TO_MPS;
	else if (!strncmp(m, "MPS", 3))
		m += 3, factor = 1.0;
	else
		return false;
	if (!scanBoundary(&m))
		return false;
	_m = m;
	_wind_dir = dir;
	_wind_speed = speed * factor;
	if (gust != NaN)
		_gust_speed = gust * factor;
	_grpcount++;
	return true;
}


// \d{3}V\d{3}
bool SGMetar::scanVariability()
{
	char *m = _m;
	int from, to;
	if (!scanNumber(&m, &from, 3))
		return false;
	if (*m++ != 'V')
		return false;
	if (!scanNumber(&m, &to, 3))
		return false;
	if (!scanBoundary(&m))
		return false;
	_m = m;
	_wind_range_from = from;
	_wind_range_to = to;
	_grpcount++;
	return true;
}


bool SGMetar::scanVisibility()
// TODO: if only directed vis are given, do still set min/max
{
	if (!strncmp(_m, "//// ", 5)) {         // spec compliant?
		_m += 5;
		_grpcount++;
		return true;
	}

	char *m = _m;
	double distance;
	int i, dir = -1;
	int modifier = SGMetarVisibility::EQUALS;
// \d{4}(N|NE|E|SE|S|SW|W|NW)?
	if (scanNumber(&m, &i, 4)) {
		if( strncmp( m, "NDV",3 ) == 0 ) {
			m+=3; // tolerate NDV (no directional validation)
		} else if (*m == 'E') {
			m++, dir = 90;
		} else if (*m == 'W') {
			m++, dir = 270;
		} else if (*m == 'N') {
			m++;
			if (*m == 'E')
				m++, dir = 45;
			else if (*m == 'W')
				m++, dir = 315;
			else
				dir = 0;
		} else if (*m == 'S') {
			m++;
			if (*m == 'E')
				m++, dir = 135;
			else if (*m == 'W')
				m++, dir = 225;
			else
				dir = 180;
                }
		if (i == 0)
			i = 50, modifier = SGMetarVisibility::LESS_THAN;
		else if (i == 9999)
			i++, modifier = SGMetarVisibility::GREATER_THAN;
		distance = i;
	} else {
// M?(\d{1,2}|\d{1,2}/\d{1,2}|\d{1,2} \d{1,2}/\d{1,2})(SM|KM)
		if (*m == 'M')
			m++, modifier = SGMetarVisibility::LESS_THAN;

		if (!scanNumber(&m, &i, 1, 2))
			return false;
		distance = i;

		if (*m == '/') {
			m++;
			if (!scanNumber(&m, &i, 1, 2))
				return false;
			distance /= i;
		} else if (*m == ' ') {
			m++;
			int denom;
			if (!scanNumber(&m, &i, 1, 2))
				return false;
			if (*m++ != '/')
				return false;
			if (!scanNumber(&m, &denom, 1, 2))
				return false;
			distance += (double)i / denom;
		}

		if (!strncmp(m, "SM", 2))
			distance *= SG_SM_TO_METER, m += 2;
		else if (!strncmp(m, "KM", 2))
			distance *= 1000, m += 2;
		else
			return false;
	}
	if (!scanBoundary(&m))
		return false;

	SGMetarVisibility *v;
	if (dir != -1)
		v = &_dir_visibility[dir / 45];
	else if (_min_visibility._distance == NaN)
		v = &_min_visibility;
	else
		v = &_max_visibility;

	v->_distance = distance;
	v->_modifier = modifier;
	v->_direction = dir;
	_m = m;
	_grpcount++;
	return true;
}


// R\d\d[LCR]?/([PM]?\d{4}V)?[PM]?\d{4}(FT)?[DNU]?
bool SGMetar::scanRwyVisRange()
{
	char *m = _m;
	int i;
	SGMetarRunway r;
	if (*m++ != 'R')
		return false;
	if (!scanNumber(&m, &i, 2))
		return false;
	if (*m == 'L' || *m == 'C' || *m == 'R')
		m++;

	char id[4];
	strncpy(id, _m + 1, i = m - _m - 1);
	id[i] = '\0';

	if (*m++ != '/')
		return false;

	int from, to;
	if (*m == 'P')
		m++, r._min_visibility._modifier = SGMetarVisibility::GREATER_THAN;
	else if (*m == 'M')
		m++, r._min_visibility._modifier = SGMetarVisibility::LESS_THAN;
	if (!scanNumber(&m, &from, 4))
		return false;
	if (*m == 'V') {
		m++;
		if (*m == 'P')
			m++, r._max_visibility._modifier = SGMetarVisibility::GREATER_THAN;
		else if (*m == 'M')
			m++, r._max_visibility._modifier = SGMetarVisibility::LESS_THAN;
		if (!scanNumber(&m, &to, 4))
			return false;
	} else
		to = from;

	if (!strncmp(m, "FT", 2)) {
		from = int(from * SG_FEET_TO_METER);
		to = int(to * SG_FEET_TO_METER);
		m += 2;
	}
	r._min_visibility._distance = from;
	r._max_visibility._distance = to;

	if (*m == '/')					// this is not in the spec!
		m++;
	if (*m == 'D')
		m++, r._min_visibility._tendency = SGMetarVisibility::DECREASING;
	else if (*m == 'N')
		m++, r._min_visibility._tendency = SGMetarVisibility::STABLE;
	else if (*m == 'U')
		m++, r._min_visibility._tendency = SGMetarVisibility::INCREASING;

	if (!scanBoundary(&m))
		return false;
	_m = m;

	_runways[id]._min_visibility = r._min_visibility;
	_runways[id]._max_visibility = r._max_visibility;
	_grpcount++;
	return true;
}


static const struct Token special[] = {
	{ "NSW",  "no significant weather" },
/*	{ "VCSH", "showers in the vicinity" },
	{ "VCTS", "thunderstorm in the vicinity" }, */
	{ 0, 0 }
};


static const struct Token description[] = {
	{ "SH", "showers of" },
	{ "TS", "thunderstorm with" },
	{ "BC", "patches of" },
	{ "BL", "blowing" },
	{ "DR", "low drifting" },
	{ "FZ", "freezing" },
	{ "MI", "shallow" },
	{ "PR", "partial" },
	{ 0, 0 }
};


static const struct Token phenomenon[] = {
	{ "DZ",   "drizzle" },
	{ "GR",   "hail" },
	{ "GS",   "small hail and/or snow pellets" },
	{ "IC",   "ice crystals" },
	{ "PE",   "ice pellets" },
	{ "RA",   "rain" },
	{ "SG",   "snow grains" },
	{ "SN",   "snow" },
	{ "UP",   "unknown precipitation" },
	{ "BR",   "mist" },
	{ "DU",   "widespread dust" },
	{ "FG",   "fog" },
	{ "FGBR", "fog bank" },
	{ "FU",   "smoke" },
	{ "HZ",   "haze" },
	{ "PY",   "spray" },
	{ "SA",   "sand" },
	{ "VA",   "volcanic ash" },
	{ "DS",   "duststorm" },
	{ "FC",   "funnel cloud/tornado waterspout" },
	{ "PO",   "well-developed dust/sand whirls" },
	{ "SQ",   "squalls" },
	{ "SS",   "sandstorm" },
	{ "UP",   "unknown" },	// ... due to failed automatic acquisition
	{ 0, 0 }
};


// (+|-|VC)?(NSW|MI|PR|BC|DR|BL|SH|TS|FZ)?((DZ|RA|SN|SG|IC|PE|GR|GS|UP){0,3})(BR|FG|FU|VA|DU|SA|HZ|PY|PO|SQ|FC|SS|DS){0,3}
bool SGMetar::scanWeather()
{
	char *m = _m;
	string weather;
	const struct Token *a;

	if ((a = scanToken(&m, special))) {
		if (!scanBoundary(&m))
			return false;
		_weather.push_back(a->text);
		_m = m;
		return true;
	}

	string pre, post;
    struct Weather w;
	if (*m == '-')
		m++, pre = "light ", w.intensity = LIGHT;
	else if (*m == '+')
		m++, pre = "heavy ", w.intensity = HEAVY;
	else if (!strncmp(m, "VC", 2))
        m += 2, post = "in the vicinity ", w.vincinity=true;
	else
		pre = "moderate ", w.intensity = MODERATE;

	int i;
	for (i = 0; i < 3; i++) {
		if (!(a = scanToken(&m, description)))
			break;
		w.descriptions.push_back(a->id);
		weather += string(a->text) + " ";
	}

	for (i = 0; i < 3; i++) {
		if (!(a = scanToken(&m, phenomenon)))
			break;
        w.phenomena.push_back(a->id);
		weather += string(a->text) + " ";
		if (!strcmp(a->id, "RA"))
			_rain = w.intensity;
		else if (!strcmp(a->id, "HA"))
			_hail = w.intensity;
		else if (!strcmp(a->id, "SN"))
			_snow = w.intensity;
	}
	if (!weather.length())
		return false;
	if (!scanBoundary(&m))
		return false;
	_m = m;
	weather = pre + weather + post;
	weather.erase(weather.length() - 1);
	_weather.push_back(weather);
    if( w.phenomena.size() > 0 )
        _weather2.push_back( w );
	_grpcount++;
	return true;
}


static const struct Token cloud_types[] = {
	{ "AC",    "altocumulus" },
	{ "ACC",   "altocumulus castellanus" },
	{ "ACSL",  "altocumulus standing lenticular" },
	{ "AS",    "altostratus" },
	{ "CB",    "cumulonimbus" },
	{ "CBMAM", "cumulonimbus mammatus" },
	{ "CC",    "cirrocumulus" },
	{ "CCSL",  "cirrocumulus standing lenticular" },
	{ "CI",    "cirrus" },
	{ "CS",    "cirrostratus" },
	{ "CU",    "cumulus" },
	{ "CUFRA", "cumulus fractus" },
	{ "NS",    "nimbostratus" },
	{ "SAC",   "stratoaltocumulus" },		// guessed
	{ "SC",    "stratocumulus" },
	{ "SCSL",  "stratocumulus standing lenticular" },
	{ "ST",    "stratus" },
	{ "STFRA", "stratus fractus" },
	{ "TCU",   "towering cumulus" },
	{ 0, 0 }
};

#include <iostream>
// (FEW|SCT|BKN|OVC|SKC|CLR|CAVOK|VV)([0-9]{3}|///)?[:cloud_type:]?
bool SGMetar::scanSkyCondition()
{
	char *m = _m;
	int i;
	SGMetarCloud cl;

	if (!strncmp(m, "//////", 6)) {
		m += 6;
		if (!scanBoundary(&m))
			return false;
		_m = m;
		return true;
	}

	if (!strncmp(m, "CLR", i = 3)				// clear
			|| !strncmp(m, "SKC", i = 3)		// sky clear
			|| !strncmp(m, "NCD", i = 3)		// nil cloud detected
			|| !strncmp(m, "NSC", i = 3)		// no significant clouds
			|| !strncmp(m, "CAVOK", i = 5))	{	// ceiling and visibility OK (implies 9999)
		m += i;
		if (!scanBoundary(&m))
			return false;

		if (i == 3) {
            cl._coverage = SGMetarCloud::COVERAGE_CLEAR;
			_clouds.push_back(cl);
		} else {
			_cavok = true;
		}
		_m = m;
		return true;
	}

	if (!strncmp(m, "VV", i = 2))				// vertical visibility
		;
	else if (!strncmp(m, "FEW", i = 3))
        cl._coverage = SGMetarCloud::COVERAGE_FEW;
	else if (!strncmp(m, "SCT", i = 3))
        cl._coverage = SGMetarCloud::COVERAGE_SCATTERED;
	else if (!strncmp(m, "BKN", i = 3))
        cl._coverage = SGMetarCloud::COVERAGE_BROKEN;
	else if (!strncmp(m, "OVC", i = 3))
        cl._coverage = SGMetarCloud::COVERAGE_OVERCAST;
	else
		return false;
	m += i;

	if (!strncmp(m, "///", 3))	// vis not measurable (e.g. because of heavy snowing)
		m += 3, i = -1;
	else if (scanBoundary(&m)) {
		_m = m;
		return true;				// ignore single OVC/BKN/...
	} else if (!scanNumber(&m, &i, 3))
		i = -1;

    if (cl._coverage == SGMetarCloud::COVERAGE_NIL) {
		if (!scanBoundary(&m))
			return false;
		if (i == -1)			// 'VV///'
			_vert_visibility._modifier = SGMetarVisibility::NOGO;
		else
			_vert_visibility._distance = i * 100 * SG_FEET_TO_METER;
		_m = m;
		return true;
	}

	if (i != -1)
		cl._altitude = i * 100 * SG_FEET_TO_METER;

	const struct Token *a;
	if ((a = scanToken(&m, cloud_types))) {
		cl._type = a->id;
		cl._type_long = a->text;
	}
	if (!scanBoundary(&m))
		return false;
	_clouds.push_back(cl);
	_m = m;
	_grpcount++;
	return true;
}


// M?[0-9]{2}/(M?[0-9]{2})?            (spec)
// (M?[0-9]{2}|XX)/(M?[0-9]{2}|XX)?    (Namibia)
bool SGMetar::scanTemperature()
{
	char *m = _m;
	int sign = 1, temp, dew;
	if (!strncmp(m, "XX/XX", 5)) {		// not spec compliant!
		_m += 5;
		return scanBoundary(&_m);
	}

	if (*m == 'M')
		m++, sign = -1;
	if (!scanNumber(&m, &temp, 2))
		return false;
	temp *= sign;

	if (*m++ != '/')
		return false;
	if (!scanBoundary(&m)) {
		if (!strncmp(m, "XX", 2))	// not spec compliant!
			m += 2, sign = 0, dew = temp;
		else {
			sign = 1;
			if (*m == 'M')
				m++, sign = -1;
			if (!scanNumber(&m, &dew, 2))
				return false;
		}
		if (!scanBoundary(&m))
			return false;
		if (sign)
			_dewp = sign * dew;
	}
	_temp = temp;
	_m = m;
	_grpcount++;
	return true;
}


double SGMetar::getRelHumidity() const
{
	if (_temp == NaN || _dewp == NaN)
		return NaN;
	double dewp = pow(10.0, 7.5 * _dewp / (237.7 + _dewp));
	double temp = pow(10.0, 7.5 * _temp / (237.7 + _temp));
	return dewp * 100 / temp;
}


// [AQ]\d{4}             (spec)
// [AQ]\d{2}(\d{2}|//)   (Namibia)
bool SGMetar::scanPressure()
{
	char *m = _m;
	double factor;
	int press, i;

	if (*m == 'A')
		factor = SG_INHG_TO_PA / 100;
	else if (*m == 'Q')
		factor = 100;
	else
		return false;
	m++;
	if (!scanNumber(&m, &press, 2))
		return false;
	press *= 100;
	if (!strncmp(m, "//", 2))	// not spec compliant!
		m += 2;
	else if (scanNumber(&m, &i, 2))
		press += i;
	else
		return false;
	if (!scanBoundary(&m))
		return false;
	_pressure = press * factor;
	_m = m;
	_grpcount++;
	return true;
}


static const char *runway_deposit[] = {
	"clear and dry",
	"damp",
	"wet or puddles",
	"frost",
	"dry snow",
	"wet snow",
	"slush",
	"ice",
	"compacted snow",
	"frozen ridges"
};


static const char *runway_deposit_extent[] = {
	0, "1-10%", "11-25%", 0, 0, "26-50%", 0, 0, 0, "51-100%"
};


static const char *runway_friction[] = {
	0,
	"poor braking action",
	"poor/medium braking action",
	"medium braking action",
	"medium/good braking action",
	"good braking action",
	0, 0, 0,
	"friction: unreliable measurement"
};


// \d\d(CLRD|[\d/]{4})(\d\d|//)
bool SGMetar::scanRunwayReport()
{
	char *m = _m;
	int i;
	char id[4];
	SGMetarRunway r;

	if (!scanNumber(&m, &i, 2))
		return false;
	if (i == 88)
		strcpy(id, "ALL");
	else if (i == 99)
		strcpy(id, "REP");		// repetition of previous report
	else if (i >= 50) {
		i -= 50;
		id[0] = i / 10 + '0', id[1] = i % 10 + '0', id[2] = 'R', id[3] = '\0';
	} else
		id[0] = i / 10 + '0', id[1] = i % 10 + '0', id[2] = '\0';

	if (!strncmp(m, "CLRD", 4)) {
		m += 4;							// runway cleared
		r._deposit_string = "cleared";
	} else {
		if (scanNumber(&m, &i, 1)) {
			r._deposit = i;
			r._deposit_string = runway_deposit[i];
		} else if (*m == '/')
			m++;
		else
			return false;

		if (*m == '1' || *m == '2' || *m == '5' || *m == '9') {	// extent of deposit
			r._extent = *m - '0';
			r._extent_string = runway_deposit_extent[*m - '0'];
		} else if (*m != '/')
			return false;

		m++;
		i = -1;
		if (!strncmp(m, "//", 2))
			m += 2;
		else if (!scanNumber(&m, &i, 2))
			return false;

		if (i == 0)
			r._depth = 0.0005;				// < 1 mm deep (let's say 0.5 :-)
		else if (i > 0 && i <= 90)
			r._depth = i / 1000.0;				// i mm deep
		else if (i >= 92 && i <= 98)
			r._depth = (i - 90) / 20.0;
		else if (i == 99)
			r._comment = "runway not in use";
		else if (i == -1)					// no depth given ("//")
			;
		else
			return false;
	}
	i = -1;
	if (m[0] == '/' && m[1] == '/')
		m += 2;
	else if (!scanNumber(&m, &i, 2))
		return false;
	if (i >= 1 && i < 90) {
		r._friction = i / 100.0;
	} else if ((i >= 91 && i <= 95) || i == 99) {
		r._friction_string = runway_friction[i - 90];
	}
	if (!scanBoundary(&m))
		return false;

	_runways[id]._deposit = r._deposit;
	_runways[id]._deposit_string = r._deposit_string;
	_runways[id]._extent = r._extent;
	_runways[id]._extent_string = r._extent_string;
	_runways[id]._depth = r._depth;
	_runways[id]._friction = r._friction;
	_runways[id]._friction_string = r._friction_string;
	_runways[id]._comment = r._comment;
	_m = m;
	_grpcount++;
	return true;
}


// WS (ALL RWYS?|RWY ?\d\d[LCR]?)?
bool SGMetar::scanWindShear()
{
	char *m = _m;
	if (strncmp(m, "WS", 2))
		return false;
	m += 2;
	if (!scanBoundary(&m))
		return false;

	if (!strncmp(m, "ALL", 3)) {
		m += 3;
		if (!scanBoundary(&m))
			return false;
		if (strncmp(m, "RWY", 3))
			return false;
		m += 3;
		if (*m == 'S')
			m++;
		if (!scanBoundary(&m))
			return false;
		_runways["ALL"]._wind_shear = true;
		_m = m;
		return true;
	}

	char id[4], *mm;
	int i, cnt;
	for (cnt = 0;; cnt++) {			// ??
		if (strncmp(m, "RWY", 3))
			break;
		m += 3;
		scanBoundary(&m);
		mm = m;
		if (!scanNumber(&m, &i, 2))
			return false;
		if (*m == 'L' || *m == 'C' || *m == 'R')
			m++;
		strncpy(id, mm, i = m - mm);
		id[i] = '\0';
		if (!scanBoundary(&m))
			return false;
		_runways[id]._wind_shear = true;
	}
	if (!cnt)
		_runways["ALL"]._wind_shear = true;
	_m = m;
	return true;
}


bool SGMetar::scanTrendForecast()
{
	char *m = _m;
	if (strncmp(m, "NOSIG", 5))
		return false;

	m += 5;
	if (!scanBoundary(&m))
		return false;
	_m = m;
	return true;
}


// (BLU|WHT|GRN|YLO|AMB|RED)
static const struct Token colors[] = {
	{ "BLU", "Blue" },      // 2500 ft,  8.0 km
	{ "WHT", "White" },     // 1500 ft,  5.0 km
	{ "GRN", "Green" },     //  700 ft,  3.7 km
	{ "YLO", "Yellow" },	//  300 ft,  1.6 km
	{ "AMB", "Amber" },     //  200 ft,  0.8 km
	{ "RED", "Red" },       // <200 ft, <0.8 km
	{ 0, 0 }
};


bool SGMetar::scanColorState()
{
	char *m = _m;
	const struct Token *a;
	if (!(a = scanToken(&m, colors)))
		return false;
	if (!scanBoundary(&m))
		return false;
	//printf(Y"Code %s\n"N, a->text);
	_m = m;
	return true;
}


bool SGMetar::scanRemark()
{
	if (strncmp(_m, "RMK", 3))
		return false;
	_m += 3;
	if (!scanBoundary(&_m))
		return false;

	while (*_m) {
		if (!scanRunwayReport()) {
			while (*_m && !isspace(*_m))
				_m++;
			scanBoundary(&_m);
		}
	}
	return true;
}


bool SGMetar::scanRemainder()
{
	char *m = _m;
	if (!(strncmp(m, "NOSIG", 5))) {
		m += 5;
		if (scanBoundary(&m))
			_m = m; //_comment.push_back("No significant tendency");
	}

	if (!scanBoundary(&m))
		return false;
	_m = m;
	return true;
}


bool SGMetar::scanBoundary(char **s)
{
	if (**s && !isspace(**s))
		return false;
	while (isspace(**s))
		(*s)++;
	return true;
}


int SGMetar::scanNumber(char **src, int *num, int min, int max)
{
	int i;
	char *s = *src;
	*num = 0;
	for (i = 0; i < min; i++) {
		if (!isdigit(*s))
			return 0;
		else
			*num = *num * 10 + *s++ - '0';
	}
	for (; i < max && isdigit(*s); i++)
		*num = *num * 10 + *s++ - '0';
	*src = s;
	return i;
}


// find longest match of str in list
const struct Token *SGMetar::scanToken(char **str, const struct Token *list)
{
	const struct Token *longest = 0;
	int maxlen = 0, len;
	const char *s;
	for (int i = 0; (s = list[i].id); i++) {
		len = strlen(s);
		if (!strncmp(s, *str, len) && len > maxlen) {
			maxlen = len;
			longest = &list[i];
		}
	}
	*str += maxlen;
	return longest;
}


void SGMetarCloud::set(double alt, Coverage cov)
{
	_altitude = alt;
	if (cov != -1)
		_coverage = cov;
}

SGMetarCloud::Coverage SGMetarCloud::getCoverage( const std::string & coverage ) 
{
	if( coverage == "clear" ) return COVERAGE_CLEAR;
	if( coverage == "few" ) return COVERAGE_FEW;
	if( coverage == "scattered" ) return COVERAGE_SCATTERED;
	if( coverage == "broken" ) return COVERAGE_BROKEN;
	if( coverage == "overcast" ) return COVERAGE_OVERCAST;
	return COVERAGE_NIL;
}

const char * SGMetarCloud::COVERAGE_NIL_STRING = "nil";
const char * SGMetarCloud::COVERAGE_CLEAR_STRING = "clear";
const char * SGMetarCloud::COVERAGE_FEW_STRING = "few";
const char * SGMetarCloud::COVERAGE_SCATTERED_STRING = "scattered";
const char * SGMetarCloud::COVERAGE_BROKEN_STRING = "broken";
const char * SGMetarCloud::COVERAGE_OVERCAST_STRING = "overcast";

void SGMetarVisibility::set(double dist, int dir, int mod, int tend)
{
	_distance = dist;
	if (dir != -1)
		_direction = dir;
	if (mod != -1)
		_modifier = mod;
	if (tend != 1)
		_tendency = tend;
}

#undef NaN
