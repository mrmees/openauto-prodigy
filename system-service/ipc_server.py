import asyncio
import grp
import inspect
import json
import logging
import os
import pwd
import socket
import struct
from collections.abc import Awaitable, Callable
from typing import Any

log = logging.getLogger(__name__)

JsonDict = dict[str, Any]
MethodHandler = Callable[[JsonDict], Awaitable[Any]]
Authorizer = Callable[[socket.socket], tuple[bool, dict[str, Any]]]


def check_peer_authorized(sock: socket.socket) -> tuple[bool, dict[str, Any]]:
    """Check whether the peer on *sock* is authorized to use the IPC server.

    Authorization policy (non-negotiable):
      - uid 0 (root): always authorized
      - Otherwise: authorized if the peer's user is a member of the
        'openauto' group (supplementary or primary GID match).
      - If the 'openauto' group does not exist: unauthorized (unless root).

    Returns (authorized, cred_info) where cred_info has pid/uid/gid keys.
    """
    cred_data = sock.getsockopt(
        socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize("iII")
    )
    pid, uid, gid = struct.unpack("iII", cred_data)
    cred_info: dict[str, Any] = {"pid": pid, "uid": uid, "gid": gid}

    # Root is always authorized
    if uid == 0:
        return True, cred_info

    try:
        oap_group = grp.getgrnam("openauto")
    except KeyError:
        # Group doesn't exist — non-root callers are unauthorized
        return False, cred_info

    # Check primary GID match
    if gid == oap_group.gr_gid:
        return True, cred_info

    # Check supplementary group membership
    try:
        username = pwd.getpwuid(uid).pw_name
    except KeyError:
        return False, cred_info

    if username in oap_group.gr_mem:
        return True, cred_info

    return False, cred_info


class IpcServer:
    def __init__(
        self,
        socket_path: str = "/run/openauto/system.sock",
        authorizer: Authorizer | None = None,
    ) -> None:
        self._socket_path = socket_path
        self._authorizer: Authorizer = authorizer or check_peer_authorized
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
        os.chmod(self._socket_path, 0o660)

        # Set socket ownership to root:openauto so group members can connect
        try:
            oap_gid = grp.getgrnam("openauto").gr_gid
            os.chown(self._socket_path, 0, oap_gid)
        except KeyError:
            log.warning(
                "openauto group does not exist yet — socket keeps default ownership"
            )
        except PermissionError:
            log.debug(
                "Cannot chown socket (not running as root) — keeping default ownership"
            )

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
            # Check peer credentials before processing any messages
            sock = writer.get_extra_info("socket")
            authorized, cred_info = self._authorizer(sock)

            if not authorized:
                log.warning(
                    "Unauthorized IPC connection denied — pid=%s uid=%s gid=%s",
                    cred_info.get("pid"),
                    cred_info.get("uid"),
                    cred_info.get("gid"),
                )
                error_response = json.dumps(
                    {"error": {"code": -32600, "message": "permission denied"}}
                )
                writer.write(error_response.encode("utf-8") + b"\n")
                await writer.drain()
                return

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
