#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::radar {

/**
 * Range presets (label on ring 3 = ¾ of outer radius).
 *
 * Recommended for ADS-B on a 1.28″ display:
 *   5 km  — pattern / very local (airfield vicinity)
 *   7 km  — local
 *  10 km  — default; neighborhood spotting
 *  15 km  — wider local area
 *  25 km  — metro / regional picture
 *  50 km  — wide regional picture
 *
 * Outer radius (for aircraft math) is ring-3 distance ÷ 0.75.
 */
struct RangePreset {
  /** Distance shown on ring 3 (¾ of outer radius), always stored in km. */
  float ring3_km;
  float outer_km;
};

constexpr float kRing3ToOuterKm = 4.0f / 3.0f;

constexpr RangePreset kRangePresets[] = {
    {5.0f, 5.0f * kRing3ToOuterKm},
    {7.0f, 7.0f * kRing3ToOuterKm},
    {10.0f, 10.0f * kRing3ToOuterKm},
    {15.0f, 15.0f * kRing3ToOuterKm},
    {25.0f, 25.0f * kRing3ToOuterKm},
    {50.0f, 50.0f * kRing3ToOuterKm},
};

constexpr size_t kRangePresetCount =
    sizeof(kRangePresets) / sizeof(kRangePresets[0]);

/** Load saved range and distance units from flash. Call once after boot. */
void rangeInit();
/** Cycle preset and save to flash. */
void rangeNext();
const RangePreset& rangeCurrent();
uint8_t rangeIndex();
/** ADSB fetch radius (km): scaled to screen edge so beyond-ring dots have data. */
float fetchRadiusKm();

bool useMiles();
bool showRunways();
/** When on, only Airbus Beluga types (config::kBelugaTypeCodes) are shown. */
bool belugaOnly();
/** When on, only Airbus types are shown (Belugas included). */
bool airbusOnly();
/** When on, aircraft altitude tags read in meters instead of feet. */
bool altMeters();
/** When on, speed is shown as a numeric tag line instead of the track vector line. */
bool speedAsText();
/** When on, the speed tag reads in km/h instead of knots. */
bool speedKmh();
/** WiFi portal checkbox: "T" = miles, otherwise km. */
void saveMilesFromPortal(const char* checkbox_value);
void saveRunwaysFromPortal(const char* checkbox_value);
void saveBelugaOnlyFromPortal(const char* checkbox_value);
void saveAirbusOnlyFromPortal(const char* checkbox_value);
void saveAltMetersFromPortal(const char* checkbox_value);
void saveSpeedAsTextFromPortal(const char* checkbox_value);
void saveSpeedKmhFromPortal(const char* checkbox_value);
void formatRing3Label(char* buf, size_t len, float ring3_km, bool use_miles);
void formatCurrentRing3Label(char* buf, size_t len);
/** Reset units, runway overlay, and the aircraft filters (e.g. with WiFi credential wipe). */
void unitsReset();

}  // namespace ui::radar
