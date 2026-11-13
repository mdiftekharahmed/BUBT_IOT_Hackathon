"""Local development seed: one room, one device, a random token written to .dev/demo.env (git-ignored).

Run from server/:  python dev_seed.py
"""
import secrets
from pathlib import Path

from app.db import SessionLocal
from app.models import Device, Room
from app.security import hash_token

ROOM_ID, DEVICE_ID = "R-305", "T17-D1"
OUT = Path(__file__).resolve().parent.parent / ".dev" / "demo.env"


def main() -> None:
    token = secrets.token_urlsafe(24)
    with SessionLocal() as db:
        if not db.get(Room, ROOM_ID):
            db.add(Room(id=ROOM_ID, name="Room 305"))
            db.flush()
        dev = db.get(Device, DEVICE_ID)
        if dev:
            dev.token_hash = hash_token(token)
        else:
            db.add(Device(id=DEVICE_ID, room_id=ROOM_ID, token_hash=hash_token(token)))
        db.commit()
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(f"DEVICE_TOKEN={token}\nDEVICE_ID={DEVICE_ID}\nROOM_ID={ROOM_ID}\n")
    print(f"seeded {DEVICE_ID} in {ROOM_ID}; token written to {OUT}")


if __name__ == "__main__":
    main()
