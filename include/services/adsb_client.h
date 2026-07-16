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

/** Fetch aircraft within fetch_radius_km of center_lat/lon from adsb.fi. */
bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km);

}  // namespace services::adsb
