"""Phone alerts: FIRE, PANIC and Heat Danger go to the teachers' Telegram.

The `alerts_sent` row is inserted BEFORE the send (unique per event) so a worker
restart never double-sends; three failed tries mark the row with an error and
stop further attempts.
"""
import logging

import httpx
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.config import settings
from app.models import AlertSent, Event, User

RETRIES = 3
log = logging.getLogger("alerts")


def matches(event: Event) -> str | None:
    """A STATUS event whose new value is DANGER counts as Heat Danger."""
    if event.type == "FIRE":
        return "FIRE"
    if event.type == "PANIC":
        return "PANIC"
    if event.type == "STATUS" and "DANGER" in (event.detail or ""):
        return "HEAT_DANGER"
    return None


class TelegramSender:
    def __init__(self, token: str) -> None:
        self.token = token

    def send(self, chat_id: str, text: str) -> None:
        response = httpx.post(
            f"https://api.telegram.org/bot{self.token}/sendMessage",
            json={"chat_id": chat_id, "text": text},
            timeout=10,
        )
        response.raise_for_status()


def teacher_chat_ids(db: Session) -> list[str]:
    rows = (
        db.execute(select(User).where(User.role == "teacher", User.telegram_chat_id.is_not(None)))
        .scalars()
        .all()
    )
    return [u.telegram_chat_id for u in rows if u.telegram_chat_id]


def _text(kind: str, ev: Event) -> str:
    return f"{kind} in room {ev.room_id} at {ev.ts:%Y-%m-%d %H:%M:%S} UTC"


def _send_with_retries(row: AlertSent, kind: str, ev: Event, sender, chat_ids: list[str], clock) -> bool:
    """Try to deliver up to RETRIES times in one pass; the last error stays on the row."""
    for attempt in range(1, RETRIES + 1):
        try:
            for chat_id in chat_ids:
                sender.send(chat_id, _text(kind, ev))
            row.sent_at = clock.now()
            row.error = None
            return True
        except Exception as exc:
            log.warning("telegram send failed for event %s (try %d): %s", ev.id, attempt, exc)
            row.error = f"attempt {attempt}: {exc}"
    return False


def dispatch_alerts(db: Session, sender, clock, chat_ids: list[str] | None = None) -> list[str]:
    """Send one message per alert-worthy event, exactly once. Returns kinds sent."""
    chat_ids = chat_ids if chat_ids is not None else teacher_chat_ids(db)
    sent: list[str] = []
    events = db.execute(select(Event).order_by(Event.id.asc())).scalars().all()
    for ev in events:
        kind = matches(ev)
        if kind is None:
            continue
        row = db.query(AlertSent).filter(AlertSent.event_id == ev.id).one_or_none()
        if row is None:
            row = AlertSent(event_id=ev.id, channel="telegram", error="pending")
            db.add(row)
            db.flush()
        if row.sent_at is not None or (row.error is not None and row.error != "pending"):
            continue  # already delivered, or terminally failed
        if _send_with_retries(row, kind, ev, sender, chat_ids, clock):
            sent.append(kind)
    db.flush()
    return sent
