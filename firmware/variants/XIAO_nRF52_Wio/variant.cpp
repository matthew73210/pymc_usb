// =============================================================
// variants/XIAO_nRF52_Wio/variant.cpp — identity pin map for the
// Seeed XIAO nRF52840 + Wio-SX1262 kit (SKU 102010710).
//
// Same scheme as the Heltec T114 variant: Arduino pin N maps to
// raw nRF52 GPIO N. Indices 0 and 1 are reserved (XTAL pins)
// and marked 0xFF so digitalWrite() silently no-ops on them.
//
// initVariant() disconnects the battery divider by default
// (P0.14 = HIGH) to keep quiescent current down — operator
// drives it LOW only when analogRead(BATTERY_PIN) is called.
// =============================================================
#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"

const uint32_t g_ADigitalPinMap[] = {
    0xff, 0xff, 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14,   15,   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28,   29,   30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42,   43,   44, 45, 46, 47
};

void initVariant() {
    // Disconnect the LiPo divider by default — saves ~30 µA in
    // standby. analogRead(BATTERY_PIN) wakers should drive
    // VBAT_ENABLE LOW before sampling, then HIGH again after.
    pinMode(VBAT_ENABLE, OUTPUT);
    digitalWrite(VBAT_ENABLE, HIGH);

    pinMode(PIN_USER_BTN, INPUT_PULLUP);
}
