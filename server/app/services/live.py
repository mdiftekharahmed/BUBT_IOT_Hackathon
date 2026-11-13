import json

from sqlalchemy import text
from sqlalchemy.orm import Session

CHANNEL = "live"


def notify(conn, room_id: str, type_: str, data: dict) -> None:
    """Send a live update to every api process listening on the `live` channel.

    `conn` is a SQLAlchemy Session (delivered when it commits) or a psycopg connection.
    """
    payload = json.dumps({"room_id": room_id, "type": type_, "data": data}, default=str)
    if isinstance(conn, Session):
        conn.execute(text("SELECT pg_notify(:channel, :payload)"), {"channel": CHANNEL, "payload": payload})
    else:
        conn.execute("SELECT pg_notify(%s, %s)", (CHANNEL, payload))
