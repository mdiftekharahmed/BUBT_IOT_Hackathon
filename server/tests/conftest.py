import os
from datetime import datetime, timedelta, timezone

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session

from app.models import Base

TEST_URL = os.environ.get("TEST_DATABASE_URL", "postgresql+psycopg://fastrobox@127.0.0.1:54329/fastrobox_test")


class FakeClock:
    def __init__(self) -> None:
        self._now = datetime(2026, 11, 13, 9, 0, tzinfo=timezone.utc)

    def now(self) -> datetime:
        return self._now

    def advance(self, seconds: float) -> None:
        self._now += timedelta(seconds=seconds)


@pytest.fixture(scope="session")
def engine():
    eng = create_engine(TEST_URL)
    Base.metadata.drop_all(eng)
    Base.metadata.create_all(eng)
    yield eng
    eng.dispose()


@pytest.fixture()
def db(engine):
    with engine.connect() as conn:
        trans = conn.begin()
        session = Session(bind=conn, join_transaction_mode="create_savepoint")
        yield session
        session.close()
        trans.rollback()


@pytest.fixture()
def clock() -> FakeClock:
    return FakeClock()


# --- shared auth fixtures (Task B) ---

JUDGE_PASSWORDS = {
    "admin": "pw-admin",
    "teacher": "pw-teacher",
    "s1": "pw-s1",
    "s2": "pw-s2",
    "guardian": "pw-guardian",
}


def seed_judge_accounts(db):
    """Admin, Teacher, S1, S2 and a Guardian linked to S1. Returns (s1_id, s2_id)."""
    from sqlalchemy import select

    from app.models import GuardianLink, Student, User
    from app.security import hash_password

    for username, role in [
        ("admin", "admin"),
        ("teacher", "teacher"),
        ("s1", "student"),
        ("s2", "student"),
        ("guardian", "guardian"),
    ]:
        db.add(User(username=username, password_hash=hash_password(JUDGE_PASSWORDS[username]), role=role))
    db.flush()

    def uid(name: str) -> int:
        return db.scalar(select(User.id).where(User.username == name))

    s1 = Student(user_id=uid("s1"), name="Student 1")
    s2 = Student(user_id=uid("s2"), name="Student 2")
    db.add_all([s1, s2])
    db.flush()
    db.add(GuardianLink(guardian_user_id=uid("guardian"), student_id=s1.id))
    db.flush()
    return s1.id, s2.id


@pytest.fixture()
def users(db):
    return seed_judge_accounts(db)


def login(client, username: str, password: str):
    return client.post("/api/auth/login", json={"username": username, "password": password})


@pytest.fixture()
def api(db, clock, users):
    from fastapi.testclient import TestClient

    from app.db import get_db
    from app.main import app, get_clock

    app.dependency_overrides[get_db] = lambda: db
    app.dependency_overrides[get_clock] = lambda: clock
    yield TestClient(app)
    app.dependency_overrides.clear()
