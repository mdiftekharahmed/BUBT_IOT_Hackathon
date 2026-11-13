# FastRobox — smart classroom safety box

An ESP32-based classroom device (temperature, flame, door, RFID taps) with a
FastAPI platform that stores telemetry, pushes live updates to the dashboard,
sends Telegram alerts, and enforces role-scoped access for **Admin, Teacher,
Student and Guardian**. The dashboard speaks English and Bangla.

## Layout

| Path | What it is |
|---|---|
| `server/` | FastAPI + SQLAlchemy + Alembic; telemetry/event ingest, live hub, auth, rooms API, Telegram worker |
| `firmware`/device | the ESP32 firmware (separate deliverable; see the question paper) |
| `simulator/` | a script that replays realistic device traffic over HTTP + WS (`--selftest` verifies the platform) |
| `web/` | Vite + React + TypeScript + Tailwind dashboard (i18next: `en`/`bn`), served by nginx |
| `deploy/` | `docker-compose.yml` (api, postgres, web, worker, test-postgres, simulator) + `.env.example` + `nginx.conf` |
| `docs/` | plans, judges' checklist and writeups (`docs/writeups/`) |

## Quick start (local, no Docker)

```bash
# 1. database: any PostgreSQL 16, or the compose test profile
docker compose -f deploy/docker-compose.yml --profile test up -d test-postgres

# 2. server (from server/)
python -m venv .venv && . .venv/bin/activate      # Windows: .venv\Scripts\activate
pip install -e ".[dev]"
alembic upgrade head
python dev_seed.py                                # dev room/device + token in ../.dev/demo.env
uvicorn app.main:app --port 8000 --reload

# 3. worker (alerts + offline sweep), second terminal
python -m app.worker

# 4. dashboard (third terminal, from web/)
npm install && npm run dev                        # proxies /api to :8000

# 5. traffic
python ../simulator/simulate.py --scenario normal # or warming | fire | panic | offline | replay | selftest
```

## Quick start (Docker)

```bash
cd deploy
cp .env.example .env        # fill POSTGRES_PASSWORD, judge passwords, DEVICE_TOKEN, Telegram
docker compose up -d --build
docker compose exec api python -m seed     # judges' accounts; prints the DEVICE_TOKEN once
```

Then open `http://localhost` (port 80 from compose). For a public HTTPS demo,
expose the VM with `tailscale funnel --bg 443`.

## Judges' accounts (`python -m seed`)

| User | Role | Sees |
|---|---|---|
| `admin` | Admin | everything + `/admin/logins` (login history) |
| `teacher` | Teacher | their rooms: state, health, Reset fire |
| `s1`, `s2` | Student | own attendance (Level 7), alert banner |
| `guardian` | Guardian | linked child (s1) attendance, alert banner |

Passwords come from `.env` (`JUDGE_*_PASSWORD`); the device token is printed
once by the seed and stored only as a hash.

## Pages

- `/login` — sign in; 5 wrong passwords lock the account for 30 s (countdown shown)
- `/` — room list (Admin/Teacher)
- `/rooms/R-305` — live state (status, temperature, humidity, heat index, trend, door, online), recent events, Reset fire (Teacher/Admin)
- `/rooms/R-305/health` — the 7 device-health fields
- `/admin/logins` — login history with times (Admin only)

A red FIRE/PANIC banner appears on every page within 3 s (live socket for
Teacher/Admin, 2 s polling for Student/Guardian). FIRE, PANIC and Heat-Danger
reaches the teachers' Telegram within 10 s.

## Testing

```bash
# server + simulator (needs a PostgreSQL on 127.0.0.1:54329, see deploy compose test profile)
cd server && python -m pytest tests

# dashboard
cd web && npm test && npm run build
```

Secrets live only in `.env` (git-ignored). `deploy/.env.example` lists every
variable the platform reads.
