#include <Arduino.h>
#include "target.h"

XiaoS3WIOBoard board;

#if defined(P_LORA_SCLK)
  static SPIClass spi;
  static RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);
#else
  static RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);
#endif

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
WeatherStationSensorManager sensors;

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
#endif

bool radio_init() {
  fallback_clock.begin();
  // NOTE: rtc_clock.begin(Wire) intentionally not called -- this project has
  // no I2C RTC chip, and Wire is disabled entirely (PIN_BOARD_SDA/SCL = -1)
  // since GPIO5/6 are repurposed for the CC1101's GDO0/GDO2 lines instead.
  pinMode(21, INPUT);
  pinMode(48, OUTPUT);

  #if defined(P_LORA_SCLK)
  spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
  return radio.std_init(&spi);
#else
  return radio.std_init();
#endif
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}
