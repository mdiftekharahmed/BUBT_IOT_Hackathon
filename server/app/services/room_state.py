"""Shared room-state helpers: latest telemetry, fire latch and the 10 s panic window."""
from datetime import timedelta

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.clock import Clock
from app.models import Device, Event, Telemetry

PANIC_HOLD_S = 10


def latest_telemetry(db: Session, room_id: str) -> Telemetry | None:
    return (
        db.execute(select(Telemetry).where(Telemetry.room_id == room_id).order_by(Telemetry.id.desc()).limit(1))
        .scalar_one_or_none()
    )


def room_device(db: Session, room_id: str) -> Device | None:
    return db.query(Device).filter(Device.room_id == room_id).first()


def _last_event(db: Session, room_id: str, types: tuple[str, ...]) -> Event | None:
    return (
        db.execute(
            select(Event).where(Event.room_id == room_id, Event.type.in_(types)).order_by(Event.id.desc()).limit(1)
        )
        .scalar_one_or_none()
    )


def fire_active(db: Session, room_id: str) -> bool:
    """A FIRE event with no later FIRE_CLEARED."""
    ev = _last_event(db, room_id, ("FIRE", "FIRE_CLEARED"))
    return ev is not None and ev.type == "FIRE"


def panic_active(db: Session, room_id: str, clock: Clock) -> bool:
    """A PANIC event younger than 10 s (event time, not request drift)."""
    ev = _last_event(db, room_id, ("PANIC",))
    if ev is None:
        return False
    when = ev.ts if not ev.ts_suspect else ev.received_at
    return clock.now() - when <= timedelta(seconds=PANIC_HOLD_S)


def active_alerts(db: Session, clock: Clock) -> list[dict]:
    """FIRE and still-standing PANIC alerts per room, for the dashboard banner."""
    out: list[dict] = []
    room_ids = {row for (row,) in db.query(Event.room_id).distinct()}
    for room_id in sorted(room_ids):
        fire = _last_event(db, room_id, ("FIRE", "FIRE_CLEARED"))
        if fire is not None and fire.type == "FIRE":
            out.append({"room_id": room_id, "kind": "FIRE", "since": fire.ts.isoformat()})
        panic = _last_event(db, room_id, ("PANIC",))
        if panic is not None and panic_active(db, room_id, clock):
            out.append({"room_id": room_id, "kind": "PANIC", "since": panic.ts.isoformat()})
    return out
