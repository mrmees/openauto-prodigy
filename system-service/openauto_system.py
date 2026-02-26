#!/usr/bin/env python3
"""OpenAuto Prodigy system-service daemon entry point."""

import asyncio
import logging
import os
import signal
import socket
import time

from ipc_server import IpcServer
from health_monitor import HealthMonitor
from config_applier import ConfigApplier

DEFAULT_SOCKET_PATH = "/run/openauto/system.sock"
DEFAULT_CONFIG_PATH = os.path.expanduser("~matt/.openauto/config.yaml")

ALLOWED_SERVICES = {"hostapd", "bluetooth", "systemd-networkd"}

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
    LOG.info("Starting openauto-system daemon")

    socket_path = os.environ.get("OPENAUTO_SYSTEM_SOCKET", DEFAULT_SOCKET_PATH)
    config_path = os.environ.get("OPENAUTO_CONFIG_PATH", DEFAULT_CONFIG_PATH)

    # --- Components ---
    ipc = IpcServer(socket_path=socket_path)
    health = HealthMonitor()
    config = ConfigApplier(config_path=config_path)

    # BT profiles (optional — only if dbus-next is installed)
    bt = None
    try:
        from bt_profiles import BtProfileManager
        bt = BtProfileManager()
        health.set_bt_restart_callback(bt.register_all)
    except ImportError:
        LOG.warning("dbus-next not available, BT profile registration disabled")

    start_time = time.monotonic()

    # --- IPC method handlers ---
    async def handle_get_health(params):
        return health.get_health()

    async def handle_get_status(params):
        return {
            "health": health.get_health(),
            "uptime": time.monotonic() - start_time,
            "version": "0.1.0",
        }

    async def handle_apply_config(params):
        section = params.get("section", "")
        result = config.apply_section(section)
        # If config wrote files that need a service restart, do it
        if result.get("ok") and result.get("restarted"):
            for svc in result["restarted"]:
                rc, out = await health.run_cmd("systemctl", "restart", svc)
                if rc != 0:
                    result["ok"] = False
                    result["error"] = f"Failed to restart {svc}: {out}"
                    break
        return result

    async def handle_restart_service(params):
        name = params.get("name", "")
        if name not in ALLOWED_SERVICES:
            return {"ok": False, "error": f"Unknown service: {name}"}
        rc, out = await health.run_cmd("systemctl", "restart", name)
        return {"ok": rc == 0, "output": out.strip()}

    ipc.register_method("get_health", handle_get_health)
    ipc.register_method("get_status", handle_get_status)
    ipc.register_method("apply_config", handle_apply_config)
    ipc.register_method("restart_service", handle_restart_service)

    # --- Start ---
    await ipc.start()

    # Initial health check
    LOG.info("Running initial health check...")
    await health.check_once()
    LOG.info("Initial health: %s", health.get_health())

    # Register BT profiles on boot
    if bt:
        try:
            await bt.register_all()
            LOG.info("BT profiles registered")
        except Exception as e:
            LOG.error("BT profile registration failed: %s", e)

    # sd_notify READY=1
    _notify_systemd_ready()

    # --- Run ---
    stop_event = asyncio.Event()

    def handle_shutdown_signal() -> None:
        LOG.info("Shutdown signal received")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, handle_shutdown_signal)

    health_task = asyncio.create_task(health.run())

    await stop_event.wait()

    # --- Shutdown ---
    health_task.cancel()
    try:
        await health_task
    except asyncio.CancelledError:
        pass

    if bt:
        await bt.close()
    await ipc.stop()
    LOG.info("Shutdown complete")


if __name__ == "__main__":
    asyncio.run(main())
