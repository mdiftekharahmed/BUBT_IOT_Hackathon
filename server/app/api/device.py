from fastapi import APIRouter, Depends, Header, HTTPException, Request
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

from app.clock import Clock
from app.db import get_db
from app.models import Device
from app.security import token_matches
from app.services.ingest import IngestResult, validate_and_store

router = APIRouter(prefix="/api/device", tags=["device"])


def get_clock() -> Clock:
    from app.clock import SystemClock
    return SystemClock()


def current_device(
    authorization: str = Header(default=""),
    device_id: str | None = Header(default=None, alias="X-Device-Id"),
    db: Session = Depends(get_db),
) -> Device:
    scheme, _, token = authorization.partition(" ")
    if scheme.lower() != "bearer" or not token:
        raise HTTPException(401, "missing bearer token")
    query = db.query(Device)
    if device_id:
        query = query.filter(Device.id == device_id)
    for dev in query.all():
        if token_matches(token, dev.token_hash):
            return dev
    raise HTTPException(401, "invalid device token")


async def _json(request: Request):
    try:
        return await request.json()
    except ValueError:
        raise HTTPException(422, "body is not valid JSON")


def _ingest(kind: str, body, device: Device, db: Session, clock: Clock) -> JSONResponse:
    single = isinstance(body, dict)
    items = [body] if single else body
    if not isinstance(items, list) or not all(isinstance(i, dict) for i in items):
        raise HTTPException(422, "body must be an object or a list of objects")
    results: list[IngestResult] = [validate_and_store(db, device, i, kind, clock) for i in items]
    db.commit()
    payload = {"results": [r.model_dump() for r in results]}
    if single:
        r = results[0]
        code = {"stored": 200, "duplicate": 409, "rejected": 422}[r.status]
        return JSONResponse(payload, status_code=code)
    return JSONResponse(payload, status_code=200)


@router.post("/telemetry")
async def telemetry(request: Request, device: Device = Depends(current_device),
                    db: Session = Depends(get_db), clock: Clock = Depends(get_clock)):
    return _ingest("telemetry", await _json(request), device, db, clock)


@router.post("/events")
async def events(request: Request, device: Device = Depends(current_device),
                 db: Session = Depends(get_db), clock: Clock = Depends(get_clock)):
    return _ingest("events", await _json(request), device, db, clock)
