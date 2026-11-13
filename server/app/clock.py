from datetime import datetime, timezone
from typing import Protocol


class Clock(Protocol):
    def now(self) -> datetime: ...


class SystemClock:
    def now(self) -> datetime:
        return datetime.now(timezone.utc)


def get_clock() -> Clock:
    """FastAPI dependency. Overridden with FakeClock in tests."""
    return SystemClock()
