// =============================================================
// tft_display.h — ST7789 1.14" 135×240 driver for Heltec T114.
//
// Same public surface as `oled_display.h` (the SSD1306 driver
// for ESP32 boards) so main.cpp's call sites compile unchanged
// regardless of which display lives on the board. The class is
// still called `OledDisplay` even though the underlying panel is
// a TFT — keeps the name consistent across the firmware family.
//
// Hardware (T114 V2-specific, see Datasheet_T114.pdf and the
// MeshCore variant in _incoming/MeshCore-main):
//   * Panel: Heltec LH114T-IF03 — IPS, 135×240, 262 K colours,
//            Sitronix ST7789V controller, 4-line SPI write-only.
//   * SPI bus: shared with the SX1262 on the default SPI
//              peripheral (SCK=P0.19, MOSI=P0.22, MISO=P0.23).
//              CS gates which device gets the burst.
//   * Control: CS = P0.11, DC/RS = P0.12, RST = P0.02.
//   * Power gates: VDD_CTL = P0.03 (panel logic rail), LEDA_CTL
//                  = P0.15 (backlight LED anode). Both must be
//                  driven HIGH before any draw; pulled LOW in
//                  turnOff() to save current.
// =============================================================
#pragma once

#include <Arduino.h>
#include <stdint.h>

class OledDisplay {
public:
    void begin();
    void showSplash();
    void showStatus(uint32_t rx, uint32_t tx,
                    const char* ssid, const char* ip,
                    const char* state, const char* version);
    void showRadioConfig(uint32_t freq_hz, uint32_t bandwidth_hz,
                         uint8_t sf, uint8_t cr, int8_t power_dbm,
                         uint16_t syncword, uint8_t preamble_len,
                         const char* version);
    void showDiagnostics(uint32_t uptime_sec,
                         const char* tcp_client_ip,
                         uint32_t usb_idle_sec,
                         uint32_t rx_count, uint32_t tx_count,
                         uint32_t crc_errors,
                         const char* version);
    void showError(const char* msg);
    void turnOn();
    void turnOff();

private:
    bool _ready = false;
};
