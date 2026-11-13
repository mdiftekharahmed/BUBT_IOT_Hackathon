# Level 4 — platform, accounts and alerting (writeup)

Deliverable: the server side of FastRobox's Level 4 — auth with lockout, a live
dashboard channel, role-scoped room APIs, Telegram alerts, the dashboard UI,
and the judges' seed/deployment. Branch `by_HIMEL`, tag `level-04-server`.

## What was built

| Task | Commit | Contents |
|---|---|---|
| Baseline | `d24e653`, `6bc800f`, `5464310` | synced `main`, embedded-PG test DB, `worker` + `test-postgres` in compose |
| Auth / roles / lockout | `8b5164c` | argon2 passwords, `sessions` table, `require_role`, atomic `LoginGuard`, `/api/auth/*`, scoped attendance |
| Live hub + device WS | `4eda6b0` | `WS /api/live` (role-scoped rooms), `WS /api/device/ws` (server→device commands + acks), NOTIFY listener |
| Rooms API | `3199ea2` | state/events/health, `reset-fire` (409 offline), `/api/alerts/active`, fire latch + 10 s panic window |
| Simulator | `96f3b26` | device-WS rehearsal client in `simulate.py`, simulator image + compose profile |
| Telegram alerts | `ba32fce` | FIRE / PANIC / Heat-Danger → teachers' chat, exactly-once, 3 retries |
| Dashboard | `30f2482` | Vite + React + TS + Tailwind + i18next UI, `useLive` reconnect, nginx image |
| Seed / deploy | this commit | `python -m seed`, Dockerfile runs migrations, README, `.env.example` |

## How the constraints are met

**Dashboard ≤ 3 s, banner on every page.** Ingest NOTIFYs a `live` channel;
each API process fans out over in-process WebSockets. The dashboard refetches
room state on every push (`useLive`), so new taps/telemetry appear without
reload, well under 3 s. The red banner lives in `AppLayout`, so it renders on
every page: Teacher/Admin get it via the live socket (refetch on each push),
Student/Guardian — who receive **no room data** on the socket — poll
`/api/alerts/active` every 2 s, which returns only `room_id/kind/since`.

**Telegram ≤ 10 s.** The worker ticks every 1 s, so a FIRE/PANIC/Heat-Danger
event is dispatched within ~1 s of ingest (3 attempts, 10 s HTTP timeout).
`alerts_sent` rows are inserted *before* sending with a unique `event_id`, so a
worker restart cannot double-send; three failed tries leave the error on the
row and stop retrying.

**Lockout: 5 wrong → 30 s, correct password refused too.** The counter is a
single atomic `UPDATE … RETURNING` with CASE branches, so parallel attempts
cannot exceed 5; the fifth wrong attempt engages the lock and every attempt
while locked — including the correct password — is refused with `423` and the
remaining seconds. The Admin's `/admin/logins` lists attempts with timestamps.

**Roles checked server-side on every route.** `require_role` / per-route
guards: Teachers only their `room_teachers` rooms (other rooms → 403), Students
only their own attendance (S2 → S1 → 403 with no data), Guardians only linked
children, Admin everything. The device WS requires a valid device token.

**No secrets in the repo.** All secrets come from `deploy/.env`
(`.env.example` lists them): `POSTGRES_PASSWORD`, `SESSION_SECRET`,
`TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID`, `JUDGE_*_PASSWORD`, `DEVICE_TOKEN`.
The device token is stored hashed and printed once by the seed.

## Judges' walkthrough

1. `docker compose -f deploy/docker-compose.yml up -d --build` (api applies
   migrations on start; worker handles alerts + offline sweep).
2. `docker compose exec api python -m seed` → five accounts, room R-305,
   device T17-D1, token printed once, page addresses listed.
3. Log in as each judge: Admin sees every room + login history; Teacher sees
   R-305 only (R-306 → 403); S2 sees no rooms; the banner appears for everyone
   when the `fire` scenario runs.
4. `SIM_SCENARIO=fire docker compose --profile sim up simulator` → red banner
   ≤ 3 s on the dashboard, Telegram message ≤ 10 s, Reset fire (Teacher/Admin)
   prints the ack in the simulator console.
5. Five wrong passwords on `/login` → 30 s lock with countdown; Admin sees the
   attempts in `/admin/logins`.
6. Toggle বাংলা with the language switch — both the login and room pages
   translate; switching back restores byte-identical English (tested).

## Limitations / next levels

- Attendance lists are empty by design until Level 7.
- Charts (Recharts) are deferred to Level 6; the dashboard shows current state
  and a flat event list.
- On a single API process the listener thread publishes directly; multi-process
  fan-out goes through PostgreSQL NOTIFY (already implemented).
