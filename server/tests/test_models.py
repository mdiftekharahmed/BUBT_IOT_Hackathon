from datetime import datetime, timezone

import pytest
from sqlalchemy.exc import IntegrityError

from app.models import Device, Event, Room, Telemetry


def _telemetry(device_id: str, seq: int, temp: float = 29.0) -> Telemetry:
    return Telemetry(
        device_id=device_id, room_id="R-305", seq=seq,
        ts=datetime(2026, 11, 13, 9, 0, tzinfo=timezone.utc), uptime_s=10,
        temp_c=temp, rh=64, heat_index_c=31.2, flame=False, door="LOCKED",
        status="SAFE", heat_trend="STABLE", rssi=-61, heap_kb=182,
        chip_temp_c=41.5, fw="1.0.0", quality="good",
    )


def test_telemetry_unique_device_seq(db):
    db.add(Room(id="R-305", name="Room 305"))
    db.flush()
    db.add(Device(id="T17-D1", room_id="R-305", token_hash="x"))
    db.flush()
    db.add(_telemetry("T17-D1", 1))
    db.flush()
    db.add(_telemetry("T17-D1", 1, temp=35.0))
    with pytest.raises(IntegrityError):
        db.flush()


def test_events_unique_device_seq(db):
    db.add(Room(id="R-306", name="Room 306"))
    db.flush()
    db.add(Device(id="T17-D2", room_id="R-306", token_hash="x"))
    db.flush()
    ts = datetime(2026, 11, 13, 9, 0, tzinfo=timezone.utc)
    db.add(Event(device_id="T17-D2", room_id="R-306", seq=5, ts=ts, type="BOOT", detail="reason=POWER_ON"))
    db.flush()
    db.add(Event(device_id="T17-D2", room_id="R-306", seq=5, ts=ts, type="BOOT", detail="again"))
    with pytest.raises(IntegrityError):
        db.flush()
