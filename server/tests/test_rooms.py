from datetime import timedelta

import pytest
from sqlalchemy import select

from app.main import app, get_clock
from app.models import Device, Event, Room, RoomTeacher, Telemetry, User
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
def rooms_scoped(db, users, clock):
    db.add(Room(id="R-305", name="Room 305"))
    db.add(Room(id="R-306", name="Room 306"))
    db.flush()
    db.add(RoomTeacher(room_id="R-305", user_id=db.scalar(select(User.id).where(User.username == "teacher"))))
    db.add(Device(id="T17-D1", room_id="R-305", token_hash=hash_token(TOKEN),
                  reset_reason="WATCHDOG", online=True, last_seen_at=clock.now()))
    db.flush()


def _telemetry(db, clock, seq=1, **over):
    rec = dict(
        device_id="T17-D1", room_id="R-305", seq=seq, ts=clock.now(), uptime_s=742,
        temp_c=31.5, rh=70.0, heat_index_c=36.1, flame=False, door="LOCKED",
        status="WARNING", heat_trend="RISING", rssi=-61, heap_kb=182,
        chip_temp_c=41.5, fw="1.0.0", quality="good",
    )
    rec.update(over)
    db.add(Telemetry(**rec))
    db.flush()


def test_state_matches_latest_telemetry(db, api, clock, rooms_scoped):
    _telemetry(db, clock, seq=1, status="WARNING")
    _telemetry(db, clock, seq=2, status="DANGER", temp_c=41.0, door="UNLOCKED")

    login(api, "admin", JUDGE_PASSWORDS["admin"])
    r = api.get("/api/rooms/R-305/state")
    assert r.status_code == 200
    state = r.json()
    assert state["status"] == "DANGER"
    assert state["temp_c"] == 41.0
    assert state["rh"] == 70.0
    assert state["heat_index_c"] == 36.1
    assert state["heat_trend"] == "RISING"
    assert state["flame"] is False
    assert state["door"] == "UNLOCKED"
    assert state["online"] is True
    assert state["quality"] == "good"
    assert state["fire_active"] is False
    assert state["panic_active"] is False


def test_state_fire_and_panic_flags(db, api, clock, rooms_scoped):
    _telemetry(db, clock)
    db.add(Event(device_id="T17-D1", room_id="R-305", seq=1, ts=clock.now(), type="FIRE", detail=""))
    db.add(Event(device_id="T17-D1", room_id="R-305", seq=2, ts=clock.now(), type="PANIC", detail=""))
    db.flush()

    login(api, "admin", JUDGE_PASSWORDS["admin"])
    state = api.get("/api/rooms/R-305/state").json()
    assert state["fire_active"] is True
    assert state["panic_active"] is True

    db.add(Event(device_id="T17-D1", room_id="R-305", seq=3, ts=clock.now(), type="FIRE_CLEARED", detail=""))
    db.flush()
    state = api.get("/api/rooms/R-305/state").json()
    assert state["fire_active"] is False
    assert state["panic_active"] is True  # clears 10 s after the PANIC

    clock.advance(11)
    state = api.get("/api/rooms/R-305/state").json()
    assert state["panic_active"] is False


def test_health_matches_device_health_fields(db, api, clock, rooms_scoped):
    _telemetry(db, clock)
    login(api, "admin", JUDGE_PASSWORDS["admin"])
    r = api.get("/api/rooms/R-305/health")
    assert r.status_code == 200
    h = r.json()
    assert h["uptime_s"] == 742
    assert h["rssi"] == -61
    assert h["heap_kb"] == 182
    assert h["chip_temp_c"] == 41.5
    assert h["fw"] == "1.0.0"
    assert h["reset_reason"] == "WATCHDOG"
    assert h["last_seen_at"] is not None


def test_rooms_list_scoped_by_role(db, api, rooms_scoped):
    login(api, "admin", JUDGE_PASSWORDS["admin"])
    ids = [r["id"] for r in api.get("/api/rooms").json()]
    assert sorted(ids) == ["R-305", "R-306"]

    login(api, "teacher", JUDGE_PASSWORDS["teacher"])
    ids = [r["id"] for r in api.get("/api/rooms").json()]
    assert ids == ["R-305"]  # teacher of that room only

    login(api, "s2", JUDGE_PASSWORDS["s2"])
    assert api.get("/api/rooms").status_code == 403


def test_teacher_cannot_read_other_room_403(api, rooms_scoped):
    login(api, "teacher", JUDGE_PASSWORDS["teacher"])
    assert api.get("/api/rooms/R-305/state").status_code == 200
    assert api.get("/api/rooms/R-306/state").status_code == 403
    assert api.get("/api/rooms/R-306/health").status_code == 403
    assert api.get("/api/rooms/R-306/events").status_code == 403


def test_reset_fire_sends_command_over_device_channel(db, api, clock, rooms_scoped):
    login(api, "teacher", JUDGE_PASSWORDS["teacher"])
    with api.websocket_connect("/api/device/ws", headers={"Authorization": f"Bearer {TOKEN}"}) as ws:
        r = api.post("/api/rooms/R-305/reset-fire")
        assert r.status_code == 200
        msg = ws.receive_json()
        assert msg["cmd"] == "reset_fire"
        assert "id" in msg


def test_reset_fire_refused_for_student_403(api, rooms_scoped):
    login(api, "s2", JUDGE_PASSWORDS["s2"])
    r = api.post("/api/rooms/R-305/reset-fire")
    assert r.status_code == 403


def test_reset_fire_offline_device_returns_409_with_message(api, rooms_scoped):
    login(api, "teacher", JUDGE_PASSWORDS["teacher"])
    r = api.post("/api/rooms/R-305/reset-fire")
    assert r.status_code == 409
    assert "offline" in r.json()["detail"]


def test_alerts_active_lists_fire_and_panic(db, api, clock, rooms_scoped):
    _telemetry(db, clock)
    db.add(Event(device_id="T17-D1", room_id="R-305", seq=1, ts=clock.now(), type="FIRE", detail=""))
    db.add(Event(device_id="T17-D1", room_id="R-306", seq=2, ts=clock.now(), type="PANIC", detail=""))
    db.flush()

    login(api, "admin", JUDGE_PASSWORDS["admin"])
    alerts = api.get("/api/alerts/active").json()
    kinds = {(a["room_id"], a["kind"]) for a in alerts}
    assert kinds == {("R-305", "FIRE"), ("R-306", "PANIC")}
    assert all("since" in a for a in alerts)

    # students and guardians poll this for the banner; they still get no room data
    login(api, "s2", JUDGE_PASSWORDS["s2"])
    alerts = api.get("/api/alerts/active").json()
    assert {(a["room_id"], a["kind"]) for a in alerts} == {("R-305", "FIRE"), ("R-306", "PANIC")}


def test_alerts_active_empty_when_all_clear(db, api, clock, rooms_scoped):
    login(api, "admin", JUDGE_PASSWORDS["admin"])
    assert api.get("/api/alerts/active").json() == []
