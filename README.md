# Plane Radar

<img width="800" height="450" alt="plane-radar" src="https://github.com/user-attachments/assets/716d0992-dab8-47ba-8f1a-2aec7f607419" />

**3D printed case (STL + assembly):** [MakerWorld](https://makerworld.com/en/models/2872376-esp32-plane-radar-live-ads-b-on-a-round-display#profileId-3207083) · **Firmware:** [Releases](https://github.com/MatixYo/ESP32-Plane-Radar/releases)

Firmware for an **ESP32-C3 Super Mini** and a **1.28″ round GC9A01** display (240×240). Shows a circular **ADS-B radar** around your configured location, with **WiFiManager** for first-time setup.

## What it does

1. **Wi‑Fi setup** (if needed) — captive portal on AP **`PlaneRadar-Setup`**
2. **Radar** — live aircraft on a sonar-style grid, from your **own tar1090 / adsb.im receiver** if configured, otherwise [adsb.fi](https://opendata.adsb.fi/)

After Wi‑Fi is saved, the device reconnects automatically; the radar runs in the main loop with periodic ADS-B updates (1 s local source, 3 s adsb.fi).

## Controls (BOOT, GPIO 9, active LOW)

| Action | Effect |
|--------|--------|
| **Short tap** | Cycle range preset (5 → 7 → 10 → 15 → 25 → 50 km); saved to flash |
| **Hold 3 s** | Clear Wi‑Fi, location, units, filters, and theme colors; reboot into setup portal |

During setup you can also hold BOOT at power-on to force a credential reset (same as the long press).

## Wi‑Fi setup portal

**First-time setup** (no saved Wi‑Fi):

1. Connect to **`PlaneRadar-Setup`**
2. Open **`http://plane-radar.local`** (preferred) or **`http://192.168.4.1`** — both are shown on the yellow setup screen; captive portal may open automatically
3. Set home Wi‑Fi, then save

**Reconfigure anytime** (after the device is on your network):

1. Open **`http://plane-radar.local`** or **`http://<device-ip>`** (e.g. from your router or serial log at boot)
2. Change Wi‑Fi, location, ADS-B source, units, filters, or theme colors; save

The same portal runs on the setup AP and on the device’s LAN IP while connected to Wi‑Fi. mDNS hostname is `plane-radar` → **plane-radar.local** (`kPortalHostname` in `config.h`). Some clients resolve `.local` slowly; use the IP if needed.

**Custom fields** (stored in NVS), grouped into **General Settings**, **Display Settings**, and **Radar Theme Colors**:

| Field | Purpose |
|-------|---------|
| **Latitude / Longitude** | Radar center and ADS-B query position (defaults in `config.h` until set) |
| **Local ADS-B URL** | Full `aircraft.json` URL of your own tar1090 / adsb.im receiver, e.g. `http://192.168.0.199:8080/data/aircraft.json`. Empty = adsb.fi only |
| **Display distance in miles** | Ring scale label in **mi** instead of **km** (e.g. `6mi` vs `10km`) |
| **Display altitude in meters** | Altitude tags in **m** instead of **ft** |
| **Show altitude** | Altitude tag on/off |
| **Show speed** | Speed indication on/off (text tag or vector line) |
| **Display speed as text tag** | Numeric speed tag instead of the magenta track vector line |
| **Display speed in km/h** | Speed tag in **km/h** instead of **kt** |
| **Show airport runways** | Major-airport runway overlay on the radar (off to hide) |
| **Filter: Only show Airbus aircraft** | Drops everything except Airbus types (Beluga included) |
| **Filter: Only show Airbus Beluga** | Drops everything except A3ST / A337 |
| **Grid / Airbus / Boeing / Beluga / Military / Other Planes Color** | Color pickers for the radar theme; a **Reset colors** button restores the defaults |

Both filters are **off by default**. With a filter on, the radar stays empty unless matching traffic is genuinely in range — a forgotten filter looks exactly like a bug.

After a reset, the device reboots and shows the setup screen immediately (no “Connecting” loop on stale credentials).

## Radar display

### Grid

- Dark blue background, subdued green rings and crosshairs
- White **N / S / E / W** at the bezel; range label on the **east** spoke (ring 3 = ¾ of outer radius)
- White center dot

Layout and default colors: `include/ui/radar_theme.h`; the grid color is overridable in the portal.

### Range presets

| Ring 3 label | Outer radius (aircraft scale) |
|------------|-------------------------------|
| 5 km / 3 mi | ~6.7 km |
| 7 km / 4 mi | ~9.3 km |
| 10 km / 6 mi | ~13.3 km (default) |
| 15 km / 9 mi | ~20 km |
| 25 km / 16 mi | ~33.3 km |
| 50 km / 31 mi | ~66.7 km |

Preset, units, filters, and theme colors persist across reboot (`planeradar` NVS namespace).

### Runways

- Major airports from OurAirports (`large_airport`); all open runway strips in range (helipads excluded)
- Teal runway lines with one ICAO label per airport (e.g. `KJFK`); toggle in the Wi‑Fi setup portal
- Update the embedded list: `python3 scripts/build_large_airports.py`

### Aircraft

- **Inside the outer ring** — heading triangle in the manufacturer color, magenta speed vector (clipped at the ring), callsign / type / altitude / speed tags
- **Outside the ring** (still within ADS-B fetch) — small **dot on the screen rim** at the correct bearing (direction cue; not distance-accurate past the ring)
- **Tags** — placed toward the **center**: west (left) → tag on the **right** of the symbol; east (right) → tag on the **left**

As range decreases (or aircraft approach), targets move inward; beyond-ring dots become full symbols when they cross the outer ring.

#### Symbol colors

Classified from the ICAO DOC 8643 type designator (ADS-B `t` field) in `include/aircraft_type.h`; the military flag wins over the manufacturer.

| Class | Default | Matched by |
|-------|---------|------------|
| **Military** | olive `#808000` | ADS-B `dbFlags` bit 0 |
| **Beluga** | amber `#ffbe00` | `A3ST` (A300-600ST), `A337` (A330-743L BelugaXL) |
| **Airbus** | blue `#005aff` | `config::kAirbusTypeCodes` (A318…A388, A400, BCS1/BCS3) |
| **Boeing** | red `#ff1e1e` | `config::kBoeingTypeCodes` (B703…B78X, incl. 737 MAX) |
| **Other / unknown** | violet `#c878ff` | everything else, including an empty type |

All five, plus the grid color, are editable in the setup portal.

#### Speed and altitude tags

- Altitude in **ft** (default) or **m**; speed in **kt** (default) or **km/h**
- Speed shows either as the magenta **track vector line** (default) or as a **numeric text tag**
- Both tags can be switched off entirely

### ADS-B

- **Primary source (optional): your own receiver** — any tar1090 / adsb.im `aircraft.json` endpoint, set as **Local ADS-B URL** in the portal. Unlimited, no internet round-trip
- **Fallback: adsb.fi** — `https://opendata.adsb.fi/api/v3/`; used when no local URL is set, or when the local one does not answer within `kAdsbLocalTimeoutMs` (1.5 s), so a powered-off receiver costs one fetch cycle, not a stalled loop
- Both readsb-derived JSON layouts are accepted (`aircraft` key for tar1090/adsb.im, `ac` for adsb.fi)
- Fetch radius: `ui::radar::fetchRadiusKm()` — scales with the active preset to roughly the screen edge (so rim dots have data). The local source is filtered by distance on the device, since it always returns everything the receiver sees
- Poll interval: `kAdsbLocalFetchIntervalMs` (1 s, local) / `kAdsbFetchIntervalMs` (3 s, adsb.fi — public limit is 1 req/s)
- Ground aircraft hidden by default (`kAdsbShowGroundAircraft`)
- Radius and type filters are applied **while parsing**, before the fixed 64-aircraft buffer fills, so slots go to matching traffic instead of whatever arrived first

## Configuration

Edit **`include/config.h`** for hardware and behavior:

| Area | Keys / notes |
|------|----------------|
| Portal | `kPortalApName`, `kPortalIp`, `kPortalHostname` / `kPortalHostUrl` (mDNS; needs `-DWM_MDNS` in `platformio.ini`) |
| Wi‑Fi timing | connect attempts, reconnect grace, portal timeout (`0` = no timeout) |
| BOOT | `kBootPin`, `kBootResetHoldMs`, `kBootTapMinMs` |
| Display SPI | pins, `kDisplayInvert`, `kDisplayRgbOrder`, `kDisplaySpiWriteHz` |
| Default location | `kDefaultRadarLat`, `kDefaultRadarLon` (until portal overrides) |
| ADS-B | `kAdsbFetchIntervalMs`, `kAdsbLocalFetchIntervalMs`, `kAdsbLocalTimeoutMs`, `kAdsbShowGroundAircraft` |
| Aircraft types | `kBelugaTypeCodes`, `kAirbusTypeCodes`, `kBoeingTypeCodes` (ICAO DOC 8643 designators) |

Range presets: `include/ui/radar_range.h` (`kRangePresets`).
Default theme colors: `include/ui/radar_theme.h` (portal values override them at runtime).

**NVS namespaces** — deliberately separate, to avoid handle conflicts:

| Namespace | Holds |
|-----------|-------|
| `planeradar` | range preset, units, filters, theme colors |
| `radar` | latitude / longitude |
| `adsbsrc` | local ADS-B source URL |

## Project layout

```
include/
  config.h
  aircraft_type.h          — manufacturer classification from ICAO type codes
  hardware/
    lgfx_config.hpp
    display.h
    display_font.h
  data/
    large_airports.h
  ui/
    radar_theme.h
    radar_range.h
    radar_display.h
    runway_overlay.h
    status_screens.h
  services/
    wifi_setup.h
    radar_location.h
    adsb_client.h
data/
  ui_font.vlw              — embedded smooth UI font (Noto Sans Bold)
scripts/
  build_large_airports.py
src/
  main.cpp
  data/
    large_airports_data.cpp
  hardware/
  ui/
  services/
```

## Wiring (GC9A01 ↔ ESP32-C3 Super Mini)

| Display | ESP32-C3 |
|---------|----------|
| VCC | 3V3 |
| GND | GND |
| RST | GPIO **0** |
| CS | GPIO **1** |
| DC | GPIO **10** |
| SDA (MOSI) | GPIO **3** |
| SCL (SCLK) | GPIO **4** |
| BOOT (user) | GPIO **9** |

## Build

```bash
pio run -t upload
pio device monitor
```

- PlatformIO env: **`supermini`**
- Serial: **115200** baud
- USB CDC on boot enabled in `platformio.ini` for the Super Mini

### Web-flashable release image

Single `.bin` for [esptool-js](https://espressif.github.io/esptool-js/) and similar tools (ESP32-C3, 4 MB, flash at **0x0**):

```bash
chmod +x scripts/merge-firmware.sh   # once
./scripts/merge-firmware.sh
```

Writes `release/plane-radar-merged.bin`. Skip rebuild if firmware is already built:

```bash
./scripts/merge-firmware.sh --no-build
```

Or via PlatformIO only (output: `.pio/build/supermini/firmware-merged.bin`):

```bash
pio run -e supermini
pio run -t merge -e supermini
```

Put the board in download mode (hold **BOOT**, tap **RESET**), then flash with Chrome/Edge over USB.

### CI and releases (GitHub Actions)

| Workflow | When | Output |
|----------|------|--------|
| [Build](.github/workflows/build.yml) | Push / PR to `main` | Artifact `plane-radar-supermini` (merged + split `.bin` files, ~90 days) |
| [Release](.github/workflows/release.yml) | Git tag `v*` (e.g. `v1.0.0`) | GitHub Release asset `plane-radar-v1.0.0.bin` + `.sha256` |

To ship a version users can download:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The release workflow builds firmware in CI and attaches the merged image to the release. Download from **Releases** on GitHub, then flash at **0x0** (ESP32-C3, 4 MB).

## Dependencies

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
