// =============================================================
// variants/XIAO_nRF52_Wio/variant.h — Adafruit nRF52 BSP pin map
// for the Seeed XIAO nRF52840 + Wio-SX1262 Meshtastic kit
// (SKU 102010710).
//
// Identity pin map in variant.cpp (same scheme as the T114
// variant) lets us use raw nRF52 GPIO numbers everywhere
// (P0.X for X<32, P1.(X-32) for X>=32). The Adafruit BSP does
// not ship a XIAO variant, hence the in-tree copy.
//
// XIAO label ↔ raw nRF52840 GPIO
//   D0  = P0.02 = 2
//   D1  = P0.03 = 3          → SX1262 DIO1 (IRQ)
//   D2  = P0.28 = 28         → SX1262 NRESET
//   D3  = P0.29 = 29         → SX1262 BUSY
//   D4  = P0.04 = 4          → SX1262 NSS
//   D5  = P0.05 = 5          → SX1262 RXEN (LNA gate, host-driven)
//   D6  = P1.11 = 43
//   D7  = P1.12 = 44
//   D8  = P1.13 = 45         → SX1262 SCK
//   D9  = P1.14 = 46         → SX1262 MISO
//   D10 = P1.15 = 47         → SX1262 MOSI
// Pin map verified against the Seeed datasheet (102010710.pdf,
// XIAO nRF52840 silkscreen) and the MeshCore xiao_nrf52 variant.
// =============================================================
#pragma once

#include "WVariant.h"

// 32.768 kHz XTAL on board.
#define USE_LFXO
#define VARIANT_MCK             (64000000ul)

#define WIRE_INTERFACES_COUNT   (1)

// Battery sense — XIAO nRF52840 routes the LiPo divider tap to
// P0.31 (A0). Gated by a P-channel FET on P0.14: drive LOW to
// connect, HIGH to disconnect (saves quiescent current).
#define BATTERY_PIN             (31)
#define VBAT_ENABLE             (14)
#define ADC_MULTIPLIER          (1510.0F / 510.0F)   // 1M / (510k+510k) divider
#define AREF_VOLTAGE            (3.0)

// Identity-mapped pin space — every Arduino pin index 2..47 maps
// straight to the same raw nRF GPIO. P0.0 / P0.1 (XTAL) sit at
// indices 0 / 1 and are unmapped (0xFF in variant.cpp).
#define PINS_COUNT              (48)
#define NUM_DIGITAL_PINS        (48)
#define NUM_ANALOG_INPUTS       (1)
#define NUM_ANALOG_OUTPUTS      (0)

// Default USB-CDC Serial — no dedicated UART for the protocol
// on this kit. UART1 brought up only if the operator wires
// something to D6/D7 themselves.
#define PIN_SERIAL1_RX          (44)   // D7 = P1.12
#define PIN_SERIAL1_TX          (43)   // D6 = P1.11

// Serial2 must exist as a symbol because main.cpp references
// PROTO_UART (= Serial2 on nRF52) at compile time even when the
// runtime BoardConfig disables the protocol UART (pin_protocol_
// uart_rx = -1). Pin it to the same physical pads as Serial1 —
// the peripheral is never .begin()'d on this board, so no
// hardware conflict; this only satisfies the linker.
#define PIN_SERIAL2_RX          (44)
#define PIN_SERIAL2_TX          (43)

// I2C on the XIAO default Wire pins (D4/D5). Note these double
// as SX1262 NSS / RXEN here — the Wio-SX1262 kit ships without
// any I2C peripheral, so the Wire bus is in practice unused.
// Defined to keep the BSP happy.
#define PIN_WIRE_SDA            (4)    // D4 = P0.04
#define PIN_WIRE_SCL            (5)    // D5 = P0.05

// Default SPI bus = SX1262 on the Wio-SX1262 carrier.
#define SPI_INTERFACES_COUNT    (1)
#define PIN_SPI_MISO            (46)   // D9  = P1.14
#define PIN_SPI_MOSI            (47)   // D10 = P1.15
#define PIN_SPI_SCK             (45)   // D8  = P1.13
#define PIN_SPI_NSS             (4)    // D4  = P0.04

// On-board LEDs (active low). XIAO nRF52840 has an RGB user LED.
#define LED_RED                 (26)   // P0.26
#define LED_GREEN               (30)   // P0.30
#define LED_BLUE                (6)    // P0.06
#define LED_BUILTIN             LED_RED
#define PIN_LED                 LED_BUILTIN
#define LED_PIN                 LED_BUILTIN
#define LED_STATE_ON            LOW

// USER button on XIAO nRF52840 silkscreen — P0.18 (active low).
// Adafruit bootloader uses the same pin for DFU detection.
#define PIN_BUTTON1             (18)
#define BUTTON_PIN              PIN_BUTTON1
#define PIN_USER_BTN            BUTTON_PIN

// XIAO nRF52840 (non-Sense) has no external QSPI flash.
// QSPI peripheral is left uninitialised by the BSP unless we
// define EXTERNAL_FLASH_DEVICES, which we deliberately don't.

// SX1262 control pins (separate from the SPI lines above).
#define USE_SX1262
#define LORA_CS                 (4)    // D4 = P0.04
#define SX126X_DIO1             (3)    // D1 = P0.03
#define SX126X_BUSY             (29)   // D3 = P0.29
#define SX126X_RESET            (28)   // D2 = P0.28
#define SX126X_RXEN             (5)    // D5 = P0.05 (LNA gate)
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8
