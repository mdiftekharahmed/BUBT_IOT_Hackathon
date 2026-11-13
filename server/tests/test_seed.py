import pytest
from sqlalchemy import select

from app.config import Settings
from app.models import Device, GuardianLink, Room, RoomTeacher, Student, User
from app.security import hash_token, verify_password
from app.services.lockout import LoginGuard
from seed import seed_all


def seed_settings() -> Settings:
    return Settings(
        judge_admin_password="pw-admin",
        judge_teacher_password="pw-teacher",
        judge_s1_password="pw-s1",
        judge_s2_password="pw-s2",
        judge_guardian_password="pw-guardian",
        device_token="tok-dev",
        telegram_chat_id="424242",
    )


def test_seed_creates_five_accounts_and_guardian_link(db, clock):
    out = seed_all(db, seed_settings())

    users = {u.username: u for u in db.query(User).all()}
    assert sorted(users) == ["admin", "guardian", "s1", "s2", "teacher"]
    assert users["admin"].role == "admin"
    assert users["teacher"].role == "teacher"
    assert users["s1"].role == "student" and users["s2"].role == "student"
    assert users["guardian"].role == "guardian"
    assert verify_password("pw-s1", users["s1"].password_hash)
    assert users["teacher"].telegram_chat_id == "424242"

    students = {s.user_id: s for s in db.query(Student).all()}
    assert len(students) == 2

    link = db.query(GuardianLink).one()
    assert link.guardian_user_id == users["guardian"].id
    assert link.student_id == students[users["s1"].id].id  # guardian watches s1 only

    assert db.get(Room, "R-305") is not None
    teach_links = db.query(RoomTeacher).all()
    assert [(l.room_id, l.user_id) for l in teach_links] == [("R-305", users["teacher"].id)]

    device = db.get(Device, "T17-D1")
    assert device is not None and device.room_id == "R-305"
    assert device.token_hash == hash_token("tok-dev")

    assert out["token"] == "tok-dev"  # printed once for the simulator/.env

    # the seeded password really unlocks the account (guard end to end)
    result = LoginGuard().check_and_record(db, "admin", "pw-admin", clock)
    assert result["ok"] is True


def test_seed_is_idempotent(db):
    first = seed_all(db, seed_settings())
    before = db.get(Device, "T17-D1").token_hash

    second = seed_all(db, seed_settings())

    assert db.query(User).count() == 5
    assert db.query(Student).count() == 2
    assert db.query(GuardianLink).count() == 1
    assert db.query(Room).count() == 1
    assert db.query(RoomTeacher).count() == 1
    assert db.query(Device).count() == 1
    assert db.get(Device, "T17-D1").token_hash == before  # the token is not rotated silently
    assert first["token"] == second["token"] == "tok-dev"
