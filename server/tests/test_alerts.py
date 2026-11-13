import pytest

from app.models import AlertSent, Device, Event, Room, User
from app.services.alerts import dispatch_alerts, matches
from tests.conftest import seed_judge_accounts


class FakeSender:
    def __init__(self, fail_times: int = 0):
        self.sent: list[tuple[str, str]] = []
        self.fail_times = fail_times
        self.attempts = 0

    def send(self, chat_id: str, text: str) -> None:
        self.attempts += 1
        if self.attempts <= self.fail_times:
            raise RuntimeError("telegram down")
        self.sent.append((chat_id, text))


@pytest.fixture()
def platform(db, users):
    """Teacher with a chat id, plus the room/device the events point at."""
    db.add(Room(id="R-305", name="Room 305"))
    db.flush()
    db.add(Device(id="T17-D1", room_id="R-305", token_hash="x"))
    db.flush()
    teacher = db.query(User).filter_by(username="teacher").one()
    teacher.telegram_chat_id = "424242"
    db.flush()
    return db


def _event(db, clock, seq=1, type_="FIRE", detail="", room="R-305"):
    ev = Event(device_id="T17-D1", room_id=room, seq=seq, ts=clock.now(), type=type_, detail=detail)
    db.add(ev)
    db.flush()
    return ev


def test_matches_rules():
    assert matches(Event(type="FIRE", detail="")) == "FIRE"
    assert matches(Event(type="PANIC", detail="")) == "PANIC"
    assert matches(Event(type="STATUS", detail="SAFE->DANGER")) == "HEAT_DANGER"
    assert matches(Event(type="STATUS", detail="SAFE->WARNING")) is None
    assert matches(Event(type="CARD_TAP", detail="uid=A1")) is None


def test_fire_event_sends_message_within_10s(platform, clock):
    ev = _event(platform, clock, type_="FIRE")
    clock.advance(9)  # a worker tick happens well inside the 10 s budget
    sender = FakeSender()

    sent = dispatch_alerts(platform, sender, clock)
    platform.commit()

    assert sent == ["FIRE"]
    assert sender.sent == [("424242", sender.sent[0][1])]
    assert "FIRE" in sender.sent[0][1] and "R-305" in sender.sent[0][1]
    row = platform.query(AlertSent).filter_by(event_id=ev.id).one()
    assert row.channel == "telegram"
    assert row.sent_at is not None
    assert (row.sent_at - ev.ts).total_seconds() <= 10


def test_panic_and_heat_danger_send(platform, clock):
    _event(platform, clock, seq=1, type_="PANIC")
    _event(platform, clock, seq=2, type_="STATUS", detail="SAFE->DANGER")
    sender = FakeSender()

    sent = dispatch_alerts(platform, sender, clock)
    platform.commit()

    assert sent == ["PANIC", "HEAT_DANGER"]
    assert len(sender.sent) == 2


def test_status_safe_to_warning_sends_nothing(platform, clock):
    _event(platform, clock, type_="STATUS", detail="SAFE->WARNING")
    sender = FakeSender()

    sent = dispatch_alerts(platform, sender, clock)
    platform.commit()

    assert sent == []
    assert sender.sent == []
    assert platform.query(AlertSent).count() == 0


def test_each_event_sends_once(platform, clock):
    _event(platform, clock, type_="PANIC")
    sender = FakeSender()

    dispatch_alerts(platform, sender, clock)
    platform.commit()
    dispatch_alerts(platform, sender, clock)
    platform.commit()

    assert len(sender.sent) == 1


def test_telegram_failure_is_retried_then_recorded(platform, clock):
    _event(platform, clock, type_="FIRE")
    failing = FakeSender(fail_times=3)

    sent = dispatch_alerts(platform, failing, clock)
    platform.commit()

    assert sent == []
    assert failing.attempts == 3  # tried three times, never gave up mid-row
    assert failing.sent == []
    row = platform.query(AlertSent).one()
    assert row.sent_at is None
    assert (row.error or "").startswith("attempt 3")

    # a terminal error row is not retried again (no duplicate spam later)
    good = FakeSender()
    assert dispatch_alerts(platform, good, clock) == []
    platform.commit()
    assert good.attempts == 0
