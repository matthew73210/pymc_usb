#!/usr/bin/env python3
"""One-shot pymc-usb probe for cautious macOS or Linux checks.

This opens the port once, keeps DTR/RTS false by default, waits before TX,
sends one command, reads once, closes cleanly, and stops. It is intentionally
less comprehensive than probe_pymc_usb.py.
"""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys
import time

import serial


ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from pymc_driver.protocol_constants import (  # noqa: E402
    CMD_SET_CONFIG,
    CMD_GET_DEBUG,
    CMD_GET_VERSION,
    CMD_NOISE_REQ,
    CMD_PING,
    CMD_RX_START,
    CMD_STATUS_REQ,
    CMD_CONFIG_RESP,
    CMD_DEBUG_RESP,
    CMD_VERSION_RESP,
    CMD_NOISE_RESP,
    CMD_PONG,
    CMD_RX_STARTED,
    CMD_STATUS_RESP,
    CMD_ERROR,
    DEBUG_RESP_FMT,
    DEBUG_RESP_SIZE,
    STATUS_RESP_FMT,
    STATUS_RESP_SIZE,
    RADIO_CONFIG_FMT,
    PROTO_SYNC,
    crc16_ccitt,
    build_frame,
)


COMMANDS = {
    "set_config": (CMD_SET_CONFIG, CMD_CONFIG_RESP),
    "debug": (CMD_GET_DEBUG, CMD_DEBUG_RESP),
    "version": (CMD_GET_VERSION, CMD_VERSION_RESP),
    "status": (CMD_STATUS_REQ, CMD_STATUS_RESP),
    "noise": (CMD_NOISE_REQ, CMD_NOISE_RESP),
    "rx-start": (CMD_RX_START, CMD_RX_STARTED),
    "rx_start": (CMD_RX_START, CMD_RX_STARTED),
    "ping": (CMD_PING, CMD_PONG),
}


def parse_bool(value: str) -> bool:
    normalized = value.strip().lower()
    if normalized in {"1", "true", "yes", "on"}:
        return True
    if normalized in {"0", "false", "no", "off"}:
        return False
    raise argparse.ArgumentTypeError(f"expected true/false, got {value!r}")


def hex_bytes(data: bytes) -> str:
    return data.hex(" ") if data else "<empty>"


def find_first_frame(data: bytes):
    i = 0
    while True:
        sync = data.find(bytes([PROTO_SYNC]), i)
        if sync < 0 or sync + 6 > len(data):
            return None
        cmd = data[sync + 1]
        length = data[sync + 2] | (data[sync + 3] << 8)
        end = sync + 1 + 1 + 2 + length + 2
        if end > len(data):
            return None
        raw = data[sync:end]
        header = raw[1:4]
        payload = raw[4 : 4 + length]
        crc_recv = raw[-2] | (raw[-1] << 8)
        crc_calc = crc16_ccitt(header + payload)
        if crc_recv == crc_calc:
            return cmd, payload, raw
        i = sync + 1


def decode(cmd: int, payload: bytes) -> str:
    if cmd == CMD_CONFIG_RESP and len(payload) >= struct.calcsize(RADIO_CONFIG_FMT):
        freq, bw, sf, cr, power, sync, pre = struct.unpack(
            RADIO_CONFIG_FMT, payload[: struct.calcsize(RADIO_CONFIG_FMT)]
        )
        return (
            f"CONFIG_RESP freq={freq} bw={bw} sf={sf} cr={cr} "
            f"power={power} sync=0x{sync:04X} pre={pre}"
        )
    if cmd == CMD_DEBUG_RESP and len(payload) >= DEBUG_RESP_SIZE:
        reset, uptime_ms, free_heap, min_heap, max_loop_us = struct.unpack(
            DEBUG_RESP_FMT, payload[:DEBUG_RESP_SIZE]
        )
        text = (
            f"DEBUG reset_reason={reset} uptime_ms={uptime_ms} "
            f"free_heap={free_heap} min_free_heap={min_heap} "
            f"max_loop_us={max_loop_us}"
        )
        if len(payload) >= DEBUG_RESP_SIZE + 38:
            values = struct.unpack("<hBBIIIIIIIIh", payload[DEBUG_RESP_SIZE : DEBUG_RESP_SIZE + 38])
            text += (
                f" last_radio_error={values[0]} radio_ready={values[1]} "
                f"rx_mode_active={values[2]} dio1_irq_count={values[3]} "
                f"rx_dio1_count={values[4]} rx_read_fail_count={values[5]} "
                f"rx_invalid_len_count={values[6]} rx_start_count={values[7]} "
                f"rx_start_fail_count={values[8]} last_rx_start_ms={values[9]} "
                f"last_rx_dio1_ms={values[10]} last_instant_rssi={values[11] / 10:.1f}dBm"
            )
        elif len(payload) >= DEBUG_RESP_SIZE + 4:
            last_error, ready, rx_active = struct.unpack("<hBB", payload[DEBUG_RESP_SIZE : DEBUG_RESP_SIZE + 4])
            text += f" last_radio_error={last_error} radio_ready={ready} rx_mode_active={rx_active}"
        return text
    if cmd == CMD_STATUS_RESP and len(payload) >= STATUS_RESP_SIZE:
        up, rx, tx, crc, rssi, snr_x10, nf_x10, temp, state = struct.unpack(
            STATUS_RESP_FMT, payload[:STATUS_RESP_SIZE]
        )
        return (
            f"STATUS uptime={up}s rx_count={rx} tx_count={tx} crc_errors={crc} "
            f"last_rssi={rssi}dBm snr={snr_x10 / 10:.1f}dB "
            f"noise_floor={nf_x10 / 10:.1f}dBm temp={temp}C radio_state={state}"
        )
    if cmd == CMD_VERSION_RESP:
        return f"VERSION {payload.decode('utf-8', 'replace')!r}"
    if cmd == CMD_NOISE_RESP and len(payload) >= 2:
        return f"NOISE {struct.unpack('<h', payload[:2])[0] / 10:.1f}dBm"
    if cmd == CMD_RX_STARTED:
        if len(payload) >= 4:
            status, last_error, rx_active = struct.unpack("<BhB", payload[:4])
            return f"RX_STARTED status={status} last_radio_error={last_error} rx_mode_active={rx_active}"
        return "RX_STARTED"
    if cmd == CMD_ERROR:
        code = payload[0] if payload else 0xFF
        return f"ERROR 0x{code:02X}"
    if cmd == CMD_PONG:
        return "PONG"
    return f"cmd=0x{cmd:02X} payload={hex_bytes(payload)}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("baudrate", type=int)
    parser.add_argument(
        "--command",
        choices=sorted(COMMANDS),
        default="debug",
        help="single command to send; default is debug",
    )
    parser.add_argument("--post-open-delay", "--open-wait", dest="post_open_delay", type=float, default=2.0)
    parser.add_argument("--read-timeout", type=float, default=2.0)
    parser.add_argument("--dtr", type=parse_bool, default=False)
    parser.add_argument("--rts", type=parse_bool, default=False)
    args = parser.parse_args()

    cmd, _expect = COMMANDS[args.command]
    payload = b""
    if args.command == "set_config":
        payload = struct.pack(
            RADIO_CONFIG_FMT,
            869_618_000,
            62_500,
            8,
            8,
            22,
            0x3444,
            17,
        )
    request = build_frame(cmd, payload)

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baudrate
    ser.timeout = args.read_timeout
    ser.write_timeout = 2.0
    ser.dsrdtr = False
    ser.rtscts = False
    ser.dtr = args.dtr
    ser.rts = args.rts

    print(
        f"Opening {args.port} at {args.baudrate}, "
        f"DTR={args.dtr}, RTS={args.rts}"
    )
    ser.open()
    try:
        time.sleep(args.post_open_delay)
        waiting = ser.in_waiting
        if waiting:
            startup = ser.read(waiting)
            print("RX startup/passive:", hex_bytes(startup))
        ser.reset_input_buffer()

        print(f"TX {args.command}:", hex_bytes(request))
        written = ser.write(request)
        print("TX wrote:", written)
        data = ser.read(512)
        print("RX raw:", hex_bytes(data) if data else "<no response>")
        frame = find_first_frame(data)
        if not frame:
            print("DECODE: no valid pymc-usb frame in single read")
            return 1
        rx_cmd, payload, raw = frame
        print("RX frame:", hex_bytes(raw))
        print("DECODE:", decode(rx_cmd, payload))
        return 0
    finally:
        ser.close()
        print("Closed cleanly")


if __name__ == "__main__":
    raise SystemExit(main())
