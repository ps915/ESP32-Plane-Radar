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
 * Allowlist of ICAO DOC 8643 type designators (the ADS-B "t" field), e.g. "A3ST".
 * Default (codes == nullptr) keeps every aircraft. Matching is case-insensitive.
 * Aircraft that report no type are dropped when a filter is active — an unknown
 * type cannot be proven to be on the list.
 */
struct TypeFilter {
  const char* const* codes = nullptr;
  size_t count = 0;
};

/**
 * Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi.
 * Filtering happens while parsing, before the kMaxAircraft cap applies, so the
 * slots go to matching traffic instead of whatever arrived first.
 */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km,
                 const TypeFilter& types = {});

}  // namespace services::adsb
