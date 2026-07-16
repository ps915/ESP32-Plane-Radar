#pragma once

#include <cstddef>

namespace services::adsb {

struct Aircraft {
  float lat;
  float lon;
  float nose_deg;
  float track_deg;
  float gs_knots;
  char callsign[9];
  char type[5];
  char alt[12];
};

constexpr size_t kMaxAircraft = 64;

size_t aircraftCount();
const Aircraft* aircraftList();

/** Hook invoked during long HTTP I/O (e.g. wifiLoop). Optional. */
using PollFn = void (*)();
void setPollFn(PollFn fn);

/**
 * Predicate over an already-trimmed ICAO DOC 8643 type designator (the ADS-B
 * "t" field), e.g. "A3ST". Returns true to keep the aircraft. Supplied by the
 * caller (see aircraft::isBeluga / isAirbus) so the services layer stays free
 * of manufacturer knowledge.
 */
using TypeMatchFn = bool (*)(const char* type);

/**
 * Type filter. Default (match == nullptr) keeps every aircraft. When a match
 * function is set, aircraft that report no type are dropped — an unknown type
 * cannot be proven to pass the predicate.
 */
struct TypeFilter {
  TypeMatchFn match = nullptr;
};

/**
 * Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi.
 * Filtering happens while parsing, before the kMaxAircraft cap applies, so the
 * slots go to matching traffic instead of whatever arrived first.
 * altitude_in_meters formats the altitude tag in meters instead of feet.
 */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km,
                 const TypeFilter& types = {}, bool altitude_in_meters = false);

}  // namespace services::adsb
