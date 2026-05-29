#!/usr/bin/env python3
"""Open a serial port safely and print any passive bytes.

This intentionally sends nothing. It is useful on Linux/Home Assistant to
check USB-CDC attach/open behavior before running the active pymc-usb probe.
"""

import sys
import time

import serial


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <serial-port> <baudrate>", file=sys.stderr)
        return 2

    port = sys.argv[1]
    baud = int(sys.argv[2])

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 1
    ser.write_timeout = 1
    ser.dtr = False
    ser.rts = False

    print(f"Opening {port} at {baud}, DTR=False, RTS=False")
    ser.open()
    time.sleep(2)
    print("Opened")
    print("in_waiting:", ser.in_waiting)
    data = ser.read(4096)
    print("RX:", data.hex(" ") if data else "<no passive data>")
    ser.close()
    print("Closed cleanly")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
