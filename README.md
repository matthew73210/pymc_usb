# pymc_usb — USB/TCP LoRa Modem for pymc_core

Firmware + Python driver that turns an ESP32 board with an SX1262 front
end into a "dumb" LoRa modem controlled from a Raspberry Pi over USB-CDC,
Wi-Fi/TCP, or (on boards with native Ethernet) wired LAN.

**Supported boards** (one source tree, picked at compile time via
`-DBOARD_<name>` in `platformio.ini`):

| Board                                                                                                       | MCU            | Front end                    | Networks      |
|-------------------------------------------------------------------------------------------------------------|----------------|------------------------------|---------------|
| **Heltec WiFi LoRa 32 V3**                                                                                  | ESP32-S3       | bare SX1262                  | Wi-Fi         |
| **Ikoka Stick** ([ndoo/ikoka-stick-meshtastic-device](https://github.com/ndoo/ikoka-stick-meshtastic-device))| XIAO ESP32-S3  | Ebyte E22-P868M30S, +30 dBm  | Wi-Fi         |
| **LilyGO T-LoRa T3-S3** v1.2/v1.3                                                                           | ESP32-S3       | bare SX1262                  | Wi-Fi         |
| **RAK3112 WisMesh**                                                                                         | ESP32-S3 (module) | SX1262 in-module          | Wi-Fi         |
| **WaveShare ESP32-P4-Nano**                                                                                 | ESP32-P4 (RISC-V) + ESP32-C6 | E22 (off-board, optional) | **Ethernet *or* Wi-Fi** — runtime auto-select: cable plugged → Ethernet wins, no link → fall back to Wi-Fi via C6 SDIO bridge. Both at once is unstable with the radio attached, see [P4-Nano notes](#porting-to-another-esp32-p4-board) |

Drop-in replacement for `SX1262Radio` in pymc_core — all MeshCore logic
(routing, encryption, retransmission) runs on the RPi. The modem handles
only the SX1262 physical layer: TX, RX, CAD, LoRa parameter configuration.

## Architecture

```
                          USB-CDC / WiFi-TCP
Raspberry Pi                                  Heltec V3
┌────────────────────┐                        ┌─────────────────┐
│ pymc_repeater      │◄ USB 921600 ────────►  │ LoRa Modem FW   │
│  └─ pymc_core      │                        │  └─ SX1262      │
│     ├─ USBLoRaRadio│──── OR ──────          │  └─ RadioLib    │
│     └─ TCPLoRaRadio│◄ TCP 5055 ─────────►   │  └─ OLED status │
│                    │                        │  └─ Wi-Fi STA   │
└────────────────────┘                        └─────────────────┘
```

- **USB mode** — cable, instant, no provisioning; ideal for single-board setups.
- **Wi-Fi/TCP mode** — no cable; modem can live anywhere on the LAN while the
  Pi sits elsewhere. Provisioned once via on-device AP portal (open AP
  `LoRa-Modem-XXXX` → `http://192.168.4.1`) or over USB with
  `USBLoRaRadio.set_wifi_credentials()`.

## Project layout

- **`firmware/`** — PlatformIO tree, five envs sharing one source.
  Each board lives in `include/boards/<env>.h`; `platformio.ini` picks
  one via `-DBOARD_<NAME>`. Prebuilt
  `bootloader.bin / partitions.bin / firmware.bin` live in
  `firmware/<env>/` (refresh with `firmware/build_release.sh`).
- **`pymc_driver/`** — Python drivers `usb_radio.py` / `tcp_radio.py`
  copied into `pymc_core/hardware/` by the installer. `test_modem.py`
  is a standalone pyserial probe.
- **`patches/`** — templates `scripts/install.sh` applies into
  pymc_core / pymc_repeater (config.py branches, web setup wizard,
  pymc_tcp config panel, sticky JWT exemption).
- **`scripts/install.sh`** — one-shot installer; idempotent.
- **`docker/`** + `docker-compose.yml` — Linux container running
  pymc_repeater that talks to the modem over LAN-TCP by default.

## Installation

Native install, Docker deployment, firmware flashing (esptool / PlatformIO /
OTA), Wi-Fi provisioning and the full pymc_core integration steps are
documented in [INSTALL.md](INSTALL.md).

## Per-board pin map

All board-specific GPIOs and policies live in
`firmware/include/boards/<name>.h` — pick the closest existing one
when adding a new carrier and edit the few fields that differ.

Per-board highlights (full pin numbers in the headers):

- **Heltec V3** — onboard SSD1306, bare SX1262, max 22 dBm, mDNS `heltec-<mac3>.local`.
- **Ikoka Stick** — XIAO ESP32-S3 + E22-P868M30S, EN-held + DIO2-as-RF-switch, max 30 dBm chip / +10 dB PA, external OLED, `ikoka-<mac3>.local`.
- **LilyGO T3-S3** — bare SX1262, no OLED, native USB-CDC, `lilygo-t3s3-<mac3>.local`.
- **RAK3112 WisMesh** — SX1262 inside the RAK3112 module, no OLED, `rak3112-<mac3>.local`.
- **WaveShare ESP32-P4-Nano** — RISC-V P4 + C6 + IP101GRI Ethernet PHY + off-board E22, runtime ETH-or-Wi-Fi (never both, see below), `p4nano-<mac3>.local`.

### E22-P RF switch (Ikoka, P4-Nano + E22)

E22 truth table from the datasheet: `EN=1, T/R CTRL=1` → TX,
`EN=1, T/R CTRL=0` → RX, `EN=0` → off. Firmware drives `EN` LOW for
5 s at boot (LDO/PA settle), then HIGH for life. `T/R CTRL` is not
wired to the MCU — the carrier ties it to SX1262 DIO2, and firmware
enables `setDio2AsRfSwitch(true)` so SX1262 toggles it on TX
automatically. A board with two MCU-driven enable lines instead just
sets `rx_pin` / `tx_pin` in `RfSwitchPolicy`.

## Porting to another ESP32-P4 board

The WaveShare ESP32-P4-Nano is the reference; copy
`firmware/include/boards/esp32_p4_nano.h` and adjust pins. Quirks
that differ from the ESP32-S3 family:

- **GPIO35 is the boot strap** (not GPIO0). Many P4 boards wire their
  BOOT button there, but GPIO35 is also RMII TXD1 — when Ethernet is
  up, the EMAC drives it as a 25 MHz output and the button reads the
  bitstream. Set `pin_user_button = -1`; firmware then auto-cycles the
  OLED screens every 4 s instead.
- **High-numbered GPIOs (49+) sit on a separate LDO domain.** RMII
  TX_EN/CLK fall there but work on the Nano; if PHY init fails on a
  different carrier, suspect this first.
- **Wi-Fi (C6 SDIO) + Ethernet (RMII) + radio together is unstable** —
  the C6 esp_hosted bridge falls off the SDIO bus every ~25 s and the
  SoC's RTC watchdog reboots. Fix: leave both `has_wifi = true` and
  `ethernet.enabled = true` in the board header and let `setup()`
  pick at runtime — Ethernet is tried first, EMAC is torn down with
  `EthernetManager::end()` if there's no link, then Wi-Fi takes over.
  Either alone with the radio is fine; both at once isn't.
- **Ethernet** is configured via `BoardConfig::EthernetConfig`
  (MDC, MDIO, RST, addr, clock direction). Static-IP fields are
  optional — leave `use_static_ip = false` for DHCP. Add new PHY
  models by extending the `EthernetPhy` enum + the mapping in
  `ethernet_manager.cpp`.
- **Debug serial** — keep `ARDUINO_USB_CDC_ON_BOOT=0`; the
  pioarduino USB-Serial-JTAG path mangles `Serial.printf` output on
  ESP32-P4. Use the second USB-C port (CH343P → UART0) for printf
  debug, or rely on TCP / `CMD_GET_DEBUG`.
- **OLED is optional** — `pin_i2c_sda = -1` short-circuits the entire
  Wire/SSD1306 path.

## Network exposure: LAN-only by design

Both TCP services — the protocol on 5055 and OTA HTTP on 80 — refuse
clients whose source address is outside RFC1918 (`10/8`, `172.16/12`,
`192.168/16`), link-local (`169.254/16`) or loopback (`127/8`). The
check runs at `accept()` time before any frame parsing or auth. NAT
port-forwards / Internet tunnels with a public source IP are dropped
unconditionally — TCP closes the socket, OTA returns 403. Hard
firmware policy; lifting it means editing
`firmware/include/net_filter.h` and re-flashing. Operators who need
remote access run a VPN whose tunnel address (WireGuard / Tailscale
in 100.64/10 → Tailscale subnet route, or any `10/8` overlay) is
inside the LAN range from the modem's point of view.

## Wire protocol v0.6

*(Full command list in `firmware/include/protocol.h`; the section below is
summarised.)*

### Frame format

```
┌──────┬──────┬───────┬──────────┬───────┐
│ SYNC │ CMD  │  LEN  │ PAYLOAD  │  CRC  │
│ 0xAA │ 1B   │ 2B LE │  0-255B  │ 2B LE │
└──────┴──────┴───────┴──────────┴───────┘
CRC-16/CCITT (poly 0x1021, init 0xFFFF) over CMD+LEN+PAYLOAD.
```

### Host → Modem

| CMD  | Name              | Payload                               |
|------|-------------------|---------------------------------------|
| 0x01 | TX_REQUEST        | Raw LoRa data (1–255 B)               |
| 0x10 | SET_CONFIG        | `RadioConfig` (14 B)                  |
| 0x11 | GET_CONFIG        | —                                     |
| 0x20 | STATUS_REQ        | —                                     |
| 0x22 | NOISE_REQ         | —                                     |
| 0x30 | CAD_REQUEST       | — (Listen Before Talk)                |
| 0x31 | RX_START          | — (restart RX continuous mode)        |
| 0x34 | SET_CAD_PARAMS    | 4 B: symNum / detPeak / detMin / exit |
| 0x41 | SET_WIFI          | ssid+pass+port+token (variable)       |
| 0x50 | AUTH              | token bytes (TCP only)                |
| 0x60 | WIFI_RESET        | —                                     |
| 0x61 | GET_WIFI          | —                                     |
| 0x70 | GET_VERSION       | —                                     |
| 0x72 | GET_DEBUG         | — (reset reason / heap / max-loop time)|
| 0xFF | PING              | —                                     |

### Modem → Host

| CMD  | Name              | Payload                               |
|------|-------------------|---------------------------------------|
| 0x02 | TX_DONE           | `airtime_us` (4 B LE)                 |
| 0x03 | TX_FAIL           | —                                     |
| 0x04 | RX_PACKET         | RSSI(2) + SNR(2) + sigRSSI(2) + data  |
| 0x12 | CONFIG_RESP       | `RadioConfig` (14 B)                  |
| 0x21 | STATUS_RESP       | `StatusResp` (24 B)                   |
| 0x23 | NOISE_RESP        | int16 LE (dBm × 10)                   |
| 0x32 | CAD_RESP          | 1 B (0=clear, 1=busy)                 |
| 0x33 | RX_STARTED        | —                                     |
| 0x35 | CAD_PARAMS_RESP   | echoes the 4-byte config              |
| 0x51 | AUTH_OK           | —                                     |
| 0x62 | WIFI_STATUS       | mode + ip + port + ssid + hostname    |
| 0x71 | VERSION_RESP      | ASCII version string                  |
| 0x73 | DEBUG_RESP        | reset(1) + uptime_ms(4) + heap(4) + min_heap(4) + max_loop_us(4) |
| 0xFE | ERROR             | error code (1 B; `0x0B` = `ERR_NO_RADIO` for boards without LoRa hardware) |
| 0xFF | PONG              | —                                     |

## Default radio parameters

Firmware boots into the MeshCore **EU Narrow / Switzerland** preset; the host
overrides these via `CMD_SET_CONFIG` at `begin()`:

| Parameter    | Value          |
|--------------|----------------|
| Frequency    | 869.618 MHz    |
| Bandwidth    | 62.5 kHz       |
| SF           | 8              |
| CR           | 4/8            |
| TX Power     | 22 dBm         |
| Sync Word    | 0x12 (private) |
| Preamble     | 16 symbols     |
| Header       | Explicit       |
| CRC          | CRC-8          |
| IQ           | Standard       |
| LDRO         | Auto           |

## OLED screens

Boot shows the pyMC splash (64×64) for ≥5 s while `setup()` runs in
parallel, then **STATUS** (RX/TX, SSID/IP, state tag, fw version) →
**RADIO** (freq/SF/BW/CR/power/sync/preamble) → **DIAGNOSTICS**
(uptime, TCP client, USB idle, RX/TX/CRC). Short PRG tap cycles them
and the panel sleeps after 30 s of idle. Boards without a usable
button (e.g. P4-Nano, where BOOT shares GPIO35 with RMII TXD1)
auto-cycle every 4 s and never sleep.

Long PRG hold (≥3 s at boot) = factory reset (wipes Wi-Fi NVS,
reboots into AP portal). Without the button, use `CMD_WIFI_RESET`
or `esptool erase_flash`.
