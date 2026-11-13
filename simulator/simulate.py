"""FastRobox demo device: sends realistic telemetry and events, or self-tests the platform.

  python simulate.py --scenario normal|warming|fire|panic|offline|replay [--count N] [--interval S]
  python simulate.py --selftest

Token and ids come from flags, environment (DEVICE_TOKEN, DEVICE_ID, ROOM_ID) or ../.dev/demo.env.
`seq` is kept in a file so restarts never reuse a number, like the real firmware.
"""
import argparse
import json
import os
import sys
import threading
import time
from pathlib import Path

import httpx

ROOT = Path(__file__).resolve().parent.parent
SEQ_FILE = ROOT / ".dev" / "sim_seq.json"


def load_env() -> dict:
    env = {}
    f = ROOT / ".dev" / "demo.env"
    if f.exists():
        for line in f.read_text().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                env[k.strip()] = v.strip()
    env.update({k: v for k, v in os.environ.items() if k in ("DEVICE_TOKEN", "DEVICE_ID", "ROOM_ID")})
    return env


def heat_index_c(t_c: float, rh: float) -> float:
    t = t_c * 9 / 5 + 32
    if t < 80:
        return round(t_c, 1)
    hi = (-42.379 + 2.04901523 * t + 10.14333127 * rh - 0.22475541 * t * rh - 0.00683783 * t * t
          - 0.05481717 * rh * rh + 0.00122874 * t * t * rh + 0.00085282 * t * rh * rh
          - 0.00000199 * t * t * rh * rh)
    return round((hi - 32) * 5 / 9, 1)


def next_status(prev: str, t: float, rh: float) -> str:
    """Warning at 32C/85%, Danger at 40C; leave Danger at 38C, Warning at 30C and 80%."""
    if prev == "DANGER":
        return "DANGER" if t > 38 else ("WARNING" if t > 30 else "SAFE")
    if t >= 40:
        return "DANGER"
    if prev == "WARNING":
        return "WARNING" if (t > 30 or rh > 80) else "SAFE"
    return "WARNING" if (t >= 32 or rh >= 85) else "SAFE"


class Device:
    def __init__(self, base: str, token: str, device_id: str, room_id: str):
        self.http = httpx.Client(base_url=base, timeout=10)
        self.token, self.device_id, self.room_id = token, device_id, room_id
        self.seq = json.loads(SEQ_FILE.read_text()).get("seq", 0) if SEQ_FILE.exists() else 0
        self.start = time.time()
        self.status, self.door, self.flame = "SAFE", "LOCKED", False

    def next_seq(self) -> int:
        self.seq += 1
        SEQ_FILE.parent.mkdir(exist_ok=True)
        SEQ_FILE.write_text(json.dumps({"seq": self.seq}))
        return self.seq

    def post(self, kind: str, body, token: str | None = None) -> httpx.Response:
        return self.http.post(f"/api/device/{kind}", json=body,
                              headers={"Authorization": f"Bearer {token or self.token}"})

    def telemetry(self, temp: float, rh: float = 60, seq: int | None = None, **over) -> dict:
        rec = {
            "device_id": self.device_id, "room_id": self.room_id, "seq": seq or self.next_seq(),
            "ts": int(time.time()), "uptime_s": int(time.time() - self.start),
            "temp_c": round(temp, 1), "rh": rh, "heat_index_c": heat_index_c(temp, rh),
            "flame": self.flame, "door": self.door, "status": self.status,
            "heat_trend": "STABLE", "rssi": -60, "heap_kb": 180, "chip_temp_c": 41.5,
            "fw": "1.0.0", "quality": "good",
        }
        rec.update(over)
        return rec

    def event(self, type_: str, detail: str = "") -> dict:
        return {"device_id": self.device_id, "room_id": self.room_id, "seq": self.next_seq(),
                "ts": int(time.time()), "type": type_, "detail": detail}

    def send(self, kind: str, rec: dict) -> httpx.Response:
        r = self.post(kind, rec)
        label = rec.get("type") or f"T={rec.get('temp_c')}C {rec.get('status')}"
        print(f"  {kind:<9} seq={rec['seq']:<5} {label:<28} -> {r.status_code}")
        return r

    def tick(self, temp: float, rh: float = 60) -> None:
        new = next_status(self.status, temp, rh) if self.status != "FIRE" else "FIRE"
        if new != self.status:
            self.send("events", self.event("STATUS", f"{self.status}->{new}"))
            self.status = new
        self.send("telemetry", self.telemetry(temp, rh))


def command_listener(base: str, token: str, stop: "threading.Event") -> None:
    """Connect to WS /api/device/ws, print every server command and ack it.

    This is how `Reset fire` and later settings/OTA commands are rehearsed
    end to end without the real board.
    """
    from websockets.sync.client import connect

    url = base.replace("https://", "wss://").replace("http://", "ws://").rstrip("/") + "/api/device/ws"
    while not stop.is_set():
        try:
            with connect(url, additional_headers={"Authorization": f"Bearer {token}"}, open_timeout=5) as ws:
                print("  [ws] connected to /api/device/ws")
                while not stop.is_set():
                    try:
                        raw = ws.recv(timeout=1)
                    except TimeoutError:
                        continue
                    msg = json.loads(raw)
                    print(f"  [ws] cmd: {json.dumps(msg)}")
                    if "id" in msg:
                        ws.send(json.dumps({"ack": msg["id"]}))
        except Exception as exc:  # server not up yet, or dropped: retry
            print(f"  [ws] {exc}; retrying in 3 s")
            stop.wait(3)


def run_scenario(dev: Device, name: str, count: int, interval: float) -> None:
    dev.send("events", dev.event("BOOT", "reason=POWER_ON"))
    if name == "normal":
        for i in range(count):
            dev.tick(28.5 + (i % 5) * 0.2)
            time.sleep(interval)
    elif name == "warming":
        n = max(count, 20)
        half = n / 2
        profile = [28 + 14 * (i / half) if i < half else 42 - 14 * ((i - half) / half) for i in range(n)]
        for t in profile:
            dev.tick(t, 62)
            time.sleep(interval)
    elif name == "fire":
        for _ in range(3):
            dev.tick(29)
            time.sleep(interval)
        dev.flame, dev.status, dev.door = True, "FIRE", "UNLOCKED"
        dev.send("events", dev.event("FIRE", "flame held 2s"))
        for _ in range(max(count, 6)):
            dev.tick(31)
            time.sleep(interval)
        dev.flame = False
        dev.send("events", dev.event("FIRE_CLEARED"))
        dev.tick(30)
    elif name == "panic":
        dev.tick(29)
        dev.send("events", dev.event("PANIC", "touch 2s"))
        dev.tick(29)
    elif name == "offline":
        dev.tick(29)
        print("  going silent for 35 s (platform should mark the device offline after 30 s)")
        time.sleep(35)
        dev.tick(29)
    elif name == "replay":
        rec = dev.telemetry(29)
        dev.send("telemetry", rec)
        dev.send("telemetry", dict(rec, temp_c=99.0))
    else:
        sys.exit(f"unknown scenario {name}")


def selftest(dev: Device) -> int:
    results: list[tuple[str, bool, str]] = []

    def check(name: str, ok: bool, detail: str = "") -> None:
        results.append((name, ok, detail))

    dev.status, dev.door, dev.flame = "SAFE", "LOCKED", False
    r = dev.post("telemetry", dev.telemetry(29.0))
    check("valid telemetry stored (200)", r.status_code == 200, str(r.status_code))

    rec = dev.telemetry(29.0)
    dev.post("telemetry", rec)
    r = dev.post("telemetry", dict(rec, temp_c=99.0))
    check("replay of same seq rejected (409)", r.status_code == 409, str(r.status_code))

    batch = [dev.telemetry(29.0) for _ in range(10)]
    dev.post("telemetry", batch[3])  # already stored, so it is a duplicate inside the batch
    r = dev.post("telemetry", batch)
    results_json = r.json().get("results", []) if r.status_code == 200 else []
    stored = [x["status"] for x in results_json].count("stored")
    check("batch of 10 with 1 duplicate stores 9", r.status_code == 200 and stored == 9, f"stored={stored}")

    r = dev.post("telemetry", dev.telemetry(29.0), token="wrong-token")
    check("wrong token refused (401)", r.status_code == 401, str(r.status_code))

    bad = dev.telemetry(29.0)
    del bad["status"]
    r = dev.post("telemetry", bad)
    check("missing field refused (422) naming it", r.status_code == 422 and "status" in r.text, str(r.status_code))

    # The judge types the Room ID on the device: a new one is accepted and the device moves.
    r = dev.post("telemetry", dev.telemetry(29.0, room_id="R-999"))
    check("new Room ID accepted (200)", r.status_code == 200, str(r.status_code))
    r = dev.post("telemetry", dev.telemetry(29.0, room_id="R 9/9"))
    check("malformed Room ID refused (422)", r.status_code == 422 and "invalid room_id" in r.text, str(r.status_code))
    r = dev.post("telemetry", dev.telemetry(29.0))
    check("back to the original room (200)", r.status_code == 200, str(r.status_code))

    ev = dev.event("BOOT", "reason=WATCHDOG")
    check("BOOT event stored (200)", dev.post("events", ev).status_code == 200)
    check("replayed event refused (409)", dev.post("events", ev).status_code == 409)

    width = max(len(n) for n, _, _ in results)
    for name, ok, detail in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name:<{width}}  {detail}")
    failed = [n for n, ok, _ in results if not ok]
    print(f"\n{len(results) - len(failed)}/{len(results)} checks passed")
    return 1 if failed else 0


def main() -> None:
    env = load_env()
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://127.0.0.1:8000")
    ap.add_argument("--token", default=env.get("DEVICE_TOKEN"))
    ap.add_argument("--device", default=env.get("DEVICE_ID", "T17-D1"))
    ap.add_argument("--room", default=env.get("ROOM_ID", "R-305"))
    ap.add_argument("--scenario", default="normal")
    ap.add_argument("--count", type=int, default=12)
    ap.add_argument("--interval", type=float, default=5.0)
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if not a.token:
        sys.exit("no device token: run `python dev_seed.py` in server/ or pass --token")
    dev = Device(a.base, a.token, a.device, a.room)
    stop = threading.Event()
    threading.Thread(target=command_listener, args=(a.base, a.token, stop), daemon=True).start()
    if a.selftest:
        code = selftest(dev)
        stop.set()
        sys.exit(code)
    run_scenario(dev, a.scenario, a.count, a.interval)
    stop.set()


if __name__ == "__main__":
    main()
