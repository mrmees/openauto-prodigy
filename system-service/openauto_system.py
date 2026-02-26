#!/usr/bin/env python3
"""OpenAuto Prodigy system-service daemon entry point."""

import asyncio
import logging
import os
import signal
import socket

from ipc_server import IpcServer

DEFAULT_SOCKET_PATH = "/run/openauto/system.sock"
LOG = logging.getLogger("openauto-system")


def _notify_systemd_ready() -> None:
    notify_socket = os.environ.get("NOTIFY_SOCKET")
    if not notify_socket:
        return

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        # systemd may provide an abstract namespace socket prefixed with '@'.
        if notify_socket.startswith("@"):
            notify_socket = "\0" + notify_socket[1:]
        sock.connect(notify_socket)
        sock.sendall(b"READY=1")
    except OSError as err:
        LOG.warning("Failed to send systemd READY notification: %s", err)
    finally:
        sock.close()


async def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    )

    socket_path = os.environ.get("OPENAUTO_SYSTEM_SOCKET", DEFAULT_SOCKET_PATH)
    ipc_server = IpcServer(socket_path=socket_path)
    await ipc_server.start()
    _notify_systemd_ready()

    stop_event = asyncio.Event()

    def handle_shutdown_signal() -> None:
        LOG.info("Shutdown signal received")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, handle_shutdown_signal)

    try:
        await stop_event.wait()
    finally:
        await ipc_server.stop()


if __name__ == "__main__":
    asyncio.run(main())
