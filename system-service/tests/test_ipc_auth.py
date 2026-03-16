"""IPC authorization tests — prove the peer credential policy via dependency injection.

These tests verify:
1. Socket permission model (0660 for group-level access)
2. Authorization integration (injected authorizer controls accept/reject)
3. check_peer_authorized() policy logic (root, openauto member, primary GID, non-member, missing group)

No test relies on the test runner's actual uid/gid.
"""

import asyncio
import json
import os
import stat
import struct
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from ipc_server import IpcServer, check_peer_authorized


class TestSocketPermissions(unittest.IsolatedAsyncioTestCase):
    """Verify the socket is created with restrictive permissions."""

    async def asyncSetUp(self) -> None:
        self._tmpdir = tempfile.TemporaryDirectory()
        self.socket_path = os.path.join(self._tmpdir.name, "test.sock")
        self.server = IpcServer(
            self.socket_path,
            authorizer=lambda sock: (True, {"pid": 0, "uid": 0, "gid": 0}),
        )
        await self.server.start()

    async def asyncTearDown(self) -> None:
        await self.server.stop()
        self._tmpdir.cleanup()

    async def test_socket_created_with_0660(self) -> None:
        mode = stat.S_IMODE(os.stat(self.socket_path).st_mode)
        self.assertEqual(mode, 0o660)


class TestAuthorizationIntegration(unittest.IsolatedAsyncioTestCase):
    """Integration tests: inject authorizer callables to simulate caller identities."""

    async def asyncSetUp(self) -> None:
        self._tmpdir = tempfile.TemporaryDirectory()
        self.socket_path = os.path.join(self._tmpdir.name, "test.sock")

    async def asyncTearDown(self) -> None:
        self._tmpdir.cleanup()

    async def test_authorized_caller_can_exchange_messages(self) -> None:
        """Server with permissive authorizer accepts connections and exchanges messages."""
        server = IpcServer(
            self.socket_path,
            authorizer=lambda sock: (True, {"pid": 100, "uid": 1000, "gid": 1000}),
        )
        await server.start()
        try:
            reader, writer = await asyncio.open_unix_connection(self.socket_path)
            request = json.dumps({"id": "auth1", "method": "ping"}) + "\n"
            writer.write(request.encode())
            await writer.drain()
            line = await asyncio.wait_for(reader.readline(), timeout=2.0)
            response = json.loads(line)
            self.assertEqual(response, {"id": "auth1", "result": "pong"})
            writer.close()
            await writer.wait_closed()
        finally:
            await server.stop()

    async def test_unauthorized_caller_receives_permission_denied(self) -> None:
        """Server with rejecting authorizer sends permission denied error."""
        server = IpcServer(
            self.socket_path,
            authorizer=lambda sock: (False, {"pid": 999, "uid": 65534, "gid": 65534}),
        )
        await server.start()
        try:
            reader, writer = await asyncio.open_unix_connection(self.socket_path)
            line = await asyncio.wait_for(reader.readline(), timeout=2.0)
            response = json.loads(line)
            self.assertEqual(
                response,
                {"error": {"code": -32600, "message": "permission denied"}},
            )
            writer.close()
            await writer.wait_closed()
        finally:
            await server.stop()

    async def test_permission_denied_has_correct_json_rpc_structure(self) -> None:
        """Permission denied response has code -32600 and message 'permission denied'."""
        server = IpcServer(
            self.socket_path,
            authorizer=lambda sock: (False, {"pid": 1, "uid": 1, "gid": 1}),
        )
        await server.start()
        try:
            reader, writer = await asyncio.open_unix_connection(self.socket_path)
            line = await asyncio.wait_for(reader.readline(), timeout=2.0)
            response = json.loads(line)
            self.assertIn("error", response)
            self.assertEqual(response["error"]["code"], -32600)
            self.assertEqual(response["error"]["message"], "permission denied")
            writer.close()
            await writer.wait_closed()
        finally:
            await server.stop()

    async def test_connection_closed_after_denial(self) -> None:
        """After receiving the error, the connection is closed by the server."""
        server = IpcServer(
            self.socket_path,
            authorizer=lambda sock: (False, {"pid": 999, "uid": 65534, "gid": 65534}),
        )
        await server.start()
        try:
            reader, writer = await asyncio.open_unix_connection(self.socket_path)
            # Read the error message
            _error_line = await asyncio.wait_for(reader.readline(), timeout=2.0)
            # Next read should return empty (connection closed)
            eof = await asyncio.wait_for(reader.readline(), timeout=2.0)
            self.assertEqual(eof, b"")
            writer.close()
            await writer.wait_closed()
        finally:
            await server.stop()


def _make_peercred(pid: int, uid: int, gid: int) -> bytes:
    """Pack pid/uid/gid into SO_PEERCRED struct format."""
    return struct.pack("iII", pid, uid, gid)


class TestCheckPeerAuthorized(unittest.TestCase):
    """Unit tests for check_peer_authorized() — mock all OS calls."""

    def _mock_sock(self, pid: int, uid: int, gid: int) -> MagicMock:
        sock = MagicMock()
        sock.getsockopt.return_value = _make_peercred(pid, uid, gid)
        return sock

    @patch("ipc_server.grp")
    @patch("ipc_server.pwd")
    def test_root_uid_is_authorized(self, mock_pwd, mock_grp) -> None:
        sock = self._mock_sock(pid=1, uid=0, gid=0)
        authorized, cred = check_peer_authorized(sock)
        self.assertTrue(authorized)
        self.assertEqual(cred["uid"], 0)
        # grp/pwd should not be called for root
        mock_grp.getgrnam.assert_not_called()

    @patch("ipc_server.pwd")
    @patch("ipc_server.grp")
    def test_openauto_group_member_authorized(self, mock_grp, mock_pwd) -> None:
        sock = self._mock_sock(pid=100, uid=1000, gid=1000)

        grp_entry = MagicMock()
        grp_entry.gr_gid = 999
        grp_entry.gr_mem = ["testuser"]
        mock_grp.getgrnam.return_value = grp_entry

        pwd_entry = MagicMock()
        pwd_entry.pw_name = "testuser"
        mock_pwd.getpwuid.return_value = pwd_entry

        authorized, cred = check_peer_authorized(sock)
        self.assertTrue(authorized)
        self.assertEqual(cred["uid"], 1000)

    @patch("ipc_server.pwd")
    @patch("ipc_server.grp")
    def test_openauto_primary_gid_authorized(self, mock_grp, mock_pwd) -> None:
        """User whose primary GID matches openauto group GID is authorized."""
        sock = self._mock_sock(pid=100, uid=1000, gid=999)

        grp_entry = MagicMock()
        grp_entry.gr_gid = 999
        grp_entry.gr_mem = []  # NOT in supplementary members
        mock_grp.getgrnam.return_value = grp_entry

        pwd_entry = MagicMock()
        pwd_entry.pw_name = "someuser"
        mock_pwd.getpwuid.return_value = pwd_entry

        authorized, cred = check_peer_authorized(sock)
        self.assertTrue(authorized)

    @patch("ipc_server.pwd")
    @patch("ipc_server.grp")
    def test_non_member_unauthorized(self, mock_grp, mock_pwd) -> None:
        sock = self._mock_sock(pid=100, uid=1000, gid=1000)

        grp_entry = MagicMock()
        grp_entry.gr_gid = 999
        grp_entry.gr_mem = ["otheruser"]
        mock_grp.getgrnam.return_value = grp_entry

        pwd_entry = MagicMock()
        pwd_entry.pw_name = "testuser"
        mock_pwd.getpwuid.return_value = pwd_entry

        authorized, cred = check_peer_authorized(sock)
        self.assertFalse(authorized)

    @patch("ipc_server.pwd")
    @patch("ipc_server.grp")
    def test_no_openauto_group_unauthorized(self, mock_grp, mock_pwd) -> None:
        """When openauto group doesn't exist, non-root callers are unauthorized."""
        sock = self._mock_sock(pid=100, uid=1000, gid=1000)
        mock_grp.getgrnam.side_effect = KeyError("openauto")

        authorized, cred = check_peer_authorized(sock)
        self.assertFalse(authorized)
