// =============================================================
// boards/heltec_tracker_v2.h — Heltec Wireless Tracker V2
// ESP32-S3 + SX1262 + KCT8103L PA/FEM, Wi-Fi/TCP modem build.
// =============================================================
#pragma once

inline const BoardConfig BOARD = {
    .name        = "Heltec Tracker V2",
    .fw_suffix   = "heltec_tracker_v2",
    .mdns_prefix = "heltec-tracker-v2",

    .pin_lora_nss  = 8,
    .pin_lora_rst  = 12,
    .pin_lora_busy = 13,
    .pin_lora_dio1 = 14,
    .pin_lora_sck  = 9,
    .pin_lora_miso = 11,
    .pin_lora_mosi = 10,

    .rf_switch = {
        .en_pin            = -1,
        .en_low_hold_ms    = 0,
        .rx_pin            = -1,
        .tx_pin            = -1,
        .dio2_as_rf_switch = true,
        .fem_power_pin      = 7,      // P_LORA_PA_POWER / VFEM_Ctrl
        .fem_power_active_high = true,
        .fem_power_settle_ms = 1,
        .fem_enable_pin     = 4,      // P_LORA_KCT8103L_PA_CSD
        .fem_enable_active_high = true,
        .fem_mode_pin       = 5,      // P_LORA_KCT8103L_PA_CTX
        .fem_mode_tx_high   = true,
        .fem_mode_rx_high   = true,   // LNA bypass, matching MeshCore default
    },

    // Tracker V2 has a TFT/GPS VEXT rail, not the SSD1306 OLED used by this
    // firmware. Leave display pins disabled; do not power GPS/display VEXT.
    .pin_i2c_sda      = -1,
    .pin_i2c_scl      = -1,
    .pin_i2c_oled_rst = -1,
    .pin_vext_enable_low = -1,

    .pin_user_button        = 0,
    .user_button_active_low = true,

    // MeshCore distinguishes the default LORA_TX_POWER=9 dBm from the board
    // ceiling MAX_LORA_TX_POWER=22 dBm. This clamp is the SX1262 chip-side
    // maximum; the KCT8103L PA/FEM path is estimated around 28 dBm at antenna.
    .max_tx_power_dbm = 22,

    .use_dio3_tcxo = true,
    .tcxo_voltage  = 1.8f,

    .has_lora_radio = true,
    .has_wifi       = true,
    .has_network    = true,
    .pin_protocol_uart_rx = -1,
    .pin_protocol_uart_tx = -1,
    .protocol_uart_baud   = 921600,
    .ethernet = { .enabled = false },
};
