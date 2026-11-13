from contextlib import asynccontextmanager

from fastapi import FastAPI

from app.api import auth, device, live, rooms, students
from app.api.device import get_clock  # noqa: F401  (re-exported for dependency overrides)
from app.services.live import start_listener


@asynccontextmanager
async def lifespan(_: FastAPI):
    start_listener()  # daemon thread: NOTIFY "live" -> hub.publish
    yield


app = FastAPI(title="FastRobox Platform", lifespan=lifespan)

app.include_router(device.router)
app.include_router(auth.router)
app.include_router(students.router)
app.include_router(live.router)
app.include_router(rooms.router)


@app.get("/api/health")
def health() -> dict:
    return {"ok": True}
