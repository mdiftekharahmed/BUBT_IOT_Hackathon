import time

import pytest
from fastapi.testclient import TestClient

from app.main import app, get_clock
from app.db import get_db
from app.models import Device, Room
from app.security import hash_token
from app.services.live import hub
from tests.conftest import JUDGE_PASSWORDS, login

TOKEN = "tok-dev"


@pytest.fixture(autouse=True)
def clean_hub():
    hub.reset()
    yield
    hub.reset()


@pytest.fixture()
def rooms_and_device(db):
    db.add(Room(id="R-305", name="Room 305"))
    db.add(Room(id="R-306", name="Room 306"))
    db.flush()
    db.add(Device(id="T17-D1", room_id="R-305", token_hash=hash_token(TOKEN)))
    db.flush()


def test_telemetry_reaches_browser_within_3s(api, rooms_and_device):
    login(api, "admin", JUDGE_PASSWORDS["admin"])
    with api.websocket_connect("/api/live") as ws:
        t0 = time.perf_counter()
        sent = hub.publish("R-305", "telemetry", {"temp_c": 29.0})
        msg = ws.receive_json()
        assert time.perf_counter() - t0 < 3.0
        assert sent >= 1
        assert msg["type"] == "telemetry"
        assert msg["room_id"] == "R-305"
        assert msg["data"]["temp_c"] == 29.0
        assert "sent_at" in msg


def test_student_gets_no_room_messages(api, rooms_and_device):
    login(api, "s2", JUDGE_PASSWORDS["s2"])
    with api.websocket_connect("/api/live") as ws:
        assert hub.publish("R-305", "telemetry", {"temp_c": 99.0}) == 0
        assert hub.publish("R-306", "event", {"type": "FIRE"}) == 0


def test_closed_session_stops_receiving(api, rooms_and_device):
    login(api, "admin", JUDGE_PASSWORDS["admin"])
    with api.websocket_connect("/api/live") as ws:
        assert hub.publish("R-305", "telemetry", {}) >= 1
        api.post("/api/auth/logout")
        assert hub.publish("R-305", "telemetry", {}) == 0


def test_command_reaches_device_and_ack_is_recorded(api, rooms_and_device):
    with api.websocket_connect("/api/device/ws", headers={"Authorization": f"Bearer {TOKEN}"}) as ws:
        assert hub.send_command("T17-D1", {"cmd": "reset_fire"}) is True
        msg = ws.receive_json()
        assert msg["cmd"] == "reset_fire"
        assert "id" in msg

        ws.send_json({"ack": msg["id"]})
        deadline = time.time() + 3
        while time.time() < deadline and not hub.acknowledged(msg["id"]):
            time.sleep(0.02)
        assert hub.acknowledged(msg["id"])


def test_command_to_offline_device_returns_false(api, rooms_and_device):
    assert hub.send_command("T17-D1", {"cmd": "reset_fire"}) is False


def test_browser_live_rejects_anonymous(api, rooms_and_device):
    """No session cookie: the socket is closed instead of delivering room data."""
    with pytest.raises(Exception):
        with api.websocket_connect("/api/live") as ws:
            ws.receive_json(timeout=1.0)
