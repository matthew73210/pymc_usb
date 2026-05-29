#!/usr/bin/env python3
"""Linux/Home Assistant safe pymc-usb protocol probe.

The first active command matches USBLoRaRadio.begin(): CMD_SET_CONFIG with a
packed RadioConfig payload. Defaults mirror the HA logs from the XIAO test
device; override them if your mesh uses different radio settings.
"""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys
import time
from dataclasses import dataclass

import serial


ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

try:
    from pymc_driver.protocol_constants import (
        PROTO_SYNC,
        MAX_LORA_PAYLOAD,
        CMD_SET_CONFIG,
        CMD_GET_CONFIG,
        CMD_STATUS_REQ,
        CMD_NOISE_REQ,
        CMD_RX_START,
        CMD_GET_VERSION,
        CMD_GET_DEBUG,
        CMD_PING,
        CMD_CONFIG_RESP,
        CMD_STATUS_RESP,
        CMD_NOISE_RESP,
        CMD_RX_STARTED,
        CMD_VERSION_RESP,
        CMD_DEBUG_RESP,
        CMD_ERROR,
        CMD_PONG,
        RADIO_CONFIG_FMT,
        STATUS_RESP_FMT,
        STATUS_RESP_SIZE,
        DEBUG_RESP_FMT,
        DEBUG_RESP_SIZE,
        crc16_ccitt,
        build_frame,
    )
except Exception:
    PROTO_SYNC = 0xAA
    MAX_LORA_PAYLOAD = 255
    CMD_SET_CONFIG = 0x10
    CMD_GET_CONFIG = 0x11
    CMD_STATUS_REQ = 0x20
    CMD_NOISE_REQ = 0x22
    CMD_RX_START = 0x31
    CMD_GET_VERSION = 0x70
    CMD_GET_DEBUG = 0x72
    CMD_PING = 0xFF
    CMD_CONFIG_RESP = 0x12
    CMD_STATUS_RESP = 0x21
    CMD_NOISE_RESP = 0x23
    CMD_RX_STARTED = 0x33
    CMD_VERSION_RESP = 0x71
    CMD_DEBUG_RESP = 0x73
    CMD_ERROR = 0xFE
    CMD_PONG = 0xFF
    RADIO_CONFIG_FMT = "<IIBBbHB"
    STATUS_RESP_FMT = "<IIIIhhhbB"
    STATUS_RESP_SIZE = struct.calcsize(STATUS_RESP_FMT)
    DEBUG_RESP_FMT = "<BIIII"
    DEBUG_RESP_SIZE = struct.calcsize(DEBUG_RESP_FMT)

    def crc16_ccitt(data: bytes) -> int:
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        return crc

    def build_frame(cmd: int, payload: bytes = b"") -> bytes:
        hdr = struct.pack("<BH", cmd, len(payload))
        return bytes([PROTO_SYNC]) + hdr + payload + struct.pack("<H", crc16_ccitt(hdr + payload))


ERROR_NAMES = {
    0x01: "ERR_CRC_MISMATCH",
    0x02: "ERR_INVALID_CMD",
    0x03: "ERR_RADIO_BUSY",
    0x04: "ERR_TX_TIMEOUT",
    0x05: "ERR_PAYLOAD_TOO_BIG",
    0x06: "ERR_INVALID_CONFIG",
    0x07: "ERR_CAD_FAILED",
    0x08: "ERR_RADIO_INIT",
    0x09: "ERR_UNAUTHORIZED",
    0x0A: "ERR_INVALID_WIFI",
    0x0B: "ERR_NO_RADIO",
    0x0C: "ERR_OTA_UNSUPPORTED",
    0x0D: "ERR_OTA_NO_BUFFER",
    0x0E: "ERR_CHANNEL_BUSY",
}

CMD_NAMES = {
    CMD_SET_CONFIG: "SET_CONFIG",
    CMD_GET_CONFIG: "GET_CONFIG",
    CMD_STATUS_REQ: "STATUS_REQ",
    CMD_NOISE_REQ: "NOISE_REQ",
    CMD_RX_START: "RX_START",
    CMD_GET_VERSION: "GET_VERSION",
    CMD_GET_DEBUG: "GET_DEBUG",
    CMD_PING: "PING",
    CMD_CONFIG_RESP: "CONFIG_RESP",
    CMD_STATUS_RESP: "STATUS_RESP",
    CMD_NOISE_RESP: "NOISE_RESP",
    CMD_RX_STARTED: "RX_STARTED",
    CMD_VERSION_RESP: "VERSION_RESP",
    CMD_DEBUG_RESP: "DEBUG_RESP",
    CMD_ERROR: "ERROR",
    CMD_PONG: "PONG",
}


@dataclass
class Frame:
    cmd: int
    payload: bytes
    raw: bytes


def hex_bytes(data: bytes) -> str:
    return data.hex(" ") if data else "<empty>"


def read_frame(ser: serial.Serial, timeout: float, expect_cmd: int | None = None) -> Frame | None:
    deadline = time.time() + timeout
    raw_prefix = bytearray()
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        if b[0] != PROTO_SYNC:
            raw_prefix.extend(b)
            continue

        hdr = ser.read(3)
        if len(hdr) < 3:
            continue
        cmd = hdr[0]
        length = struct.unpack("<H", hdr[1:3])[0]
        if length > MAX_LORA_PAYLOAD + 32:
            print(f"RX skipped implausible length {length} after sync")
            continue
        payload = ser.read(length) if length else b""
        crc_bytes = ser.read(2)
        if len(payload) != length or len(crc_bytes) != 2:
            continue
        raw = bytes([PROTO_SYNC]) + hdr + payload + crc_bytes
        crc_recv = struct.unpack("<H", crc_bytes)[0]
        crc_calc = crc16_ccitt(hdr + payload)
        if crc_recv != crc_calc:
            print(f"RX bad CRC: {hex_bytes(raw)} recv=0x{crc_recv:04X} calc=0x{crc_calc:04X}")
            continue
        if expect_cmd is not None and cmd != expect_cmd and cmd != CMD_ERROR:
            print(f"RX other frame: {describe_frame(Frame(cmd, payload, raw))}")
            continue
        if raw_prefix:
            print(f"RX ignored non-frame bytes before sync: {hex_bytes(bytes(raw_prefix))}")
        return Frame(cmd, payload, raw)
    return None


def describe_frame(frame: Frame) -> str:
    name = CMD_NAMES.get(frame.cmd, f"0x{frame.cmd:02X}")
    return f"{name} payload_len={len(frame.payload)} raw={hex_bytes(frame.raw)}"


def decode_frame(frame: Frame) -> str:
    if frame.cmd == CMD_CONFIG_RESP and len(frame.payload) >= struct.calcsize(RADIO_CONFIG_FMT):
        freq, bw, sf, cr, power, sync, pre = struct.unpack(
            RADIO_CONFIG_FMT, frame.payload[: struct.calcsize(RADIO_CONFIG_FMT)]
        )
        return (
            f"CONFIG_RESP freq={freq} bw={bw} sf={sf} cr={cr} "
            f"power={power} sync=0x{sync:04X} pre={pre}"
        )
    if frame.cmd == CMD_STATUS_RESP and len(frame.payload) >= STATUS_RESP_SIZE:
        up, rx, tx, crc, rssi, snr_x10, nf_x10, temp, state = struct.unpack(
            STATUS_RESP_FMT, frame.payload[:STATUS_RESP_SIZE]
        )
        states = ["idle/rx", "tx", "error"]
        state_name = states[state] if state < len(states) else f"unknown({state})"
        return (
            f"STATUS_RESP uptime={up}s rx_count={rx} tx_count={tx} crc_errors={crc} "
            f"last_rssi={rssi}dBm snr={snr_x10 / 10:.1f}dB "
            f"noise_floor={nf_x10 / 10:.1f}dBm temp={temp}C state={state_name}"
        )
    if frame.cmd == CMD_NOISE_RESP and len(frame.payload) >= 2:
        return f"NOISE_RESP noise_floor={struct.unpack('<h', frame.payload[:2])[0] / 10:.1f}dBm"
    if frame.cmd == CMD_VERSION_RESP:
        return f"VERSION_RESP {frame.payload.decode('utf-8', 'replace')!r}"
    if frame.cmd == CMD_DEBUG_RESP and len(frame.payload) >= DEBUG_RESP_SIZE:
        reset, uptime_ms, free_heap, min_heap, max_loop_us = struct.unpack(
            DEBUG_RESP_FMT, frame.payload[:DEBUG_RESP_SIZE]
        )
        extra = ""
        if len(frame.payload) >= DEBUG_RESP_SIZE + 38:
            (
                last_radio_error,
                radio_ready,
                rx_mode_active,
                dio1_irq_count,
                rx_dio1_count,
                rx_read_fail_count,
                rx_invalid_len_count,
                rx_start_count,
                rx_start_fail_count,
                last_rx_start_ms,
                last_rx_dio1_ms,
                last_instant_rssi_x10,
            ) = struct.unpack(
                "<hBBIIIIIIIIh",
                frame.payload[DEBUG_RESP_SIZE : DEBUG_RESP_SIZE + 38],
            )
            extra = (
                f" last_radio_error={last_radio_error} "
                f"radio_ready={radio_ready} rx_mode_active={rx_mode_active} "
                f"dio1_irq_count={dio1_irq_count} rx_dio1_count={rx_dio1_count} "
                f"rx_read_fail_count={rx_read_fail_count} "
                f"rx_invalid_len_count={rx_invalid_len_count} "
                f"rx_start_count={rx_start_count} "
                f"rx_start_fail_count={rx_start_fail_count} "
                f"last_rx_start_ms={last_rx_start_ms} "
                f"last_rx_dio1_ms={last_rx_dio1_ms} "
                f"last_instant_rssi={last_instant_rssi_x10 / 10:.1f}dBm"
            )
        elif len(frame.payload) >= DEBUG_RESP_SIZE + 4:
            last_radio_error, radio_ready, rx_mode_active = struct.unpack(
                "<hBB",
                frame.payload[DEBUG_RESP_SIZE : DEBUG_RESP_SIZE + 4],
            )
            extra = (
                f" last_radio_error={last_radio_error} "
                f"radio_ready={radio_ready} rx_mode_active={rx_mode_active}"
            )
        elif len(frame.payload) > DEBUG_RESP_SIZE:
            extra = f" extra={hex_bytes(frame.payload[DEBUG_RESP_SIZE:])}"
        return (
            f"DEBUG_RESP reset_reason={reset} uptime_ms={uptime_ms} "
            f"free_heap={free_heap} min_free_heap={min_heap} "
            f"max_loop_us={max_loop_us}{extra}"
        )
    if frame.cmd == CMD_RX_STARTED:
        if len(frame.payload) >= 4:
            status, last_radio_error, rx_mode_active = struct.unpack("<BhB", frame.payload[:4])
            return (
                f"RX_STARTED status={status} last_radio_error={last_radio_error} "
                f"rx_mode_active={rx_mode_active}"
            )
        return "RX_STARTED"
    if frame.cmd == CMD_PONG:
        return "PONG"
    if frame.cmd == CMD_ERROR:
        code = frame.payload[0] if frame.payload else 0xFF
        return f"ERROR 0x{code:02X} {ERROR_NAMES.get(code, 'UNKNOWN')}"
    return describe_frame(frame)


def send_and_expect(ser: serial.Serial, cmd: int, payload: bytes, expect_cmd: int, timeout: float) -> Frame | None:
    frame = build_frame(cmd, payload)
    print(f"TX {CMD_NAMES.get(cmd, hex(cmd))}: {hex_bytes(frame)} ({len(frame)} bytes)")
    written = ser.write(frame)
    print(f"TX wrote {written} bytes")
    resp = read_frame(ser, timeout, expect_cmd)
    if resp is None:
        print(f"RX <no valid {CMD_NAMES.get(expect_cmd, hex(expect_cmd))} within {timeout:.1f}s>")
        return None
    print("RX", describe_frame(resp))
    print("DECODE", decode_frame(resp))
    return resp


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="serial port, for example /dev/serial/by-id/usb-Seeed_XIAO-Wio-SX1262_...-if00")
    parser.add_argument("baudrate", type=int, help="configured pymc_usb.baudrate")
    parser.add_argument("--frequency", type=int, default=869_618_000)
    parser.add_argument("--bandwidth", type=int, default=62_500)
    parser.add_argument("--spreading-factor", type=int, default=8)
    parser.add_argument("--coding-rate", type=int, default=8)
    parser.add_argument("--tx-power", type=int, default=22)
    parser.add_argument("--sync-word", type=lambda value: int(value, 0), default=0x3444)
    parser.add_argument("--preamble-length", type=int, default=17)
    parser.add_argument("--open-wait", type=float, default=1.0)
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--dtr", action="store_true", help="assert DTR after open; default leaves DTR false")
    parser.add_argument("--rts", action="store_true", help="assert RTS after open; default leaves RTS false")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config_payload = struct.pack(
        RADIO_CONFIG_FMT,
        args.frequency,
        args.bandwidth,
        args.spreading_factor,
        args.coding_rate,
        args.tx_power,
        args.sync_word,
        args.preamble_length,
    )

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baudrate
    ser.timeout = 0.2
    ser.write_timeout = 2.0
    ser.dsrdtr = False
    ser.rtscts = False
    ser.dtr = bool(args.dtr)
    ser.rts = bool(args.rts)

    print(
        f"Opening {args.port} at {args.baudrate}, "
        f"DTR={bool(args.dtr)}, RTS={bool(args.rts)}"
    )
    ser.open()
    try:
        time.sleep(args.open_wait)
        waiting = ser.in_waiting
        if waiting:
            boot = ser.read(waiting)
            print(f"RX startup/passive bytes before drain: {hex_bytes(boot)}")
        ser.reset_input_buffer()

        print("First command mirrors USBLoRaRadio.begin(): CMD_SET_CONFIG")
        config_resp = send_and_expect(
            ser, CMD_SET_CONFIG, config_payload, CMD_CONFIG_RESP, timeout=max(args.timeout, 10.0)
        )
        ok = config_resp is not None and config_resp.cmd == CMD_CONFIG_RESP

        for cmd, expect in (
            (CMD_GET_VERSION, CMD_VERSION_RESP),
            (CMD_STATUS_REQ, CMD_STATUS_RESP),
            (CMD_GET_CONFIG, CMD_CONFIG_RESP),
            (CMD_RX_START, CMD_RX_STARTED),
            (CMD_NOISE_REQ, CMD_NOISE_RESP),
            (CMD_GET_DEBUG, CMD_DEBUG_RESP),
            (CMD_PING, CMD_PONG),
        ):
            send_and_expect(ser, cmd, b"", expect, timeout=args.timeout)

        if ok:
            print("PASS: device decoded SET_CONFIG and returned CONFIG_RESP.")
            return 0
        print(
            "FAIL: serial port opened, but no valid pymc-usb CONFIG_RESP was decoded. "
            "Check baud/config, DTR/RTS/reset behavior, firmware image, and USB passthrough."
        )
        return 1
    finally:
        ser.close()
        print("Closed cleanly")


if __name__ == "__main__":
    raise SystemExit(main())
