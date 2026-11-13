import json
import time

import psycopg
import pytest

from app.models import Device, Room
from app.services.live import notify
from app.worker import sweep_offline
from tests.conftest import TEST_URL


@pytest.fixture()
def device(db, clock):
    db.add(Room(id="R-305", name="Room 305"))
    db.flush()
    dev = Device(id="T17-D1", room_id="R-305", token_hash="x", online=True, last_seen_at=clock.now())
    db.add(dev)
    db.flush()
    return dev


def test_device_goes_offline_after_30s(db, clock, device):
    sent = []
    notifier = lambda *a: sent.append(a)

    clock.advance(29)
    assert sweep_offline(db, clock, notifier=notifier) == []
    assert device.online is True

    clock.advance(2)  # 31 s of silence
    assert sweep_offline(db, clock, notifier=notifier) == ["T17-D1"]
    db.refresh(device)
    assert device.online is False


def test_sweep_publishes_offline_event(db, clock, device):
    sent = []
    clock.advance(31)
    sweep_offline(db, clock, notifier=lambda *a: sent.append(a))
    assert len(sent) == 1
    _db, room_id, type_, data = sent[0]
    assert (room_id, type_) == ("R-305", "device_offline")
    assert data["device_id"] == "T17-D1"


def test_sweep_is_idempotent(db, clock, device):
    clock.advance(31)
    assert sweep_offline(db, clock, notifier=lambda *a: None) == ["T17-D1"]
    assert sweep_offline(db, clock, notifier=lambda *a: None) == []


def test_never_seen_device_stays_untouched(db, clock):
    db.add(Room(id="R-400", name="Room 400"))
    db.flush()
    db.add(Device(id="T17-D9", room_id="R-400", token_hash="x", online=False, last_seen_at=None))
    db.flush()
    clock.advance(3600)
    assert sweep_offline(db, clock, notifier=lambda *a: None) == []


def test_notify_roundtrip_reaches_listener(engine):
    """NOTIFY only reaches listeners on commit, so this uses real connections."""
    dsn = TEST_URL.replace("postgresql+psycopg://", "postgresql://")
    with psycopg.connect(dsn, autocommit=True) as listener:
        listener.execute("LISTEN live")
        with psycopg.connect(dsn, autocommit=True) as sender:
            notify(sender, "R-305", "telemetry", {"temp_c": 29.0})
        deadline = time.time() + 3
        got = None
        for n in listener.notifies(timeout=3):
            got = n
            break
        assert got is not None and time.time() < deadline
        payload = json.loads(got.payload)
        assert payload == {"room_id": "R-305", "type": "telemetry", "data": {"temp_c": 29.0}}
