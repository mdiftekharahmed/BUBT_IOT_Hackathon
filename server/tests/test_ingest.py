import hashlib
from datetime import timezone

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import func, select

from app.clock import SystemClock
from app.db import get_db
from app.main import app, get_clock
from app.models import Device, Event, Room, Telemetry

TOKEN = "tok-secret"
BASE_TS = 1794567890  # 2026-11-13


def _record(seq: int, **over) -> dict:
    rec = {
        "device_id": "T17-D1", "room_id": "R-305", "seq": seq, "ts": BASE_TS + seq,
        "uptime_s": 742, "temp_c": 29.0, "rh": 64, "heat_index_c": 31.2, "flame": False,
        "door": "LOCKED", "status": "SAFE", "heat_trend": "STABLE", "rssi": -61,
        "heap_kb": 182, "chip_temp_c": 41.5, "fw": "1.0.0", "quality": "good",
    }
    rec.update(over)
    return rec


@pytest.fixture()
def client(db, clock):
    db.add(Room(id="R-305", name="Room 305"))
    db.add(Room(id="R-306", name="Room 306"))
    db.flush()
    db.add(Device(id="T17-D1", room_id="R-305", token_hash=hashlib.sha256(TOKEN.encode()).hexdigest()))
    db.flush()
    app.dependency_overrides[get_db] = lambda: db
    app.dependency_overrides[get_clock] = lambda: clock
    yield TestClient(app)
    app.dependency_overrides.clear()


def _post(client, path, body, token=TOKEN):
    return client.post(f"/api/device/{path}", json=body, headers={"Authorization": f"Bearer {token}"})


def _count(db, model):
    return db.scalar(select(func.count()).select_from(model))


def test_valid_telemetry_stored(client, db):
    r = _post(client, "telemetry", _record(1))
    assert r.status_code == 200
    assert r.json()["results"][0]["status"] == "stored"
    assert _count(db, Telemetry) == 1
    dev = db.get(Device, "T17-D1")
    assert dev.online is True and dev.fw == "1.0.0" and dev.last_seen_at is not None


def test_replay_returns_409_and_keeps_first(client, db):
    assert _post(client, "telemetry", _record(1, temp_c=29.0)).status_code == 200
    r = _post(client, "telemetry", _record(1, temp_c=99.0))
    assert r.status_code == 409
    assert _count(db, Telemetry) == 1
    assert db.scalar(select(Telemetry.temp_c)) == 29.0


def test_batch_with_one_duplicate_stores_nine(client, db):
    assert _post(client, "telemetry", _record(4)).status_code == 200
    batch = [_record(s) for s in range(1, 11)]
    r = _post(client, "telemetry", batch)
    assert r.status_code == 200
    results = r.json()["results"]
    assert len(results) == 10
    assert [x["status"] for x in results].count("stored") == 9
    assert results[3]["status"] == "duplicate"
    assert _count(db, Telemetry) == 10


def test_wrong_token_401(client, db):
    assert _post(client, "telemetry", _record(1), token="nope").status_code == 401
    assert _count(db, Telemetry) == 0


def test_missing_field_422_names_the_field(client):
    rec = _record(1)
    del rec["status"]
    r = _post(client, "telemetry", rec)
    assert r.status_code == 422
    assert "status" in r.text


def test_new_room_id_moves_device(client, db):
    """The judge types the Room ID on the device's setup page: the device's room follows it."""
    r = _post(client, "telemetry", _record(1, room_id="R-412"))
    assert r.status_code == 200
    assert db.get(Room, "R-412") is not None
    assert db.get(Device, "T17-D1").room_id == "R-412"
    assert db.scalar(select(Telemetry.room_id)) == "R-412"


def test_room_move_keeps_teacher_access(client, db):
    from app.models import RoomTeacher, User
    teacher = User(username="t-move", password_hash="x", role="teacher")
    db.add(teacher)
    db.flush()
    db.add(RoomTeacher(room_id="R-305", user_id=teacher.id))
    db.flush()
    assert _post(client, "telemetry", _record(1, room_id="R-412")).status_code == 200
    assert db.query(RoomTeacher).filter_by(room_id="R-412", user_id=teacher.id).one_or_none() is not None


def test_bad_room_id_rejected(client, db):
    r = _post(client, "telemetry", _record(1, room_id="R 412"))
    assert r.status_code == 422
    assert "invalid room_id" in r.text
    assert _count(db, Telemetry) == 0
    assert db.get(Device, "T17-D1").room_id == "R-305"


def test_room_switch_keeps_seq_unique(client, db):
    assert _post(client, "telemetry", _record(5)).status_code == 200
    assert _post(client, "telemetry", _record(5, room_id="R-412")).status_code == 409
    assert _count(db, Telemetry) == 1


def test_bad_clock_sets_ts_suspect(client, db):
    assert _post(client, "telemetry", _record(1, ts=1546300800)).status_code == 200  # 2019
    assert _post(client, "telemetry", _record(2, ts=BASE_TS + 2 * 86400)).status_code == 200  # ahead
    assert _post(client, "telemetry", _record(3)).status_code == 200
    flags = dict(db.execute(select(Telemetry.seq, Telemetry.ts_suspect)).all())
    assert flags == {1: True, 2: True, 3: False}


def test_event_stored_and_boot_sets_reset_reason(client, db):
    ev = {"device_id": "T17-D1", "room_id": "R-305", "seq": 7, "ts": BASE_TS,
          "type": "BOOT", "detail": "reason=WATCHDOG"}
    assert _post(client, "events", ev).status_code == 200
    assert _count(db, Event) == 1
    assert db.get(Device, "T17-D1").reset_reason == "WATCHDOG"
    assert _post(client, "events", ev).status_code == 409


def test_online_again_on_next_message(client, db):
    dev = db.get(Device, "T17-D1")
    dev.online = False
    db.flush()
    assert _post(client, "telemetry", _record(1)).status_code == 200
    db.refresh(dev)
    assert dev.online is True
