import re
from datetime import datetime, timedelta, timezone
from typing import Literal

from pydantic import BaseModel, ValidationError
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.orm import Session

from app.clock import Clock
from app.models import Device, Event, Telemetry
from app.services.live import notify

EARLIEST_VALID_TS = datetime(2024, 1, 1, tzinfo=timezone.utc)
MAX_FUTURE = timedelta(days=1)


class TelemetryIn(BaseModel):
    device_id: str
    room_id: str
    seq: int
    ts: int
    uptime_s: int
    temp_c: float | None
    rh: float | None
    heat_index_c: float | None
    flame: bool
    door: Literal["LOCKED", "UNLOCKED"]
    status: Literal["SAFE", "WARNING", "DANGER", "FIRE"]
    heat_trend: Literal["RISING", "STABLE", "FALLING"] | None
    rssi: int | None
    heap_kb: int | None
    chip_temp_c: float | None
    fw: str
    quality: Literal["good", "bad"]


class EventIn(BaseModel):
    device_id: str
    room_id: str
    seq: int
    ts: int
    type: str
    detail: str = ""


class IngestResult(BaseModel):
    status: Literal["stored", "duplicate", "rejected"]
    reason: str | None = None


def _ts(ts: int, clock: Clock) -> tuple[datetime, bool]:
    try:
        when = datetime.fromtimestamp(ts, tz=timezone.utc)
    except (OverflowError, OSError, ValueError):
        return clock.now(), True
    suspect = when < EARLIEST_VALID_TS or when > clock.now() + MAX_FUTURE
    return when, suspect


def validate_and_store(db: Session, device: Device, record: dict, kind: str, clock: Clock) -> IngestResult:
    model = TelemetryIn if kind == "telemetry" else EventIn
    try:
        item = model.model_validate(record)
    except ValidationError as exc:
        fields = ", ".join(sorted({".".join(map(str, e["loc"])) for e in exc.errors()}))
        return IngestResult(status="rejected", reason=f"invalid fields: {fields}")
    if item.device_id != device.id:
        return IngestResult(status="rejected", reason="device_id does not match token")
    if item.room_id != device.room_id:
        return IngestResult(status="rejected", reason="room_id does not match device")

    when, suspect = _ts(item.ts, clock)
    values = item.model_dump()
    values["ts"] = when
    values["ts_suspect"] = suspect
    table = Telemetry if kind == "telemetry" else Event
    constraint = "uq_telemetry_device_seq" if kind == "telemetry" else "uq_events_device_seq"
    stmt = insert(table).values(**values).on_conflict_do_nothing(constraint=constraint).returning(table.id)
    stored = db.execute(stmt).scalar_one_or_none() is not None

    was_online = device.online
    device.last_seen_at = clock.now()
    device.online = True
    if not was_online:
        notify(db, device.room_id, "device_online", {"device_id": device.id})
    if stored:
        notify(db, device.room_id, "telemetry" if kind == "telemetry" else "event", values)
    if stored and kind == "telemetry":
        device.fw = item.fw
    if stored and kind == "events" and item.type == "BOOT":
        m = re.search(r"reason=(\w+)", item.detail)
        if m:
            device.reset_reason = m.group(1)
    return IngestResult(status="stored" if stored else "duplicate")
