from fastapi import FastAPI

app = FastAPI(title="FastRobox Platform")


@app.get("/api/health")
def health() -> dict:
    return {"ok": True}
