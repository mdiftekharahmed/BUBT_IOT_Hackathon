import asyncio

from fastapi import APIRouter, Depends, Header, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

from app.clock import Clock, get_clock  # noqa: F401  (get_clock re-exported; overridden in tests)
from app.db import get_db
from app.models import Device
from app.security import token_matches
from app.services.ingest import IngestResult, validate_and_store
from app.services.live import hub

router = APIRouter(prefix="/api/device", tags=["device"])


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


async def _read_acks(ws: WebSocket) -> None:
    try:
        while True:
            data = await ws.receive_json()
            if isinstance(data, dict) and "ack" in data:
                hub.record_ack(str(data["ack"]))
    except Exception:
        return  # socket closed


@router.websocket("/ws")
async def device_ws(ws: WebSocket, device: Device = Depends(current_device)):
    """Server-to-device command channel: delivers {"cmd": ..., "id": ...},
    accepts {"ack": id}. No inbound port on the device."""
    await ws.accept()
    queue = hub.add_device(device.id)
    reader = asyncio.create_task(_read_acks(ws))
    try:
        while True:
            msg = await queue.get()
            await ws.send_json(msg)
    except (WebSocketDisconnect, RuntimeError):
        pass
    finally:
        reader.cancel()
        hub.remove_device(device.id)
        try:
            await ws.close()
        except Exception:
            pass
