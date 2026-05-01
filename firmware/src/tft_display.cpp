// =============================================================
// tft_display.cpp — Heltec T114 ST7789 (LH114T-IF03) driver.
//
// Compiled into every env (because src_filter `+<*>` picks it up)
// but the contents are gated on -DBOARD_HELTEC_T114 so non-T114
// builds skip it without needing per-env src_filter exclusions.
// On T114 it replaces the SSD1306 oled_display.cpp (which the
// heltec_t114 env excludes via build_src_filter).
//
// The TFT lives on the second hardware SPI peripheral (`SPI1` in
// the Adafruit BSP — PIN_SPI1_SCK=P1.08, MOSI=P1.09, MISO=P1.11
// from our variant.h), separate from the SX1262 bus. VDD_CTL and
// LEDA_CTL are both active-LOW power gates: driving them HIGH
// turns the panel off (this caught us during bring-up — early
// firmware drove HIGH thinking they were active-high enables and
// got a black screen with the radio still working).
//
// Public API matches OledDisplay so main.cpp doesn't care which
// panel lives on the board.
// =============================================================
#if defined(BOARD_HELTEC_T114)

#include "tft_display.h"
#include "splash_logo.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ─── T114 TFT pin map (variant Heltec_T114_Board) ────────────
// All Arduino pin indices in the custom variant equal the raw
// nRF GPIO number, so these constants double as datasheet refs.
static constexpr int8_t PIN_TFT_RST_GPIO     = 2;   // P0.02
static constexpr int8_t PIN_TFT_VDD_CTL_GPIO = 3;   // P0.03 — panel logic VDD enable
static constexpr int8_t PIN_TFT_CS_GPIO      = 11;  // P0.11
static constexpr int8_t PIN_TFT_DC_GPIO      = 12;  // P0.12
static constexpr int8_t PIN_TFT_BL_GPIO      = 15;  // P0.15 — backlight LED anode (LEDA_CTL)

// ─── Panel geometry ──────────────────────────────────────────
// Native ST7789 framebuffer is 240×320; the LH114T-IF03 only
// exposes a 135×240 visible window. We render in landscape
// (rotation=1) — the T114 enclosure is wider than tall when the
// USB-C port is on the right, and 240×135 leaves room for the
// 4-line status block without wrapping.
static constexpr int16_t TFT_W = 240;
static constexpr int16_t TFT_H = 135;

// ─── Layout constants ────────────────────────────────────────
// Rotation 1 = landscape (240 wide × 135 tall). Header (banner +
// state tag) sits at the top, body follows top-down. All draw
// calls use Adafruit_GFX's classic 6×8 font scaled 1× or 2×.
static constexpr uint16_t COLOUR_BG       = 0x0000;   // black
static constexpr uint16_t COLOUR_FG       = 0xFFFF;   // white
static constexpr uint16_t COLOUR_HEADER   = 0x07E0;   // green
static constexpr uint16_t COLOUR_ACCENT   = 0x07FF;   // cyan
static constexpr uint16_t COLOUR_WARN     = 0xFD20;   // orange
static constexpr uint16_t COLOUR_ERR      = 0xF800;   // red

// Adafruit_ST7789(SPIClass*, CS, DC, RST). Bind to SPI1 — its
// pins (SCK=40, MOSI=41, MISO=43) are dedicated to the TFT and
// don't fight with the SX1262 bus on SPI.
static Adafruit_ST7789 tft(&SPI1, PIN_TFT_CS_GPIO, PIN_TFT_DC_GPIO, PIN_TFT_RST_GPIO);

void OledDisplay::begin() {
    // VDD_CTL and LEDA_CTL are both ACTIVE-LOW power gates on the
    // T114 (PMOS high-side switches). Drive LOW to turn the panel
    // logic and backlight on. RST stays HIGH for normal operation
    // — Adafruit_ST7789::init() does its own LOW pulse internally.
    pinMode(PIN_TFT_VDD_CTL_GPIO, OUTPUT);
    pinMode(PIN_TFT_BL_GPIO, OUTPUT);
    pinMode(PIN_TFT_RST_GPIO, OUTPUT);
    digitalWrite(PIN_TFT_VDD_CTL_GPIO, LOW);
    digitalWrite(PIN_TFT_BL_GPIO, LOW);
    digitalWrite(PIN_TFT_RST_GPIO, HIGH);
    delay(10);   // VDD ramp before we hit the controller

    // Bring up the dedicated TFT SPI bus. SX1262 stays on `SPI`,
    // so this one starts cold and we configure it explicitly.
    SPI1.begin();

    // init() takes the *native* portrait dimensions (135×240) so
    // the driver applies the correct column offset (52 px) for
    // the LH114T-IF03 subwindow; setRotation(1) then rotates into
    // our preferred landscape orientation.
    tft.init(135, 240, SPI_MODE0);
    tft.setRotation(1);                     // landscape, USB-C right
    tft.setSPISpeed(40000000);              // 40 MHz — same as MeshCore
    tft.fillScreen(COLOUR_BG);
    tft.setTextColor(COLOUR_FG, COLOUR_BG);
    tft.setTextWrap(false);
    _ready = true;
}

// ─── Helpers ─────────────────────────────────────────────────

static void drawHeaderBar(const char* state, const char* version) {
    tft.fillRect(0, 0, TFT_W, 18, COLOUR_HEADER);
    tft.setCursor(2, 5);
    tft.setTextColor(COLOUR_BG, COLOUR_HEADER);
    tft.setTextSize(1);
    tft.print(state ? state : "...");
    if (version) {
        // Right-align version string to the panel's right edge.
        int16_t x1, y1; uint16_t w, h;
        tft.getTextBounds(version, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor(TFT_W - (int16_t)w - 2, 5);
        tft.print(version);
    }
    tft.setTextColor(COLOUR_FG, COLOUR_BG);
}

static void drawLabelValue(int16_t y, const char* label, const String& value,
                           uint16_t labelColour = COLOUR_ACCENT) {
    tft.setCursor(2, y);
    tft.setTextSize(1);
    tft.setTextColor(labelColour, COLOUR_BG);
    tft.print(label);
    tft.setTextColor(COLOUR_FG, COLOUR_BG);
    tft.println(value);
}

// ─── Splash ──────────────────────────────────────────────────

void OledDisplay::showSplash() {
    if (!_ready) return;
    tft.fillScreen(COLOUR_BG);

    // Centre the 64×64 monochrome pyMC logo on the panel and
    // double it up to 128×128 for visibility on the bigger screen.
    const int16_t logoSrc = SPLASH_LOGO_W;   // 64
    const int16_t logoOut = logoSrc * 2;     // 128
    const int16_t x0 = (TFT_W - logoOut) / 2;
    const int16_t y0 = (TFT_H - logoOut) / 2 - 12;
    for (int16_t row = 0; row < logoSrc; row++) {
        for (int16_t col = 0; col < logoSrc; col++) {
            uint16_t byteIdx = row * (logoSrc / 8) + (col / 8);
            uint8_t bit = 7 - (col % 8);
            uint8_t b = pgm_read_byte(&SPLASH_LOGO[byteIdx]);
            if (b & (1 << bit)) {
                tft.fillRect(x0 + col * 2, y0 + row * 2, 2, 2, COLOUR_FG);
            }
        }
    }

    tft.setCursor(0, y0 + logoOut + 12);
    tft.setTextColor(COLOUR_ACCENT, COLOUR_BG);
    tft.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    const char* tag = "pyMC";
    tft.getTextBounds(tag, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((TFT_W - (int16_t)w) / 2, y0 + logoOut + 12);
    tft.print(tag);
    tft.setTextSize(1);
    tft.setTextColor(COLOUR_FG, COLOUR_BG);
}

// ─── Status screen ───────────────────────────────────────────

void OledDisplay::showStatus(uint32_t rx, uint32_t tx,
                             const char* ssid, const char* ip,
                             const char* state, const char* version) {
    if (!_ready) return;
    tft.fillScreen(COLOUR_BG);
    drawHeaderBar(state, version);

    int16_t y = 26;
    drawLabelValue(y,  "RX: ", String(rx));         y += 14;
    drawLabelValue(y,  "TX: ", String(tx));         y += 14;
    drawLabelValue(y,  "SSID:", String(ssid ? ssid : "---")); y += 14;
    drawLabelValue(y,  "IP:  ", String(ip ? ip : "---"));     y += 14;
}

// ─── Radio screen ────────────────────────────────────────────

void OledDisplay::showRadioConfig(uint32_t freq_hz, uint32_t bandwidth_hz,
                                  uint8_t sf, uint8_t cr, int8_t power_dbm,
                                  uint16_t syncword, uint8_t preamble_len,
                                  const char* version) {
    if (!_ready) return;
    tft.fillScreen(COLOUR_BG);
    drawHeaderBar("RADIO", version);

    int16_t y = 26;
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.3f MHz", freq_hz / 1e6);
        drawLabelValue(y, "Freq:", String(buf));
        y += 14;
    }
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)bandwidth_hz);
        drawLabelValue(y, "BW:  ", String(buf));
        y += 14;
    }
    drawLabelValue(y, "SF:  ", String((unsigned)sf));        y += 14;
    drawLabelValue(y, "CR:  ", String("4/") + String((unsigned)cr)); y += 14;
    drawLabelValue(y, "Pwr: ", String((int)power_dbm) + " dBm"); y += 14;
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%04X", (unsigned)syncword);
        drawLabelValue(y, "Sync:", String(buf));
        y += 14;
    }
    drawLabelValue(y, "Pre: ", String((unsigned)preamble_len)); y += 14;
}

// ─── Diagnostics screen ──────────────────────────────────────

void OledDisplay::showDiagnostics(uint32_t uptime_sec,
                                  const char* tcp_client_ip,
                                  uint32_t usb_idle_sec,
                                  uint32_t rx_count, uint32_t tx_count,
                                  uint32_t crc_errors,
                                  const char* version) {
    if (!_ready) return;
    tft.fillScreen(COLOUR_BG);
    drawHeaderBar("DIAG", version);

    int16_t y = 26;
    {
        uint32_t h = uptime_sec / 3600;
        uint32_t m = (uptime_sec / 60) % 60;
        uint32_t s = uptime_sec % 60;
        char buf[24];
        snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
                 (unsigned long)h, (unsigned long)m, (unsigned long)s);
        drawLabelValue(y, "Up:  ", String(buf));
        y += 14;
    }
    drawLabelValue(y, "TCP: ",
                   String(tcp_client_ip && tcp_client_ip[0] ? tcp_client_ip : "—"));
    y += 14;
    if (usb_idle_sec == UINT32_MAX) {
        drawLabelValue(y, "USB: ", String("—"));
    } else {
        drawLabelValue(y, "USB: ", String((unsigned long)usb_idle_sec) + "s idle");
    }
    y += 14;
    drawLabelValue(y, "RX:  ", String(rx_count));   y += 14;
    drawLabelValue(y, "TX:  ", String(tx_count));   y += 14;
    drawLabelValue(y, "CRC: ", String(crc_errors),
                   crc_errors ? COLOUR_WARN : COLOUR_ACCENT);
    y += 14;
}

// ─── Error / power ───────────────────────────────────────────

void OledDisplay::showError(const char* msg) {
    if (!_ready) return;
    tft.fillScreen(COLOUR_BG);
    tft.fillRect(0, 0, TFT_W, 18, COLOUR_ERR);
    tft.setCursor(2, 5);
    tft.setTextColor(COLOUR_FG, COLOUR_ERR);
    tft.setTextSize(1);
    tft.print("ERROR");
    tft.setTextColor(COLOUR_FG, COLOUR_BG);

    if (msg) {
        tft.setCursor(2, 26);
        tft.setTextSize(2);
        tft.println(msg);
        tft.setTextSize(1);
    }
}

void OledDisplay::turnOff() {
    if (!_ready) return;
    // Active-low gates: HIGH disables the rail.
    digitalWrite(PIN_TFT_BL_GPIO, HIGH);
    digitalWrite(PIN_TFT_VDD_CTL_GPIO, HIGH);
    tft.enableSleep(true);
}

void OledDisplay::turnOn() {
    if (!_ready) return;
    digitalWrite(PIN_TFT_VDD_CTL_GPIO, LOW);
    tft.enableSleep(false);
    digitalWrite(PIN_TFT_BL_GPIO, LOW);
}

#endif  // BOARD_HELTEC_T114
