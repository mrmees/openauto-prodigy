import asyncio
import json
import os
import stat
import tempfile
import unittest

from ipc_server import IpcServer


class IpcServerTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self) -> None:
        self._tmpdir = tempfile.TemporaryDirectory()
        self.socket_path = os.path.join(self._tmpdir.name, "test.sock")
        self.server = IpcServer(self.socket_path)
        await self.server.start()

    async def asyncTearDown(self) -> None:
        await self.server.stop()
        self._tmpdir.cleanup()

    async def _request(self, payload: str) -> dict:
        reader, writer = await asyncio.open_unix_connection(self.socket_path)
        writer.write(payload.encode())
        await writer.drain()
        line = await asyncio.wait_for(reader.readline(), timeout=2.0)
        writer.close()
        await writer.wait_closed()
        return json.loads(line)

    async def test_server_creates_socket(self) -> None:
        self.assertTrue(os.path.exists(self.socket_path))

    async def test_socket_permissions_allow_non_root_clients(self) -> None:
        mode = stat.S_IMODE(os.stat(self.socket_path).st_mode)
        self.assertEqual(mode, 0o666)

    async def test_ping_returns_pong(self) -> None:
        request = json.dumps({"id": "1", "method": "ping"}) + "\n"
        response = await self._request(request)
        self.assertEqual(response, {"id": "1", "result": "pong"})

    async def test_unknown_method_returns_error(self) -> None:
        request = json.dumps({"id": "2", "method": "nonexistent", "params": {}}) + "\n"
        response = await self._request(request)
        self.assertEqual(response["id"], "2")
        self.assertEqual(response["error"]["code"], "UNKNOWN_METHOD")
        self.assertIn("Unknown method", response["error"]["message"])

    async def test_malformed_json_returns_error(self) -> None:
        response = await self._request("not json\n")
        self.assertEqual(response["error"]["code"], "PARSE_ERROR")

    async def test_registered_method_is_dispatched(self) -> None:
        async def echo(params):
            return {"value": params["value"]}

        self.server.register_method("echo", echo)
        request = json.dumps({"id": "3", "method": "echo", "params": {"value": 42}}) + "\n"
        response = await self._request(request)
        self.assertEqual(response, {"id": "3", "result": {"value": 42}})

    async def test_multiple_clients_supported_concurrently(self) -> None:
        async def send(client_id: str) -> dict:
            request = json.dumps({"id": client_id, "method": "ping", "params": {}}) + "\n"
            return await self._request(request)

        responses = await asyncio.gather(send("a"), send("b"), send("c"))
        self.assertEqual([item["result"] for item in responses], ["pong", "pong", "pong"])
