#include "WeatherStationSensorManager.h"

// PlatformIO's dependency scanner pulls every .cpp under src/helpers/sensors/
// into every board environment's build, not just the one that actually wants
// it (see build_src_filter in variants/xiao_s3_wio_weatherstation/platformio.ini).
// rtl_433_ESP.h only exists as a lib_dep for that one environment, so guard
// the whole file on its presence -- other environments get an empty
// translation unit instead of a hard build failure.
#if __has_include(<rtl_433_ESP.h>)

#include <math.h>
#include <ArduinoJson.h>
#include <rtl_433_ESP.h>

#ifndef RF_MODULE_FREQUENCY
#error "WeatherStationSensorManager requires RF_MODULE_FREQUENCY to be defined (build flag), e.g. 915.00 for US band"
#endif
#ifndef RF_MODULE_GDO0
#error "WeatherStationSensorManager requires RF_MODULE_GDO0 to be defined (build flag) -- the CC1101's GDO0 pin"
#endif

// rtl_433's fineoffset.c decoder (fineoffset_WH24_callback) reports one of
// these two model strings for this hardware family, depending on whether the
// unit has the optional pressure-sensor add-on (which extends the packet
// past 215 bits). Units without it -- and so without pressure_hPa -- report
// as WH65B; all other fields are identical either way.
static bool isOurSensorModel(const char* model) {
  return strcmp(model, "Fineoffset-WS69") == 0 || strcmp(model, "Fineoffset-WH65B") == 0;
}

static rtl_433_ESP rf;
static char rf_message_buffer[512];

WeatherStationSensorManager::Reading WeatherStationSensorManager::_reading;
float    WeatherStationSensorManager::_last_rain_mm = NAN;
uint32_t WeatherStationSensorManager::_last_rain_tip_ms = 0;
float    WeatherStationSensorManager::_rain_rate_mmh = 0;
#if WEATHERSTATION_MOCK_DATA
unsigned long WeatherStationSensorManager::_next_mock_update = 0;
#endif

void WeatherStationSensorManager::onDecodedMessage(char* json) {
  Serial.printf("WeatherStation: decoded message: %s\n", json);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("WeatherStation: JSON parse failed: %s\n", err.c_str());
    return;
  }

  const char* model = doc["model"].as<const char*>();
  if (model == NULL || !isOurSensorModel(model)) {
    Serial.printf("WeatherStation: ignoring unrecognized model: %s\n", model == NULL ? "(none)" : model);
    return;  // not our sensor
  }

  Reading r;  // fresh reading, all fields default to NAN until set below

  if (!doc["temperature_C"].isNull()) r.temperature_c  = doc["temperature_C"].as<float>();
  if (!doc["humidity"].isNull())      r.humidity_pct   = doc["humidity"].as<float>();
  if (!doc["pressure_hPa"].isNull())  r.pressure_hpa   = doc["pressure_hPa"].as<float>();
  if (!doc["wind_dir_deg"].isNull())  r.wind_dir_deg   = doc["wind_dir_deg"].as<float>();
  if (!doc["wind_avg_m_s"].isNull())  r.wind_speed_mps = doc["wind_avg_m_s"].as<float>();
  if (!doc["wind_max_m_s"].isNull())  r.wind_gust_mps  = doc["wind_max_m_s"].as<float>();
  if (!doc["rain_mm"].isNull())       r.rain_total_mm  = doc["rain_mm"].as<float>();
  if (!doc["uvi"].isNull())           r.uv_index       = doc["uvi"].as<float>();
  if (!doc["light_lux"].isNull()) {
    // Fine Offset's own documented approximation for daylight spectrum
    // (see the WS69 protocol notes in rtl_433's fineoffset.c): Lux / 126.
    r.solar_wm2 = doc["light_lux"].as<float>() / 126.0f;
  }

  r.valid = true;
  r.updated_at_ms = millis();

  if (!isnan(r.rain_total_mm)) {
    if (!isnan(_last_rain_mm) && r.rain_total_mm > _last_rain_mm) {
      float delta_mm = r.rain_total_mm - _last_rain_mm;
      float delta_hours = (r.updated_at_ms - _last_rain_tip_ms) / 3600000.0f;
      if (delta_hours > 0) _rain_rate_mmh = delta_mm / delta_hours;
      _last_rain_tip_ms = r.updated_at_ms;
    }
    // else: no tick (not raining right now), or the counter went backwards
    // (reset, e.g. on a battery change) -- either way just resync the
    // baseline below without touching the last computed rate/tip time.
    _last_rain_mm = r.rain_total_mm;
  }

  _reading = r;
}

bool WeatherStationSensorManager::begin() {
  rf.setCallback(onDecodedMessage, rf_message_buffer, sizeof(rf_message_buffer));
  rf.initReceiver(RF_MODULE_GDO0, RF_MODULE_FREQUENCY);

#ifdef RF_CC1101
  // Push the AGC's target channel-filter amplitude to its max (42dB, vs the
  // 33dB default) so it works harder pulling a weak signal up before the bit
  // slicer -- this link is typically marginal. See RADIOLIB_CC1101_REG_AGCCTRL2.
  extern CC1101 radio;
  radio.SPIsetRegValue(RADIOLIB_CC1101_REG_AGCCTRL2, RADIOLIB_CC1101_MAGN_TARGET_42_DB, 2, 0);
#endif

  rf.enableReceiver();
  return true;
}

void WeatherStationSensorManager::loop() {
#if WEATHERSTATION_MOCK_DATA
  unsigned long now = millis();
  if ((long)(now - _next_mock_update) >= 0) {
    _next_mock_update = now + 30000;  // refresh every 30s so it's visibly "live", not frozen

    Reading r;  // MOCK -- not from a real CC1101 decode, see WEATHERSTATION_MOCK_DATA
    float wobble = (float)((now / 1000) % 20);       // 0..19, slow drift for visible movement
    r.temperature_c = 21.0f + wobble * 0.1f;          // ~21.0-22.9 C
    r.humidity_pct   = 55.0f + wobble * 0.5f;         // ~55-64.5 %
    r.pressure_hpa   = 1015.0f - wobble * 0.05f;      // ~1014-1015 hPa
    r.wind_dir_deg   = wobble * 18.0f;                // 0-342 deg, cycles every 20 updates
    r.valid = true;
    r.updated_at_ms = now;
    _reading = r;

    Serial.println("WeatherStation: [MOCK] injected simulated reading (WEATHERSTATION_MOCK_DATA=1, not real sensor data)");
  }
#endif
  rf.loop();
}

static const char* compassPoint(float deg) {
  static const char* points[] = {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
  };
  int idx = ((int)((deg / 22.5f) + 0.5f)) & 0xF;
  return points[idx];
}

bool WeatherStationSensorManager::getWeatherReportText(char* dest, size_t dest_size) {
  if (!_reading.valid) {
    snprintf(dest, dest_size, "No weather data received yet.");
    return false;
  }

  uint32_t age_ms = millis() - _reading.updated_at_ms;
  bool stale = age_ms > WEATHERSTATION_STALE_MS;
  uint32_t age_min = age_ms / 60000;

  char buf[192];
  size_t len = 0;
  buf[0] = 0;

  if (!isnan(_reading.temperature_c)) {
    float temp_f = _reading.temperature_c * 9.0f / 5.0f + 32.0f;
    len += snprintf(buf + len, sizeof(buf) - len, "%.1f\xC2\xB0""F", temp_f);
  }
  if (!isnan(_reading.humidity_pct)) {
    len += snprintf(buf + len, sizeof(buf) - len, "%s%.0f%% RH", len ? ", " : "", _reading.humidity_pct);
  }
  if (!isnan(_reading.pressure_hpa)) {
    float inHg = _reading.pressure_hpa * 0.0295299830714f;
    len += snprintf(buf + len, sizeof(buf) - len, "%s%.2f inHg", len ? ", " : "", inHg);
  }
  if (!isnan(_reading.wind_speed_mps)) {
    float mph = _reading.wind_speed_mps * 2.23694f;
    if (!isnan(_reading.wind_dir_deg)) {
      len += snprintf(buf + len, sizeof(buf) - len, "%swind %.0f mph from %s", len ? ", " : "", mph, compassPoint(_reading.wind_dir_deg));
    } else {
      len += snprintf(buf + len, sizeof(buf) - len, "%swind %.0f mph", len ? ", " : "", mph);
    }
  }
  // rain_total_mm is a lifetime tipping-bucket counter, not useful to show
  // directly -- show the derived rate instead, and only while it's actually
  // raining (a tick within the last WEATHERSTATION_RAIN_ACTIVE_MS).
  if (_rain_rate_mmh > 0 && millis() - _last_rain_tip_ms <= WEATHERSTATION_RAIN_ACTIVE_MS) {
    float rate_in_hr = _rain_rate_mmh * 0.0393701f;
    len += snprintf(buf + len, sizeof(buf) - len, "%srain %.2f in/hr", len ? ", " : "", rate_in_hr);
  }
  if (!isnan(_reading.solar_wm2)) {
    len += snprintf(buf + len, sizeof(buf) - len, "%ssolar %.0f W/m\xC2\xB2", len ? ", " : "", _reading.solar_wm2);
  }
  if (!isnan(_reading.uv_index)) {
    len += snprintf(buf + len, sizeof(buf) - len, "%sUV %.1f", len ? ", " : "", _reading.uv_index);
  }

  if (len == 0) {
    snprintf(dest, dest_size, "No weather data received yet.");
    return false;
  }

  if (stale) {
    snprintf(dest, dest_size, "%s (STALE, last seen %lu min ago)", buf, (unsigned long)age_min);
    return false;
  }

  if (age_min > 0) {
    snprintf(dest, dest_size, "%s (as of %lu min ago)", buf, (unsigned long)age_min);
  } else {
    snprintf(dest, dest_size, "%s", buf);
  }
  return true;
}

bool WeatherStationSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  if (!(requester_permissions & TELEM_PERM_ENVIRONMENT)) return true;
  if (!_reading.valid) return true;
  if (millis() - _reading.updated_at_ms > WEATHERSTATION_STALE_MS) return true;  // no fresh data to offer

  if (!isnan(_reading.temperature_c))  telemetry.addTemperature(WS_CH_CORE, _reading.temperature_c);
  if (!isnan(_reading.humidity_pct))   telemetry.addRelativeHumidity(WS_CH_CORE, _reading.humidity_pct);
  if (!isnan(_reading.pressure_hpa))   telemetry.addBarometricPressure(WS_CH_CORE, _reading.pressure_hpa);

  // Wind direction dropped -- CayenneLPP's Direction type carries no context
  // (just "degrees", could be wind/compass/GPS course/etc), so the companion
  // app can only show a bare "Direction: 262" with no indication it's wind.
  // Combined with direction being of little standalone use without paired
  // speed or a tracked trend, not worth the ambiguity. Still parsed/cached
  // in _reading above if a better way to surface it comes up later.
  // if (!isnan(_reading.wind_dir_deg))   telemetry.addDirection(WS_CH_CORE, _reading.wind_dir_deg);

  // Channels 3+ (wind speed/gust, rain, solar, UV) are disabled for now --
  // CayenneLPP has no dedicated type for these, so they were showing up in
  // the companion app as unlabeled "Analog Input"/"Generic Sensor" channels.
  // Re-enable once there's a better way to surface these (custom channel
  // naming, a different encoding, etc). Data is still parsed and cached in
  // _reading above, just not sent as telemetry.
  //
  // if (!isnan(_reading.wind_speed_mps)) telemetry.addAnalogInput(WS_CH_WIND_SPEED, _reading.wind_speed_mps);
  // if (!isnan(_reading.wind_gust_mps))  telemetry.addAnalogInput(WS_CH_WIND_GUST, _reading.wind_gust_mps);
  // if (!isnan(_reading.rain_total_mm))  telemetry.addAnalogInput(WS_CH_RAIN_TOTAL, _reading.rain_total_mm);
  // if (!isnan(_reading.solar_wm2))      telemetry.addGenericSensor(WS_CH_SOLAR, _reading.solar_wm2);
  // if (!isnan(_reading.uv_index))       telemetry.addGenericSensor(WS_CH_UV, _reading.uv_index);

  return true;
}

#endif  // __has_include(<rtl_433_ESP.h>)
