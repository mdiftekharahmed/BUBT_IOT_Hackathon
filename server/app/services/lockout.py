"""Login lockout: 5 wrong passwords in a row lock the account for 30 s.

The counter is moved by a single atomic UPDATE ... RETURNING so parallel attempts
cannot skip the lock: the first four wrong attempts increment, the fifth sets
locked_until, and every attempt after that sees the lock and is refused without
counting further.
"""
import math
from datetime import timedelta

from sqlalchemy import and_, case, select, update
from sqlalchemy.orm import Session

from app.clock import Clock
from app.models import LoginAttempt, User
from app.security import verify_password

LOCK_AFTER_N = 5
LOCK_FOR_S = 30


def _remaining(locked_until, now) -> int:
    return max(1, int(math.ceil((locked_until - now).total_seconds())))


class LoginGuard:
    def check_and_record(self, db: Session, username: str, password: str, clock: Clock) -> dict:
        """Return {"ok": bool, "locked_for_s": int}; every attempt is recorded."""
        now = clock.now()
        user = db.execute(select(User).where(User.username == username)).scalar_one_or_none()

        def record(success: bool) -> None:
            db.add(LoginAttempt(username=username, success=success, at=now))
            db.flush()

        if user is None:
            record(False)
            return {"ok": False, "locked_for_s": 0}
        if user.locked_until is not None and user.locked_until > now:
            record(False)
            return {"ok": False, "locked_for_s": _remaining(user.locked_until, now)}

        locked = User.locked_until > now
        expired = and_(User.locked_until.is_not(None), User.locked_until <= now)

        if verify_password(password, user.password_hash):
            _, locked_until = db.execute(
                update(User)
                .where(User.id == user.id)
                .values(failed_attempts=0)
                .returning(User.failed_attempts, User.locked_until)
            ).one()
            db.flush()
            if locked_until is not None and locked_until > now:
                # a parallel attempt engaged the lock between the read and now
                record(False)
                return {"ok": False, "locked_for_s": _remaining(locked_until, now)}
            record(True)
            return {"ok": True, "locked_for_s": 0}

        _, locked_until = db.execute(
            update(User)
            .where(User.id == user.id)
            .values(
                failed_attempts=case(
                    (locked, User.failed_attempts),  # already locked: do not count
                    (expired, 1),  # lock expired: start a fresh row
                    else_=User.failed_attempts + 1,
                ),
                locked_until=case(
                    (locked, User.locked_until),
                    (expired, None),
                    (User.failed_attempts + 1 >= LOCK_AFTER_N, now + timedelta(seconds=LOCK_FOR_S)),
                    else_=User.locked_until,
                ),
            )
            .returning(User.failed_attempts, User.locked_until)
        ).one()
        db.flush()
        record(False)
        if locked_until is not None and locked_until > now:
            return {"ok": False, "locked_for_s": _remaining(locked_until, now)}
        return {"ok": False, "locked_for_s": 0}
