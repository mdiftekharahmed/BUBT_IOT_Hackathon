from concurrent.futures import ThreadPoolExecutor

import pytest
from sqlalchemy import select, text
from sqlalchemy.orm import Session

from app.models import User
from app.security import hash_password
from app.services.lockout import LoginGuard
from tests.conftest import JUDGE_PASSWORDS, login


def test_password_is_argon2_hash(db, api, users):
    h = db.scalar(select(User.password_hash).where(User.username == "s1"))
    assert h.startswith("$argon2")


def test_five_wrong_passwords_lock_30s(api, clock):
    statuses = [login(api, "teacher", "wrong").status_code for _ in range(5)]
    assert statuses[:4] == [401, 401, 401, 401]
    assert statuses[4] in (401, 423)  # the 5th wrong password engages the lock
    # the correct password is also refused while the lock is on
    r = login(api, "teacher", JUDGE_PASSWORDS["teacher"])
    assert r.status_code == 423
    assert r.json()["detail"]["locked_for_s"] == 30
    clock.advance(31)
    r = login(api, "teacher", JUDGE_PASSWORDS["teacher"])
    assert r.status_code == 200
    assert api.get("/api/auth/me").json() == {"username": "teacher", "role": "teacher"}


def test_parallel_wrong_passwords_cannot_exceed_five(engine, clock):
    with engine.begin() as conn:
        for username, role in [("teacher", "teacher"), ("admin", "admin")]:
            conn.execute(
                text("INSERT INTO users (username, password_hash, role) VALUES (:u, :h, :r)"),
                {"u": username, "h": hash_password("correct-horse"), "r": role},
            )
    try:

        def attempt(_):
            with Session(engine) as s, s.begin():
                return LoginGuard().check_and_record(s, "teacher", "wrong", clock)

        with ThreadPoolExecutor(max_workers=10) as pool:
            results = list(pool.map(attempt, range(10)))

        locked = [r for r in results if r["locked_for_s"] > 0]
        assert len(locked) >= 1  # the lock engaged during the burst
        assert all(r["ok"] is False for r in results)

        with engine.connect() as conn:
            row = conn.execute(
                text("SELECT failed_attempts, locked_until FROM users WHERE username = 'teacher'")
            ).one()
        assert row[0] == 5  # exactly five attempts were counted, not ten
        assert row[1] > clock.now()

        # a 6th attempt, even with the correct password, is refused while locked
        with Session(engine) as s, s.begin():
            r = LoginGuard().check_and_record(s, "teacher", "correct-horse", clock)
        assert r["ok"] is False
        assert r["locked_for_s"] > 0
    finally:
        with engine.begin() as conn:
            conn.execute(text("TRUNCATE login_attempts, users CASCADE"))


def test_history_lists_attempts_with_times(db, api, clock, users):
    login(api, "teacher", "wrong")
    clock.advance(2)
    login(api, "teacher", JUDGE_PASSWORDS["teacher"])

    login(api, "admin", JUDGE_PASSWORDS["admin"])  # history is for the Admin
    r = api.get("/api/auth/history")
    assert r.status_code == 200
    rows = r.json()
    listed = [(row["username"], row["success"]) for row in rows]
    assert listed == [("admin", True), ("teacher", True), ("teacher", False)]
    assert rows[1]["at"] > rows[2]["at"]

    login(api, "s2", JUDGE_PASSWORDS["s2"])
    assert api.get("/api/auth/history").status_code == 403


def test_unauthenticated_401(api, users):
    assert api.get("/api/auth/me").status_code == 401
    assert api.get("/api/auth/history").status_code == 401
