#!/usr/bin/env python3
"""Live plot of the scope data pushed by the uscope_driver scope server.

Needs numpy and matplotlib; the websocket client is implemented with the standard library.
Only the newest frame is ever plotted, if the plot is slower than the data the older frames are discarded.
"""
#  //  Copyright 2026 Filippo Savi <filssavi@gmail.com>
#  //
#  //  Licensed under the Apache License, Version 2.0 (the "License");
#  //  you may not use this file except in compliance with the License.
#  //  You may obtain a copy of the License at
#  //
#  //      http://www.apache.org/licenses/LICENSE-2.0
#  //
#  // Unless required by applicable law or agreed to in writing, software
#  // distributed under the License is distributed on an "AS IS" BASIS,
#  // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  // See the License for the specific language governing permissions and
#  // limitations under the License.

import argparse
import base64
import os
import socket
import struct
import threading
import time

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

OP_CONT, OP_TEXT, OP_BINARY, OP_CLOSE, OP_PING, OP_PONG = 0x0, 0x1, 0x2, 0x8, 0x9, 0xA
HEADER = struct.Struct("<QQII")  # sequence, timestamp_ns, n_channels, n_samples (see scope_frame.hpp)


def recv_exact(sock, n):
    data = bytearray()
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("connection closed by the server")
        data += chunk
    return bytes(data)


def send_frame(sock, opcode, payload=b""):
    mask = os.urandom(4)
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    sock.sendall(bytes([0x80 | opcode, 0x80 | len(payload)]) + mask + masked)  # control frames: < 126 bytes


def connect(host, port):
    sock = socket.socket()
    # a small receive buffer keeps the amount of stale data queued in the kernel low
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
    sock.settimeout(5)
    sock.connect((host, port))
    key = base64.b64encode(os.urandom(16)).decode()
    sock.sendall((
        f"GET / HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
    ).encode())
    # byte by byte, so that nothing of the first frame (which can follow the response immediately) is consumed
    response = b""
    while not response.endswith(b"\r\n\r\n"):
        response += recv_exact(sock, 1)
    if b" 101 " not in response.split(b"\r\n")[0]:
        raise ConnectionError("websocket handshake failed: " + response.split(b"\r\n")[0].decode())
    sock.settimeout(None)
    return sock


def messages(sock):
    """Yields the complete binary messages, answering to the control frames"""
    message = bytearray()
    while True:
        b0, b1 = recv_exact(sock, 2)
        fin, opcode, length = b0 & 0x80, b0 & 0x0F, b1 & 0x7F
        if length == 126:
            length = struct.unpack(">H", recv_exact(sock, 2))[0]
        elif length == 127:
            length = struct.unpack(">Q", recv_exact(sock, 8))[0]
        payload = recv_exact(sock, length)  # server frames are not masked

        if opcode == OP_PING:
            send_frame(sock, OP_PONG, payload)
        elif opcode == OP_CLOSE:
            raise ConnectionError("connection closed by the server")
        elif opcode in (OP_BINARY, OP_CONT):
            message += payload
            if fin:
                yield bytes(message)
                message = bytearray()


def decode(message):
    """Splits a frame in per channel arrays; the driver already sends float32 values, grouped by channel"""
    sequence, timestamp, n_channels, n_samples = HEADER.unpack_from(message)
    values = np.frombuffer(message, dtype="<f4", offset=HEADER.size, count=n_channels * n_samples)
    return sequence, timestamp, list(values.reshape(n_channels, n_samples))


class receiver(threading.Thread):
    """Keeps only the newest frame, so a slow plot never works on old data"""

    def __init__(self, host, port):
        super().__init__(daemon=True)
        self.sock = connect(host, port)
        self.lock = threading.Lock()
        self.latest = None
        self.received = 0
        self.error = None

    def run(self):
        try:
            for message in messages(self.sock):
                with self.lock:
                    self.latest = message
                    self.received += 1
        except (ConnectionError, OSError) as e:
            self.error = e

    def take(self):
        with self.lock:
            message, self.latest = self.latest, None
        return message


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8990, help="scope server port (--scope-port of the driver)")
    parser.add_argument("--interval", type=int, default=30, help="plot refresh period in ms")
    parser.add_argument("--snapshot", metavar="PNG", help="save the first received frame to a file and exit")
    args = parser.parse_args()

    rx = receiver(args.host, args.port)
    rx.start()

    fig, axes = plt.subplots(6, 1, sharex=True, figsize=(10, 8))
    lines = [ax.plot([], [], lw=0.8)[0] for ax in axes]
    for i, ax in enumerate(axes):
        ax.set_ylabel(f"ch {i}")
        ax.grid(True, alpha=0.3)
    axes[-1].set_xlabel("sample")

    state = {"last_seq": None, "dropped": 0, "t": time.time(), "n": 0, "fps": 0.0}

    def update(_):
        if rx.error:
            fig.suptitle(f"disconnected: {rx.error}")
            return lines
        message = rx.take()
        if message is None:
            return lines

        sequence, timestamp, channels = decode(message)
        if state["last_seq"] is not None:
            state["dropped"] += sequence - state["last_seq"] - 1
        state["last_seq"] = sequence

        state["n"] += 1
        now = time.time()
        if now - state["t"] >= 1:
            state["fps"], state["n"], state["t"] = state["n"] / (now - state["t"]), 0, now

        for line, ax, samples in zip(lines, axes, channels):
            line.set_data(np.arange(len(samples)), samples)
            ax.relim()
            ax.autoscale_view()
        age_ms = (time.time_ns() - timestamp) / 1e6
        fig.suptitle(f"seq {sequence}   {state['fps']:.1f} plots/s   skipped {state['dropped']}   age {age_ms:.0f} ms")

        if args.snapshot:
            fig.savefig(args.snapshot)
            os._exit(0)
        return lines

    if args.snapshot:
        while True:  # no window: drive the update by hand
            update(None)
            time.sleep(args.interval / 1000)

    anim = FuncAnimation(fig, update, interval=args.interval, cache_frame_data=False)  # noqa: F841
    plt.show()


if __name__ == "__main__":
    main()
