// =============================================================
// boards/heltec_t114.h — Heltec Mesh Node T114 (HT-n5262)
//
// nRF52840 + SX1262, USB-C, optional 1.14" TFT-LCD (not driven by
// this firmware — iter 1 supports USB-CDC + UART protocol only).
// No Wi-Fi, no Ethernet, no internal/built-in OLED. Bluetooth 5.0
// hardware is present but unused.
//
// Pin map confirmed against the Heltec datasheet (Datasheet_T114.pdf
// in _incoming/) cross-referenced with the MeshCore T114 variant
// (`_incoming/MeshCore-main/variants/heltec_t114/`). All numbers
// are raw nRF52840 GPIO indices; on T114 the silkscreen "GPIO X"
// equals P0.X for X<32 and P1.(X-32) for X>=32. Our custom variant
// (firmware/variants/Heltec_T114_Board/) maps Arduino pin N → raw
// nRF GPIO N, so these values pass straight through to the SDK.
//   GPIO 20 — SX1262 DIO1 (IRQ)
//   GPIO 17 — SX1262 BUSY
//   GPIO 25 — SX1262 NRESET
//   GPIO 24 — SX1262 NSS (chip select)
//   GPIO 19 — SX1262 SCK (shared with the optional ST7789 TFT)
//   GPIO 22 — SX1262 MOSI (shared with TFT)
//   GPIO 23 — SX1262 MISO (TFT is write-only so this stays
//              dedicated to the radio in practice)
//
// Buttons:
//   GPIO 18 — hardware RESET button (wired to nRF52 reset line,
//              not exposed as GPIO input)
//   GPIO 42 — USER button (active-low, used as PRG)
//
// Optional dedicated protocol UART (UART1 on nRF52):
//   GPIO  9 — UART1_RX (silked 0.09 on P1)
//   GPIO 10 — UART1_TX (silked 0.10 on P1)
// Wired by default in this profile so the T114 can sit on a
// sector-array controller's UART without dropping its USB-CDC
// debug channel. Set to -1 to disable.
//
// Power:
//   3.3V via USB-C (LDO), or LiPo, or solar — whichever is
//   plugged in first. Max TX 21 dBm chip-side per the datasheet
//   (no external PA on this board variant).
//
// TX current at +20 dBm: ~157 mA @ 868 MHz. Sleep: 11 µA.
// =============================================================
#pragma once

inline const BoardConfig BOARD = {
    .name        = "Heltec T114",
    .fw_suffix   = "heltec_t114",
    .mdns_prefix = "t114",   // unused — nRF52 has no Wi-Fi/mDNS

    // SX1262 control pins.
    .pin_lora_nss  = 24,
    .pin_lora_rst  = 25,
    .pin_lora_busy = 17,
    .pin_lora_dio1 = 20,
    .pin_lora_sck  = 19,
    .pin_lora_miso = 23,
    .pin_lora_mosi = 22,

    .rf_switch = {
        .en_pin            = -1,
        .en_low_hold_ms    = 0,
        .rx_pin            = -1,
        .tx_pin            = -1,
        .dio2_as_rf_switch = true,   // SX1262 internal switch via DIO2
    },

    // No I2C OLED on this board (TFT-LCD is on a separate SPI bus
    // and isn't wired up in iter 1 firmware).
    .pin_i2c_sda      = -1,
    .pin_i2c_scl      = -1,
    .pin_i2c_oled_rst = -1,
    .pin_vext_enable_low = -1,

    .pin_user_button         = 42,
    .user_button_active_low  = true,

    .max_tx_power_dbm = 21,           // datasheet 21 ±1 dBm

    .use_dio3_tcxo = true,
    .tcxo_voltage  = 1.8f,

    .has_lora_radio = true,
    .has_wifi       = false,    // nRF52 has BT but not Wi-Fi
    .has_network    = false,    // gates the entire WiFi+TCP+OTA stack

    // Protocol on UART1 by default (sector-array controller wiring).
    // Flip to -1/-1 to talk only over USB-CDC.
    .pin_protocol_uart_rx = 9,
    .pin_protocol_uart_tx = 10,
    .protocol_uart_baud   = 921600,

    .ethernet = { .enabled = false },
};
