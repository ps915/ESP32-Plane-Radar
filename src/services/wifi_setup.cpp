#include "services/wifi_setup.h"

#include <WiFi.h>
#include <WiFiManager.h>

#include <cstdio>

#include <Preferences.h>
#include <esp_system.h>
#include <esp_wifi.h>

#ifdef WM_MDNS
#include <ESPmDNS.h>
#endif

#include <vector>
#include "config.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"
#include "ui/status_screens.h"

portMUX_TYPE s_boot_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool s_boot_tap_pending = false;
volatile bool s_boot_is_down = false;
volatile unsigned long s_boot_down_ms = 0;
bool s_long_press_handled = false;
bool s_boot_interrupt_attached = false;

void IRAM_ATTR onBootButtonIsr() {
  const bool down = digitalRead(config::kBootPin) == LOW;
  const unsigned long now = millis();
  portENTER_CRITICAL_ISR(&s_boot_mux);
  if (down) {
    s_boot_is_down = true;
    s_boot_down_ms = now;
  } else if (s_boot_is_down) {
    const unsigned long held = now - s_boot_down_ms;
    if (held >= config::kBootTapMinMs && held < config::kBootResetHoldMs) {
      s_boot_tap_pending = true;
    }
    s_boot_is_down = false;
  }
  portEXIT_CRITICAL_ISR(&s_boot_mux);
}

void initBootButton() {
  pinMode(config::kBootPin, INPUT_PULLUP);
  if (s_boot_interrupt_attached) {
    return;
  }
  attachInterrupt(digitalPinToInterrupt(static_cast<uint8_t>(config::kBootPin)),
                  onBootButtonIsr, CHANGE);
  s_boot_interrupt_attached = true;
}

namespace {

/** Separate from planeradar prefs (rangeInit) to avoid NVS handle conflicts. */
constexpr char kWifiPrefsNamespace[] = "wifi";
constexpr char kPrefsForcePortalKey[] = "portal";

bool s_force_config_portal = false;
WiFiManager s_wm;
bool s_wm_configured = false;

void ensureWifiManager();
void startLanWebPortal();
void stopLanWebPortal();
bool wifiLinkUp();

constexpr int kCoordParamLen = 20;
constexpr char kCoordInputAttrs[] =
    " type=\"number\" step=\"0.000001\"";

WiFiManagerParameter s_header_general("<h2>General Settings</h2><hr>");
WiFiManagerParameter s_header_display("<h2>Display Settings</h2><hr>");
WiFiManagerParameter s_header_colors("<h2>Radar Theme Colors</h2><hr>");

WiFiManagerParameter s_param_lat("radar_lat", "Latitude (deg)", "0",
                                kCoordParamLen, kCoordInputAttrs);
WiFiManagerParameter s_param_lon("radar_lon", "Longitude (deg)", "0",
                                kCoordParamLen, kCoordInputAttrs);

constexpr int kAdsbUrlParamLen = 120;
constexpr char kAdsbUrlInputAttrs[] =
    " type=\"url\" placeholder=\"http://192.168.0.199:8080/data/aircraft.json\"";
WiFiManagerParameter s_param_adsb_url(
    "adsb_url", "Local ADS-B URL (tar1090 aircraft.json — empty = adsb.fi)", "",
    kAdsbUrlParamLen, kAdsbUrlInputAttrs);

char s_miles_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_miles("use_miles", "Display distance in miles (off = km)", "T", 2,
                                   s_miles_checkbox_attrs, WFM_LABEL_AFTER);

char s_runways_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_runways("show_runways", "Show airport runways", "T", 2,
                                     s_runways_checkbox_attrs, WFM_LABEL_AFTER);

char s_beluga_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_beluga("beluga_only", "Filter: Only show Airbus Beluga", "T", 2,
                                    s_beluga_checkbox_attrs, WFM_LABEL_AFTER);

char s_airbus_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_airbus("airbus_only", "Filter: Only show Airbus aircraft", "T", 2,
                                    s_airbus_checkbox_attrs, WFM_LABEL_AFTER);

char s_altm_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_altm("alt_meters", "Display altitude in meters (off = feet)", "T", 2,
                                  s_altm_checkbox_attrs, WFM_LABEL_AFTER);

char s_spdtext_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_spdtext("speed_text", "Display speed as text tag (off = vector line)",
                                     "T", 2, s_spdtext_checkbox_attrs, WFM_LABEL_AFTER);

char s_spdkmh_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_spdkmh("speed_kmh", "Display speed in km/h (off = knots)", "T", 2,
                                    s_spdkmh_checkbox_attrs, WFM_LABEL_AFTER);

char s_showspeed_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_showspeed("show_speed", "Show speed (text or vector)", "T", 2,
                                       s_showspeed_checkbox_attrs, WFM_LABEL_AFTER);

char s_showalt_checkbox_attrs[32] = "type=\"checkbox\"";
WiFiManagerParameter s_param_showalt("show_alt", "Show altitude", "T", 2,
                                     s_showalt_checkbox_attrs, WFM_LABEL_AFTER);

char s_color_grid_attrs[32] = "type=\"color\"";
WiFiManagerParameter s_param_color_grid("color_grid", "Grid Color", "#106420", 8, s_color_grid_attrs);

char s_color_airbus_attrs[32] = "type=\"color\"";
WiFiManagerParameter s_param_color_airbus("color_airbus", "Airbus Color", "#005aff", 8, s_color_airbus_attrs);

char s_color_boeing_attrs[32] = "type=\"color\"";
WiFiManagerParameter s_param_color_boeing("color_boeing", "Boeing Color", "#ff1e1e", 8, s_color_boeing_attrs);

char s_color_beluga_attrs[32] = "type=\"color\"";
WiFiManagerParameter s_param_color_beluga("color_beluga", "Beluga Color", "#ffbe00", 8, s_color_beluga_attrs);

char s_color_military_attrs[32] = "type=\"color\"";
WiFiManagerParameter s_param_color_military("color_military", "Military Color", "#808000", 8, s_color_military_attrs);

char s_color_other_attrs[32] = "type=\"color\"";
WiFiManagerParameter s_param_color_other("color_other", "Other Planes Color", "#c878ff", 8, s_color_other_attrs);

WiFiManagerParameter s_param_reset_colors(
    "<button type=\"button\" onclick=\"resetColorsToDefault()\" style=\"margin-top:15px;margin-bottom:10px;width:100%;\">Reset Colors to Default</button>"
    "<script>"
    "function resetColorsToDefault() {"
    "  document.getElementById('color_grid').value = '#106420';"
    "  document.getElementById('color_airbus').value = '#005aff';"
    "  document.getElementById('color_boeing').value = '#ff1e1e';"
    "  document.getElementById('color_beluga').value = '#ffbe00';"
    "  document.getElementById('color_military').value = '#808000';"
    "  document.getElementById('color_other').value = '#c878ff';"
    "}"
    "</script>"
);

void refreshPortalParamDefaults() {
  char lat_buf[kCoordParamLen + 1];
  char lon_buf[kCoordParamLen + 1];
  snprintf(lat_buf, sizeof(lat_buf), "%.6f", services::location::lat());
  snprintf(lon_buf, sizeof(lon_buf), "%.6f", services::location::lon());
  s_param_lat.setValue(lat_buf, kCoordParamLen);
  s_param_lon.setValue(lon_buf, kCoordParamLen);
  s_param_adsb_url.setValue(services::adsb::localUrl(), kAdsbUrlParamLen);
  snprintf(s_miles_checkbox_attrs, sizeof(s_miles_checkbox_attrs), "type=\"checkbox\"%s",
           ui::radar::useMiles() ? " checked" : "");
  s_param_miles.setValue("T", 2);
  snprintf(s_runways_checkbox_attrs, sizeof(s_runways_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::showRunways() ? " checked" : "");
  s_param_runways.setValue("T", 2);
  snprintf(s_beluga_checkbox_attrs, sizeof(s_beluga_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::belugaOnly() ? " checked" : "");
  s_param_beluga.setValue("T", 2);
  snprintf(s_airbus_checkbox_attrs, sizeof(s_airbus_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::airbusOnly() ? " checked" : "");
  s_param_airbus.setValue("T", 2);
  snprintf(s_altm_checkbox_attrs, sizeof(s_altm_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::altMeters() ? " checked" : "");
  s_param_altm.setValue("T", 2);
  snprintf(s_spdtext_checkbox_attrs, sizeof(s_spdtext_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::speedAsText() ? " checked" : "");
  s_param_spdtext.setValue("T", 2);
  snprintf(s_spdkmh_checkbox_attrs, sizeof(s_spdkmh_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::speedKmh() ? " checked" : "");
  s_param_spdkmh.setValue("T", 2);

  snprintf(s_showspeed_checkbox_attrs, sizeof(s_showspeed_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::showSpeed() ? " checked" : "");
  s_param_showspeed.setValue("T", 2);

  snprintf(s_showalt_checkbox_attrs, sizeof(s_showalt_checkbox_attrs),
           "type=\"checkbox\"%s", ui::radar::showAltitude() ? " checked" : "");
  s_param_showalt.setValue("T", 2);

  s_param_color_grid.setValue(ui::radar::colorGrid(), 7);
  s_param_color_airbus.setValue(ui::radar::colorAirbus(), 7);
  s_param_color_boeing.setValue(ui::radar::colorBoeing(), 7);
  s_param_color_beluga.setValue(ui::radar::colorBeluga(), 7);
  s_param_color_military.setValue(ui::radar::colorMilitary(), 7);
  s_param_color_other.setValue(ui::radar::colorOther(), 7);
}

void onPortalParamsSaved() {
  if (!services::location::saveFromStrings(s_param_lat.getValue(),
                                           s_param_lon.getValue())) {
    Serial.println("Invalid lat/lon in portal — keeping previous location");
  }
  services::adsb::saveLocalUrl(s_param_adsb_url.getValue());
  ui::radar::saveMilesFromPortal(s_param_miles.getValue());
  ui::radar::saveRunwaysFromPortal(s_param_runways.getValue());
  ui::radar::saveBelugaOnlyFromPortal(s_param_beluga.getValue());
  ui::radar::saveAirbusOnlyFromPortal(s_param_airbus.getValue());
  ui::radar::saveAltMetersFromPortal(s_param_altm.getValue());
  ui::radar::saveSpeedAsTextFromPortal(s_param_spdtext.getValue());
  ui::radar::saveSpeedKmhFromPortal(s_param_spdkmh.getValue());
  ui::radar::saveShowSpeedFromPortal(s_param_showspeed.getValue());
  ui::radar::saveShowAltitudeFromPortal(s_param_showalt.getValue());

  ui::radar::saveColorGrid(s_param_color_grid.getValue());
  ui::radar::saveColorAirbus(s_param_color_airbus.getValue());
  ui::radar::saveColorBoeing(s_param_color_boeing.getValue());
  ui::radar::saveColorBeluga(s_param_color_beluga.getValue());
  ui::radar::saveColorMilitary(s_param_color_military.getValue());
  ui::radar::saveColorOther(s_param_color_other.getValue());
}

void attachPortalParams(WiFiManager& wm) {
  refreshPortalParamDefaults();
  wm.addParameter(&s_header_general);
  wm.addParameter(&s_param_lat);
  wm.addParameter(&s_param_lon);
  wm.addParameter(&s_param_adsb_url);

  wm.addParameter(&s_header_display);
  wm.addParameter(&s_param_showspeed);
  wm.addParameter(&s_param_showalt);
  wm.addParameter(&s_param_runways);
  wm.addParameter(&s_param_spdtext);
  wm.addParameter(&s_param_spdkmh);
  wm.addParameter(&s_param_altm);
  wm.addParameter(&s_param_miles);
  wm.addParameter(&s_param_airbus);
  wm.addParameter(&s_param_beluga);

  wm.addParameter(&s_header_colors);
  wm.addParameter(&s_param_color_grid);
  wm.addParameter(&s_param_color_airbus);
  wm.addParameter(&s_param_color_boeing);
  wm.addParameter(&s_param_color_beluga);
  wm.addParameter(&s_param_color_military);
  wm.addParameter(&s_param_color_other);
  wm.addParameter(&s_param_reset_colors);

  wm.setSaveParamsCallback(onPortalParamsSaved);
}

void markForceConfigPortal() {
  s_force_config_portal = true;
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, false)) {
    return;
  }
  prefs.putBool(kPrefsForcePortalKey, true);
  prefs.end();
}

bool consumeForceConfigPortal() {
  if (s_force_config_portal) {
    s_force_config_portal = false;
    Preferences prefs;
    if (prefs.begin(kWifiPrefsNamespace, false)) {
      prefs.remove(kPrefsForcePortalKey);
      prefs.end();
    }
    return true;
  }

  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) {
    return false;
  }
  const bool pending = prefs.getBool(kPrefsForcePortalKey, false);
  prefs.end();
  if (!pending) {
    return false;
  }

  if (prefs.begin(kWifiPrefsNamespace, false)) {
    prefs.remove(kPrefsForcePortalKey);
    prefs.end();
  }
  return true;
}

bool storedWifiCredentials() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&mode) != ESP_OK || mode == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
    delay(50);
  }

  wifi_config_t conf = {};
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) {
    return false;
  }
  return conf.sta.ssid[0] != '\0';
}

void eraseWifiCredentials() {
  stopLanWebPortal();
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_OFF);
  delay(100);

  ensureWifiManager();
  WiFi.persistent(true);
  s_wm.resetSettings();
  s_wm.erase();
  WiFi.disconnect(true, true);
  WiFi.persistent(false);

  WiFi.mode(WIFI_OFF);
  delay(100);
}

void resetWifiCredentials() {
  markForceConfigPortal();
  eraseWifiCredentials();
  services::location::clear();
  services::adsb::clearLocalUrl();
  ui::radar::unitsReset();
  ui::radar::colorsReset();
  Serial.println("WiFi credentials, location, source, and units cleared");
}

void onConfigPortalApStarted(WiFiManager*) {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  statusScreenPortal();
#ifdef WM_MDNS
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("Setup portal: http://%s.local (or http://%s)\n",
                  config::kPortalHostname, config::kPortalIp);
  } else {
    Serial.printf("Setup portal: http://%s (mDNS unavailable)\n", config::kPortalIp);
  }
#else
  Serial.printf("Setup portal: http://%s\n", config::kPortalIp);
#endif
}

bool wifiLinkUp() {
  return WiFi.status() == WL_CONNECTED &&
         WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

void ensureWifiManager() {
  if (s_wm_configured) {
    return;
  }
  s_wm.setConfigPortalTimeout(config::kWifiPortalTimeoutSec);
  s_wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                           IPAddress(255, 255, 255, 0));
  s_wm.setHostname(config::kPortalHostname);
  s_wm.setAPCallback(onConfigPortalApStarted);

  std::vector<const char*> menu = {"wifi", "custom", "info", "restart", "exit"};
  s_wm.setMenu(menu);
  s_wm.setCustomMenuHTML("<form action='/param' method='get'><button>Settings</button></form><br/>");
  s_wm.setParamsPage(true);

  attachPortalParams(s_wm);
  s_wm_configured = true;
}

void startLanWebPortal() {
  if (!wifiLinkUp() || s_wm.getWebPortalActive() ||
      s_wm.getConfigPortalActive()) {
    return;
  }
  refreshPortalParamDefaults();
  WiFi.mode(WIFI_STA);
  s_wm.setConfigPortalBlocking(false);
#ifdef WM_MDNS
  MDNS.end();
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
#endif
  s_wm.startWebPortal();
  Serial.printf("LAN config: http://%s.local or http://%s\n",
                config::kPortalHostname, WiFi.localIP().toString().c_str());
}

void stopLanWebPortal() {
  if (!s_wm.getWebPortalActive()) {
    return;
  }
  s_wm.stopWebPortal();
#ifdef WM_MDNS
  MDNS.end();
#endif
}

void prepareSta() {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
}

void startStaConnect(const String& ssid, const String& pass) {
  prepareSta();
  if (ssid.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
  } else {
    WiFi.begin();
  }
}

bool waitForLinkWithUi(const char* ssid_for_ui, unsigned long attempt_ms) {
  const unsigned long deadline = millis() + attempt_ms;
  while (millis() < deadline) {
    if (wifiLinkUp()) {
      return true;
    }
    bootButtonPollLongPress();
    statusScreenConnectingTick();
    delay(config::kWifiConnectingFrameMs);
  }
  return wifiLinkUp();
}

bool tryConnectWithUi(const String& ssid, const String& pass, bool show_ui) {
  if (wifiLinkUp()) {
    return true;
  }

  const char* ui_ssid = ssid.length() > 0 ? ssid.c_str() : "network";
  if (show_ui) {
    statusScreenConnectingBegin(ui_ssid);
  }

  for (uint8_t attempt = 1; attempt <= config::kWifiConnectAttempts; ++attempt) {
    if (attempt > 1) {
      Serial.printf("WiFi connect retry %u/%u\n", attempt,
                    config::kWifiConnectAttempts);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(400);
    }

    startStaConnect(ssid, pass);

    if (waitForLinkWithUi(ui_ssid, config::kWifiConnectAttemptMs)) {
      return true;
    }
  }

  return false;
}

bool connectSavedNetwork(bool show_ui) {
  if (!storedWifiCredentials()) {
    return false;
  }

  ensureWifiManager();
  const String ssid = s_wm.getWiFiSSID();
  if (ssid.length() == 0) {
    return false;
  }
  const String pass = s_wm.getWiFiPass();
  return tryConnectWithUi(ssid, pass, show_ui);
}

bool openConfigPortal() {
  stopLanWebPortal();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);
  statusScreenPortal();
  s_wm.setConfigPortalBlocking(false);
  s_wm.startConfigPortal(config::kPortalApName);
  while (s_wm.getConfigPortalActive()) {
    bootButtonPollLongPress();
    if (s_wm.process()) {
      return true;
    }
    delay(10);
  }
  return wifiLinkUp();
}

}  // namespace

bool wifiShowsSetupScreenOnBoot() {
  if (s_force_config_portal) {
    return true;
  }
  Preferences prefs;
  if (!prefs.begin(kWifiPrefsNamespace, true)) {
    return false;
  }
  const bool pending = prefs.getBool(kPrefsForcePortalKey, false);
  prefs.end();
  return pending;
}

bool wifiBootButtonPressed() {
  return digitalRead(config::kBootPin) == LOW;
}

void bootButtonInit() { initBootButton(); }

bool bootButtonConsumeTap() {
  portENTER_CRITICAL(&s_boot_mux);
  const bool tap = s_boot_tap_pending;
  if (tap) {
    s_boot_tap_pending = false;
  }
  portEXIT_CRITICAL(&s_boot_mux);
  return tap;
}

void bootButtonPollLongPress() {
  if (wifiBootButtonPressed()) {
    portENTER_CRITICAL(&s_boot_mux);
    if (!s_boot_is_down) {
      s_boot_is_down = true;
      s_boot_down_ms = millis();
    }
    const unsigned long down_ms = s_boot_down_ms;
    portEXIT_CRITICAL(&s_boot_mux);

    if (!s_long_press_handled &&
        millis() - down_ms >= config::kBootResetHoldMs) {
      s_long_press_handled = true;
      Serial.println("BOOT held — resetting WiFi");
      wifiResetCredentialsAndReboot();
    }
  } else {
    portENTER_CRITICAL(&s_boot_mux);
    s_boot_is_down = false;
    portEXIT_CRITICAL(&s_boot_mux);
    s_long_press_handled = false;
  }
}

void wifiResetCredentialsAndReboot() {
  resetWifiCredentials();
  statusScreenWifiReset();
  delay(800);
  esp_restart();
}

bool wifiReconnect() {
  initBootButton();
  Serial.println("WiFi reconnecting...");
  return connectSavedNetwork(true);
}

void wifiLoop() {
  ensureWifiManager();
  if (wifiLinkUp()) {
    if (!s_wm.getWebPortalActive() && !s_wm.getConfigPortalActive()) {
      startLanWebPortal();
    }
    if (s_wm.getWebPortalActive() || s_wm.getConfigPortalActive()) {
      bootButtonPollLongPress();
      s_wm.process();
    }
  } else {
    stopLanWebPortal();
  }
}

bool wifiSetupConnect() {
  initBootButton();
  ensureWifiManager();

  const bool force_portal = consumeForceConfigPortal();
  WiFi.setAutoReconnect(false);

  if (force_portal) {
    eraseWifiCredentials();
    WiFi.mode(WIFI_OFF);
    delay(100);
  }

  if (force_portal) {
    Serial.println("Opening WiFi setup portal (after reset)");
    if (openConfigPortal() && wifiLinkUp()) {
      WiFi.setAutoReconnect(true);
      Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
      return true;
    }
    Serial.println("WiFi connection failed");
    statusScreenConnectFailed();
    return false;
  }

  Serial.println("Connecting to WiFi (portal opens if needed)...");

  if (wifiLinkUp()) {
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  if (storedWifiCredentials() && connectSavedNetwork(true)) {
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  if (storedWifiCredentials()) {
    Serial.println("Saved WiFi could not connect — opening setup portal");
  } else {
    Serial.println("No saved WiFi — opening setup portal");
  }

  if (openConfigPortal() && wifiLinkUp()) {
    WiFi.setAutoReconnect(true);
    Serial.printf("Connected: %s  IP %s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("WiFi connection failed");
  statusScreenConnectFailed();
  return false;
}
