# FastRobox Web Infrastructure (Stage 0 to Level 4) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the multi-container web platform that receives device telemetry and events over HTTPS, stores each message once, shows it live in a role-protected dashboard, and sends Teacher phone alerts. It covers the server side of Levels 2, 3 and 4.

**Architecture:** Five images on one Docker network behind one public HTTPS address. `web` (nginx) is the only public container and proxies `/api` to `api`. `api` (FastAPI) handles device ingest, auth and the live WebSocket hub. `worker` (same code base, different command) runs timed jobs and alerts. `postgres` holds all state and carries live updates between `worker` and `api` with LISTEN/NOTIFY. `simulator` is an optional profile that stands in for the board until the hardware is wired.

**Tech Stack:** Python 3.12, FastAPI, SQLAlchemy 2, Alembic, argon2-cffi, httpx (Telegram), PostgreSQL 16, React + Vite + TypeScript, Tailwind, Recharts, i18next, nginx, Docker Compose, Tailscale Funnel, pytest, Vitest.

**Spec:** `FastRobox_1.0_QuestionPaper_FOR_TEAMS.md`, `fastrobox_1_0_judges_checklist.md` and the master plan `2026-11-13-fastrobox-12-levels.md` (Stage 0.4, Level 2 tasks 2.5, Level 3 server parts, Level 4). The master plan's interfaces are not changed here.

## Global Constraints

- Telemetry fields exactly: `device_id, room_id, seq, ts, uptime_s, temp_c, rh, heat_index_c, flame, door, status, heat_trend, rssi, heap_kb, chip_temp_c, fw, quality` (snake_case). `(device_id, seq)` is unique; a replay gets `409` and the row count does not change.
- Event fields exactly: `device_id, room_id, seq, ts, type, detail`. Device events known at this stage: `BOOT STATUS FIRE FIRE_CLEARED SILENCED PANIC CARD_TAP SENSOR_ERROR`.
- Device is `offline` after 30 s without a message and `online` again on the next one.
- Roles: `admin teacher student guardian`, checked by the server on every route. Passwords stored with argon2.
- Lockout: after 5 wrong passwords in a row the account is locked for 30 s, even for the correct password. Every attempt is stored (time, username, success).
- Dashboard updates within 3 s of new data, without reloading. FIRE and PANIC banner on every logged-in dashboard within 3 s. Phone message within 10 s of FIRE, PANIC or Heat Danger.
- Every dashboard page switches between English and Bangla.
- No secret in the repository: `.env` is git-ignored; `deploy/.env.example` holds names only.
- Public HTTPS address; the device token is checked on every device request.
- The `status` field is one of `SAFE WARNING DANGER FIRE`; `door` is `LOCKED` or `UNLOCKED`; `heat_trend` is `RISING STABLE FALLING`; `quality` is `good` or `bad`.

## Review Focus

1. **Replay with a different body.** Same `(device_id, seq)`, different values. Expected: `409`, stored row unchanged (the first message wins). Test in Task 3.
2. **Batch with one duplicate.** A batch of 10 where item 4 is a replay. Expected: the other 9 are stored, the reply lists per-item results, and the device can delete the 9 and the duplicate (both count as delivered). Test in Task 3.
3. **Device clock wrong.** `ts` far in the future or before 2024. Expected: the record is stored, `quality` is kept, and `ts_suspect=true` is set so charts can skip it; the live dashboard uses server receive time for "last seen". Test in Task 3.
4. **Two logins at once during lockout.** Parallel wrong passwords must not slip past 5 attempts. Expected: the counter update is atomic and the 6th attempt is refused. Test in Task 6.
5. **WebSocket after logout or role change.** A closed session must stop receiving live data; a Student must never receive another room's messages. Expected: the hub checks the session on connect and on a 60-second recheck, and filters by the user's allowed rooms. Test in Task 5.

---

## File Structure

```
deploy/
  docker-compose.yml        services: web, api, worker, postgres; profile "sim": simulator
  .env.example              POSTGRES_PASSWORD, SESSION_SECRET, TELEGRAM_BOT_TOKEN, ...
  nginx.conf                static files, /api proxy, WebSocket upgrade for /api/live and /api/device/ws
server/
  Dockerfile                one image, two commands: api (uvicorn) and worker (python -m app.worker)
  pyproject.toml
  alembic/ , alembic.ini
  app/
    main.py                 FastAPI app, router wiring, /api/health
    config.py               Settings from environment
    db.py                   engine, session dependency
    models.py               all tables of this plan
    clock.py                Clock protocol + SystemClock (injected for tests)
    security.py             hash_password, verify_password, new_session, require_role
    api/
      device.py             POST /api/device/telemetry, /events; WS /api/device/ws
      auth.py               login, logout, me, login history
      rooms.py              rooms list, room state, recent events, health, reset fire
      students.py           GET /api/students/{id}/attendance (empty until Level 7, role-checked now)
    services/
      ingest.py             validate_and_store(record) -> IngestResult
      live.py               LiveHub: connect, publish, listen to NOTIFY
      lockout.py            LoginGuard
      alerts.py             AlertRules, TelegramSender
    worker.py               offline sweep, alert dispatch loop
  seed.py                   judges' accounts, rooms, first device + token
  tests/                    conftest.py (Postgres test db, FakeClock) + one file per task
simulator/
  Dockerfile
  simulate.py               publishes realistic telemetry and scripted events to the API
web/
  Dockerfile                multi-stage: node build, then nginx serving dist
  src/
    api.ts, live.ts         fetch wrapper, WebSocket client with reconnect
    i18n/en.json, bn.json
    pages/Login.tsx Room.tsx Health.tsx LoginHistory.tsx
    components/Banner.tsx StatusCard.tsx RecentEvents.tsx LangSwitch.tsx
  src/__tests__/            Vitest
```

Shared interfaces used by every task:

- `IngestResult = {"status": "stored" | "duplicate" | "rejected", "reason": str | None}`
- `LiveHub.publish(room_id: str, type: str, data: dict) -> None`; message to browsers is `{"type": str, "room_id": str, "data": dict, "sent_at": str}`.
- `require_role(*roles: str) -> Callable` FastAPI dependency returning the `User`.
- `Clock.now() -> datetime` (UTC, timezone-aware).
- NOTIFY channel `live` with payload `{"room_id", "type", "data"}` as JSON.

---

## Task 1: Repository skeleton and container stack (master plan Task 0.1 and 0.4)

**Files:** Create `deploy/docker-compose.yml`, `deploy/.env.example`, `deploy/nginx.conf`, `server/Dockerfile`, `server/app/main.py`, `server/app/config.py`, `web/Dockerfile`, `.gitignore`

**Produces:** four running containers; `GET /api/health -> {"ok": true}` through nginx.

- [ ] **Step 1: Write the failing test** `server/tests/test_health.py::test_health_ok` asserting `client.get("/api/health").json() == {"ok": True}`.
- [ ] **Step 2: Run** `pytest server/tests/test_health.py -v`. Expected: FAIL (no app).
- [ ] **Step 3: Implement** `app.main:app` with the health route and `Settings` (env names in `.env.example`); `server/Dockerfile` (python:3.12-slim, non-root user, `CMD uvicorn app.main:app --host 0.0.0.0`); nginx proxying `/api/` with `proxy_http_version 1.1` and `Upgrade` headers for WebSockets; `web/Dockerfile` placeholder page.
- [ ] **Step 4: Run** the test again. Expected: PASS. Then `docker compose -f deploy/docker-compose.yml up -d --build` and `curl http://localhost/api/health`. Expected: `{"ok":true}`.
- [ ] **Step 5: Commit** `chore: container skeleton with api, web and postgres`.

## Task 2: Data model and migrations

**Files:** Create `server/app/models.py`, `server/app/db.py`, `server/alembic/versions/0001_initial.py`, `server/tests/conftest.py`, `server/tests/test_models.py`

**Produces:** tables `users(id, username unique, password_hash, role, locked_until)`, `login_attempts(id, username, success, at)`, `rooms(id, name)`, `room_teachers(room_id, user_id)`, `students(id, user_id, name)`, `guardian_links(guardian_user_id, student_id)`, `devices(id, room_id, token_hash, last_seen_at, online, fw, reset_reason)`, `telemetry` (all telemetry fields plus `received_at`, `ts_suspect`, unique `(device_id, seq)`), `events` (event fields plus `received_at`, unique `(device_id, seq)`), `alerts_sent(id, event_id, channel, sent_at)`, `attendance(id, student_id, class_id, status, at)`.

- [ ] **Step 1: Write the failing test** `test_telemetry_unique_device_seq`: insert two telemetry rows with the same `(device_id, seq)`; assert `IntegrityError`.
- [ ] **Step 2: Run** it. Expected: FAIL (no model).
- [ ] **Step 3: Implement** the models and the first Alembic migration; `conftest.py` creates a throwaway Postgres schema per test session and provides a `FakeClock` fixture (`FakeClock.advance(seconds: float) -> None`).
- [ ] **Step 4: Run** `pytest server/tests/test_models.py -v` and `alembic upgrade head` inside the api container. Expected: PASS, no errors.
- [ ] **Step 5: Commit** `feat: initial schema and migrations`.

## Task 3: Device ingest (Level 2 task 2.5)

**Files:** Create `server/app/api/device.py`, `server/app/services/ingest.py`, `server/tests/test_ingest.py`

**Consumes:** models from Task 2.
**Produces:** `POST /api/device/telemetry` and `/events` accepting one object or a list; `ingest.validate_and_store(db, device, record: dict, kind: str, clock) -> IngestResult`; device auth dependency `current_device` (Bearer token compared against `devices.token_hash` with `hmac.compare_digest`).

- [ ] **Step 1: Write failing tests** in `test_ingest.py`: `test_valid_telemetry_stored`; `test_replay_returns_409_and_keeps_first` (second body differs, first stored values remain, row count 1); `test_batch_with_one_duplicate_stores_nine` (reply has 10 per-item results); `test_wrong_token_401`; `test_missing_field_422_names_the_field`; `test_wrong_room_for_device_rejected`; `test_bad_clock_sets_ts_suspect` (`ts` in 2019 and one 2 days ahead).
- [ ] **Step 2: Run** `pytest server/tests/test_ingest.py -v`. Expected: FAIL.
- [ ] **Step 3: Implement** with a Pydantic model per kind (enums for status, door, trend, quality), insert with `ON CONFLICT (device_id, seq) DO NOTHING` and read the row count to decide `stored` or `duplicate`; update `devices.last_seen_at`, `online`, `fw`; set `reset_reason` from `BOOT` events; after each stored record call `LiveHub.publish`.
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Commit** `feat: idempotent device ingest`.

## Task 4: Worker, offline detection and live fan-out between processes

**Files:** Create `server/app/worker.py`, `server/app/services/live.py` (NOTIFY half), `server/tests/test_worker.py`

**Consumes:** `Clock`, `devices`. **Produces:** `sweep_offline(db, clock, limit_s: int = 30) -> list[str]` (device ids newly offline); `notify(db, room_id, type, data)`; worker loop every 5 s.

- [ ] **Step 1: Write failing tests:** `test_device_goes_offline_after_30s` (FakeClock: seen at t=0, sweep at t=29 changes nothing, at t=31 returns the device); `test_online_again_on_next_message` (ingest after the sweep sets `online=True`); `test_sweep_publishes_offline_event` (a NOTIFY payload with `type="device_offline"`).
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** the sweep with one `UPDATE ... RETURNING` and a NOTIFY per changed device; `python -m app.worker` loops sweep + alert dispatch (Task 9).
- [ ] **Step 4: Run** the tests. Expected: PASS. Then stop `api` for 20 s and start it: the worker keeps running and the device state is correct.
- [ ] **Step 5: Commit** `feat: worker with offline sweep`.

## Task 5: Live hub, browser WebSocket and device command channel

**Files:** Modify `server/app/services/live.py`; create `server/app/api/live.py`, `server/tests/test_live.py`; extend `server/app/api/device.py` with `WS /api/device/ws`

**Produces:** `WS /api/live` (browser, session cookie); `LiveHub.send_command(device_id: str, cmd: dict) -> bool` (False when the device is not connected); `WS /api/device/ws` (device, Bearer token) that delivers `{"cmd": ..., "id": ...}` and records `{"ack": id}`.

- [ ] **Step 1: Write failing tests:** `test_telemetry_reaches_browser_within_3s` (connect test WebSocket, post telemetry, receive within 3 s); `test_student_gets_no_room_messages`; `test_closed_session_stops_receiving` (logout then publish, nothing arrives); `test_command_reaches_device_and_ack_is_stored`; `test_command_to_offline_device_returns_false`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** the hub: each api process listens on NOTIFY `live` and forwards to its connected sockets filtered by the user's allowed rooms (Admin all, Teacher via `room_teachers`, Student and Guardian none); recheck the session every 60 s; ping every 20 s.
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Commit** `feat: live hub and device command channel`.

## Task 6: Auth, roles and lockout (Level 4 tasks 1, 2, 7)

**Files:** Create `server/app/security.py`, `server/app/api/auth.py`, `server/app/api/students.py`, `server/app/services/lockout.py`, `server/tests/test_auth.py`, `server/tests/test_roles.py`

**Produces:** `POST /api/auth/login`, `POST /api/auth/logout`, `GET /api/auth/me`, `GET /api/auth/history` (admin); `LoginGuard.check_and_record(db, username, password, clock) -> LoginResult` with `LoginResult = {"ok": bool, "locked_for_s": int}`.

- [ ] **Step 1: Write failing tests:** `test_password_is_argon2_hash`; `test_five_wrong_passwords_lock_30s` (6th attempt with the correct password refused; after `clock.advance(31)` accepted); `test_parallel_wrong_passwords_cannot_exceed_five` (10 concurrent requests, exactly 5 counted before the lock); `test_history_lists_attempts_with_times` (admin only, 403 for others); `test_student_s2_cannot_open_s1_attendance_403_no_data`; `test_guardian_cannot_open_unlinked_child_403`; `test_teacher_only_page_refused_for_student`; `test_unauthenticated_401`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** argon2 hashing; a server-side session row with HttpOnly, SameSite=Lax cookie; `require_role`; ownership checks (Teacher sees rooms from `room_teachers`, Student only own `students.id`, Guardian only ids in `guardian_links`); the lock counter updated with a single atomic `UPDATE users SET ... RETURNING` so concurrent attempts cannot skip it.
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Commit** `feat: auth, roles and login lockout`.

## Task 7: Room API (Level 4 tasks 3 and 4, server part)

**Files:** Create `server/app/api/rooms.py`, `server/tests/test_rooms.py`

**Produces:** `GET /api/rooms` (scoped by role); `GET /api/rooms/{id}/state` -> `{status, temp_c, rh, heat_index_c, heat_trend, flame, door, online, last_seen_at, quality, fire_active, panic_active}`; `GET /api/rooms/{id}/events?limit=50` (recent taps and events); `GET /api/rooms/{id}/health` -> `{last_seen_at, uptime_s, rssi, heap_kb, chip_temp_c, fw, reset_reason}`; `POST /api/rooms/{id}/reset-fire` (teacher of that room or admin).

- [ ] **Step 1: Write failing tests:** `test_state_matches_latest_telemetry`; `test_health_matches_device_health_fields`; `test_teacher_cannot_read_other_room_403`; `test_reset_fire_sends_command_over_device_channel` (uses the Task 5 test device socket; expects `{"cmd": "reset_fire"}`); `test_reset_fire_refused_for_student_403`; `test_reset_fire_offline_device_returns_409_with_message`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** the routes from the latest `telemetry` row plus unresolved `FIRE` / `PANIC` events (`fire_active` clears on `FIRE_CLEARED`; `panic_active` clears 10 s after `PANIC`); reset-fire calls `LiveHub.send_command`.
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Commit** `feat: room state, health and reset-fire api`.

## Task 8: Simulator image

**Files:** Create `simulator/Dockerfile`, `simulator/simulate.py`, `simulator/tests/test_scenarios.py`

**Produces:** `python simulate.py --base https://host --token T --device T17-D1 --room R-305 --scenario normal|warming|fire|panic|offline-30s|replay` posting telemetry every 5 s with increasing `seq` (record of last `seq` in a local file so restarts never reuse one).

- [ ] **Step 1: Write failing tests:** `test_seq_never_reused_after_restart`; `test_warming_scenario_crosses_32_then_40_c`; `test_replay_scenario_resends_last_seq`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** scenarios with the paper's thresholds (32 °C Warning, 40 °C Danger); `fire` sets `flame=true`, `status=FIRE`, `door=UNLOCKED` and sends a `FIRE` event; `panic` sends `PANIC`; it also connects to `WS /api/device/ws` and prints commands it receives (so Reset fire can be seen end to end).
- [ ] **Step 4: Run** the tests, then `docker compose --profile sim up simulator`. Expected: rows appear in `telemetry` every 5 s.
- [ ] **Step 5: Commit** `feat: device simulator image`.

## Task 9: Phone alerts (Level 4 task 5)

**Files:** Create `server/app/services/alerts.py`, `server/tests/test_alerts.py`; extend `server/app/worker.py`

**Produces:** `AlertRules.matches(event: Event) -> str | None` returning `"FIRE"`, `"PANIC"` or `"HEAT_DANGER"`; `TelegramSender.send(chat_id: str, text: str) -> None`; worker loop `dispatch_alerts(db, sender, clock)` every 1 s; `alerts_sent` rows with `sent_at`.

- [ ] **Step 1: Write failing tests:** `test_fire_event_sends_message_within_10s` (FakeClock and a fake sender); `test_panic_and_heat_danger_send`; `test_status_safe_to_warning_sends_nothing`; `test_each_event_sends_once` (dispatch twice, one message); `test_telegram_failure_is_retried_then_recorded` (3 tries, then `alerts_sent` row marks `error`).
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** the rules (`STATUS` event with new value `DANGER` is Heat Danger); one `alerts_sent` row inserted before sending so a restart never double-sends; Teacher chat ids come from `users.telegram_chat_id` (add the column in a second migration `0002`); message text names the room, event and time.
- [ ] **Step 4: Run** the tests. Expected: PASS. Then post a simulated FIRE and confirm the message reaches the phone within 10 s.
- [ ] **Step 5: Commit** `feat: telegram alerts for fire, panic and heat danger`.

## Task 10: Dashboard UI (Level 4 tasks 3, 4, 6)

**Files:** Create everything under `web/src/` listed in File Structure; tests in `web/src/__tests__/`

**Consumes:** the routes from Tasks 6, 7 and the `/api/live` socket. **Produces:** pages `/login`, `/rooms/:id`, `/rooms/:id/health`, `/admin/logins`; `useLive(roomId) -> {state, events, connected}`.

- [ ] **Step 1: Write failing tests (Vitest):** `Room renders status, temperature, humidity, heat index, trend, door and online from state`; `Banner shows FIRE and PANIC in red and is present on every page when fire_active`; `Reset fire button visible for teacher and admin, hidden for student`; `useLive reconnects after a dropped socket and refetches state`; `LangSwitch changes every visible label to Bangla and back` (snapshot of two pages); `Login shows locked message with seconds remaining`.
- [ ] **Step 2: Run** `npm test`. Expected: FAIL.
- [ ] **Step 3: Implement** the pages and components; `live.ts` reconnects with 1, 2, 4, 8 s gaps and refetches state on reconnect; all strings through i18next with `en.json` and `bn.json` holding the same keys (a test asserts key sets are equal); hide navigation items the role cannot open (the server still enforces).
- [ ] **Step 4: Run** `npm test` and `npm run build`. Expected: PASS; `web` image builds.
- [ ] **Step 5: Commit** `feat: role-aware live dashboard with English and Bangla`.

## Task 11: Seed, deploy and Level 4 evidence

**Files:** Create `server/seed.py`, `README.md` (platform part), `docs/writeups/L04_platform.md`; modify `deploy/docker-compose.yml`

**Produces:** `python -m seed` creating Admin, Teacher, Student S1, Student S2, Guardian linked to S1, one room, one device and printing its token once; passwords read from `.env`.

- [ ] **Step 1: Write the failing test** `test_seed_creates_five_accounts_and_guardian_link`; also `test_seed_is_idempotent`.
- [ ] **Step 2: Run** it. Expected: FAIL.
- [ ] **Step 3: Implement** the seed and the deploy steps: `docker compose up -d --build` on the VM, `tailscale funnel --bg 443`, Alembic upgrade on api start.
- [ ] **Step 4: Verify on the public address from a phone on mobile data:** login for each role; each sees only its allowed pages; run the `fire` simulator scenario and confirm the red banner within 3 s and the Telegram message within 10 s; `Reset fire` reaches the simulator; 5 wrong passwords lock the account for 30 s and the Admin history lists them; switch to Bangla on two pages and back.
- [ ] **Step 5: Commit** `feat: seed judges accounts and deploy` and tag `level-04-server` (the full `level-04` tag waits for the firmware parts).

---

## Coverage of the Spec

| Requirement | Task |
|---|---|
| Public HTTPS site, hashed passwords | 1, 6, 11 |
| Four roles checked on every request | 6, 7, 5 |
| Live room dashboard within 3 s, banner, Reset fire | 5, 7, 10 |
| Device health panel (7 fields) | 7, 10 |
| Phone alerts within 10 s | 9 |
| English and Bangla | 10 |
| Lockout 5 wrong, 30 s, login history | 6, 10 |
| Judges' accounts | 11 |
| Level 2 ingest rules: token, `(device_id, seq)` replay rejected, offline within 30 s, reconnect after backend restart | 3, 4, 8 |
| Multiple Docker images | 1, 4, 8, 10 (web, api, worker, postgres, simulator) |

## Open Items

- Telegram bot token and the Teacher's chat id (from you, into `.env` on the VM).
- VM access and Tailscale Funnel enabled on the tailnet (Funnel needs the node to be allowed in the tailnet policy).
- Whether `api` and `worker` should be separate images (this plan builds one image run with two commands; splitting is a Dockerfile target change).
