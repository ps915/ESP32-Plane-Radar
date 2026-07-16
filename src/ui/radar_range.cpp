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
constexpr char kPrefsSpdTextKey[] = "spdText";
constexpr char kPrefsSpdKmhKey[] = "spdKmh";
constexpr uint8_t kDefaultRangeIndex = 2;  // 10 km ring
constexpr float kKmPerMile = 1.609344f;
constexpr char kPrefsShowSpeedKey[] = "showSpeed";
constexpr char kPrefsShowAltKey[] = "showAlt";

constexpr char kPrefsColorGridKey[] = "colGrid";
constexpr char kPrefsColorAirbusKey[] = "colAirbus";
constexpr char kPrefsColorBoeingKey[] = "colBoeing";
constexpr char kPrefsColorBelugaKey[] = "colBeluga";
constexpr char kPrefsColorMilitaryKey[] = "colMil";
constexpr char kPrefsColorOtherKey[] = "colOther";

char s_color_grid[8] = "#106420";
char s_color_airbus[8] = "#005aff";
char s_color_boeing[8] = "#ff1e1e";
char s_color_beluga[8] = "#ffbe00";
char s_color_military[8] = "#808000";
char s_color_other[8] = "#c878ff";

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
/** false = speed shown as track vector line (default); true = numeric tag. */
bool s_speed_as_text = false;
/** false = speed tag in knots (source unit); true = km/h. */
bool s_speed_kmh = false;
bool s_show_speed = true;
bool s_show_altitude = true;

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

void saveSpeedAsText() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsSpdTextKey, s_speed_as_text);
  s_prefs.end();
}

void saveSpeedKmh() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsSpdKmhKey, s_speed_kmh);
  s_prefs.end();
}

void saveShowSpeed() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsShowSpeedKey, s_show_speed);
  s_prefs.end();
}

void saveShowAltitude() {
  if (!s_prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  s_prefs.putBool(kPrefsShowAltKey, s_show_altitude);
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
  s_speed_as_text = s_prefs.getBool(kPrefsSpdTextKey, false);
  s_speed_kmh = s_prefs.getBool(kPrefsSpdKmhKey, false);
  s_show_speed = s_prefs.getBool(kPrefsShowSpeedKey, true);
  s_show_altitude = s_prefs.getBool(kPrefsShowAltKey, true);
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

bool showSpeed() { return s_show_speed; }

bool showAltitude() { return s_show_altitude; }

bool speedAsText() { return s_speed_as_text; }

bool speedKmh() { return s_speed_kmh; }

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

void saveSpeedAsTextFromPortal(const char* checkbox_value) {
  s_speed_as_text = portalCheckboxChecked(checkbox_value);
  saveSpeedAsText();
  Serial.printf("Speed display: %s\n", s_speed_as_text ? "text" : "vector line");
}

void saveSpeedKmhFromPortal(const char* checkbox_value) {
  s_speed_kmh = portalCheckboxChecked(checkbox_value);
  saveSpeedKmh();
  Serial.printf("Speed units: %s\n", s_speed_kmh ? "km/h" : "knots");
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
  s_speed_as_text = false;
  s_speed_kmh = false;
  s_show_speed = true;
  s_show_altitude = true;
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.remove(kPrefsMilesKey);
    s_prefs.remove(kPrefsRunwaysKey);
    s_prefs.remove(kPrefsBelugaKey);
    s_prefs.remove(kPrefsAirbusKey);
    s_prefs.remove(kPrefsAltMKey);
    s_prefs.remove(kPrefsSpdTextKey);
    s_prefs.remove(kPrefsSpdKmhKey);
    s_prefs.remove(kPrefsShowSpeedKey);
    s_prefs.remove(kPrefsShowAltKey);
    s_prefs.end();
  }
}

void saveShowSpeedFromPortal(const char* checkbox_value) {
  s_show_speed = portalCheckboxChecked(checkbox_value);
  saveShowSpeed();
  Serial.printf("Show speed: %s\n", s_show_speed ? "on" : "off");
}

void saveShowAltitudeFromPortal(const char* checkbox_value) {
  s_show_altitude = portalCheckboxChecked(checkbox_value);
  saveShowAltitude();
  Serial.printf("Show altitude: %s\n", s_show_altitude ? "on" : "off");
}

void colorsInit() {
  if (!s_prefs.begin(kPrefsNamespace, true)) {
    return;
  }
  String val;
  val = s_prefs.getString(kPrefsColorGridKey, "#106420");
  strncpy(s_color_grid, val.c_str(), sizeof(s_color_grid) - 1);
  s_color_grid[sizeof(s_color_grid) - 1] = '\0';

  val = s_prefs.getString(kPrefsColorAirbusKey, "#005aff");
  strncpy(s_color_airbus, val.c_str(), sizeof(s_color_airbus) - 1);
  s_color_airbus[sizeof(s_color_airbus) - 1] = '\0';

  val = s_prefs.getString(kPrefsColorBoeingKey, "#ff1e1e");
  strncpy(s_color_boeing, val.c_str(), sizeof(s_color_boeing) - 1);
  s_color_boeing[sizeof(s_color_boeing) - 1] = '\0';

  val = s_prefs.getString(kPrefsColorBelugaKey, "#ffbe00");
  strncpy(s_color_beluga, val.c_str(), sizeof(s_color_beluga) - 1);
  s_color_beluga[sizeof(s_color_beluga) - 1] = '\0';

  val = s_prefs.getString(kPrefsColorMilitaryKey, "#808000");
  strncpy(s_color_military, val.c_str(), sizeof(s_color_military) - 1);
  s_color_military[sizeof(s_color_military) - 1] = '\0';

  val = s_prefs.getString(kPrefsColorOtherKey, "#c878ff");
  strncpy(s_color_other, val.c_str(), sizeof(s_color_other) - 1);
  s_color_other[sizeof(s_color_other) - 1] = '\0';

  s_prefs.end();
}

const char* colorGrid() { return s_color_grid; }
const char* colorAirbus() { return s_color_airbus; }
const char* colorBoeing() { return s_color_boeing; }
const char* colorBeluga() { return s_color_beluga; }
const char* colorMilitary() { return s_color_military; }
const char* colorOther() { return s_color_other; }

void saveColorGrid(const char* hex) {
  if (hex == nullptr || hex[0] != '#' || strlen(hex) != 7) return;
  strncpy(s_color_grid, hex, sizeof(s_color_grid) - 1);
  s_color_grid[sizeof(s_color_grid) - 1] = '\0';
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsColorGridKey, s_color_grid);
    s_prefs.end();
  }
}

void saveColorAirbus(const char* hex) {
  if (hex == nullptr || hex[0] != '#' || strlen(hex) != 7) return;
  strncpy(s_color_airbus, hex, sizeof(s_color_airbus) - 1);
  s_color_airbus[sizeof(s_color_airbus) - 1] = '\0';
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsColorAirbusKey, s_color_airbus);
    s_prefs.end();
  }
}

void saveColorBoeing(const char* hex) {
  if (hex == nullptr || hex[0] != '#' || strlen(hex) != 7) return;
  strncpy(s_color_boeing, hex, sizeof(s_color_boeing) - 1);
  s_color_boeing[sizeof(s_color_boeing) - 1] = '\0';
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsColorBoeingKey, s_color_boeing);
    s_prefs.end();
  }
}

void saveColorBeluga(const char* hex) {
  if (hex == nullptr || hex[0] != '#' || strlen(hex) != 7) return;
  strncpy(s_color_beluga, hex, sizeof(s_color_beluga) - 1);
  s_color_beluga[sizeof(s_color_beluga) - 1] = '\0';
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsColorBelugaKey, s_color_beluga);
    s_prefs.end();
  }
}

void saveColorMilitary(const char* hex) {
  if (hex == nullptr || hex[0] != '#' || strlen(hex) != 7) return;
  strncpy(s_color_military, hex, sizeof(s_color_military) - 1);
  s_color_military[sizeof(s_color_military) - 1] = '\0';
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsColorMilitaryKey, s_color_military);
    s_prefs.end();
  }
}

void saveColorOther(const char* hex) {
  if (hex == nullptr || hex[0] != '#' || strlen(hex) != 7) return;
  strncpy(s_color_other, hex, sizeof(s_color_other) - 1);
  s_color_other[sizeof(s_color_other) - 1] = '\0';
  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.putString(kPrefsColorOtherKey, s_color_other);
    s_prefs.end();
  }
}

void colorsReset() {
  strcpy(s_color_grid, "#106420");
  strcpy(s_color_airbus, "#005aff");
  strcpy(s_color_boeing, "#ff1e1e");
  strcpy(s_color_beluga, "#ffbe00");
  strcpy(s_color_military, "#808000");
  strcpy(s_color_other, "#c878ff");

  if (s_prefs.begin(kPrefsNamespace, false)) {
    s_prefs.remove(kPrefsColorGridKey);
    s_prefs.remove(kPrefsColorAirbusKey);
    s_prefs.remove(kPrefsColorBoeingKey);
    s_prefs.remove(kPrefsColorBelugaKey);
    s_prefs.remove(kPrefsColorMilitaryKey);
    s_prefs.remove(kPrefsColorOtherKey);
    s_prefs.end();
  }
}

}  // namespace ui::radar
