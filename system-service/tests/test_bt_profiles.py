"""Tests for BT profile registration via dbus-next."""

import sys
import types
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

# ---------------------------------------------------------------------------
# Provide a fake dbus_next module so bt_profiles can be imported even when
# dbus-next is not installed.  The class methods use deferred imports, but
# we need the module present for ``from dbus_next.errors import DBusError``
# to resolve inside register_all().
# ---------------------------------------------------------------------------

_NEED_FAKE_DBUS = "dbus_next" not in sys.modules

if _NEED_FAKE_DBUS:
    _dbus_next = types.ModuleType("dbus_next")
    _dbus_next_aio = types.ModuleType("dbus_next.aio")
    _dbus_next_errors = types.ModuleType("dbus_next.errors")

    class _FakeDBusError(Exception):
        """Stand-in for dbus_next.errors.DBusError."""
        def __init__(self, error_name: str, message: str = ""):
            self.type = error_name
            super().__init__(f"{error_name}: {message}")

    class _FakeVariant:
        def __init__(self, sig, value):
            self.signature = sig
            self.value = value

    class _FakeBusType:
        SYSTEM = "system"
        SESSION = "session"

    _dbus_next_errors.DBusError = _FakeDBusError
    _dbus_next.errors = _dbus_next_errors
    _dbus_next.Variant = _FakeVariant
    _dbus_next.BusType = _FakeBusType
    _dbus_next.aio = _dbus_next_aio
    _dbus_next_aio.MessageBus = MagicMock()

    sys.modules["dbus_next"] = _dbus_next
    sys.modules["dbus_next.aio"] = _dbus_next_aio
    sys.modules["dbus_next.errors"] = _dbus_next_errors

from bt_profiles import (  # noqa: E402
    AA_UUID,
    HFP_AG_UUID,
    HSP_HS_UUID,
    PROFILES,
    BtProfileManager,
)
from dbus_next.errors import DBusError  # noqa: E402


class TestBtProfileConstants(unittest.TestCase):
    """Verify UUID constants and profile definitions."""

    def test_hfp_ag_uuid(self):
        self.assertEqual(HFP_AG_UUID, "0000111f-0000-1000-8000-00805f9b34fb")

    def test_hsp_hs_uuid(self):
        self.assertEqual(HSP_HS_UUID, "00001108-0000-1000-8000-00805f9b34fb")

    def test_aa_uuid(self):
        self.assertEqual(AA_UUID, "4de17a00-52cb-11e6-bdf4-0800200c9a66")

    def test_all_profiles_require_server_role(self):
        for profile in PROFILES:
            self.assertEqual(profile["options"]["Role"], "server")

    def test_all_profiles_no_auth_required(self):
        for profile in PROFILES:
            self.assertFalse(profile["options"]["RequireAuthentication"])
            self.assertFalse(profile["options"]["RequireAuthorization"])

    def test_aa_profile_channel_8(self):
        aa = [p for p in PROFILES if p["uuid"] == AA_UUID][0]
        self.assertEqual(aa["options"]["Channel"], 8)

    def test_three_profiles_defined(self):
        self.assertEqual(len(PROFILES), 3)


class TestBtProfileRegistration(unittest.IsolatedAsyncioTestCase):
    """Verify register_all() calls D-Bus correctly."""

    async def test_register_all_calls_three_times(self):
        mgr = BtProfileManager()
        mock_proxy = AsyncMock()

        with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
            await mgr.register_all()
            self.assertEqual(mock_proxy.call_register_profile.call_count, 3)

    async def test_register_all_passes_correct_uuids(self):
        mgr = BtProfileManager()
        mock_proxy = AsyncMock()

        with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
            await mgr.register_all()

        registered_uuids = [
            call.args[1]
            for call in mock_proxy.call_register_profile.call_args_list
        ]
        self.assertIn(HFP_AG_UUID, registered_uuids)
        self.assertIn(HSP_HS_UUID, registered_uuids)
        self.assertIn(AA_UUID, registered_uuids)

    async def test_register_all_passes_correct_paths(self):
        mgr = BtProfileManager()
        mock_proxy = AsyncMock()

        with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
            await mgr.register_all()

        registered_paths = [
            call.args[0]
            for call in mock_proxy.call_register_profile.call_args_list
        ]
        self.assertIn("/org/openauto/hfp_ag", registered_paths)
        self.assertIn("/org/openauto/hsp_hs", registered_paths)
        self.assertIn("/org/openauto/aa_wireless", registered_paths)

    async def test_already_exists_is_swallowed(self):
        mgr = BtProfileManager()
        mock_proxy = AsyncMock()
        mock_proxy.call_register_profile.side_effect = DBusError(
            "org.bluez.Error.AlreadyExists", "Already exists"
        )

        with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
            # Should not raise
            await mgr.register_all()

    async def test_other_dbus_errors_propagate(self):
        mgr = BtProfileManager()
        mock_proxy = AsyncMock()
        mock_proxy.call_register_profile.side_effect = DBusError(
            "org.bluez.Error.Failed", "Something went wrong"
        )

        with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
            with self.assertRaises(DBusError):
                await mgr.register_all()

    async def test_close_disconnects_bus(self):
        mgr = BtProfileManager()
        mock_bus = MagicMock()
        mgr._bus = mock_bus

        await mgr.close()

        mock_bus.disconnect.assert_called_once()
        self.assertIsNone(mgr._bus)

    async def test_close_noop_when_no_bus(self):
        mgr = BtProfileManager()
        # Should not raise
        await mgr.close()
