"""Judges' seed: the five accounts, one room with its teacher, one device.

Run from server/:  python -m seed

Idempotent — rerunning adds nothing and does not rotate the device token.
Passwords come from .env (JUDGE_ADMIN_PASSWORD, JUDGE_TEACHER_PASSWORD,
JUDGE_S1_PASSWORD, JUDGE_S2_PASSWORD, JUDGE_GUARDIAN_PASSWORD); the device
token comes from DEVICE_TOKEN (a random one is generated when unset) and is
printed once in plain text — it is stored only as a hash.
"""
import secrets

from sqlalchemy.orm import Session

from app.config import Settings
from app.models import Device, GuardianLink, Room, RoomTeacher, Student, User
from app.security import hash_password, hash_token

ROOM_ID, DEVICE_ID = "R-305", "T17-D1"

JUDGES = [
    # username, role, settings field, .env variable
    ("admin", "admin", "judge_admin_password", "JUDGE_ADMIN_PASSWORD"),
    ("teacher", "teacher", "judge_teacher_password", "JUDGE_TEACHER_PASSWORD"),
    ("s1", "student", "judge_s1_password", "JUDGE_S1_PASSWORD"),
    ("s2", "student", "judge_s2_password", "JUDGE_S2_PASSWORD"),
    ("guardian", "guardian", "judge_guardian_password", "JUDGE_GUARDIAN_PASSWORD"),
]
STUDENT_NAMES = {"s1": "Student 1", "s2": "Student 2"}


def seed_all(db: Session, settings: Settings) -> dict:
    """Create everything that is missing; return {"token", "room_id", "device_id"}."""
    users: dict[str, User] = {}
    for username, role, password_field, _env in JUDGES:
        user = db.query(User).filter(User.username == username).one_or_none()
        if user is None:
            user = User(
                username=username,
                role=role,
                password_hash=hash_password(getattr(settings, password_field)),
            )
            db.add(user)
            db.flush()
        users[username] = user

    teacher = users["teacher"]
    if settings.telegram_chat_id:
        teacher.telegram_chat_id = settings.telegram_chat_id

    students: dict[str, Student] = {}
    for username in ("s1", "s2"):
        student = db.query(Student).filter(Student.user_id == users[username].id).one_or_none()
        if student is None:
            student = Student(user_id=users[username].id, name=STUDENT_NAMES[username])
            db.add(student)
            db.flush()
        students[username] = student

    guardian = users["guardian"]
    link = (
        db.query(GuardianLink)
        .filter(
            GuardianLink.guardian_user_id == guardian.id,
            GuardianLink.student_id == students["s1"].id,
        )
        .one_or_none()
    )
    if link is None:
        db.add(GuardianLink(guardian_user_id=guardian.id, student_id=students["s1"].id))
        db.flush()

    if db.get(Room, ROOM_ID) is None:
        db.add(Room(id=ROOM_ID, name="Room 305"))
        db.flush()
    teach_link = (
        db.query(RoomTeacher)
        .filter(RoomTeacher.room_id == ROOM_ID, RoomTeacher.user_id == teacher.id)
        .one_or_none()
    )
    if teach_link is None:
        db.add(RoomTeacher(room_id=ROOM_ID, user_id=teacher.id))
        db.flush()

    token = settings.device_token or secrets.token_urlsafe(24)
    device = db.get(Device, DEVICE_ID)
    if device is None:
        db.add(Device(id=DEVICE_ID, room_id=ROOM_ID, token_hash=hash_token(token)))
        db.flush()
    elif settings.device_token and not device.token_hash == hash_token(token):
        device.token_hash = hash_token(token)  # deliberate rotation via .env
        db.flush()

    return {"token": token, "room_id": ROOM_ID, "device_id": DEVICE_ID}


def main() -> None:
    from app.db import SessionLocal

    settings = Settings()
    missing = [env for _u, _r, field, env in JUDGES if not getattr(settings, field)]
    if missing:
        raise SystemExit(f"set the judge passwords in .env first: {', '.join(missing)}")

    with SessionLocal() as db:
        out = seed_all(db, settings)
        db.commit()

    print("seeded users: admin, teacher, s1, s2, guardian (guardian -> s1)")
    print(f"room {out['room_id']} (teacher: teacher); device {out['device_id']}")
    print(f"DEVICE_TOKEN={out['token']}   # printed once; keep it out of the repo")
    print("pages:")
    print("  login          /login")
    print(f"  room           /rooms/{out['room_id']}")
    print(f"  device health  /rooms/{out['room_id']}/health")
    print("  admin logins   /admin/logins")


if __name__ == "__main__":
    main()
