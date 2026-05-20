// =============================================================
// boards/xiao_nrf52_wio.h — Seeed XIAO nRF52840 + Wio-SX1262
// Meshtastic kit (SKU 102010710).
//
// Hardware refs:
//   * Seeed product page + datasheet (102010710.pdf in _incoming/
//     once vendored)
//   * MeshCore xiao_nrf52 variant (build_flags pinout)
//   * Adafruit nRF52 bootloader OTAFIX xiao_nrf52840_ble (USB IDs,
//     P0.18 BTN0, P0.26 LED_RED reference)
//   * Custom variant: firmware/variants/XIAO_nRF52_Wio/ provides
//     an identity Arduino-pin ↔ raw-nRF-GPIO map, so the numbers
//     below match the silicon directly.
//
// Pin map (raw nRF52840 GPIO):
//   P0.04 = SX1262 NSS  (XIAO label D4)
//   P0.28 = SX1262 RST  (D2)
//   P0.29 = SX1262 BUSY (D3)
//   P0.03 = SX1262 DIO1 (D1, IRQ)
//   P0.05 = SX1262 RXEN (D5, LNA gate — RadioLib drives HIGH on RX)
//   P1.13 = SX1262 SCK  (D8)
//   P1.14 = SX1262 MISO (D9)
//   P1.15 = SX1262 MOSI (D10)
//   P0.18 = USER button (also Adafruit bootloader DFU button)
//
// No Wi-Fi, no Ethernet, no onboard display — USB-CDC transport
// only (XIAO has native USB on nRF52 USB peripheral). BLE 5.0
// hardware present but not driven by this firmware.
//
// RF switch: SX1262 controls TX path via DIO2 (internal switch),
// and the Wio-SX1262 carrier adds an external LNA whose enable
// rides on D5 (RXEN). Both must be wired in rf_switch{}.
//
// Max chip TX power on bare SX1262: 22 dBm. No external PA.
// =============================================================
#pragma once

inline const BoardConfig BOARD = {
    .name        = "XIAO nRF52840 + Wio-SX1262",
    .fw_suffix   = "xiao_nrf52_wio",
    .mdns_prefix = "xiao-nrf",   // unused — nRF52 has no Wi-Fi/mDNS

    // SX1262 control pins (raw nRF52 GPIO via the identity variant).
    .pin_lora_nss  = 4,    // D4
    .pin_lora_rst  = 28,   // D2
    .pin_lora_busy = 29,   // D3
    .pin_lora_dio1 = 3,    // D1
    // SPI line numbers are informational on nRF52 (firmware calls
    // SPI.begin() without arguments — the variant.h PIN_SPI_* macros
    // pick the bus pins) but must be != -1 so main.cpp's SPI-init
    // branch fires.
    .pin_lora_sck  = 45,   // D8  = P1.13
    .pin_lora_miso = 46,   // D9  = P1.14
    .pin_lora_mosi = 47,   // D10 = P1.15

    .rf_switch = {
        .en_pin            = -1,
        .en_low_hold_ms    = 0,
        .rx_pin            = 5,    // RXEN — LNA gate on D5 (P0.05)
        .tx_pin            = -1,   // TX path driven internally via DIO2
        .dio2_as_rf_switch = true,
    },

    // No I2C peripheral on this kit. D4/D5 are claimed by NSS/RXEN,
    // so the silkscreen "SDA/SCL" labels are not honoured here.
    .pin_i2c_sda      = -1,
    .pin_i2c_scl      = -1,
    .pin_i2c_oled_rst = -1,
    .pin_vext_enable_low = -1,

    .pin_user_button         = 18,   // P0.18 (Adafruit BTN0 / DFU)
    .user_button_active_low  = true,

    .max_tx_power_dbm = 22,           // bare SX1262 chip ceiling

    .use_dio3_tcxo = true,
    .tcxo_voltage  = 1.8f,            // 32 MHz TCXO on Wio-SX1262

    .has_lora_radio = true,
    .has_wifi       = false,    // nRF52 has BLE but not Wi-Fi
    .has_network    = false,    // no WiFi/TCP/OTA stack on this build

    // No dedicated protocol UART — USB-CDC only.
    .pin_protocol_uart_rx = -1,
    .pin_protocol_uart_tx = -1,
    .protocol_uart_baud   = 921600,

    .ethernet = { .enabled = false },
};
