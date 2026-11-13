from datetime import datetime

from sqlalchemy import (
    Boolean, DateTime, Float, ForeignKey, Integer, String, UniqueConstraint, func,
)
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


class Base(DeclarativeBase):
    pass


def _now():
    return func.now()


class User(Base):
    __tablename__ = "users"
    id: Mapped[int] = mapped_column(primary_key=True)
    username: Mapped[str] = mapped_column(String(64), unique=True)
    password_hash: Mapped[str] = mapped_column(String(255))
    role: Mapped[str] = mapped_column(String(16))  # admin | teacher | student | guardian
    locked_until: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    failed_attempts: Mapped[int] = mapped_column(Integer, default=0)
    telegram_chat_id: Mapped[str | None] = mapped_column(String(32))


class LoginAttempt(Base):
    __tablename__ = "login_attempts"
    id: Mapped[int] = mapped_column(primary_key=True)
    username: Mapped[str] = mapped_column(String(64), index=True)
    success: Mapped[bool] = mapped_column(Boolean)
    at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=_now())


class Room(Base):
    __tablename__ = "rooms"
    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    name: Mapped[str] = mapped_column(String(128))


class RoomTeacher(Base):
    __tablename__ = "room_teachers"
    room_id: Mapped[str] = mapped_column(ForeignKey("rooms.id"), primary_key=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"), primary_key=True)


class Student(Base):
    __tablename__ = "students"
    id: Mapped[int] = mapped_column(primary_key=True)
    user_id: Mapped[int | None] = mapped_column(ForeignKey("users.id"))
    name: Mapped[str] = mapped_column(String(128))


class GuardianLink(Base):
    __tablename__ = "guardian_links"
    guardian_user_id: Mapped[int] = mapped_column(ForeignKey("users.id"), primary_key=True)
    student_id: Mapped[int] = mapped_column(ForeignKey("students.id"), primary_key=True)


class Device(Base):
    __tablename__ = "devices"
    id: Mapped[str] = mapped_column(String(32), primary_key=True)
    room_id: Mapped[str] = mapped_column(ForeignKey("rooms.id"))
    token_hash: Mapped[str] = mapped_column(String(128))
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    online: Mapped[bool] = mapped_column(Boolean, default=False)
    fw: Mapped[str | None] = mapped_column(String(32))
    reset_reason: Mapped[str | None] = mapped_column(String(32))


class Telemetry(Base):
    __tablename__ = "telemetry"
    __table_args__ = (UniqueConstraint("device_id", "seq", name="uq_telemetry_device_seq"),)
    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    room_id: Mapped[str] = mapped_column(String(32), index=True)
    seq: Mapped[int] = mapped_column(Integer)
    ts: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    uptime_s: Mapped[int] = mapped_column(Integer)
    temp_c: Mapped[float | None] = mapped_column(Float)
    rh: Mapped[float | None] = mapped_column(Float)
    heat_index_c: Mapped[float | None] = mapped_column(Float)
    flame: Mapped[bool] = mapped_column(Boolean)
    door: Mapped[str] = mapped_column(String(16))
    status: Mapped[str] = mapped_column(String(16))
    heat_trend: Mapped[str | None] = mapped_column(String(16))
    rssi: Mapped[int | None] = mapped_column(Integer)
    heap_kb: Mapped[int | None] = mapped_column(Integer)
    chip_temp_c: Mapped[float | None] = mapped_column(Float)
    fw: Mapped[str] = mapped_column(String(32))
    quality: Mapped[str] = mapped_column(String(8))
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=_now())
    ts_suspect: Mapped[bool] = mapped_column(Boolean, default=False)


class Event(Base):
    __tablename__ = "events"
    __table_args__ = (UniqueConstraint("device_id", "seq", name="uq_events_device_seq"),)
    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    room_id: Mapped[str] = mapped_column(String(32), index=True)
    seq: Mapped[int] = mapped_column(Integer)
    ts: Mapped[datetime] = mapped_column(DateTime(timezone=True), index=True)
    type: Mapped[str] = mapped_column(String(32), index=True)
    detail: Mapped[str] = mapped_column(String(512), default="")
    received_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=_now())
    ts_suspect: Mapped[bool] = mapped_column(Boolean, default=False)


class AlertSent(Base):
    __tablename__ = "alerts_sent"
    id: Mapped[int] = mapped_column(primary_key=True)
    event_id: Mapped[int] = mapped_column(ForeignKey("events.id"), unique=True)
    channel: Mapped[str] = mapped_column(String(16))
    sent_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    error: Mapped[str | None] = mapped_column(String(256))


class Attendance(Base):
    __tablename__ = "attendance"
    id: Mapped[int] = mapped_column(primary_key=True)
    student_id: Mapped[int] = mapped_column(ForeignKey("students.id"), index=True)
    class_id: Mapped[int | None] = mapped_column(Integer)
    status: Mapped[str] = mapped_column(String(16))
    at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
