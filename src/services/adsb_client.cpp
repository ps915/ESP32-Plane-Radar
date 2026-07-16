#include "services/adsb_client.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>
#include <Preferences.h>

#include <cmath>
#include <cstring>

#include "config.h"

namespace services::adsb {

namespace {

constexpr char kApiBase[] = "https://opendata.adsb.fi/api/v3/lat/";
constexpr float kKmPerNm = 1.852f;
constexpr int kConnectAttemptMs = 200;
constexpr unsigned long kRequestTimeoutMs = 10000;

/** Own NVS namespace — see CLAUDE.md: keep source config off the other three. */
constexpr char kSrcPrefsNamespace[] = "adsbsrc";
constexpr char kSrcKeyUrl[] = "url";

Aircraft s_aircraft[kMaxAircraft];
size_t s_aircraft_count = 0;
PollFn s_poll_fn = nullptr;

/** Local tar1090/adsb.im aircraft.json URL; empty = disabled → adsb.fi only. */
char s_local_url[kLocalUrlMaxLen] = "";
/** Source that served the last successful fetch (drives the poll interval). */
bool s_using_local = false;

void pollNetwork() {
  if (s_poll_fn != nullptr) {
    s_poll_fn();
  }
}

int performGetWithPoll(HTTPClient& http, unsigned long budget_ms) {
  http.setConnectTimeout(kConnectAttemptMs);
  const unsigned long deadline = millis() + budget_ms;
  while (millis() < deadline) {
    pollNetwork();
    const int code = http.GET();
    if (code > 0) {
      return code;
    }
    if (code != HTTPC_ERROR_CONNECTION_REFUSED &&
        code != HTTPC_ERROR_NOT_CONNECTED) {
      return code;
    }
    delay(5);
  }
  return HTTPC_ERROR_READ_TIMEOUT;
}

bool readResponseBodyWithPoll(HTTPClient& http, String& payload,
                              unsigned long budget_ms) {
  WiFiClient* stream = http.getStreamPtr();
  if (stream == nullptr) {
    return false;
  }

  const int content_length = http.getSize();
  if (content_length > 0) {
    payload.reserve(static_cast<unsigned>(content_length + 1));
  }

  uint8_t buffer[512];
  const unsigned long deadline = millis() + budget_ms;
  while (millis() < deadline) {
    pollNetwork();
    const int available = stream->available();
    if (available > 0) {
      const int to_read =
          available > static_cast<int>(sizeof(buffer)) ? static_cast<int>(sizeof(buffer))
                                                       : available;
      const int read_bytes = stream->readBytes(buffer, to_read);
      if (read_bytes > 0) {
        payload.concat(reinterpret_cast<const char*>(buffer),
                       static_cast<unsigned>(read_bytes));
      }
    }
    if (content_length > 0 &&
        static_cast<int>(payload.length()) >= content_length) {
      break;
    }
    if (!http.connected() && stream->available() <= 0) {
      break;
    }
    delay(1);
  }

  return payload.length() > 0;
}

float kmToNauticalMiles(float km) { return km / kKmPerNm; }

/**
 * Great-circle distance (km), equirectangular approximation — accurate to well
 * under 1% at radar ranges. Mirrors adsb.fi's server-side `dist` semantics so
 * the local source (which returns everything) is filtered to the same set.
 */
float distanceKm(double lat1, double lon1, double lat2, double lon2) {
  constexpr double kKmPerDeg = 111.195;  // 2·π·6371 / 360
  constexpr double kDegToRad = 0.017453292519943295;
  const double dlat = (lat2 - lat1) * kKmPerDeg;
  const double mean_lat = (lat1 + lat2) * 0.5 * kDegToRad;
  const double dlon = (lon2 - lon1) * kKmPerDeg * cos(mean_lat);
  return static_cast<float>(sqrt(dlat * dlat + dlon * dlon));
}

bool readJsonFloat(const JsonObject& obj, const char* key, float* out) {
  if (obj[key].is<float>() || obj[key].is<double>() || obj[key].is<int>()) {
    *out = obj[key].as<float>();
    return true;
  }
  return false;
}

float pickNoseHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickTrackHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickGroundSpeed(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "gs", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "tas", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "ias", &v)) {
    return v;
  }
  return 0.0f;
}

bool isOnGround(const JsonObject& plane) {
  if (!plane["alt_baro"].is<const char*>()) {
    return false;
  }
  return strcmp(plane["alt_baro"].as<const char*>(), "ground") == 0;
}

void copyJsonStringTrimmed(const JsonObject& obj, const char* key, char* out,
                           size_t out_len) {
  out[0] = '\0';
  if (out_len == 0 || !obj[key].is<const char*>()) {
    return;
  }
  const char* s = obj[key].as<const char*>();
  size_t n = strnlen(s, out_len - 1);
  while (n > 0 && s[n - 1] == ' ') {
    --n;
  }
  memcpy(out, s, n);
  out[n] = '\0';
}

void formatAltitudeTag(const JsonObject& plane, char* out, size_t out_len) {
  out[0] = '\0';
  if (out_len == 0) {
    return;
  }

  if (plane["alt_baro"].is<const char*>()) {
    const char* s = plane["alt_baro"].as<const char*>();
    if (strcmp(s, "ground") == 0) {
      strncpy(out, "GND", out_len - 1);
      out[out_len - 1] = '\0';
      return;
    }
  }

  float alt = 0.0f;
  if (readJsonFloat(plane, "alt_baro", &alt) ||
      readJsonFloat(plane, "alt_geom", &alt)) {
    snprintf(out, out_len, "%d ft", static_cast<int>(lroundf(alt)));
  }
}

void fillTagFields(Aircraft* ac, const JsonObject& plane) {
  copyJsonStringTrimmed(plane, "flight", ac->callsign, sizeof(ac->callsign));
  if (ac->callsign[0] == '\0') {
    copyJsonStringTrimmed(plane, "hex", ac->callsign, sizeof(ac->callsign));
  }

  copyJsonStringTrimmed(plane, "t", ac->type, sizeof(ac->type));
  formatAltitudeTag(plane, ac->alt, sizeof(ac->alt));
}

/**
 * Fetch + parse one source into s_aircraft. Returns false only on a transport,
 * HTTP, or parse failure (so the caller can fall back) — an empty aircraft list
 * is a success. `client` may be a plain WiFiClient (local http) or a
 * WiFiClientSecure (adsb.fi https). Both readsb-derived layouts are accepted:
 * the array key is `aircraft` (tar1090/adsb.im) or `ac` (adsb.fi). When
 * `server_filtered` is false the list is unfiltered, so each aircraft is dropped
 * here if it lies outside `fetch_radius_km` — before the kMaxAircraft cap, so a
 * far target never steals a slot from a nearby one.
 */
bool fetchFromSource(WiFiClient& client, const char* url, bool server_filtered,
                     unsigned long budget_ms, double center_lat,
                     double center_lon, float fetch_radius_km,
                     const char* label) {
  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.printf("adsb: http.begin failed (%s)\n", label);
    return false;
  }

  http.setTimeout(budget_ms);
  const int code = performGetWithPoll(http, budget_ms);
  if (code != HTTP_CODE_OK) {
    Serial.printf("adsb: HTTP %d (%s)\n", code, label);
    http.end();
    return false;
  }

  String payload;
  if (!readResponseBodyWithPoll(http, payload, budget_ms)) {
    Serial.printf("adsb: empty response (%s)\n", label);
    http.end();
    return false;
  }
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("adsb: JSON parse error (%s): %s\n", label, err.c_str());
    return false;
  }

  JsonArray planes = doc["aircraft"].as<JsonArray>();
  if (planes.isNull()) {
    planes = doc["ac"].as<JsonArray>();
  }
  if (planes.isNull()) {
    s_aircraft_count = 0;
    Serial.printf("adsb: 0 aircraft (%s)\n", label);
    return true;
  }

  size_t n = 0;
  for (JsonObject plane : planes) {
    if (n >= kMaxAircraft) {
      break;
    }
    if (!plane["lat"].is<float>() || !plane["lon"].is<float>()) {
      continue;
    }
    if (isOnGround(plane) && !config::kAdsbShowGroundAircraft) {
      continue;
    }

    const float plat = plane["lat"].as<float>();
    const float plon = plane["lon"].as<float>();
    if (!server_filtered &&
        distanceKm(center_lat, center_lon, plat, plon) > fetch_radius_km) {
      continue;
    }

    s_aircraft[n].lat = plat;
    s_aircraft[n].lon = plon;
    s_aircraft[n].nose_deg = pickNoseHeading(plane);
    s_aircraft[n].track_deg = pickTrackHeading(plane);
    s_aircraft[n].gs_knots = pickGroundSpeed(plane);
    fillTagFields(&s_aircraft[n], plane);
    ++n;
  }

  s_aircraft_count = n;
  Serial.printf("adsb: %u aircraft (%s)\n", static_cast<unsigned>(n), label);
  return true;
}

}  // namespace

void setPollFn(PollFn fn) { s_poll_fn = fn; }

size_t aircraftCount() { return s_aircraft_count; }

const Aircraft* aircraftList() { return s_aircraft; }

const char* localUrl() { return s_local_url; }

bool usingLocalSource() { return s_using_local; }

void configInit() {
  Preferences prefs;
  if (prefs.begin(kSrcPrefsNamespace, true)) {
    const String url = prefs.getString(kSrcKeyUrl, "");
    prefs.end();
    strncpy(s_local_url, url.c_str(), sizeof(s_local_url) - 1);
    s_local_url[sizeof(s_local_url) - 1] = '\0';
  }
  if (s_local_url[0] != '\0') {
    Serial.printf("adsb: local source %s\n", s_local_url);
  } else {
    Serial.println("adsb: no local source — using adsb.fi only");
  }
}

void clearLocalUrl() {
  s_local_url[0] = '\0';
  Preferences prefs;
  if (prefs.begin(kSrcPrefsNamespace, false)) {
    prefs.remove(kSrcKeyUrl);
    prefs.end();
  }
}

void saveLocalUrl(const char* url) {
  String trimmed(url != nullptr ? url : "");
  trimmed.trim();

  if (trimmed.length() == 0) {
    clearLocalUrl();
    Serial.println("adsb: local source cleared — using adsb.fi only");
    return;
  }
  if (!trimmed.startsWith("http://") && !trimmed.startsWith("https://")) {
    Serial.printf("adsb: ignoring local URL (need http:// or https://): %s\n",
                  trimmed.c_str());
    return;
  }
  if (trimmed.length() >= static_cast<int>(sizeof(s_local_url))) {
    Serial.println("adsb: local URL too long — not saved");
    return;
  }

  strncpy(s_local_url, trimmed.c_str(), sizeof(s_local_url) - 1);
  s_local_url[sizeof(s_local_url) - 1] = '\0';

  Preferences prefs;
  if (prefs.begin(kSrcPrefsNamespace, false)) {
    prefs.putString(kSrcKeyUrl, s_local_url);
    prefs.end();
  }
  Serial.printf("adsb: local source saved: %s\n", s_local_url);
}

bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km) {
  // 1. Local tar1090/adsb.im first, if configured. It returns the whole picture
  //    (no server-side range filter), so fetchFromSource filters by radius.
  if (s_local_url[0] != '\0') {
    WiFiClient client;
    if (fetchFromSource(client, s_local_url, /*server_filtered=*/false,
                        config::kAdsbLocalTimeoutMs, center_lat, center_lon,
                        fetch_radius_km, "local")) {
      s_using_local = true;
      return true;
    }
  }

  // 2. Fallback: public adsb.fi (already range-filtered via the dist param).
  const float dist_nm = kmToNauticalMiles(fetch_radius_km);
  String url = kApiBase;
  url += String(center_lat, 6);
  url += "/lon/";
  url += String(center_lon, 6);
  url += "/dist/";
  url += String(dist_nm, 1);

  WiFiClientSecure client;
  client.setInsecure();
  if (fetchFromSource(client, url.c_str(), /*server_filtered=*/true,
                      kRequestTimeoutMs, center_lat, center_lon, fetch_radius_km,
                      "adsb.fi")) {
    s_using_local = false;
    return true;
  }

  return false;
}

}  // namespace services::adsb
