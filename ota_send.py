#!/usr/bin/env python3
"""
ota_send.py – interactive console for LoPy4 over TCP.

Supports:
 - basic command REPL with prompt (PING, AUTH, led_on, etc.)
 - sending OTA firmware binary over TCP using /ota <file>

Default target:
 - IP:   192.168.10.125
 - Port: 8080
 - Token: "hunter2"

You can override these via environment variables:
 - LOPY_ADDR
 - LOPY_PORT
 - LOPY_TOKEN
"""

import os, socket, struct, zlib, sys, pathlib

ADDR   = os.getenv("LOPY_ADDR",  "192.168.10.125")
PORT   = int(os.getenv("LOPY_PORT", "8080"))
TOKEN  = os.getenv("LOPY_TOKEN", "hunter2")
CHUNK  = 1024


def send_line(sock: socket.socket, line: str, expect_ok=True) -> str:
    """
    Sends a line (command) to the device and waits for a reply.

    Args:
        sock: Connected socket object.
        line: Command to send (string).
        expect_ok: If True, logs warning if reply doesn't start with 'OK'.

    Returns:
        Device response (string).
    """
    sock.sendall((line + "\n").encode())
    reply = sock.recv(256).decode(errors="ignore").strip()
    if expect_ok and not reply.startswith("OK"):
        print(f"[device] {reply}")
    return reply


def ota(sock: socket.socket, file_path: pathlib.Path):
    """
    Performs an OTA update by sending a binary file over the socket.

    Args:
        sock: Connected socket object.
        file_path: Path to .bin firmware file.
    """
    data  = file_path.read_bytes()
    size  = len(data)
    crc32 = zlib.crc32(data) & 0xFFFFFFFF
    print(f"[OTA] {file_path.name}  {size} bytes  CRC 0x{crc32:08X}")

    if not send_line(sock, f"OTA {size} {crc32:08X}", expect_ok=False).startswith("ACK"):
        print("[OTA] header rejected")
        return

    for off in range(0, size, CHUNK):
        sock.sendall(data[off : off + CHUNK])
    sock.sendall(struct.pack("<I", crc32))  # CRC footer
    print("[OTA] upload complete – device will reboot")


def main():
    print(f"[console] connecting to {ADDR}:{PORT} …")
    with socket.create_connection((ADDR, PORT), timeout=5) as s:
        # Authenticate once.
        send_line(s, f"AUTH {TOKEN}", expect_ok=False)

        # Simple REPL loop.
        while True:
            try:
                line = input("LoPy> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not line:
                continue
            if line.lower() in ("/q", "/quit", "exit"):
                break
            if line.lower().startswith("/ota "):
                bin_path = pathlib.Path(line[5:].strip())
                if not bin_path.is_file():
                    print("[err] file not found")
                else:
                    ota(s, bin_path)
                continue

            reply = send_line(s, line, expect_ok=False)
            print("LoPy4:", reply)

    print("[console] bye.")


if __name__ == "__main__":
    try:
        main()
    except (socket.timeout, ConnectionRefusedError) as e:
        sys.exit(f"[err] cannot connect: {e}")
