from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.clock import Clock, get_clock
from app.db import get_db
from app.models import Device, Event, Room, RoomTeacher, User
from app.security import current_user, require_role
from app.services import room_state as state_svc
from app.services.live import hub

router = APIRouter(tags=["rooms"])


def _teach_room_ids(db: Session, user: User) -> set[str]:
    rows = db.query(RoomTeacher).filter(RoomTeacher.user_id == user.id).all()
    return {row.room_id for row in rows}


def _room_guard(room_id: str, user: User, db: Session) -> None:
    """Admin: any room. Teacher: only the rooms they teach. Others: 403."""
    if user.role == "admin":
        return
    if user.role == "teacher" and room_id in _teach_room_ids(db, user):
        return
    raise HTTPException(403, "not allowed")


def _room_or_404(db: Session, room_id: str) -> Room:
    room = db.get(Room, room_id)
    if room is None:
        raise HTTPException(404, "no such room")
    return room


@router.get("/api/rooms")
def list_rooms(user: User = Depends(require_role("admin", "teacher")), db: Session = Depends(get_db)):
    if user.role == "admin":
        rooms = db.query(Room).order_by(Room.id).all()
    else:
        teach = _teach_room_ids(db, user)
        rooms = [r for r in db.query(Room).order_by(Room.id).all() if r.id in teach]
    return [{"id": r.id, "name": r.name} for r in rooms]


@router.get("/api/rooms/{room_id}/state")
def room_state(room_id: str, user: User = Depends(current_user), db: Session = Depends(get_db),
               clock: Clock = Depends(get_clock)):
    _room_or_404(db, room_id)
    _room_guard(room_id, user, db)
    tel = state_svc.latest_telemetry(db, room_id)  # type: ignore[arg-type]
    device = state_svc.room_device(db, room_id)  # type: ignore[arg-type]
    return {
        "status": tel.status if tel else None,
        "temp_c": tel.temp_c if tel else None,
        "rh": tel.rh if tel else None,
        "heat_index_c": tel.heat_index_c if tel else None,
        "heat_trend": tel.heat_trend if tel else None,
        "flame": tel.flame if tel else None,
        "door": tel.door if tel else None,
        "online": device.online if device else False,
        "last_seen_at": device.last_seen_at.isoformat() if device and device.last_seen_at else None,
        "quality": tel.quality if tel else None,
        "fire_active": state_svc.fire_active(db, room_id),  # type: ignore[arg-type]
        "panic_active": state_svc.panic_active(db, room_id, clock),  # type: ignore[arg-type]
    }


@router.get("/api/rooms/{room_id}/events")
def room_events(room_id: str, limit: int = 50, user: User = Depends(current_user),
                db: Session = Depends(get_db)):
    _room_or_404(db, room_id)
    _room_guard(room_id, user, db)
    rows = (
        db.execute(
            select(Event)
            .where(Event.room_id == room_id)
            .order_by(Event.id.desc())
            .limit(max(1, min(limit, 200)))
        )
        .scalars()
        .all()
    )
    return [
        {
            "device_id": r.device_id,
            "seq": r.seq,
            "ts": r.ts.isoformat(),
            "type": r.type,
            "detail": r.detail,
            "received_at": r.received_at.isoformat() if r.received_at else None,
        }
        for r in rows
    ]


@router.get("/api/rooms/{room_id}/health")
def room_health(room_id: str, user: User = Depends(current_user), db: Session = Depends(get_db)):
    _room_or_404(db, room_id)
    _room_guard(room_id, user, db)
    tel = state_svc.latest_telemetry(db, room_id)  # type: ignore[arg-type]
    device = state_svc.room_device(db, room_id)  # type: ignore[arg-type]
    return {
        "last_seen_at": device.last_seen_at.isoformat() if device and device.last_seen_at else None,
        "uptime_s": tel.uptime_s if tel else None,
        "rssi": tel.rssi if tel else None,
        "heap_kb": tel.heap_kb if tel else None,
        "chip_temp_c": tel.chip_temp_c if tel else None,
        "fw": (tel.fw if tel and tel.fw else device.fw) if (tel or device) else None,
        "reset_reason": device.reset_reason if device else None,
    }


@router.post("/api/rooms/{room_id}/reset-fire")
def reset_fire(room_id: str, user: User = Depends(require_role("teacher", "admin")),
               db: Session = Depends(get_db)):
    _room_or_404(db, room_id)
    _room_guard(room_id, user, db)
    device = state_svc.room_device(db, room_id)  # type: ignore[arg-type]
    if device is None or not hub.send_command(device.id, {"cmd": "reset_fire"}):
        raise HTTPException(409, "device offline; cannot send reset_fire")
    return {"ok": True, "room_id": room_id}


@router.get("/api/alerts/active")
def alerts_active(user: User = Depends(current_user), db: Session = Depends(get_db),
                  clock: Clock = Depends(get_clock)):
    """Banner source for roles that receive no room messages on /api/live."""
    return state_svc.active_alerts(db, clock)  # type: ignore[arg-type]

