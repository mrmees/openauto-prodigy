"""BT profile registration via dbus-next for BlueZ ProfileManager1."""

import asyncio
import logging

log = logging.getLogger(__name__)

HFP_AG_UUID = "0000111f-0000-1000-8000-00805f9b34fb"
HSP_HS_UUID = "00001108-0000-1000-8000-00805f9b34fb"
AA_UUID = "4de17a00-52cb-11e6-bdf4-0800200c9a66"

PROFILES = [
    {
        "uuid": HFP_AG_UUID,
        "path": "/org/openauto/hfp_ag",
        "options": {
            "Name": "Hands-Free AG",
            "Role": "server",
            "RequireAuthentication": False,
            "RequireAuthorization": False,
        },
    },
    {
        "uuid": HSP_HS_UUID,
        "path": "/org/openauto/hsp_hs",
        "options": {
            "Name": "Headset",
            "Role": "server",
            "RequireAuthentication": False,
            "RequireAuthorization": False,
        },
    },
    {
        "uuid": AA_UUID,
        "path": "/org/openauto/aa_wireless",
        "options": {
            "Name": "Android Auto Wireless",
            "Role": "server",
            "Channel": 8,
            "RequireAuthentication": False,
            "RequireAuthorization": False,
        },
    },
]


class BtProfileManager:
    def __init__(self):
        self._bus = None

    async def _get_profile_manager(self):
        """Get BlueZ ProfileManager1 D-Bus proxy."""
        from dbus_next.aio import MessageBus
        from dbus_next import BusType

        if self._bus is None:
            self._bus = await MessageBus(bus_type=BusType.SYSTEM).connect()

        introspection = await self._bus.introspect("org.bluez", "/org/bluez")
        proxy = self._bus.get_proxy_object(
            "org.bluez", "/org/bluez", introspection
        )
        return proxy.get_interface("org.bluez.ProfileManager1")

    async def register_all(self):
        """Register all BT profiles with BlueZ."""
        from dbus_next.errors import DBusError
        from dbus_next import Variant

        profile_mgr = await self._get_profile_manager()

        for profile in PROFILES:
            # Convert options to D-Bus Variant dict
            options = {}
            for key, value in profile["options"].items():
                if isinstance(value, bool):
                    options[key] = Variant("b", value)
                elif isinstance(value, int):
                    options[key] = Variant("q", value)  # uint16
                elif isinstance(value, str):
                    options[key] = Variant("s", value)

            try:
                await profile_mgr.call_register_profile(
                    profile["path"], profile["uuid"], options
                )
                log.info(
                    "Registered BT profile: %s (%s)",
                    profile["options"]["Name"],
                    profile["uuid"],
                )
            except DBusError as e:
                if "AlreadyExists" in str(e):
                    log.info(
                        "BT profile already registered: %s",
                        profile["options"]["Name"],
                    )
                else:
                    log.error(
                        "Failed to register %s: %s",
                        profile["options"]["Name"],
                        e,
                    )
                    raise

    async def close(self):
        """Disconnect from D-Bus."""
        if self._bus:
            self._bus.disconnect()
            self._bus = None
