import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # import simulate.py

import json  # noqa: E402

import httpx  # noqa: E402

import simulate  # noqa: E402


def test_seq_never_reused_after_restart(monkeypatch, tmp_path):
    monkeypatch.setattr(simulate, "SEQ_FILE", tmp_path / "seq.json")
    d1 = simulate.Device("http://test", "tok", "T17-D1", "R-305")
    issued = [d1.next_seq() for _ in range(5)]

    d2 = simulate.Device("http://test", "tok", "T17-D1", "R-305")  # "restart"
    assert d2.next_seq() > max(issued)


def test_warming_scenario_crosses_32_then_40_c():
    statuses = []
    prev = "SAFE"
    for t in [28, 30, 31, 32, 35, 38, 39, 40, 42]:
        prev = simulate.next_status(prev, t, 62)
        statuses.append(prev)
    assert "WARNING" in statuses and "DANGER" in statuses
    assert statuses.index("WARNING") < statuses.index("DANGER")
    # cooling: leaves Danger at 38 and Warning at 30, per the paper
    assert simulate.next_status("DANGER", 38.0, 62) == "WARNING"
    assert simulate.next_status("WARNING", 30.0, 62) == "SAFE"


def test_replay_scenario_resends_last_seq(monkeypatch, tmp_path):
    seen: set[tuple[str, int]] = set()
    calls: list[tuple[int, int]] = []  # (seq, status)

    def handler(request: httpx.Request) -> httpx.Response:
        body = json.loads(request.content)
        kind = "telemetry" if str(request.url.path).endswith("/telemetry") else "events"
        key = (body["device_id"], body["seq"])
        status = 409 if key in seen else 200
        seen.add(key)
        calls.append((kind, body["seq"], status))
        return httpx.Response(status, json={"results": [{"status": "stored" if status == 200 else "duplicate"}]})

    monkeypatch.setattr(simulate, "SEQ_FILE", tmp_path / "seq.json")
    dev = simulate.Device("http://test", "tok", "T17-D1", "R-305")
    dev.http = httpx.Client(base_url="http://test", transport=httpx.MockTransport(handler))

    simulate.run_scenario(dev, "replay", 1, 0)

    tel = [c for c in calls if c[0] == "telemetry"]
    assert len(tel) == 2
    assert tel[0][1] == tel[1][1]  # the same seq was sent again
    assert tel[0][2] == 200 and tel[1][2] == 409  # and rejected as a replay
