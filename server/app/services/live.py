"""Live update fan-out.

`notify()` is the cross-process half: any process that stores a row also queues a
NOTIFY on the `live` channel. Every api process runs one listener thread
(`start_listener`) that turns those notifications into `hub.publish()` calls,
which fan out to the browser WebSockets connected to that process.
"""
import asyncio
import json
import threading
import uuid
from datetime import datetime, timezone

import psycopg
from sqlalchemy import text
from sqlalchemy.orm import Session

from app.config import settings

CHANNEL = "live"


def notify(conn, room_id: str, type_: str, data: dict) -> None:
    """Send a live update to every api process listening on the `live` channel.

    `conn` is a SQLAlchemy Session (delivered when it commits) or a psycopg connection.
    """
    payload = json.dumps({"room_id": room_id, "type": type_, "data": data}, default=str)
    if isinstance(conn, Session):
        conn.execute(text("SELECT pg_notify(:channel, :payload)"), {"channel": CHANNEL, "payload": payload})
    else:
        conn.execute("SELECT pg_notify(%s, %s)", (CHANNEL, payload))


async def _safe_send(ws, msg: dict) -> None:
    try:
        await ws.send_json(msg)
    except Exception:
        pass


class _Browser:
    __slots__ = ("ws", "rooms", "sid", "loop")

    def __init__(self, ws, rooms: set[str], sid: str | None, loop) -> None:
        self.ws = ws
        self.rooms = rooms
        self.sid = sid
        self.loop = loop


class LiveHub:
    """Registry of this process's browser and device sockets.

    All methods are safe to call from any thread (test threads, worker-style
    code, the listener thread) as well as from the event loop itself.
    """

    def __init__(self) -> None:
        self._browsers: list[_Browser] = []
        self._devices: dict[str, tuple[asyncio.Queue, asyncio.AbstractEventLoop]] = {}
        self._pending: dict[str, dict] = {}
        self._acks: dict[str, str] = {}

    def reset(self) -> None:
        self._browsers = []
        self._devices = {}
        self._pending = {}
        self._acks = {}

    # --- browsers ---

    def add_browser(self, ws, rooms: set[str], sid: str | None) -> None:
        self._browsers.append(_Browser(ws, rooms, sid, asyncio.get_running_loop()))

    def remove_browser(self, ws) -> None:
        self._browsers = [b for b in self._browsers if b.ws is not ws]

    def session_revoked(self, sid: str | None) -> None:
        """Drop every browser connection bound to a logged-out or expired session."""
        if not sid:
            return
        self._browsers = [b for b in self._browsers if b.sid != sid]

    def publish(self, room_id: str, type_: str, data: dict) -> int:
        """Push a message to every allowed browser connection; returns how many got it."""
        msg = {
            "type": type_,
            "room_id": room_id,
            "data": data,
            "sent_at": datetime.now(timezone.utc).isoformat(),
        }
        sent = 0
        for b in list(self._browsers):
            if room_id in b.rooms:
                try:
                    asyncio.run_coroutine_threadsafe(_safe_send(b.ws, msg), b.loop)
                    sent += 1
                except RuntimeError:
                    pass  # loop already closed; the socket will be reaped below
        return sent

    # --- devices ---

    def add_device(self, device_id: str) -> asyncio.Queue:
        q: asyncio.Queue = asyncio.Queue()
        self._devices[device_id] = (q, asyncio.get_running_loop())
        return q

    def remove_device(self, device_id: str) -> None:
        self._devices.pop(device_id, None)

    def send_command(self, device_id: str, cmd: dict) -> bool:
        """Queue a server-to-device command; False when the device is not connected."""
        entry = self._devices.get(device_id)
        if entry is None:
            return False
        msg = {"id": uuid.uuid4().hex, **cmd}
        self._pending[msg["id"]] = {"device_id": device_id, "cmd": cmd}
        queue, loop = entry
        loop.call_soon_threadsafe(queue.put_nowait, msg)
        return True

    def record_ack(self, cmd_id: str) -> None:
        self._pending.pop(cmd_id, None)
        self._acks[cmd_id] = datetime.now(timezone.utc).isoformat()

    def acknowledged(self, cmd_id: str) -> bool:
        return cmd_id in self._acks


hub = LiveHub()


def start_listener() -> None:
    """Start the daemon thread that turns NOTIFY payloads into hub publishes."""

    def run() -> None:
        dsn = settings.database_url.replace("postgresql+psycopg://", "postgresql://")
        try:
            conn = psycopg.connect(dsn, autocommit=True)
        except Exception:
            return
        conn.execute(f"LISTEN {CHANNEL}")
        while True:
            try:
                for n in conn.notifies(timeout=5):
                    payload = json.loads(n.payload or "{}")
                    hub.publish(payload.get("room_id"), payload.get("type") or "", payload.get("data") or {})
            except Exception:
                pass  # keep listening; the next NOTIFY retries

    threading.Thread(target=run, daemon=True, name="live-listener").start()
