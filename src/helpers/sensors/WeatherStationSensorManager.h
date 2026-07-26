#pragma once

#include <Mesh.h>
#include <helpers/SensorManager.h>

#ifndef WEATHERSTATION_STALE_MS
#define WEATHERSTATION_STALE_MS  (10UL * 60 * 1000)   // treat reading as gone if older than this
#endif

// When set, loop() fabricates a plausible-looking reading on a timer instead
// of relying on real CC1101/rtl_433_ESP decodes -- useful for testing the
// mesh/telemetry pipeline without a live sensor in range.
#ifndef WEATHERSTATION_MOCK_DATA
#define WEATHERSTATION_MOCK_DATA 0
#endif

// rain_mm from the sensor is a lifetime tipping-bucket counter, not a rate --
// it only ever goes up. We derive a rain rate from the delta between ticks,
// and only consider it "currently raining" if a tick happened within this
// window (otherwise the counter may just be sitting idle on a dry day).
#ifndef WEATHERSTATION_RAIN_ACTIVE_MS
#define WEATHERSTATION_RAIN_ACTIVE_MS  (15UL * 60 * 1000)
#endif

// Fixed channel layout. Channel 0 is not usable (CayenneLPP's reader treats
// it as an end-of-data sentinel), so 1 is the lowest valid channel. Channel 1
// is weather (so it's not mixed in behind battery); channel 2 is the node's
// own battery voltage, added explicitly on this fixed channel number in
// SensorMesh.cpp instead of the default TELEM_CHANNEL_SELF (which is 1, and
// would collide with weather).
#define WS_CH_CORE           1  // temperature, humidity, pressure, wind direction
#define WS_CH_BATTERY        2  // added in SensorMesh.cpp, not here -- listed for reference
#define WS_CH_WIND_SPEED     3
#define WS_CH_WIND_GUST      4
#define WS_CH_RAIN_TOTAL     5
#define WS_CH_SOLAR          6
#define WS_CH_UV             7

// Receives Fine Offset weather-station telemetry (temperature, humidity,
// pressure, wind, rain, solar/UV) directly over a CC1101 sub-GHz receiver
// (via rtl_433_ESP's ported rtl_433 FSK decoder), and serves the most recent
// reading when queried. Never sends anything onto the mesh unprompted --
// querySensors() only runs when a peer requests telemetry, same as any other
// SensorManager implementation.
class WeatherStationSensorManager : public SensorManager {
public:
  WeatherStationSensorManager() {}

  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  void loop() override;

  // Full free-text weather report (all fields, human units, no CayenneLPP
  // channel/type constraints) for the "!weather" group-channel command.
  // Writes a null-terminated string into dest (dest_size bytes) and returns
  // true if it reflects a real (non-stale) reading, false if dest instead
  // explains why there's no fresh reading to report.
  bool getWeatherReportText(char* dest, size_t dest_size);

  // Called from the rtl_433_ESP message callback (must be reachable from a
  // free function, since the library's callback signature isn't a member fn).
  static void onDecodedMessage(char* json);

private:
  struct Reading {
    bool     valid = false;
    uint32_t updated_at_ms = 0;

    float temperature_c   = NAN;
    float humidity_pct    = NAN;
    float pressure_hpa    = NAN;
    float wind_dir_deg    = NAN;
    float wind_speed_mps  = NAN;
    float wind_gust_mps   = NAN;
    float rain_total_mm   = NAN;
    float solar_wm2       = NAN;
    float uv_index        = NAN;
  };

  static Reading _reading;

  // Rain-rate tracking, derived from the lifetime rain_mm counter (see
  // WEATHERSTATION_RAIN_ACTIVE_MS above).
  static float    _last_rain_mm;      // previous counter value, NAN until first seen
  static uint32_t _last_rain_tip_ms;  // millis() of the most recent counter increase
  static float    _rain_rate_mmh;     // rate computed from that last tick

#if WEATHERSTATION_MOCK_DATA
  static unsigned long _next_mock_update;
#endif
};
