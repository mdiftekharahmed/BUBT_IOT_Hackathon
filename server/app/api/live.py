import asyncio

from fastapi import APIRouter, Cookie, Depends, WebSocket
from sqlalchemy.orm import Session

from app.clock import SystemClock, get_clock  # noqa: F401  (get_clock re-exported for overrides)
from app.db import get_db
from app.models import Room, RoomTeacher, User
from app.security import COOKIE_NAME, current_user, load_session_user
from app.services.live import hub

router = APIRouter(tags=["live"])

_PING_EVERY_S = 20
_RECHECK_EVERY_PINGS = 3  # session rechecked every 60 s


def allowed_room_ids(db: Session, user: User) -> set[str]:
    """Admin: every room. Teacher: the rooms they teach. Student/Guardian: none."""
    if user.role == "admin":
        return {row.id for row in db.query(Room).all()}
    if user.role == "teacher":
        rows = db.query(RoomTeacher).filter(RoomTeacher.user_id == user.id).all()
        return {row.room_id for row in rows}
    return set()


@router.websocket("/api/live")
async def browser_live(
    ws: WebSocket,
    user: User = Depends(current_user),
    db: Session = Depends(get_db),
    fastrobox_session: str | None = Cookie(default=None, alias=COOKIE_NAME),
):
    rooms = allowed_room_ids(db, user)
    await ws.accept()
    hub.add_browser(ws, rooms, fastrobox_session)

    async def drain() -> None:
        try:
            async for _ in ws.iter_text():
                pass  # browsers only read; a message here means a client talking — ignore
        except Exception:
            pass

    async def pinger() -> None:
        count = 0
        while True:
            await asyncio.sleep(_PING_EVERY_S)
            count += 1
            if count % _RECHECK_EVERY_PINGS == 0:
                try:
                    alive = load_session_user(db, fastrobox_session, SystemClock())
                except Exception:
                    alive = None
                if alive is None or alive.id != user.id:
                    return
            try:
                await ws.send_json({"ping": True})
            except Exception:
                return

    receiver = asyncio.create_task(drain())
    keeper = asyncio.create_task(pinger())
    try:
        await asyncio.wait({receiver, keeper}, return_when=asyncio.FIRST_COMPLETED)
    finally:
        receiver.cancel()
        keeper.cancel()
        hub.remove_browser(ws)
        try:
            await ws.close()
        except Exception:
            pass
