#pragma once

#include <cstddef>
#include <cstdint>

#include <driver/gpio.h>

namespace config {

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

// --- BOOT button (ESP32-C3 Super Mini, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_9;
constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

// --- Display: GC9A01 1.28" round 240×240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_0;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_1;
constexpr gpio_num_t kDisplayPinDc = GPIO_NUM_10;
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_3;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_4;  // display SCL

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;

constexpr uint32_t kDisplaySpiWriteHz = 40000000;
// GC9A01 modules often need invert + BGR for correct black/green output
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = true;

// --- Radar center defaults (overridden via WiFi setup portal) ---
// Airbus Stade (Ottenbecker Damm) — under the Beluga traffic to/from
// Hamburg-Finkenwerder, ~30 km east.
constexpr double kDefaultRadarLat = 53.5748;
constexpr double kDefaultRadarLon = 9.4964;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** Legacy scale unused — fetch uses radar::fetchRadiusKm() to screen edge. */
constexpr float kAdsbFetchRadiusScale = 1.0f;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;

/**
 * ICAO DOC 8643 type designators for the Airbus Beluga fleet, matched against
 * the ADS-B "t" field when the portal's Beluga filter is on.
 * A3ST = A300-600ST (original), A337 = A330-743L (BelugaXL — its own code, not A3ST).
 */
constexpr const char* kBelugaTypeCodes[] = {"A3ST", "A337"};
constexpr size_t kBelugaTypeCodeCount =
    sizeof(kBelugaTypeCodes) / sizeof(kBelugaTypeCodes[0]);

/**
 * ICAO DOC 8643 type designators for the Airbus fleet (excluding the Beluga
 * codes above, which are checked first so they keep their own colour). Matched
 * case-insensitively against the ADS-B "t" field. BCS1/BCS3 = A220 (ex-CSeries),
 * an Airbus product. Nachpflegbar, wenn neue Muster auftauchen.
 */
constexpr const char* kAirbusTypeCodes[] = {
    "A318", "A319", "A19N", "A320", "A20N", "A321", "A21N",
    "A306", "A30B", "A310", "A332", "A333", "A338", "A339",
    "A342", "A343", "A345", "A346", "A359", "A35K", "A388",
    "A400", "BCS1", "BCS3"};
constexpr size_t kAirbusTypeCodeCount =
    sizeof(kAirbusTypeCodes) / sizeof(kAirbusTypeCodes[0]);

/** ICAO DOC 8643 type designators for the Boeing fleet (incl. 737 MAX B3xM). */
constexpr const char* kBoeingTypeCodes[] = {
    "B703", "B712", "B721", "B722", "B731", "B732", "B733", "B734",
    "B735", "B736", "B737", "B738", "B739", "B37M", "B38M", "B39M",
    "B3XM", "B741", "B742", "B743", "B744", "B748", "B74S", "BLCF",
    "B752", "B753", "B762", "B763", "B764", "B772", "B773", "B77L",
    "B77W", "B788", "B789", "B78X"};
constexpr size_t kBoeingTypeCodeCount =
    sizeof(kBoeingTypeCodes) / sizeof(kBoeingTypeCodes[0]);

// --- UI colors (RGB565) — status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
