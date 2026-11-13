from fastapi import APIRouter, Cookie, Depends, HTTPException, Response
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.clock import Clock, get_clock
from app.config import settings
from app.db import get_db
from app.models import LoginAttempt, User
from app.security import (
    COOKIE_NAME,
    create_session,
    current_user,
    require_role,
    revoke_session,
)
from app.services.lockout import LoginGuard

router = APIRouter(prefix="/api/auth", tags=["auth"])
_guard = LoginGuard()


class LoginIn(BaseModel):
    username: str
    password: str


@router.post("/login")
def login(
    body: LoginIn,
    response: Response,
    db: Session = Depends(get_db),
    clock: Clock = Depends(get_clock),
):
    result = _guard.check_and_record(db, body.username, body.password, clock)
    if result["ok"]:
        user = db.execute(select(User).where(User.username == body.username)).scalar_one()
        sid = create_session(db, user, clock)
        db.commit()
        response.set_cookie(
            COOKIE_NAME,
            sid,
            httponly=True,
            samesite="lax",
            secure=settings.cookie_secure,
            max_age=7 * 24 * 3600,
        )
        return {"username": user.username, "role": user.role}
    db.commit()
    if result["locked_for_s"] > 0:
        raise HTTPException(423, {"locked_for_s": result["locked_for_s"]})
    raise HTTPException(401, "invalid credentials")


@router.post("/logout")
def logout(
    fastrobox_session: str | None = Cookie(default=None, alias=COOKIE_NAME),
    db: Session = Depends(get_db),
    clock: Clock = Depends(get_clock),
):
    revoke_session(db, fastrobox_session, clock)
    db.commit()
    response = Response()
    response.delete_cookie(COOKIE_NAME)
    return {"ok": True}


@router.get("/me")
def me(user: User = Depends(current_user)):
    return {"username": user.username, "role": user.role}


@router.get("/history")
def history(user: User = Depends(require_role("admin")), db: Session = Depends(get_db)):
    rows = (
        db.execute(select(LoginAttempt).order_by(LoginAttempt.at.desc(), LoginAttempt.id.desc()).limit(200))
        .scalars()
        .all()
    )
    return [{"at": r.at.isoformat(), "username": r.username, "success": r.success} for r in rows]
