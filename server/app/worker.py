"""Background jobs: offline sweep and Telegram alerts.  Run: python -m app.worker"""
import logging
import time
from datetime import timedelta

from sqlalchemy import update
from sqlalchemy.orm import Session

from app.clock import Clock, SystemClock
from app.config import settings
from app.models import Device
from app.services.alerts import TelegramSender, dispatch_alerts
from app.services.live import notify

log = logging.getLogger("worker")
OFFLINE_AFTER_S = 30
TICK_S = 1  # alert budget is 10 s; the worker must look for new events every second
SWEEP_EVERY_TICKS = 5  # offline sweep keeps the old 5 s cadence


def sweep_offline(db: Session, clock: Clock, limit_s: int = OFFLINE_AFTER_S, notifier=notify) -> list[str]:
    """Mark devices silent for more than `limit_s` seconds offline; return their ids."""
    cutoff = clock.now() - timedelta(seconds=limit_s)
    rows = db.execute(
        update(Device)
        .where(Device.online.is_(True), Device.last_seen_at < cutoff)
        .values(online=False)
        .returning(Device.id, Device.room_id, Device.last_seen_at)
    ).all()
    for device_id, room_id, last_seen in rows:
        notifier(db, room_id, "device_offline", {"device_id": device_id, "last_seen_at": last_seen.isoformat()})
    return [r.id for r in rows]


def main() -> None:
    from app.db import SessionLocal

    logging.basicConfig(level=logging.INFO)
    clock = SystemClock()
    sender = TelegramSender(settings.telegram_bot_token) if settings.telegram_bot_token else None
    if sender is None:
        log.warning("TELEGRAM_BOT_TOKEN not set; alerts are disabled")
    log.info("worker started")
    tick = 0
    while True:
        try:
            with SessionLocal() as db:
                if tick % SWEEP_EVERY_TICKS == 0:
                    gone = sweep_offline(db, clock)
                    if gone:
                        log.info("offline: %s", ", ".join(gone))
                if sender is not None:
                    sent = dispatch_alerts(db, sender, clock)
                    if sent:
                        log.info("alerts sent: %s", ", ".join(sent))
                db.commit()
        except Exception:  # keep the loop alive; the next pass retries
            log.exception("worker tick failed")
        tick += 1
        time.sleep(TICK_S)


if __name__ == "__main__":
    main()
