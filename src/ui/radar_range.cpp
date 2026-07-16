#include "ui/radar_range.h"

#include "ui/radar_theme.h"

#include <Preferences.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui::radar {

namespace {

constexpr char kPrefsNamespace[] = "planeradar";
constexpr char kPrefsRangeKey[] = "rangeIdx";
constexpr char kPrefsMilesKey[] = "useMiles";
constexpr char kPrefsRunwaysKey[] = "showRwys";
constexpr char kPrefsBelugaKey[] = "belugaOnly";
constexpr char kPrefsAirbusKey[] = "airbusOnly";
constexpr char kPrefsAltMKey[] = "altMeters";
constexpr uint8_t kDefaultRangeIndex = 2;  // 10 km ring
constexpr float kKmPerMile = 1.609344f;

Preferences s_prefs;
uint8_t s_range_index = kDefaultRangeIndex;
bool s_use_miles = false;
bool s_show_runways = true;
/** Off by default: on, the radar is empty unless a Beluga is actually in range. */
bool s_beluga_only = false;
/** Off by default: on, only Airbus (incl. Beluga) are shown. */
bool s_airbus_only = false;
/** false = altitude tags in feet (source unit); true = meters. */
bool s_alt_meters = false;

void saveRangeIndex() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putUChar(kPrefsRangeKey, s_range_index);
  s_prefs.end();
}

void saveUseMiles() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsMilesKey, s_use_miles);
  s_prefs.end();
}

void saveShowRunways() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsRunwaysKey, s_show_runways);
  s_prefs.end();
}

void saveBelugaOnly() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsBelugaKey, s_beluga_only);
  s_prefs.end();
}

void saveAirbusOnly() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsAirbusKey, s_airbus_only);
  s_prefs.end();
}

void saveAltMeters() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsAltMKey, s_alt_meters);
  s_prefs.end();
}

bool portalCheckboxChecked(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  // WiFiManager checkbox submits its value= attribute ("T", or "F" if we prefilled F).
  if ((value[0] == 'T' || value[0] == 't' || value[0] == 'F' || value[0] == 'f') &&
      value[1] == '\0') {
    return true;
  }
  return strcmp(value, "on") == 0;
}

}  // namespace

void rangeInit() {
  if (!s_prefs.begin(kPrefsNamespace, true)) {
    return;
  }
  const uint8_t saved = s_prefs.getUChar(kPrefsRangeKey, kDefaultRangeIndex);
  s_range_index =
      (saved < kRangePresetCount) ? saved : kDefaultRangeIndex;
  s_use_miles = s_prefs.getBool(kPrefsMilesKey, false);
  s_show_runways = s_prefs.getBool(kPrefsRunwaysKey, true);
  s_beluga_only = s_prefs.getBool(kPrefsBelugaKey, false);
  s_airbus_only = s_prefs.getBool(kPrefsAirbusKey, false);
  s_alt_meters = s_prefs.getBool(kPrefsAltMKey, false);
  s_prefs.end();
}

void rangeNext() {
  s_range_index = static_cast<uint8_t>((s_range_index + 1) % kRangePresetCount);
  saveRangeIndex();
}

const RangePreset& rangeCurrent() { return kRangePresets[s_range_index]; }

uint8_t rangeIndex() { return s_range_index; }

float fetchRadiusKm() {
  const float outer_km = rangeCurrent().outer_km;
  const float screen_r_px =
      static_cast<float>(kCenterX - kBeyondRingScreenMarginPx);
  return outer_km * (screen_r_px / static_cast<float>(kGridOuterRadius));
}

bool useMiles() { return s_use_miles; }

bool showRunways() { return s_show_runways; }

bool belugaOnly() { return s_beluga_only; }

bool airbusOnly() { return s_airbus_only; }

bool altMeters() { return s_alt_meters; }

void saveMilesFromPortal(const char* checkbox_value) {
  s_use_miles = portalCheckboxChecked(checkbox_value);
  saveUseMiles();
  Serial.printf("Distance units: %s\n", s_use_miles ? "miles" : "km");
}

void saveRunwaysFromPortal(const char* checkbox_value) {
  s_show_runways = portalCheckboxChecked(checkbox_value);
  saveShowRunways();
  Serial.printf("Runway overlay: %s\n", s_show_runways ? "on" : "off");
}

void saveBelugaOnlyFromPortal(const char* checkbox_value) {
  s_beluga_only = portalCheckboxChecked(checkbox_value);
  saveBelugaOnly();
  Serial.printf("Beluga filter: %s\n", s_beluga_only ? "on" : "off");
}

void saveAirbusOnlyFromPortal(const char* checkbox_value) {
  s_airbus_only = portalCheckboxChecked(checkbox_value);
  saveAirbusOnly();
  Serial.printf("Airbus filter: %s\n", s_airbus_only ? "on" : "off");
}

void saveAltMetersFromPortal(const char* checkbox_value) {
  s_alt_meters = portalCheckboxChecked(checkbox_value);
  saveAltMeters();
  Serial.printf("Altitude units: %s\n", s_alt_meters ? "meters" : "feet");
}

void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles) {
  if (use_miles) {
    const int mi = static_cast<int>(lroundf(ring3_km / kKmPerMile));
    snprintf(buf, len, "%dmi", mi);
  } else {
    const int km = static_cast<int>(lroundf(ring3_km));
    snprintf(buf, len, "%dkm", km);
  }
}

void formatCurrentRing3Label(char* buf, size_t len) {
  formatRing3Label(buf, len, rangeCurrent().ring3_km, s_use_miles);
}

void unitsReset() {
  s_use_miles = false;
  s_show_runways = true;
  s_beluga_only = false;
  s_airbus_only = false;
  s_alt_meters = false;
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.remove(kPrefsMilesKey);
    s_prefs.remove(kPrefsRunwaysKey);
    s_prefs.remove(kPrefsBelugaKey);
    s_prefs.remove(kPrefsAirbusKey);
    s_prefs.remove(kPrefsAltMKey);
    s_prefs.end();
  }
}

}  // namespace ui::radar
