from fastapi import FastAPI

from app.api import device
from app.api.device import get_clock  # noqa: F401  (re-exported for dependency overrides)

app = FastAPI(title="FastRobox Platform")
app.include_router(device.router)


@app.get("/api/health")
def health() -> dict:
    return {"ok": True}
