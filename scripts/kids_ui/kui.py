"""PC side of the AI Passport kids UI maintenance console (USB Serial/JTAG).

Runs with the ESP-IDF Python (needs only pyserial). The port is opened with
DTR and RTS low, so opening it does not reset the device.

    python scripts/kids_ui/kui.py --port COM3 vol 1          # set and store the volume
    python scripts/kids_ui/kui.py --port COM3 shot --out DIR # save the screen as PNG
    python scripts/kids_ui/kui.py --port COM3 demo --out DIR # gallery PNGs + contact sheet
    python scripts/kids_ui/kui.py --port COM3 chat           # toggle the chat (OK key)

--log FILE appends everything the device prints; --echo also shows the device
log (long hex runs redacted); --linger S keeps reading S seconds afterwards.
"""
import argparse
import base64
import re
import struct
import sys
import time
import zlib
from pathlib import Path

import serial

HEX_RUN = re.compile(r"[0-9a-fA-F]{32,}")
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")

# 5x7 glyphs for the contact sheet labels (one byte per row, bit 4 = left).
FONT = {
    "0": "0e11131519110e", "1": "040c040404040e", "2": "0e11010204081f", "3": "1f02040201110e",
    "4": "02060a121f0202", "5": "1f101e0101110e", "6": "0608101e11110e", "7": "1f010204080808",
    "8": "0e11110e11110e", "9": "0e11110f01020c", "a": "00000e010f110f", "b": "1010161911111e",
    "c": "00000e1010110e", "d": "01010d1311110f", "e": "00000e111f100e", "f": "0609081c080808",
    "g": "000f11110f010e", "h": "10101619111111", "i": "04000c0404040e", "j": "0200060202120c",
    "k": "10101214181412", "l": "0c04040404040e", "m": "00001a15151111", "n": "00001619111111",
    "o": "00000e1111110e", "p": "00001e111e1010", "q": "00000d130f0101", "r": "00001619101010",
    "s": "00000e100e011e", "t": "08081c08080906", "u": "0000111111130d", "v": "00001111110a04",
    "w": "0000111115150a", "x": "0000110a040a11", "y": "000011110f010e", "z": "00001f0204081f",
    "_": "0000000000001f", "-": "0000001f000000",
}
FONT = {k: bytes.fromhex(v) for k, v in FONT.items()}


def redact(text):
    return HEX_RUN.sub("<hex-redacted>", ANSI.sub("", text))


class Device:
    def __init__(self, port, log=None, echo=False):
        self.serial = None
        deadline = time.time() + 20
        while self.serial is None:
            try:
                s = serial.Serial()
                s.port = port
                s.baudrate = 115200
                s.timeout = 0.1
                s.write_timeout = 2
                s.dtr = False
                s.rts = False
                s.open()
                self.serial = s
            except serial.SerialException as exc:
                if time.time() > deadline:
                    raise SystemExit(f"cannot open {port}: {exc}")
                time.sleep(0.5)
        self.buffer = b""
        self.log = open(log, "ab") if log else None
        self.echo = echo

    def close(self):
        self.serial.close()
        if self.log:
            self.log.close()

    def send(self, command):
        self.serial.write(command.encode("ascii") + b"\n")

    def line(self, deadline):
        """Next line printed by the device, or None at the deadline."""
        while b"\n" not in self.buffer:
            if time.time() > deadline:
                return None
            chunk = self.serial.read(65536)
            if chunk and self.log:
                self.log.write(chunk)
            self.buffer += chunk
        raw, self.buffer = self.buffer.split(b"\n", 1)
        text = raw.decode("utf-8", "replace").rstrip("\r")
        if self.echo and not text.startswith("KUI:TILE"):
            print(redact(text), flush=True)
        return text

    def linger(self, seconds):
        deadline = time.time() + seconds
        while self.line(deadline) is not None:
            pass


class Frame:
    def __init__(self, name, width, height):
        self.name = name
        self.width = width
        self.height = height
        self.pixels = bytearray(width * height * 2)
        self.covered = bytearray(width * height)
        self.errors = []

    def add_tile(self, fields):
        x1, y1, x2, y2 = (int(v) for v in fields[:4])
        crc = int(fields[4], 16)
        data = base64.b64decode(fields[5]) if len(fields) > 5 else b""
        w, h = x2 - x1 + 1, y2 - y1 + 1
        if len(data) != w * h * 2 or zlib.crc32(data) != crc:
            self.errors.append(f"bad tile {x1},{y1}-{x2},{y2}")
            return
        for row in range(h):
            start = ((y1 + row) * self.width + x1) * 2
            self.pixels[start:start + w * 2] = data[row * w * 2:(row + 1) * w * 2]
            self.covered[(y1 + row) * self.width + x1:(y1 + row) * self.width + x2 + 1] = b"\1" * w

    def complete(self):
        return not self.errors and all(self.covered)

    def rgb(self):
        out = bytearray(self.width * self.height * 3)
        for i, (v,) in enumerate(struct.iter_unpack("<H", self.pixels)):
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            out[i * 3:i * 3 + 3] = bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
        return out


def read_frames(device, timeout, until):
    """Collect frames until a line starting with `until` (or one frame)."""
    frames = []
    frame = None
    deadline = time.time() + timeout
    while True:
        text = device.line(deadline)
        if text is None:
            raise SystemExit("timed out waiting for the device")
        if text.startswith("KUI:ERR"):
            raise SystemExit(text)
        fields = text.split(" ")
        if fields[0] == "KUI:FRAME":
            frame = Frame(fields[1], int(fields[2]), int(fields[3]))
        elif fields[0] == "KUI:TILE" and frame:
            frame.add_tile(fields[1:])
        elif fields[0] == "KUI:END" and frame:
            frames.append(frame)
            print(f"frame {len(frames):2d} {frame.name}: {'ok' if frame.complete() else 'INCOMPLETE ' + ' '.join(frame.errors)}")
            frame = None
            if until is None:
                return frames
            deadline = time.time() + timeout
        elif until and text.startswith(until):
            return frames


def write_png(path, width, height, rgb):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    stride = width * 3
    raw = b"".join(b"\0" + bytes(rgb[y * stride:(y + 1) * stride]) for y in range(height))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def contact_sheet(frames, path, columns=6):
    gap, label = 8, 22
    w, h = frames[0].width, frames[0].height
    rows = (len(frames) + columns - 1) // columns
    sw, sh = gap + columns * (w + gap), gap + rows * (h + label + gap)
    sheet = bytearray(bytes((24, 24, 30)) * sw * sh)

    def put(x, y, color):
        sheet[(y * sw + x) * 3:(y * sw + x) * 3 + 3] = bytes(color)

    for n, frame in enumerate(frames):
        ox = gap + (n % columns) * (w + gap)
        oy = gap + (n // columns) * (h + label + gap)
        text = f"{n + 1:02d} {frame.name}"
        for i, ch in enumerate(text):
            for row, bits in enumerate(FONT.get(ch, bytes(7))):
                for col in range(5):
                    if bits & (0x10 >> col):
                        for dy in range(2):
                            for dx in range(2):
                                put(ox + i * 12 + col * 2 + dx, oy + 2 + row * 2 + dy, (230, 230, 240))
        rgb = frame.rgb()
        for y in range(h):
            start = ((oy + label + y) * sw + ox) * 3
            sheet[start:start + w * 3] = rgb[y * w * 3:(y + 1) * w * 3]
    write_png(path, sw, sh, sheet)


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--log", help="append the raw device output to this file")
    parser.add_argument("--echo", action="store_true", help="print the device log (redacted)")
    parser.add_argument("--linger", type=float, default=0, help="keep reading this many seconds")
    sub = parser.add_subparsers(dest="command", required=True)
    vol = sub.add_parser("vol", help="set and store the output volume")
    vol.add_argument("volume", type=int)
    vol.add_argument("--retry", type=float, default=5, help="resend until acknowledged (seconds)")
    shot = sub.add_parser("shot", help="save the current screen")
    shot.add_argument("--out", type=Path, required=True)
    shot.add_argument("--name", default="shot")
    demo = sub.add_parser("demo", help="save the scripted gallery")
    demo.add_argument("--out", type=Path, required=True)
    sub.add_parser("chat", help="toggle the chat state")
    args = parser.parse_args()

    device = Device(args.port, args.log, args.echo)
    status = 0
    try:
        if args.command == "vol":
            ack = f"KUI:VOL {args.volume} OK"
            deadline = time.time() + args.retry
            done = False
            while not done and time.time() < deadline:
                device.send(f"KUI VOL {args.volume}")
                wait = time.time() + 0.5
                while (text := device.line(wait)) is not None:
                    if text.startswith(ack):
                        done = True
                        break
            print(ack if done else "no acknowledgement")
            status = 0 if done else 1
        elif args.command == "chat":
            device.send("KUI CHAT")
            deadline = time.time() + 5
            while (text := device.line(deadline)) is not None and not text.startswith("KUI:CHAT OK"):
                pass
            print("KUI:CHAT OK" if text else "no acknowledgement")
            status = 0 if text else 1
        elif args.command == "shot":
            args.out.mkdir(parents=True, exist_ok=True)
            for attempt in range(3):
                device.send("KUI SHOT")
                frame = read_frames(device, 15, None)[0]
                if frame.complete():
                    path = args.out / f"{args.name}.png"
                    write_png(path, frame.width, frame.height, frame.rgb())
                    print(f"saved {path}")
                    break
            else:
                status = 1
        else:
            args.out.mkdir(parents=True, exist_ok=True)
            device.send("KUI DEMO")
            frames = read_frames(device, 30, "KUI:DEMO END")
            good = [f for f in frames if f.complete()]
            for n, frame in enumerate(frames):
                if frame.complete():
                    write_png(args.out / f"{n + 1:02d}_{frame.name}.png", frame.width, frame.height,
                              frame.rgb())
            if good:
                contact_sheet(frames, args.out / "contact_sheet.png")
            print(f"saved {len(good)}/{len(frames)} frames and contact_sheet.png to {args.out}")
            status = 0 if frames and len(good) == len(frames) else 1
        if args.linger:
            device.linger(args.linger)
    finally:
        device.close()
    sys.exit(status)


if __name__ == "__main__":
    main()
