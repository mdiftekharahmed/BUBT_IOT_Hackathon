# System architecture (Level 2)

```
                        ESP32-S3-CAM (device T17-D1)
        +--------------------------------------------------------+
        | 10 FreeRTOS tasks on both cores: the 7 from Level 1    |
        | + net / web / cloud (Level 2)                          |
        |  - flash (NVS): ssid, pass, room, token, seq_end       |
        |  - flash (LittleFS): status page, setup page           |
        |  - local web server: IP address, smartclass-t17.local  |
        +-------------------------+------------------------------+
                                  |  HTTPS, server certificate checked
                                  |  POST /api/device/telemetry (every 5 s)
                                  |  POST /api/device/events (BOOT, CARD_TAP)
                                  |  Authorization: Bearer <device token>
                                  v
   +----------------------+   public HTTPS    +---------------------------+
   |  VPS  (Caddy)        | ----------------> |  VM  (Docker Compose)     |
   |  Let's Encrypt cert  |  reverse proxy    |                           |
   |  coldchain.durnibar  |                   |  web (nginx) :80          |
   |  .tech               |                   |    |  /api  -> api        |
   +----------------------+                   |    v                      |
                                              |  api (FastAPI)            |
                                              |    - token check          |
                                              |    - unique (device,seq)  |
                                              |    - live hub (WebSocket) |
                                              |    |                      |
                                              |    v                      |
                                              |  postgres  <---- worker   |
                                              |   telemetry,  (offline    |
                                              |   events,      after 20 s,|
                                              |   devices,     Telegram   |
                                              |   users ...    alerts)    |
                                              +-------------+-------------+
                                                            |  live socket
                                                            v
                                               Dashboard in the browser
                                               (login, roles, English/Bangla)

   Later levels (not part of Level 2): AI services (face, people, hand-raise,
   CSI) and the weather service plug into the api / postgres in the same way.
```

**Data flow of one telemetry record:** sensors → `status` task builds a record every 5 s (`seq` from flash, `ts` from NTP) → `cloud` task sends it over HTTPS → api checks the token and stores it once → postgres → live hub pushes it to every open dashboard.

**Why replays are rejected:** the table has a unique key on `(device_id, seq)`. The api inserts with `ON CONFLICT DO NOTHING`; if no row was inserted it answers `409` and the row count stays the same.

**Why `seq` is never reused:** the device keeps a number in flash that is always 100 ahead of the last `seq` it handed out, so after any restart or power cut it starts from that number.
