# FastRobox Web Platform Completion (remaining Tasks 5–11) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish the web platform so Levels 2 and 4 pass their judges' checks (rows A4, A6, B1, B2 and evidence item L4). This plan picks up where commit `782e1a9 "telemetry"` on `origin/main` stopped: original Tasks 1–4 are done (container skeleton, data model + migration, idempotent device ingest, worker + offline sweep + NOTIFY). Everything here completes the remaining original Tasks 5–11.

**Spec:** `FastRobox_1.0_QuestionPaper_FOR_TEAMS.md`, `fastrobox_1_0_judges_checklist.md`, master plan `2026-11-13-fastrobox-12-levels_implementationPlan.md` (Stage 0.4, Level 2 task 2.5, Level 4 tasks 4.1–4.7), predecessor plan `2026-11-13-web-infrastructure-levels-0-4.md` (its Global Constraints, Review Focus items 4–5 and shared interfaces stay in force).

**Out of scope:** firmware (master-plan Levels 1–3, 5–12 device sides). Round A needs the real board; until it exists, rehearse the platform with the simulator.

## Baseline (verified 2026-11-13)

- `origin/main` = `782e1a9`: `app/{main,config,db,models,clock,security,worker}.py`, `app/api/device.py`, `app/services/{ingest,live(NOTIFY half)}.py`, `alembic/` with `0001_initial_schema`, tests (`test_health`, `test_models`, `test_ingest` ×9, `test_worker` ×5), `simulator/simulate.py` (6 scenarios + selftest, seq persisted to file), `dev_seed.py`.
- Working branches `by_HIMEL` / `by_SOBUJ` are at `0f948cf` — **one commit behind**; merge first.
- **Docker is not installed on this machine.** Test database must come from a local PostgreSQL install, a teammate's machine, or the VM (Task A gives both paths).
- Local Python is 3.15: tests run, but `uvicorn[standard]` fails to install (no `pyyaml` wheels yet). Use Python 3.12 (`py -3.12 -m venv .venv` or `uv venv -p 3.12`).
- The simulator does **not** yet connect to `WS /api/device/ws` — required for the Reset-fire rehearsal; added in Task E.

## Tech Stack (delta from predecessor plan)

argon2-cffi (passwords), node 20 + Vite + React + TypeScript + Tailwind + i18next + Vitest + Testing Library (dashboard), PostgreSQL 16 (also as local test DB). Recharts is deferred to Level 6 (history charts).

## Execution order deviation (deliberate)

The predecessor plan ordered Task 5 (live hub) before Task 6 (auth), but Task 5's own tests need sessions and roles (`test_student_gets_no_room_messages`, `test_closed_session_stops_receiving`). **Auth (original Task 6) is executed first, then the live hub (original Task 5).** Original task numbers are kept in brackets.

## Global Constraints (carried from the predecessor plan)

- Telemetry/event fields exactly as specified; `(device_id, seq)` unique; replay → `409`, row count unchanged. [done, regression-protected]
- Device offline after 30 s of silence, online on the next message. [done, regression-protected]
- Dashboard updates within 3 s of new data, no reload. FIRE and PANIC banner on every logged-in dashboard within 3 s. Telegram within 10 s of FIRE, PANIC or Heat Danger.
- Roles `admin teacher student guardian` checked by the server on every request; passwords argon2-hashed; lockout 5 wrong → 30 s (even the correct password), every attempt logged, history admin-only.
- Every page switches English/Bangla.
- No secret in the repository: `.env` git-ignored; `deploy/.env.example` names only.
- Public HTTPS; device token checked on every device request.

## Review Focus (uncovered failure modes)

1. **Parallel wrong passwords must not exceed 5.** Atomic counter via one `UPDATE users ... RETURNING`; concurrent attempts cannot skip the lock. Task B.
2. **WebSocket after logout or role change.** Session rechecked on connect and every 60 s; a closed session stops receiving; Student/Guardian never receive room messages. Task C.
3. **Exactly-once Telegram across worker restarts.** `alerts_sent` row (unique `event_id`) inserted *before* sending. Task F.
4. **`panic_active` expiry timing.** Computed from the PANIC event's time (+10 s), not from request-arrival drift. Task D.
5. **Banner for roles with no room access.** Student/Guardian get no room data on `/api/live` (predecessor rule) yet master-plan 4.3 wants the banner on *every* logged-in dashboard. Resolution: `GET /api/alerts/active` (any authenticated role, no room payload beyond id/kind), polled every 2 s by those clients; Teacher/Admin get the banner via the live socket. Tasks C, D, G.

## File Structure (delta only)

```
deploy/
  docker-compose.yml        + worker service; + simulator and test-postgres profiles
  .env.example              + TELEGRAM_CHAT_ID, JUDGE_*_PASSWORD, DEVICE_TOKEN
server/
  app/
    security.py             + argon2 hash/verify, session create/load/revoke, require_role
    api/auth.py             login, logout, me, history
    api/students.py         GET /api/students/{id}/attendance (empty, role-checked)
    api/live.py             WS /api/live (browser, session cookie)
    api/rooms.py            rooms list, state, events, health, reset-fire; /api/alerts/active
    services/lockout.py     LoginGuard (atomic counter)
    services/room_state.py  shared fire_active / panic_active logic
    services/live.py        + socket registry, NOTIFY listener, send_command
    services/alerts.py      AlertRules, TelegramSender
    worker.py               + dispatch_alerts (1 s loop)
    api/device.py           + WS /api/device/ws
    main.py                 + router wiring
  alembic/versions/0002_sessions.py
  seed.py                   judges' accounts (replaces dev_seed for judging; dev_seed stays for dev)
  tests/                    test_auth.py test_roles.py test_live.py test_rooms.py test_alerts.py test_seed.py
  Dockerfile                CMD runs `alembic upgrade head && uvicorn ...`
simulator/
  Dockerfile, tests/test_scenarios.py, simulate.py + device-WS rehearsal client
web/                        Vite app: package.json, vite.config.ts, tailwind, src/{main.tsx, api.ts, live.ts,
  App.tsx, i18n/{en,bn}.json, pages/{Login,Room,Health,LoginHistory}.tsx,
  components/{Banner,StatusCard,RecentEvents,LangSwitch}.tsx, __tests__/
  Dockerfile                multi-stage: node build → nginx serving dist
README.md                   build, run, test, deploy (platform part)
docs/writeups/L04_platform.md
```

Shared interfaces (unchanged from predecessor): `IngestResult`, `LiveHub.publish(room_id, type, data)` → message `{"type", "room_id", "data", "sent_at"}`, `require_role(*roles) -> User`, `Clock.now() -> datetime (UTC)`, NOTIFY channel `live`. New: `LoginResult = {"ok": bool, "locked_for_s": int}`; `LiveHub.send_command(device_id, cmd) -> bool`; device messages `{"cmd": ..., "id": ...}` down, `{"ack": id}` up.

---

## Task A: Baseline — sync, clean metadata, green suite, test database (setup)

**Files:** `.gitignore`, `deploy/docker-compose.yml` (+worker, +test-postgres profile)

- [ ] **Step 1:** `git fetch origin` and merge `origin/main` into the working branch (`by_HIMEL`). Untrack build metadata: add `server/fastrobox_server.egg-info/` to `.gitignore`, `git rm -r --cached server/fastrobox_server.egg-info`. Commit `chore: sync main and untrack build metadata`.
- [ ] **Step 2:** Stand up a test PostgreSQL on `127.0.0.1:54329` with role `fastrobox` and db `fastrobox_test`. With Docker (any machine that has it): `docker run -d --name fastrobox-test-pg -e POSTGRES_USER=fastrobox -e POSTGRES_DB=fastrobox_test -e POSTGRES_HOST_AUTH_METHOD=trust -p 54329:5432 postgres:16-alpine`. Without Docker (this machine): install PostgreSQL 16, set `port = 54329`, then `CREATE ROLE fastrobox LOGIN;` and `CREATE DATABASE fastrobox_test OWNER fastrobox;`.
- [ ] **Step 3:** Create a Python 3.12 venv, `pip install -e "server[dev]"`, run `pytest server/tests -v`. Expected: all pass (health, models, 9 ingest, 5 worker) — this validates Tasks 2–4 of the predecessor plan for real.
- [ ] **Step 4:** Add to `deploy/docker-compose.yml`: `worker` service (same build as `api`, `command: python -m app.worker`, `env_file: .env`, depends_on postgres healthy) and a `test-postgres` service under `profiles: [test]` with the same env as Step 2's docker run.
- [ ] **Step 5:** Commit `chore: worker service and test database profile`.

## Task B [original 6]: Auth, roles, lockout

**Files:** modify `app/security.py`, `pyproject.toml` (+`argon2-cffi>=23`); create `app/api/auth.py`, `app/api/students.py`, `app/services/lockout.py`, `alembic/versions/0002_sessions.py`, `tests/test_auth.py`, `tests/test_roles.py`

**Produces:** `POST /api/auth/login` (200 + HttpOnly SameSite=Lax cookie | 423 `{"locked_for_s": n}` | 401), `POST /api/auth/logout`, `GET /api/auth/me`, `GET /api/auth/history` (admin only); `LoginGuard.check_and_record(db, username, password, clock) -> LoginResult`; `require_role(*roles)`; `GET /api/students/{id}/attendance` (empty payload, role-checked now).

- [ ] **Step 1: failing tests** in `test_auth.py` + `test_roles.py`: `test_password_is_argon2_hash`; `test_five_wrong_passwords_lock_30s` (6th attempt with the correct password refused; after `clock.advance(31)` accepted); `test_parallel_wrong_passwords_cannot_exceed_five` (10 concurrent requests, exactly 5 counted before the lock); `test_history_lists_attempts_with_times` (admin 200, others 403); `test_student_s2_cannot_open_s1_attendance_403_no_data`; `test_guardian_cannot_open_unlinked_child_403`; `test_teacher_only_page_refused_for_student`; `test_unauthenticated_401`.
- [ ] **Step 2:** run → FAIL.
- [ ] **Step 3: implement.** argon2 hashing; `sessions` table (id, user_id, created_at, expires_at, revoked_at) in migration `0002`; server-side session row + cookie; `require_role`; ownership scoping (Teacher sees rooms via `room_teachers`, Student only own `students.id`, Guardian only ids in `guardian_links`); the lock counter updated by a single atomic `UPDATE users SET failed_attempts = failed_attempts + 1, locked_until = CASE ... END ... RETURNING` so concurrent attempts cannot skip it; success resets the counter; every attempt into `login_attempts` (column already in `0001`).
- [ ] **Step 4:** run tests → PASS; `pytest server/tests` still green.
- [ ] **Step 5:** Commit `feat: auth, roles and login lockout`.

## Task C [original 5]: Live hub, browser WebSocket, device command channel

**Files:** modify `app/services/live.py`, `app/api/device.py`; create `app/api/live.py`, `tests/test_live.py`; wire router in `app/main.py`

**Produces:** `WS /api/live` (session cookie); per-connection allowed rooms (Admin: all; Teacher: `room_teachers`; Student/Guardian: none); each api process listens on NOTIFY `live` and forwards to its sockets as `{"type", "room_id", "data", "sent_at"}`; session recheck every 60 s; ping every 20 s; `LiveHub.send_command(device_id, cmd) -> bool` (False when the device is not connected); `WS /api/device/ws` (Bearer token) delivering `{"cmd": ..., "id": ...}` and accepting `{"ack": id}` (in-memory pending registry + log).

- [ ] **Step 1: failing tests:** `test_telemetry_reaches_browser_within_3s`; `test_student_gets_no_room_messages`; `test_closed_session_stops_receiving` (logout then publish, nothing arrives); `test_command_reaches_device_and_ack_is_recorded`; `test_command_to_offline_device_returns_false`.
- [ ] **Step 2:** run → FAIL.
- [ ] **Step 3: implement** the hub, the two WS endpoints and the app-lifespan NOTIFY listener task.
- [ ] **Step 4:** run tests → PASS; full suite green.
- [ ] **Step 5:** Commit `feat: live hub and device command channel`.

## Task D [original 7]: Rooms API and active alerts

**Files:** create `app/api/rooms.py`, `app/services/room_state.py`, `tests/test_rooms.py`

**Produces:** `GET /api/rooms` (role-scoped); `GET /api/rooms/{id}/state` → `{status, temp_c, rh, heat_index_c, heat_trend, flame, door, online, last_seen_at, quality, fire_active, panic_active}`; `GET /api/rooms/{id}/events?limit=50`; `GET /api/rooms/{id}/health` → `{last_seen_at, uptime_s, rssi, heap_kb, chip_temp_c, fw, reset_reason}`; `POST /api/rooms/{id}/reset-fire` (teacher of that room or admin; sends over the device channel); `GET /api/alerts/active` (any authenticated role; Student/Guardian banner source; returns `[{room_id, kind: FIRE|PANIC, since}]`).

`fire_active` = unresolved `FIRE` event (clears on `FIRE_CLEARED`); `panic_active` = `PANIC` event younger than 10 s (event time, Review Focus 4); both computed in `room_state.py` and reused by both endpoints.

- [ ] **Step 1: failing tests:** `test_state_matches_latest_telemetry`; `test_health_matches_device_health_fields`; `test_teacher_cannot_read_other_room_403`; `test_reset_fire_sends_command_over_device_channel` (uses the Task C device socket; expects `{"cmd": "reset_fire"}`); `test_reset_fire_refused_for_student_403`; `test_reset_fire_offline_device_returns_409_with_message`.
- [ ] **Step 2:** run → FAIL.
- [ ] **Step 3: implement.** State/health read from the latest `telemetry` row + `devices` row; `reset-fire` calls `LiveHub.send_command`, `409 {"detail": "device offline"}` when it returns False.
- [ ] **Step 4:** run tests → PASS; full suite green.
- [ ] **Step 5:** Commit `feat: room state, health and reset-fire api`.

## Task E [original 8]: Simulator finished + compose profiles

**Files:** create `simulator/Dockerfile`, `simulator/tests/test_scenarios.py`; modify `simulator/simulate.py` (+device-WS client that connects with the Bearer token, prints every received `{"cmd", "id"}` and answers `{"ack": id}`); add `simulator` service to compose under `profiles: [sim]`.

- [ ] **Step 1: failing tests** (pure logic, no server needed): `test_seq_never_reused_after_restart` (a new `Device` instance with the same seq file continues above every number already issued); `test_warming_scenario_crosses_32_then_40_c` (feeding the warming ramp yields SAFE → WARNING → DANGER at the paper's thresholds); `test_replay_scenario_resends_last_seq` (posting the last seq again returns 409).
- [ ] **Step 2:** run → FAIL.
- [ ] **Step 3: implement** the Dockerfile (python:3.12-slim + httpx) and the WS client.
- [ ] **Step 4:** run tests → PASS. On a Docker machine: `docker compose --profile sim up simulator` and confirm rows appear every 5 s and Reset fire prints on the simulator console.
- [ ] **Step 5:** Commit `feat: simulator image with device-ws rehearsal client`.

## Task F [original 9]: Phone alerts

**Files:** create `app/services/alerts.py`, `tests/test_alerts.py`; modify `app/worker.py` (tick every 1 s; sweep every 5th tick; `dispatch_alerts(db, sender, clock)` each tick)

**Produces:** `AlertRules.matches(event) -> "FIRE" | "PANIC" | "HEAT_DANGER" | None` (a `STATUS` event whose new value is `DANGER` is Heat Danger); `TelegramSender.send(chat_id, text)`; `alerts_sent` rows (inserted **before** sending — Review Focus 3) with `sent_at`, or `error` after 3 failed tries; Teacher chat ids from `users.telegram_chat_id`; message names room, kind and time.

- [ ] **Step 1: failing tests:** `test_fire_event_sends_message_within_10s` (FakeClock + fake sender); `test_panic_and_heat_danger_send`; `test_status_safe_to_warning_sends_nothing`; `test_each_event_sends_once` (dispatch twice, one message); `test_telegram_failure_is_retried_then_recorded` (3 tries, then an `error` row).
- [ ] **Step 2:** run → FAIL.
- [ ] **Step 3: implement** rules, sender (httpx), dispatch loop.
- [ ] **Step 4:** run tests → PASS. Manual: with a real `TELEGRAM_BOT_TOKEN` and the Teacher's chat id in `.env`, run the `fire` simulator scenario and confirm the phone message within 10 s.
- [ ] **Step 5:** Commit `feat: telegram alerts for fire, panic and heat danger`.

## Task G [original 10]: Dashboard UI

**Files:** scaffold `web/` (Vite + React + TS, Tailwind, i18next, Vitest + Testing Library); `web/Dockerfile` multi-stage (node:20-alpine build → nginx:1.27-alpine + `deploy/nginx.conf`); replace the placeholder `web/index.html`.

**Produces:** routes `/login`, `/rooms/:id`, `/rooms/:id/health`, `/admin/logins`; `api.ts` (fetch wrapper, credentials, 401/403/423 normalization); `live.ts` (WS client, reconnect gaps 1, 2, 4, 8 s, refetch state on reconnect, `connected` flag); `useLive(roomId) -> {state, events, connected}`; `Banner` (red on FIRE and PANIC; Teacher/Admin via the live socket ≤3 s, Student/Guardian via 2 s polling of `/api/alerts/active` — Review Focus 5) shown on **every** page; `StatusCard`; `RecentEvents` (recent taps and events); `LangSwitch` (all labels through i18next, `en.json`/`bn.json` with equal key sets); Reset fire button visible for Teacher/Admin only, disabled while offline (the server still enforces); Login shows the locked message with a countdown from `locked_for_s`.

- [ ] **Step 1: failing Vitest tests:** `Room renders status, temperature, humidity, heat index, trend, door and online from state`; `Banner shows FIRE and PANIC in red and is present on every page when fire_active`; `Reset fire button visible for teacher and admin, hidden for student`; `useLive reconnects after a dropped socket and refetches state`; `LangSwitch changes every visible label to Bangla and back` (snapshot of two pages); `Login shows locked message with seconds remaining`; plus a test asserting `en.json` and `bn.json` key sets are equal.
- [ ] **Step 2:** `npm test` → FAIL.
- [ ] **Step 3: implement** pages and components; hide navigation items the role cannot open.
- [ ] **Step 4:** `npm test` and `npm run build` → PASS; on a Docker machine the `web` image builds. Requires Node.js ≥ 20 locally (Open Item).
- [ ] **Step 5:** Commit `feat: role-aware live dashboard with English and Bangla`.

## Task H [original 11]: Seed, deploy and Level 4 evidence

**Files:** create `server/seed.py` (judges' seed; `dev_seed.py` stays for development), `README.md` (platform part), `docs/writeups/L04_platform.md`; modify `server/Dockerfile` (`CMD ["sh","-c","alembic upgrade head && uvicorn app.main:app --host 0.0.0.0 --port 8000"]`), `deploy/.env.example` (+`TELEGRAM_CHAT_ID`, `JUDGE_ADMIN_PASSWORD`, `JUDGE_TEACHER_PASSWORD`, `JUDGE_S1_PASSWORD`, `JUDGE_S2_PASSWORD`, `JUDGE_GUARDIAN_PASSWORD`, `DEVICE_TOKEN`).

**Produces:** `python -m seed` creating Admin, Teacher, S1, S2, Guardian→S1, one room + `room_teachers`, one device + token printed once; passwords from `.env`; idempotent on rerun; prints the page-address list.

- [ ] **Step 1: failing tests** `test_seed_creates_five_accounts_and_guardian_link`, `test_seed_is_idempotent` (second run: still 5 users, 1 guardian link, no duplicate device).
- [ ] **Step 2:** run → FAIL.
- [ ] **Step 3: implement** seed + README + Dockerfile CMD change.
- [ ] **Step 4: deploy and verify on the public address from a phone on mobile data:** `docker compose -f deploy/docker-compose.yml up -d --build`, `python -m seed` inside the api container, `tailscale funnel --bg 443`. Then, using the `fire` and `panic` simulator scenarios: login for each role, each seeing only its allowed pages; red banner ≤3 s; Telegram ≤10 s; Reset fire reaches the simulator console; 5 wrong passwords lock 30 s and the Admin history lists them with times; Bangla switch on two pages and back; Student S2 opening S1's attendance → 403, no data.
- [ ] **Step 5:** Commit `feat: seed judges accounts and deploy` and tag `level-04-server`.

---

## Coverage of the Spec

| Requirement (source) | Task |
|---|---|
| A4: replay rejected, row count unchanged (judges A4; master 2.5) | done in `782e1a9`; regression-run in Task A |
| A6: offline within 30 s, online again with BOOT event (judges A6) | done in `782e1a9`; dashboard display Task G |
| B1: roles see only what is allowed; Student/Guardian scoping (judges B1; master 4.1) | B |
| B1: tap shows ≤5 s without reload (judges B1; master 4.3, 3 s) | C, G |
| B1: health panel matches the `health` output, 7 fields (judges B1; master 4.4) | D, G |
| B2: red PANIC banner ≤3 s; Telegram ≤10 s (judges B2; master 4.3, 4.5) | C, F, G |
| B2: S2 → S1 attendance refused, no data (judges B2; master 4.1) | B |
| L4 evidence: four roles scoped; Bangla on two pages; 5 wrong → 30 s + admin history; hashed passwords; teacher-only/guardian refusals (checklist L4) | B, D, G, H |
| Reset fire over the device channel (master 4.3; reused by Level 3's A6 row) | C, D |
| Judges' accounts + printed addresses (master 4.7) | H |
| Dashboard ≤3 s, banner on every logged-in dashboard (global constraints; master 4.3) | C, D, G (Review Focus 5) |
| Exactly-once alerts across restarts (Review Focus 3) | F |
| Parallel lockout cannot exceed 5 (predecessor Review Focus 4) | B |
| WS dies with session; no room data to Student/Guardian (predecessor Review Focus 5) | C |
| Five Docker images: web, api, worker, postgres, simulator (predecessor coverage) | A, E, G |
| Public HTTPS + Funnel; no secrets in repo (global constraints) | H |

## Open Items

- Telegram bot token and the Teacher's chat id → `deploy/.env` on the VM.
- SSH access to the VM and Tailscale Funnel enabled for the node.
- Node.js ≥ 20 on the working machine (Task G) or run the UI tests on a teammate's machine / CI.
- Docker absent on this machine: test DB from a local PostgreSQL 16 install, or the compose `test` profile on a Docker machine.
- Local Python 3.15 cannot install `uvicorn[standard]` (pyyaml wheels missing) — use Python 3.12.
- Team ID spelling `T17` to confirm with organizers (master plan Open Item).
- Firmware Levels 1–3 are required before Round A and before Level 4's live rows use the real panic pad — separate track (master plan).
