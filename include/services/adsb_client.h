#pragma once

#include <cstddef>

namespace services::adsb {

// --- Data source configuration (local tar1090/adsb.im, adsb.fi fallback) ---

/** Max length of the stored local source URL (incl. NUL). */
constexpr size_t kLocalUrlMaxLen = 128;

/** Load the persisted local source URL from NVS. Call once after boot. */
void configInit();

/**
 * Full URL of a local tar1090/adsb.im aircraft.json endpoint, e.g.
 * "http://192.168.0.199:8080/data/aircraft.json". Empty string = local source
 * disabled → fetchUpdate() uses adsb.fi directly.
 */
const char* localUrl();

/** Persist the local source URL to NVS (empty clears it). */
void saveLocalUrl(const char* url);

/** Remove the stored local source URL from NVS and reset to empty. */
void clearLocalUrl();

/**
 * True if the most recent successful fetchUpdate() served data from the local
 * source. Drives the poll interval (local is faster). False before the first
 * success or after a fallback to adsb.fi.
 */
bool usingLocalSource();

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
 * Fetch aircraft within fetch_radius_km of center_lat/lon. Tries the local
 * source (see localUrl()) first, falling back to adsb.fi if it is unset or
 * unreachable. Filtering (radius + types) happens while parsing, before the
 * kMaxAircraft cap applies, so the slots go to matching traffic instead of
 * whatever arrived first. altitude_in_meters formats the altitude tag in
 * meters instead of feet.
 */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km,
                 const TypeFilter& types = {}, bool altitude_in_meters = false);

}  // namespace services::adsb
