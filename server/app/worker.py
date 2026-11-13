"""Background jobs: offline sweep now, phone alerts in Task 9.  Run: python -m app.worker"""
import logging
import time
from datetime import timedelta

from sqlalchemy import update
from sqlalchemy.orm import Session

from app.clock import Clock, SystemClock
from app.models import Device
from app.services.live import notify

log = logging.getLogger("worker")
OFFLINE_AFTER_S = 30
LOOP_EVERY_S = 5


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
    log.info("worker started")
    while True:
        try:
            with SessionLocal() as db:
                gone = sweep_offline(db, clock)
                db.commit()
            if gone:
                log.info("offline: %s", ", ".join(gone))
        except Exception:  # keep the loop alive; the next pass retries
            log.exception("sweep failed")
        time.sleep(LOOP_EVERY_S)


if __name__ == "__main__":
    main()
