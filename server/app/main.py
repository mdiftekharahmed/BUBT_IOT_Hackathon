from fastapi import FastAPI

from app.api import auth, device, students
from app.api.device import get_clock  # noqa: F401  (re-exported for dependency overrides)

app = FastAPI(title="FastRobox Platform")
app.include_router(device.router)
app.include_router(auth.router)
app.include_router(students.router)


@app.get("/api/health")
def health() -> dict:
    return {"ok": True}
