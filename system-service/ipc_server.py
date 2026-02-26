import asyncio
import inspect
import json
import logging
import os
from collections.abc import Awaitable, Callable
from typing import Any

log = logging.getLogger(__name__)

JsonDict = dict[str, Any]
MethodHandler = Callable[[JsonDict], Awaitable[Any]]


class IpcServer:
    def __init__(self, socket_path: str = "/run/openauto/system.sock") -> None:
        self._socket_path = socket_path
        self._server: asyncio.AbstractServer | None = None
        self._methods: dict[str, MethodHandler] = {}
        self.register_method("ping", self._handle_ping)

    @property
    def socket_path(self) -> str:
        return self._socket_path

    def register_method(self, name: str, handler: MethodHandler) -> None:
        if not name:
            raise ValueError("Method name cannot be empty")
        if not inspect.iscoroutinefunction(handler):
            raise TypeError("Method handler must be an async callable")
        self._methods[name] = handler

    async def start(self) -> None:
        if os.path.exists(self._socket_path):
            os.unlink(self._socket_path)

        socket_dir = os.path.dirname(self._socket_path)
        if socket_dir:
            os.makedirs(socket_dir, exist_ok=True)

        self._server = await asyncio.start_unix_server(self._handle_client, path=self._socket_path)
        os.chmod(self._socket_path, 0o666)
        log.info("IPC server listening on %s", self._socket_path)

    async def stop(self) -> None:
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None

        if os.path.exists(self._socket_path):
            os.unlink(self._socket_path)

    async def _handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break

                response = await self._dispatch(line.decode("utf-8").strip())
                writer.write(json.dumps(response).encode("utf-8") + b"\n")
                await writer.drain()
        except Exception as err:
            log.warning("Client connection failed: %s", err)
        finally:
            writer.close()
            await writer.wait_closed()

    async def _dispatch(self, raw: str) -> JsonDict:
        try:
            message = json.loads(raw)
        except json.JSONDecodeError:
            return {
                "error": {
                    "code": "PARSE_ERROR",
                    "message": "Invalid JSON",
                }
            }

        if not isinstance(message, dict):
            return {
                "error": {
                    "code": "INVALID_REQUEST",
                    "message": "Request must be a JSON object",
                }
            }

        msg_id = message.get("id")
        method = message.get("method")
        params = message.get("params", {})

        if not isinstance(method, str) or not method:
            return {
                "id": msg_id,
                "error": {
                    "code": "INVALID_REQUEST",
                    "message": "Missing or invalid method",
                },
            }

        if not isinstance(params, dict):
            return {
                "id": msg_id,
                "error": {
                    "code": "INVALID_REQUEST",
                    "message": "params must be an object",
                },
            }

        handler = self._methods.get(method)
        if handler is None:
            return {
                "id": msg_id,
                "error": {
                    "code": "UNKNOWN_METHOD",
                    "message": f"Unknown method: {method}",
                },
            }

        try:
            result = await handler(params)
            return {"id": msg_id, "result": result}
        except Exception as err:
            log.exception("Method %s failed", method)
            return {
                "id": msg_id,
                "error": {
                    "code": "INTERNAL_ERROR",
                    "message": str(err),
                },
            }

    async def _handle_ping(self, params: JsonDict) -> str:
        _ = params
        return "pong"
