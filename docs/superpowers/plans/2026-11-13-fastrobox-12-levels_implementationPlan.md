# FastRobox 1.0 Smart Classroom — 12-Level Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Team 17 smart-classroom system (one ESP32-S3-CAM + a public web platform) so that all 12 levels pass the judges' live checks and evidence checks, in strict order.

**Architecture:** One firmware project and one backend monolith. The device sends telemetry and events to the backend over HTTPS (idempotent on `(device_id, seq)`) and keeps one secure WebSocket open for server→device commands (settings, OTA, reset fire, weather alerts, camera position). The backend is a single FastAPI app with PostgreSQL that also runs all server-side AI (face match, anti-spoofing, person detection, hand-raise, CSI) and serves the React dashboard.

**Tech Stack:**

| Part | Choice | Why |
|---|---|---|
| Firmware build | PlatformIO, `espressif32@6.x` (Arduino core 2.0.x on FreeRTOS) | Core 2.0.x bundles esp-dl face detection for the S3; FreeRTOS tasks, task watchdog, NVS, LittleFS, mDNS, OTA and touch are all built in |
| Firmware libs | `MFRC522`, `DHTesp`, `ArduinoJson`, `NimBLE-Arduino`, `esp32-camera`, `ESPAsyncWebServer`, `WebSockets` | Smallest set that covers the kit and the paper |
| Device → server | HTTPS POST (telemetry, events, face crops), root CA pinned in firmware | Paper allows "HTTPS or MQTT over TLS"; one port (443) works through any tunnel or hotspot |
| Server → device | WSS (secure WebSocket) opened by the device | Commands must arrive within 3–10 s; no inbound port on the device |
| Backend | Python 3.12, FastAPI, SQLAlchemy 2, Alembic, PostgreSQL 16 | One language for web and ML; Postgres gives the unique `(device_id, seq)` constraint and row locks for the one-Ack rule |
| Dashboard | React + Vite + TypeScript, Tailwind, Recharts, i18next (English/Bangla), built to static files served by FastAPI | Live view, seat-zone drawing, 2D map and charts need a real client; still one deployable |
| Auth | argon2 password hashes, HttpOnly session cookie, role check as a FastAPI dependency on every route | Paper: hashed (bcrypt or argon2), roles checked by the server on every request |
| AI | InsightFace `buffalo_s` (ONNX) for face embeddings; MiniFASNet (Silent-Face) for anti-spoofing; YOLOv8n (ONNX) for people; own scikit-learn classifier on MediaPipe pose features for hand-raise; scikit-learn on CSI amplitude features for presence and zone | Pre-trained models are allowed for faces and people; the hand-raise and CSI classifiers are trained by the team |
| AI assistant | Claude API with tool calls that run the same query functions as the daily report | Answers come only from stored data and match the report |
| Phone alert | Telegram bot | Fastest of the three allowed options |
| Weather | Open-Meteo, fetched by the server every 15 minutes and cached | Free; the browser never calls the service |
| Hosting | Docker Compose on the team VM (app + Postgres), public HTTPS through Tailscale Funnel | The VM is already on Tailscale; Funnel gives a public HTTPS address with a valid certificate |
| Tests | PlatformIO `native` env with Unity for pure firmware logic; pytest for the backend | Timed rules (hysteresis, holds, lockouts, windows) are testable off the hardware |

**Spec:** `FastRobox_1.0_QuestionPaper_FOR_TEAMS.md` (the cleaned question paper) and `fastrobox_1_0_judges_checklist.md` (live checks, rows A1–C8, and the evidence check). Executors read both. The original PDF contains an injected "errata" block with different values; ignore it and use only the markdown.

## Global Constraints

- Team ID is 17, written `T17` as in the paper's `T07` examples. Keep it in one constant (`TEAM_ID`); judges choose the Wi-Fi network, Room ID and class times on the spot, so nothing else is hard-coded.
- Only the kit: one ESP32-S3-CAM, RC522, DHT11, 3-pin flame sensor, active buzzer, jumper wires (one bare end is the panic pad). No other parts.
- Levels pass in strict order; a later level must never remove an earlier level's behaviour. Run the full earlier-level check list before tagging each level.
- Serial Monitor: 115200 baud. Every line starts with one of `[STATUS] [CARD] [EVENT] [ERROR] [NET] [OTA] [CAM] [BLE] [CMD]`.
- `[STATUS]` line every second, exact order: `up= T= RH= HI= flame= door= state= trend= rssi= heap= psram= chip= fw=`; a field not yet built prints `-`.
- Telemetry every 5 ± 1 s as JSON with exactly the fields in paper section 6 (snake_case). `(device_id, seq)` is unique in the database. `seq` only increases, even across restarts. `ts` is the reading time in UTC seconds.
- Every event the device sends or receives is also printed as `[EVENT] <type> <detail>`.
- RGB LED: 2 Hz heartbeat (250 ms on / 250 ms off) in the state colour; when several states apply, the lower number in the Device Signals table wins.
- Local status page at the device IP and at `http://smartclass-t17.local`, files in LittleFS, updating at least every 2 seconds.
- Access point `SmartClass-T17`, 8-character password printed on serial at start-up, setup page at `192.168.4.1`.
- No password, key or token in firmware, website code or the repository. Secrets come from the setup page (device) and from `.env` on the VM (server); `.env` is git-ignored.
- Public HTTPS platform; passwords hashed with argon2; roles checked by the server on every request.
- Fire overrides every other door rule. Heat and the trend never change the door.
- Evidence per level: video, two-page write-up with the five headings, Git tag `level-NN`, uploaded as `T17_L<NN>_video.mp4` and `T17_L<NN>_writeup.pdf`. README kept current from Level 2.
- No blocking waits in firmware (`delay()` only inside a task's own `vTaskDelayUntil` loop).

## Review Focus

Failure modes the paper implies but no single level's checks exercise:

1. **Clock not set yet.** Readings taken before NTP syncs (first boot, offline boot) have no valid `ts`. Expected: store uptime with the record and convert once time is known; never send `ts=0`. Test in Task 2.4 and Task 12.3.
2. **`seq` after a power cut.** A cut between sending and saving `seq` could reuse a number. Expected: reserve `seq` in blocks of 100 in NVS and jump to the next block on boot. Test in Task 2.4.
3. **Judge's phone hotspot.** iPhone hotspots drop idle clients and may hide the SSID list. Expected: setup page also accepts a typed SSID; reconnect with increasing gaps without restarting. Test in Task 2.2.
4. **Memory with camera + TLS + BLE + AP together.** Expected: PSRAM used for frame buffers, TLS and BLE never started in the same instant, free heap stays above 40 KB in `health`. Soak test in Task 12.5.
5. **Status hold with a 5-second sensor.** The DHT11 is read every 5 s but a status must hold 3 s. Expected: the hold timer runs on wall time from the reading that first crossed the limit, and one noisy reading never flips the status. Test in Task 3.1.

---

## System Layout

```
ESP32-S3-CAM (firmware/)
  core 0: net task (Wi-Fi, HTTPS, WSS, NTP, BLE), camera task
  core 1: sensor tasks (flame 100 ms, RFID 200 ms, DHT 5 s), state task,
          signal task (LED + buzzer), serial task (status line + commands)
        |  HTTPS POST  /api/device/telemetry | /events | /checkin | /frames | /csi
        |  WSS         /api/device/ws   (commands down, acks up)
        v
VM: Docker Compose
  app (FastAPI)  --  postgres
    api/      device ingest, auth, rooms, attendance, settings, ota, weather, history
    ai/       face, spoof, people, handraise, csi, assistant
    web/      built React dashboard (static)
        ^  HTTPS + WebSocket /api/live
  Browsers and phones (dashboard), Telegram (Teacher alerts)
```

## File Structure

```
firmware/
  platformio.ini            board, partitions, lib deps, native test env
  partitions.csv            nvs, otadata, ota_0, ota_1, littlefs
  include/pins.h            every pin in one place (filled in Task 0.2)
  include/config.h          TEAM_ID, periods, limits' defaults, fw version
  src/main.cpp              creates tasks, boot reason, start-up flame check
  src/core/                 pure logic, no hardware calls (unit-tested on PC)
    status_machine.{h,cpp}  Safe/Warning/Danger + hold + hysteresis
    heat.{h,cpp}            heat index, trend from last 10 readings
    signals.{h,cpp}         priority table -> colour, blink, buzzer pattern
    seq_store.{h,cpp}       block-reserved sequence numbers
    record_log.{h,cpp}      offline ring log with CRC (Level 12)
  src/tasks/                sensors, state, signal, serial, net, camera, csi, ble
  src/net/                  wifi_setup, http_client, ws_client, ota, time
  src/web/                  local status page server + JSON endpoint
  data/                     LittleFS: status.html, setup.html, app.js, style.css
  test/                     Unity tests for src/core
server/
  app/main.py               FastAPI app, routers, static files
  app/db.py, models.py      SQLAlchemy models; alembic/ for migrations
  app/api/                  device.py auth.py rooms.py attendance.py settings.py
                            ota.py weather.py history.py camera.py model.py
  app/ai/                   face.py spoof.py people.py handraise.py csi.py assistant.py
  app/services/             alerts.py (Telegram), live.py (WebSocket hub),
                            attendance_rules.py, lockout.py, report.py
  training/                 handraise_train.py, csi_train.py, datasets/ (git-ignored images)
  tests/                    pytest
web/                        React app (pages per feature, i18n/en.json, i18n/bn.json)
deploy/                     docker-compose.yml, Dockerfile, .env.example
docs/                       wiring.md, architecture.md, writeups/L01..L12, README assets
README.md                   build and run (from Level 2)
```

Interfaces shared by all levels:

- Device → server: `POST /api/device/telemetry` (one record or a batch), `POST /api/device/events`; header `Authorization: Bearer <device token>`. Reply `200` stored, `409` duplicate `(device_id, seq)`.
- Server → device over WSS, JSON `{ "cmd": ..., "id": ..., ...}`: `settings`, `reset_fire`, `weather_alert`, `weather_clear`, `ota`, `camera_position`, `register_card`, `class_cards`, `night_mode`. The device answers `{ "ack": id }`.
- Firmware: `struct Readings`, `enum class Status { SAFE, WARNING, DANGER, FIRE }`, `enum class Door { LOCKED, UNLOCKED }`, and one `SystemState` guarded by a mutex; tasks talk through FreeRTOS queues `eventQueue` (to the net task) and `signalQueue` (to the signal task).
- Browser: `GET /api/live` WebSocket pushes `{type, room_id, data}` for every dashboard update.

---

## Stage 0 — Foundations (before Level 1)

### Task 0.1: Repository and toolchain

- [ ] Install PlatformIO Core; create `firmware/`, `server/`, `web/`, `deploy/`, `docs/`, `.gitignore` (`.env`, `.pio/`, `node_modules/`, datasets).
- [ ] `platformio.ini`: env `esp32s3cam` (Arduino, PSRAM on, 115200 monitor, custom `partitions.csv`) and env `native` (Unity).
- [ ] Verify: `pio run -e esp32s3cam` builds an empty sketch; `pio test -e native` runs zero tests. Commit.

### Task 0.2: Board and wiring (blocks everything else)

- [ ] Identify the exact ESP32-S3-CAM board and flash size; record the camera pins it already uses.
- [ ] Choose free pins for DHT11 data, flame DO, buzzer, RC522 (SCK, MOSI, MISO, SDA, RST), one touch-capable pin for the panic pad; confirm the onboard RGB LED pin and type (WS2812 or plain).
- [ ] Write them in `include/pins.h` and draw `docs/wiring.md` (needed for the Level 1 write-up).
- [ ] Verify: a throwaway sketch reads each sensor, blinks the LED, beeps, and captures one camera frame without pin clashes.

### Task 0.3: Risk spikes (throwaway, half a day)

- [ ] On-chip face detection from esp-dl runs on a camera frame and returns a box (Level 7).
- [ ] `esp_wifi_set_csi` delivers at least 20 packets per second while connected to a hotspot (Level 11). If the hotspot is quiet, the device pings the gateway at 25 Hz to generate traffic.
- [ ] Wi-Fi station + access point + NimBLE advertising + camera run together without a crash (Level 12).
- [ ] If any spike fails on Arduino core 2.0.x, switch the firmware to ESP-IDF with Arduino as a component before Level 1 and note it in the README.

### Task 0.4: VM and public address

- [ ] Get SSH access to the VM working (key or Tailscale SSH), remove the old service and images under `/opt/` after listing them.
- [ ] `deploy/docker-compose.yml` with `app` and `postgres`; enable Tailscale Funnel to the app on 443.
- [ ] Verify: `https://<vm>.ts.net/api/health` returns `{"ok":true}` from a phone on mobile data.

---

## Group 1 — Device Foundation (80 marks)

### Level 1: Dual-Core Tasks, Health and Watchdog (35) — checklist A1, A2

**Files:** `src/main.cpp`, `src/tasks/{sensors,state,signal,serial}.cpp`, `src/core/signals.*`, `test/test_signals.cpp`

**Produces:** `SystemState`, `eventQueue`, `signalQueue`, `Signals::current(const SystemState&) -> Pattern`, `printEvent(type, detail)`.

- [ ] **1.1 Tasks and timing.** Create tasks pinned to cores as in System Layout, each with `vTaskDelayUntil`: flame 100 ms, RFID 200 ms, DHT11 5 s, status line 1 s. Test (native): `Signals::current` returns green 2 Hz for Safe and magenta for a sensor error.
- [ ] **1.2 Card taps.** Report `[CARD] uid=<hex>` once per tap; a held card gives one line (re-arm only after the card has been absent for 2 polls). Check: 3 taps + one 5-second hold = exactly 4 lines.
- [ ] **1.3 Status line and commands.** `[STATUS]` in the fixed format with `chip=` from the internal temperature sensor, `heap=`, `psram=`; `HI=-` and `trend=-` for now. Commands `help status health tasks ver hang`, each reply starting `[CMD]`. `health` prints all nine values in the Serial Commands table; `tasks` lists each task with core and free stack (at least 3 tasks, both cores).
- [ ] **1.4 Watchdog and boot reason.** Task watchdog with a 5 s timeout on the main tasks; `hang` spins forever. On every start print `[EVENT] BOOT reason=` one of `POWER_ON SOFTWARE WATCHDOG CRASH BROWNOUT` mapped from `esp_reset_reason()`. Check: restart within 10 s, `reason=WATCHDOG`.
- [ ] **1.5 Start-up door rule.** Read the flame sensor before anything else: no flame → `LOCKED`, flame → `UNLOCKED`.
- [ ] **1.6 Sensor errors.** DHT11 failing for 2 reads (within 12 s) or RC522 version register unreadable (within 3 s) → `[ERROR] SENSOR ERROR: <name>`, magenta, everything else keeps running, clears on return.
- [ ] **1.7 Evidence.** Video (both wires unplugged 15 s), write-up with wiring and task diagrams, tag `level-01`.

### Level 2: Wi-Fi Setup and Secure Telemetry (45) — checklist A3, A4, A6

**Files:** `src/net/{wifi_setup,http_client,time}.cpp`, `src/core/seq_store.*`, `src/web/*`, `data/*`, `server/app/api/device.py`, `server/app/models.py`, `README.md`

**Produces:** NVS keys `ssid pass room_id token`; `SeqStore::next() -> uint32_t`; `POST /api/device/telemetry|events`; tables `devices`, `telemetry`, `events`; `devices.online`.

- [ ] **2.1 Setup mode.** No saved network → AP `SmartClass-T17`, random 8-character password printed on serial, blue light, setup page at `192.168.4.1` listing networks with signal strength plus password, Room ID and device token fields; a typed SSID is also accepted. Save to NVS.
- [ ] **2.2 Connect and recover.** `[NET]` lines for connecting, IP, mDNS name, drops and reconnects. Reconnect gaps 1, 2, 4, 8, 15 s without restarting. Wrong password or unreachable network → back to setup mode within 15 s with the error on the setup page. BOOT button held 5 s or `reset` clears the details. Level 1 tasks never stall during any of this.
- [ ] **2.3 Local status page.** Served from LittleFS at the IP and `smartclass-t17.local`; `GET /api/state` JSON polled every second; shows every field in Level 2 task 3, with connection state `LIVE` when the server is reachable.
- [ ] **2.4 Telemetry.** NTP for `ts` (Asia/Dhaka only for display). `SeqStore` reserves blocks of 100 in NVS. HTTPS POST every 5 s with the pinned root CA; card taps and boot sent at once as `CARD_TAP` and `BOOT`. Tests (native): after a simulated restart `next()` is greater than every number issued before; a record made before time sync carries the corrected `ts`.
- [ ] **2.5 Server ingest.** Token check, unique `(device_id, seq)`, `409` on replay with the row count unchanged (pytest). A background job marks a device offline after 30 s of silence and online on the next message.
- [ ] **2.6 Evidence.** README (build, flash, run the platform); three video items; architecture diagram; tag `level-02`.

---

## Group 2 — Classroom Safety (50 marks)

### Level 3: Heat, Fire, Panic and Fail-Safe Door (50) — checklist A5, A6, A7

**Files:** `src/core/{status_machine,heat}.*`, `src/tasks/state.cpp`, `test/test_status_machine.cpp`, `test/test_heat.cpp`

**Produces:** `StatusMachine::update(Readings, nowMs) -> Status`; `heatIndexC(tempC, rh) -> float`; `Trend trendOf(last10)`; NVS keys `fire_latched`, `teacher_tags`; events `STATUS FIRE FIRE_CLEARED SILENCED PANIC`.

- [ ] **3.1 Status machine.** Warning at ≥ 32 °C or ≥ 85 % RH; Danger at ≥ 40 °C; leave Danger at ≤ 38 °C, leave Warning at ≤ 30 °C and ≤ 80 % RH; a new status counts after holding 3 s. Tests: a ramp up and down gives exactly one Danger→Warning and one Warning→Safe; one spike reading changes nothing.
- [ ] **3.2 Bad readings.** Outside 0–50 °C or 20–90 % RH, or a failed read → `quality=bad`, last status kept, value not used. Test: unplugging during Warning never falls to Safe.
- [ ] **3.3 Heat index and trend.** NOAA Rothfusz formula converted to °C; trend from the slope of the last 10 readings (Rising above +0.3 °C, Falling below −0.3 °C across the window, else Stable). The trend output is not an input to signals or the door (enforced by the type: `Signals::current` does not take it).
- [ ] **3.4 Fire.** Flame continuous for 2 s → FIRE at once: continuous buzzer pattern, fast red, `FIRE – EVACUATE` on the local page, door `UNLOCKED`, event to the cloud within 3 s, latch saved in NVS before anything else. After a power cut the door is still `UNLOCKED`.
- [ ] **3.5 Silence and reset.** The kit key tag is the Teacher tag (list in NVS). While the flame is present nothing silences (`CANNOT SILENCE – FLAME PRESENT`); after it has gone only a Teacher tag silences (`SILENCED`); the door stays `UNLOCKED` until `reset_fire` arrives from a Teacher or Admin (serial fallback command until Level 4 provides the button).
- [ ] **3.6 Panic pad.** Calibrate the touch baseline at start-up and print it; touch for 2 s → PANIC (red/blue, panic buzzer for 10 s, local page, event within 3 s); under 1 s ignored.
- [ ] **3.7 Evidence.** Four video items (hair dryer, flick, gentle warming, unplug); safety design note; tag `level-03`. Round A dry run: rows A1–A7 in order with a stopwatch.

---

## Group 3 — Platform and Control (110 marks)

### Level 4: Secure Platform, Roles and Alerts (40) — checklist B1, B2

**Files:** `server/app/api/{auth,rooms}.py`, `server/app/services/{alerts,live}.py`, `web/src/pages/{Login,Room,Health,LoginHistory}.tsx`, `web/src/i18n/{en,bn}.json`, `src/net/ws_client.cpp`

**Produces:** `require_role(*roles)` dependency; `/api/live` hub `publish(room_id, type, data)`; tables `users`, `login_attempts`, `rooms`, `guardian_links`, `alerts_sent`; device WSS command channel.

- [ ] **4.1 Auth and roles.** argon2 hashes; session cookie; Admin, Teacher, Student, Guardian checked on every route and scoped by ownership (Teacher's rooms, Student's own records, Guardian's linked children). pytest: Student S2 opening S1's attendance gets 403 with no data; a Guardian opening another child gets 403.
- [ ] **4.2 Lockout and history.** 5 wrong passwords in a row lock the account for 30 s, including the correct password; every attempt is logged (time, user, result) and listed for the Admin. pytest with a fake clock.
- [ ] **4.3 Live room dashboard.** Ingest publishes to `/api/live`; the page updates within 3 s without reloading: status, temperature, humidity, heat index, trend, fire state, door, recent taps, online/offline; red FIRE/PANIC banner on every logged-in dashboard; Reset fire button (Teacher, Admin) sends `reset_fire` over WSS.
- [ ] **4.4 Device health panel.** Last seen, uptime, Wi-Fi signal, free heap, chip temperature, firmware version, last reset reason — same numbers as the `health` command (device sends them in telemetry plus the BOOT event).
- [ ] **4.5 Phone alerts.** Telegram message to the Teacher within 10 s for FIRE, PANIC and Heat Danger; store each in `alerts_sent` with its time.
- [ ] **4.6 Language.** Every label through i18next; an English/Bangla switch on every page.
- [ ] **4.7 Judges' accounts and evidence.** Seed script creates Admin, Teacher, S1, S2, Guardian→S1 with passwords from `.env`; print the page address list. Three video items; tag `level-04`.

### Level 5: Remote Settings, OTA and Rollback (30) — checklist B3, B4

**Files:** `server/app/api/{settings,ota}.py`, `web/src/pages/{Settings,Firmware}.tsx`, `src/net/ota.cpp`, `partitions.csv`

**Produces:** command `settings {warning_c, danger_c}`, command `ota {url, sha256, size, version}`; events `SETTINGS_APPLIED OTA_DONE OTA_REJECTED OTA_ROLLBACK`; table `settings_log`.

- [ ] **5.1 Settings.** Admin-only page; validate 0–50 °C and Warning below Danger with a clear message (45 °C Warning refused while Danger is 40); push over WSS; the device applies at once, saves to NVS, shows the limits on the local page within 10 s, and sends `SETTINGS_APPLIED`. Every change logged with who and when. `StatusMachine` takes limits as parameters; leaving stays 2 °C below.
- [ ] **5.2 OTA.** Admin uploads a `.bin`; the server stores it with its SHA-256; the device downloads over HTTPS into the other OTA partition, checks size, SHA-256 and the image header, restarts, and reports `OTA_DONE`. `[OTA]` lines for progress, check result and version. Settings, Wi-Fi details and the fire latch survive (they live in NVS).
- [ ] **5.3 Reject and roll back.** A damaged file fails the check → `OTA_REJECTED`, current version keeps running. New firmware boots as pending-verify; it calls `esp_ota_mark_app_valid_cancel_rollback()` only after reaching the server; if not reached within 60 s it rolls back and the old firmware sends `OTA_ROLLBACK`.
- [ ] **5.4 Test files.** Build script produces the three files: working (higher version), damaged (cut in half), faulty (wrong server address).
- [ ] **5.5 Evidence.** Four video items; partition layout in the write-up; tag `level-05`.

### Level 6: Weather, History and AI Assistant (40) — checklist B5, B6

**Files:** `server/app/api/{weather,history}.py`, `server/app/services/report.py`, `server/app/ai/assistant.py`, `web/src/pages/{Weather,History,Report,Assistant}.tsx`

**Produces:** `daily_report(room_id, date) -> Report`; `query_events(room_id, types, start, end)`; tables `weather_cache`, `notices`; commands `weather_alert {severity, text}` and `weather_clear`.

- [ ] **6.1 Weather.** Server fetches current weather and the 24-hour forecast every 15 minutes and caches it with its fetch time; if the service fails, serve the cache with its age and `Data stale`. The device gets a one-line summary for its local page.
- [ ] **6.2 Notice board.** Warnings with severity (Advisory, Warning, Danger), source and time window; derived from the forecast and labelled as such when no official alert exists. Admin can add and remove a TEST notice.
- [ ] **6.3 Danger notice.** Red banner on every logged-in dashboard within 10 s; within the same 10 s the device glows purple, beeps three times, prints `[EVENT] WEATHER_ALERT <severity>` and shows `WEATHER ALERT` with the text. Removing the notice clears both.
- [ ] **6.4 History and report.** Chart (temperature, humidity, heat index, up to 24 h); event history filtered by type and time; daily report with min, max, average, alarm counts per type and attendance totals (zero until Level 7); CSV export of all three. pytest: the CSV rows equal the page rows.
- [ ] **6.5 Assistant.** Teacher asks in English or Bangla; the model may only call `daily_report` and `query_events`; when a tool returns nothing the answer is "no data". pytest: a question about an empty date returns "no data"; a max-temperature question equals the report value.
- [ ] **6.6 Access and evidence.** Teachers see only their rooms; Students and Guardians get 403 on history, report and assistant. Three video items; tag `level-06`.

---

## Group 4 — Card and Face Access (80 marks)

### Level 7: Card and Face Check-In (50) — checklist B7, B8

**Files:** `src/tasks/camera.cpp`, `server/app/api/attendance.py`, `server/app/ai/face.py`, `server/app/services/attendance_rules.py`, `web/src/pages/{Enrol,Cards,Schedule,Attendance,ImageLog,Guardian}.tsx`

**Produces:** `POST /api/device/checkin` (multipart: `uid`, cropped JPEG) → `{result, reason, name}`; tables `students`, `face_embeddings`, `cards`, `classes`, `enrolments`, `attendance`, `image_log`, `notifications`; `mark_for(now, class_start) -> Present|Late|Absent`.

- [ ] **7.1 Enrolment.** Consent notice, at least 3 face images per student, embeddings stored, photos deleted in the same request. Register card: Admin presses the button, the next tap on the device links the UID.
- [ ] **7.2 On-device detection.** On a tap the camera task runs esp-dl face detection for up to 3 s; no face → `[EVENT] NO_FACE`, denial pattern, no server call. Face → crop, print `[CAM] FACE w=<px> h=<px>`, send the crop and UID only.
- [ ] **7.3 Decision.** Server checks in order: card known (`UNKNOWN CARD`), a class running now in this room (`NO CLASS NOW`), student in that class (`NOT IN THIS CLASS`), face matches the card's owner. Grant → door `UNLOCKED` for 5 s, solid green, one beep; tap-to-unlock under 5 s. Log each received image's pixel size (image not kept).
- [ ] **7.4 Attendance rules.** One record per student per class: Present within 10 minutes, Late after, Absent at 30 minutes for anyone enrolled and not checked in; a repeat check-in opens the door and adds nothing. Guardian notified within 60 s of Late or Absent. pytest with a fake clock: A Present, B Late at 11 min, D Absent at 31 min, Guardian sees only B and D.
- [ ] **7.5 Evidence.** Three video items; privacy note with measured match accuracy; tag `level-07`.

### Level 8: Spoofing, Lockout and Data Deletion (30) — checklist B9

**Files:** `server/app/ai/spoof.py`, `server/app/services/lockout.py`, `web/src/pages/{Alerts,BlockedCards,DeleteStudent}.tsx`

**Produces:** events `FACE_MISMATCH SPOOF CARD_BLOCKED ACK STUDENT_DELETED`; tables `snapshots`, `card_blocks`, `audit_log`; `POST /api/alerts/{id}/ack`.

- [ ] **8.1 Mismatch.** Face not the card owner → `FACE MISMATCH`, never card-only; private snapshot (Teacher and Admin; 403 for a Student by address); Teacher alert within 10 s.
- [ ] **8.2 Photo spoofing.** Liveness model on the crop plus a second crop 300 ms later (a screen shows no change in pose and has moiré); a phone photo never unlocks. Measure the false-accept rate on 20 phone-photo trials for the write-up.
- [ ] **8.3 Lockout.** Three mismatches for one card within 2 minutes block it for 5 minutes (`CARD BLOCKED`, even for the owner); Admin Unblock button. pytest with a fake clock.
- [ ] **8.4 One acknowledgment.** `UPDATE alerts SET acked_by, acked_at WHERE id=? AND acked_by IS NULL`; zero rows changed → "already acknowledged". pytest fires 10 parallel requests and expects exactly one success.
- [ ] **8.5 Delete student.** Admin delete removes embeddings, card link and snapshots in one transaction and writes the audit log. pytest: no row mentions the student afterwards.
- [ ] **8.6 Evidence.** Three video items; tag `level-08`. Round B dry run: rows B1–B9 in order.

---

## Group 5 — Classroom Vision and Wi-Fi Sensing (130 marks)

### Level 9: Occupancy, Live View, Night Watch and Tamper (50) — checklist C1, C2, C4, C6

**Files:** `server/app/api/camera.py`, `server/app/ai/people.py`, `server/app/services/camera_health.py`, `web/src/pages/{Camera,SeatMap,LiveView}.tsx`, `src/tasks/camera.cpp`

**Produces:** `POST /api/device/frames` (JPEG, about 3 per second at ROOM, 1 per 2 s at DOOR); events `OCCUPANCY OVER_CAPACITY INTRUDER CAMERA_FAULT CAMERA_OK`; tables `camera_positions`, `seat_zones`; commands `camera_position`, `night_mode`.

- [ ] **9.1 Positions.** Admin saves DOOR and ROOM with a reference snapshot each; Teacher selects one; `[CAM] position=`; a tap at ROOM is denied with `CAMERA NOT AT DOOR`.
- [ ] **9.2 Zones and count.** Admin draws at least two seat zones on the ROOM snapshot; every 2 s the server counts people; a zone is Occupied when a person's box centre is inside; changes hold 3 s and reach the dashboard within 6 s. Expected sequence in C1: 0/none, 1/zone 2, 2/zone 2 only.
- [ ] **9.3 Live view.** Frames relayed in memory to Teacher and Admin dashboards with boxes, at least 2 per second (target 3 to clear "20 updates in 10 s"); 403 for Students and Guardians; frames never written to disk.
- [ ] **9.4 Capacity and night watch.** Count above capacity for 3 s → `OVER CAPACITY` on dashboard and local page. Night mode: a person for 3 s → `INTRUDER`, one snapshot, Admin alert within 10 s.
- [ ] **9.5 Camera health.** Compare each frame with the reference: brightness and edge density for COVERED, feature-match or structural-similarity score for MOVED; require the fault for 2 consecutive checks and ignore regions inside person boxes so walkers cause no fault. Detect within 6 s, back to OK within 6 s; show the live scores. On a fault: Teacher alert with a snapshot within 10 s, cyan light, `CAMERA FAULT: <state>` on the local page, check-ins denied with `CAMERA FAULT`.
- [ ] **9.6 Evidence.** Three video items; measured counting accuracy and frame rate; tag `level-09`.

### Level 10: Own-Trained Hand-Raise Detector (40) — checklist C3

**Files:** `server/training/handraise_train.py`, `server/app/ai/handraise.py`, `server/app/api/model.py`, `web/src/pages/Model.tsx`

**Produces:** `handraise.joblib`, `metrics.json` (counts, held-out accuracy, confusion matrix, ms per prediction); event `HAND_RAISED {seat}`.

- [ ] **10.1 Dataset.** At ROOM, collect at least 100 raised and 100 not-raised seat-zone images (include the two decoy movements: touching the head, stretching arms sideways); hold out at least 20 % by session, never by random frame.
- [ ] **10.2 Training.** Features: pose keypoints from a pre-trained model (wrist above shoulder, elbow angle); classifier: the team's own logistic regression or small MLP in scikit-learn. Script in the repository writes the model and `metrics.json`.
- [ ] **10.3 Live.** A hand up for at least 2 s in a seat zone → `HAND RAISED – Seat <n>` on the Teacher dashboard within 5 s, once per raise (re-arm after the hand has been down 2 s); per-seat count for today; local page shows the last raise.
- [ ] **10.4 Evidence.** Model page video; tag `level-10`.

### Level 11: Wi-Fi CSI Presence and Localization (40) — checklist C4, C5

**Files:** `src/tasks/csi.cpp`, `server/training/csi_train.py`, `server/app/ai/csi.py`, `web/src/pages/RoomMap.tsx`

**Produces:** `POST /api/device/csi` (windows of amplitude features, 1 per second); event `PRESENCE EMPTY|OCCUPIED`; table `csi_samples`, `zone_predictions`; `csi` serial command.

- [ ] **11.1 Capture.** CSI callback on the connected access point at 20 packets per second or more (25 Hz gateway ping as the traffic source); on the device compute per-subcarrier amplitude mean and variance over a 1-second window and send them. `csi` prints the rate, subcarrier count and channel.
- [ ] **11.2 Dataset.** A collection page labels windows as zone 1–4 or empty; collect several sessions per label at the station on the day (the grid is marked there); hold out at least 20 % by session.
- [ ] **11.3 Models.** Presence: variance threshold plus classifier, state held 3 s, EMPTY/OCCUPIED within 5 s. Zone: random forest or SVM on the features; majority vote over the last 5 windows so the zone shown at the end of 10 s is stable.
- [ ] **11.4 Display.** Live 2D map with the predicted zone highlighted within 3 s of each prediction; local page shows presence and zone; Model page shows the CSI dataset, held-out accuracy and confusion matrix. The camera is not read anywhere in this code path (lens covered, cyan fault expected).
- [ ] **11.5 Evidence.** Video item; tag `level-11`.

---

## Group 6 — Resilience (50 marks)

### Level 12: Offline, Bluetooth and Power-Cut Recovery (50) — checklist C7, C8

**Files:** `src/core/record_log.*`, `src/tasks/{net,ble}.cpp`, `test/test_record_log.cpp`, `server/app/api/device.py`

**Produces:** `RecordLog::append(bytes)`, `RecordLog::peekOldest()`, `RecordLog::ack()`, `RecordLog::pending() -> uint32_t`; command `class_cards {uids, class_id}`; event `OFFLINE_ENTRY`.

- [ ] **12.1 Connection state.** `LIVE → OFFLINE → SYNCING → LIVE` on serial (`[NET] ...`), local page and dashboard; blue heartbeat while offline unless a higher-priority state applies.
- [ ] **12.2 Offline entry.** While online the server pushes the running class's card UIDs; the device keeps them in flash. Offline: those cards open the door by card alone and record `OFFLINE_ENTRY` with the tap time; no other card or tag opens it (the Teacher tag does not).
- [ ] **12.3 Record log.** Every reading and event appended to a LittleFS ring log sized for at least 30 minutes of telemetry (360 records plus events); each record has a length and CRC; a half-written record is discarded on boot; replay oldest first, deleting only after the server answers 200 or 409. Tests (native): truncated tail is dropped; replay order; records keep their original `ts`; a record written before time sync gets a corrected `ts`.
- [ ] **12.4 Stay reachable.** After 30 s without Wi-Fi also start the `SmartClass-T17` access point with the same password (`[NET] AP ON`); the local page shows the waiting-records count. BLE advertising as `SC-T17-<STATE>` (for example `SC-T17-OFFLINE`, `SC-T17-FIRE`) with `[BLE] advertising <name>`.
- [ ] **12.5 Restore and soak.** Door state, fire latch, settings, Room ID and Teacher tags restored after every restart; light, local page, door and dashboard agree within 20 s after two power cuts in a row. Soak: 30 minutes offline with camera, AP and BLE on; free heap stays above 40 KB.
- [ ] **12.6 Server side.** Batch ingest keeps original times; attendance from an `OFFLINE_ENTRY` uses the tap time and is flagged face-not-verified.
- [ ] **12.7 Evidence.** Two video items; tag `level-12`. Round C dry run: rows C1–C8 in order.

---

## Order of Work and Judging Calls

| Step | Work | Gate before moving on |
|---|---|---|
| 1 | Stage 0, then Levels 1–3 | Round A dry run passes twice in a row |
| 2 | Levels 4–6 | Rows B1–B6 pass; Levels 1–3 re-checked |
| 3 | Levels 7–8 | Rows B7–B9 pass with three real people |
| 4 | Levels 9–10 | Rows C1–C3 pass; datasets collected with the real camera position |
| 5 | Levels 11–12 | Rows C4–C8 pass; CSI data is re-collected at the station on the day |

Use the four judging calls as: Round A (1–3), Round B (4–8), Round C (9–12), and one spare for second attempts. Record each level's video as soon as its dry run passes, because evidence is due 30 minutes after judging closes.

## Open Items

- Exact ESP32-S3-CAM model, flash size and free pins (Task 0.2).
- SSH access to the VM (Task 0.4).
- Team ID spelling: the paper's examples use `T07`, so this plan uses `T17` everywhere (`T17-D1`, `SmartClass-T17`, `SC-T17-<STATE>`, `T17_L01_video.mp4`); the checklist header says `17`, so confirm with the organizers.
