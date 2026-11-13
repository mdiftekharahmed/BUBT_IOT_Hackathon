"""Device-token hashing, argon2 passwords and server-side sessions with role checks."""
import hashlib
import hmac
import secrets
from datetime import timedelta

import argon2
from fastapi import Cookie, Depends, HTTPException
from sqlalchemy.orm import Session

from app.clock import Clock
from app.clock import get_clock  # noqa: F401  (FastAPI dependency, overridden in tests)
from app.db import get_db
from app.models import SessionRow, User

COOKIE_NAME = "fastrobox_session"
SESSION_TTL = timedelta(days=7)

_hasher = argon2.PasswordHasher()


# --- device tokens ---

def hash_token(token: str) -> str:
    return hashlib.sha256(token.encode()).hexdigest()


def token_matches(token: str, token_hash: str) -> bool:
    return hmac.compare_digest(hash_token(token), token_hash)


# --- passwords ---

def hash_password(password: str) -> str:
    return _hasher.hash(password)


def verify_password(password: str, password_hash: str) -> bool:
    try:
        return bool(_hasher.verify(password_hash, password))
    except argon2.exceptions.VerificationError:
        return False


# --- sessions ---

def create_session(db: Session, user: User, clock: Clock) -> str:
    sid = secrets.token_urlsafe(32)
    db.add(SessionRow(id=sid, user_id=user.id, expires_at=clock.now() + SESSION_TTL))
    db.flush()
    return sid


def load_session_user(db: Session, sid: str | None, clock: Clock) -> User | None:
    if not sid:
        return None
    row = db.get(SessionRow, sid)
    if row is None or row.revoked_at is not None or row.expires_at <= clock.now():
        return None
    return db.get(User, row.user_id)


def revoke_session(db: Session, sid: str | None, clock: Clock) -> None:
    if not sid:
        return
    row = db.get(SessionRow, sid)
    if row is not None and row.revoked_at is None:
        row.revoked_at = clock.now()


def current_user(
    fastrobox_session: str | None = Cookie(default=None, alias=COOKIE_NAME),
    db: Session = Depends(get_db),
    clock: Clock = Depends(get_clock),
) -> User:
    user = load_session_user(db, fastrobox_session, clock)
    if user is None:
        raise HTTPException(401, "not logged in")
    return user


def require_role(*roles: str):
    """FastAPI dependency: 401 when not logged in, 403 when the role does not match."""

    def dep(user: User = Depends(current_user)) -> User:
        if user.role not in roles:
            raise HTTPException(403, "not allowed")
        return user

    return dep
