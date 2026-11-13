# FastRobox 1.0 — Judges' Checklist
**12 levels $\cdot$ 500 marks $\cdot$ one copy per team**

| Field | Value | Field | Value |
| :--- | :--- | :--- | :--- |
| **Team ID** | 17 | **Team name** | |
| **Judge** | | **Station** | |

**How to use:** the team calls you (at most four calls) and names the levels to judge — consecutive levels, starting with its next level not yet passed. Use the round sheet for those levels and do only the rows tagged with them, in order; one action may count for several levels. Tick a line (Att. 1 or Att. 2) only when you see it yourself. A level passes when all its lines are ticked in the same attempt; if a level fails, stop the call there. Once or twice per call, ask the team to repeat one "Video must show" item live (spot check); if it fails, that level's attempt fails. A time up to $0.5\text{ s}$ over a limit: measure once more, the better result counts. Two failed attempts close that level and all later levels. 2nd-attempt pass = $70\%$ of the marks.

**Calls used:** $\square$ 1  $\square$ 2  $\square$ 3  $\square$ 4 &emsp; **Spot checks (level and item):** ___________________________

---

## Round A — Levels 1–3
* **Time:** about $10\text{ min}$
* **Bring:** Your phone with a $2.4\text{ GHz}$ hotspot; candle and lighter; fire extinguisher; stopwatch
* **Before you start:** The Serial Monitor is open on the team's laptop at 115200 baud. Turn on your phone's hotspot ($2.4\text{ GHz}$; on iPhone turn on "Maximize Compatibility"). Keep it on until the round ends. Keep the fire extinguisher within reach.

| Row | Do this | Lv | You must see | Att. 1 | Att. 2 |
| :---: | :--- | :---: | :--- | :---: | :---: |
| **A1** | Watch the light for $10\text{ seconds}$ while tapping the kit card 3 times, then hold the card on the reader for $5\text{ seconds}$. Ask the team to type `"health"`, then `"tasks"`. | **1** | The light keeps a steady $2\text{ Hz}$ rhythm; exactly 3 `"[CARD]"` lines for the taps and 1 for the held card | $\square$ | $\square$ |
| | | **1** | `"health"` gives every value listed for it in the Serial Commands table; `"tasks"` lists at least 3 tasks using both cores | $\square$ | $\square$ |
| **A2** | Ask the team to type `"hang"`. | **1** | The board restarts by itself within $10\text{ seconds}$, prints `"[EVENT] BOOT reason=WATCHDOG"` and starts with the door `"LOCKED"` | $\square$ | $\square$ |
| **A3** | Ask the team to type `"reset"`. On the team's laptop, join `"SmartClass-<TeamID>"` with the password shown on the Serial Monitor, open `192.168.4.1` and type in your hotspot's name and password, a Room ID you choose and the team's device token. Then connect the laptop to your hotspot and open the device's IP address and the mDNS address. | **2** | Blue light; the access point asks for the password; the device joins your hotspot and prints its IP address and mDNS name | $\square$ | $\square$ |
| | | **2** | Both addresses show the local status page with your Room ID and every field | $\square$ | $\square$ |
| **A4** | Tap the card. Ask the team to show the newest database records for $20\text{ seconds}$, then to send an old message again (same `"seq"`). | **2** | The local page updates within $2\text{ seconds}$ of the tap; records arrive every $5\pm1\text{ seconds}$ with every field, and the tap is stored with its UID | $\square$ | $\square$ |
| | | **2** | The resent message is rejected and the number of records does not change | $\square$ | $\square$ |
| **A5** | Hold the candle about $30\text{ cm}$ from the flame sensor for at least $3\text{ seconds}$ and, with the flame still there, tap the Teacher tag. Put the candle out; tap the student card, then the Teacher tag. | **3** | `"FIRE"`: continuous buzzer, fast red flashing, door `"UNLOCKED"`, stored in the database within $3\text{ seconds}$ | $\square$ | $\square$ |
| | | **3** | Nothing silences the buzzer while the flame is there; after it has gone, only the Teacher tag does, and the door stays `"UNLOCKED"` | $\square$ | $\square$ |
| **A6** | Unplug the board for $40\text{ seconds}$ and plug it back in. Then ask the team to send Reset fire. | **2** | The platform shows offline within $30\text{ seconds}$ and online again afterwards, with a `"BOOT"` event; the device reconnects with no setup | $\square$ | $\square$ |
| | | **3** | After the restart the door is still `"UNLOCKED"`; Reset fire makes the door `"LOCKED"` and the status Safe | $\square$ | $\square$ |
| **A7** | Hold the free metal end of the panic wire for $3\text{ seconds}$. | **3** | `"PANIC"` (light, buzzer, local page), stored in the database within $3\text{ seconds}$ | $\square$ | $\square$ |

* **After the round:** The team may set the device up on its own network again.

---

## Round B — Levels 4–8
* **Time:** about $15\text{ min}$
* **Bring:** Your phone (camera); two browsers (laptop and phone); stopwatch
* **Before you start:** Collect the judges' logins and the list of page addresses from the team. Check that the team has its three firmware files ready: a working one with a higher version number, a damaged copy, and a faulty one that cannot reach the server. Set-up by the team for Level 7: A and B are in the class held in this room; C is in another class; D is enrolled here but will not come; the card is registered to A; a Guardian is linked to B and D. Ask everyone not taking part in a row to stay out of the camera's view.

| Row | Do this | Lv | You must see | Att. 1 | Att. 2 |
| :---: | :--- | :---: | :--- | :---: | :---: |
| **B1** | Log in as two roles you choose. As Teacher, keep the dashboard open, tap the card, and ask the team to type `"health"`. | **4** | Each role sees only what it is allowed to see | $\square$ | $\square$ |
| | | **4** | The tap shows within $5\text{ seconds}$ without reloading, and the device health panel matches the `"health"` output | $\square$ | $\square$ |
| **B2** | Touch the panic pad for $3\text{ seconds}$ and ask the team to show the Teacher's phone. Then, logged in as Student S2, open S1's attendance page address. | **4** | The Teacher's dashboard shows a red `"PANIC"` banner within $3\text{ seconds}$, and the phone shows the message within $10\text{ seconds}$ | $\square$ | $\square$ |
| | | **4** | `"Not allowed"` (401 or 403) and no data | $\square$ | $\square$ |
| **B3** | As Admin, set a Warning limit you choose between $25^{\circ}\text{C}$ and $35^{\circ}\text{C}$. | **5** | The local status page shows the new limit within $10\text{ seconds}$ | $\square$ | $\square$ |
| **B4** | As Admin, upload the team's working firmware file with the higher version number, and wait. | **5** | Within $3\text{ minutes}$ the device restarts with the new version on the Serial Monitor, the local page and the dashboard health panel; the new limit and the Wi-Fi details are kept | $\square$ | $\square$ |
| **B5** | Open the dashboard in two browsers as two different roles. As Admin, add a Danger TEST notice; then remove it. | **6** | Both dashboards show the weather, the 24-hour forecast and the red banner within $10\text{ seconds}$; within $10\text{ seconds}$ the device glows purple, beeps three times and its local page shows `"WEATHER ALERT"` with the text | $\square$ | $\square$ |
| | | **6** | Removing the notice clears the banner and the device alert | $\square$ | $\square$ |
| **B6** | Open today's daily report. Then, as Teacher, ask the assistant one question you choose about today's stored data. | **6** | The report shows today's minimum, maximum and average and the number of alarms of each type, including the fire and panic from Round A | $\square$ | $\square$ |
| | | **6** | The assistant's answer matches the report | $\square$ | $\square$ |
| **B7** | As Admin, set the start of the class in this room to now. Tap the card with nobody facing the camera; then A taps it while facing the camera. | **7** | Denied with `"NO FACE"` within $4\text{ seconds}$, decided by the device itself (`"NO_FACE"` on the Serial Monitor) | $\square$ | $\square$ |
| | | **7** | A gets in within $5\text{ seconds}$ and is marked Present; the log of received images shows an image smaller than the full camera frame, of the size printed on the Serial Monitor | $\square$ | $\square$ |
| **B8** | As Admin, set the class start to $11\text{ minutes}$ ago and register the card to B; B checks in. | **7** | B is marked Late and the Guardian gets the Late notice within $60\text{ seconds}$ | $\square$ | $\square$ |
| **B9** | Register the card to A again. B taps it while facing the camera. Then take a photo of A with your phone and show it to the camera on your phone screen while tapping the card, for $10\text{ seconds}$. | **8** | Denied with `"FACE MISMATCH"`; the Teacher sees an alert with a snapshot within $10\text{ seconds}$ | $\square$ | $\square$ |
| | | **8** | The phone photo never unlocks the door | $\square$ | $\square$ |

* **After the round:** Ask the team to set the Warning limit back to $32^{\circ}\text{C}$.

---

## Round C — Levels 9–12
* **Time:** about $12\text{ min}$
* **Bring:** Two chairs; masking tape; your phone; stopwatch; paper for the trial orders
* **Before you start:** Place two chairs in seat zones 1 and 2 of the camera's `"ROOM"` view. Check that the $2\times2$ floor grid (about $1\text{ m}$ per zone, numbered 1–4) marked before the clock started is still in place. The device is `"LIVE"` on the team's own hotspot. Ask everyone not taking part in a row to stay out of the camera's view and outside the grid.

| Row | Do this | Lv | You must see | Att. 1 | Att. 2 |
| :---: | :--- | :---: | :--- | :---: | :---: |
| **C1** | As Admin, set the capacity to 1 and select `"ROOM"` (allow $1\text{ minute}$ to re-aim). Wait up to $6\text{ seconds}$ after each of these: view empty; one person sits in seat zone 2; a second person stands in view outside the seat zones. | **9** | The count and seat map are right each time: 0 / none; 1 / zone 2; 2 / zone 2 only | $\square$ | $\square$ |
| | | **9** | `"OVER CAPACITY"` appears on the dashboard and the local page only while 2 people are in view | $\square$ | $\square$ |
| **C2** | Open the live view as Teacher and count its updates for $10\text{ seconds}$ while walking in front of the camera. | **9** | At least 20 updates in $10\text{ seconds}$, with a box following you | $\square$ | $\square$ |
| **C3** | Write down a random order of 4 trials and run them with a team member in seat 1 or 2: two hands raised for $3\text{ seconds}$, and two other movements (touch head; stretch arms sideways). | **10** | At least 3 of the 4 trials are correct | $\square$ | $\square$ |
| | | **10** | Each detected raise appears within $5\text{ seconds}$, on the correct seat, once, and the per-seat count goes up by one | $\square$ | $\square$ |
| **C4** | Cover the lens with masking tape. Everyone stands outside the floor grid, at least $2\text{ m}$ away, and keeps still for $10\text{ seconds}$. | **9** | `"COVERED"` within $6\text{ seconds}$, with a Teacher alert, a snapshot and the cyan light | $\square$ | $\square$ |
| | | **11** | `"EMPTY"` within $5\text{ seconds}$, staying `"EMPTY"` for the whole $10\text{ seconds}$ | $\square$ | $\square$ |
| **C5** | Write down a random order of the 4 grid zones. A judge walks into the grid and stands in each zone in that order for $10\text{ seconds}$ (the zone shown at the end of the $10\text{ seconds}$ counts), then walks out to at least $3\text{ m}$ away. | **11** | `"OCCUPIED"` within $5\text{ seconds}$ of entering; at least 3 of the 4 zones are correct, and each time the dashboard map highlights the zone shown on the local page within $3\text{ seconds}$ | $\square$ | $\square$ |
| | | **11** | `"EMPTY"` within $5\text{ seconds}$ of leaving | $\square$ | $\square$ |
| **C6** | Remove the tape from the lens. | **9** | The camera is back to `"OK"` within $6\text{ seconds}$ | $\square$ | $\square$ |
| **C7** | As Admin, set the start of the class in this room to now, with A in it and the card registered to A. Ask the team to switch its hotspot off for $90\text{ seconds}$. After $40\text{ seconds}$, join `"SmartClass-<TeamID>"` on your phone and open `192.168.4.1`; ask the team to show a BLE scanner on its own phone. A taps the card; then tap the Teacher tag. | **12** | `"OFFLINE"` on the Serial Monitor and the local page, with the waiting-records count rising; `"SC-<TeamID>-OFFLINE"` in the BLE scanner | $\square$ | $\square$ |
| | | **12** | A is let in (solid green); the Teacher tag does not unlock the door | $\square$ | $\square$ |
| **C8** | At $90\text{ seconds}$, ask the team to switch the hotspot on. Wait $60\text{ seconds}$ and ask for the outage records. | **12** | `"SYNCING"`, then `"LIVE"`; every outage record arrives once with its original time; A is Present at the tap time, flagged face-not-verified | $\square$ | $\square$ |

---

## Evidence Check (after judging closes, for every level passed live)
For each level, also check: video at least 720p, team ID and level stated, at most $10\text{ minutes}$; write-up at most two pages with the five headings and screenshots of every surface the level uses; Git tag `level-NN` with commit hash; uploaded within $30\text{ minutes}$. Any missing item means the level scores zero.

### Level 1: Dual-Core Tasks, Health and Watchdog
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L1** | Video | The DHT11 data wire and the RC522 SDA wire unplugged for $15\text{ seconds}$: `"SENSOR ERROR"` for the RFID reader within $3\text{ seconds}$ and for the DHT11 within $12\text{ seconds}$, with magenta light and no freeze; both clear after reconnecting | $\square$ |
| | Write-up | A `"[STATUS]"` line every second in the fixed format | $\square$ |
| | Write-up | Read periods: DHT11 $5\text{ s}$, flame $100\text{ ms}$, RFID $200\text{ ms}$ | $\square$ |
| | Write-up | Flame at start-up gives door `"UNLOCKED"` | $\square$ |

### Level 2: Wi-Fi Setup and Secure Telemetry
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L2** | Video | The hotspot switched off for $20\text{ seconds}$: the device reconnects within $30\text{ seconds}$ without restarting (uptime keeps counting) | $\square$ |
| | Video | The BOOT button held for $5\text{ seconds}$: the access point returns; set up with a wrong hotspot password, the device is back in setup mode within $15\text{ seconds}$ and the page shows an error | $\square$ |
| | Video | The backend stopped for $20\text{ seconds}$: records resume within $30\text{ seconds}$ without resetting the device | $\square$ |
| | Write-up | HTTPS or MQTT over TLS with the server's certificate checked | $\square$ |
| | Write-up | No password, key or token in the repository; the README explains how to build and run the system | $\square$ |

### Level 3: Heat, Fire, Panic and Fail-Safe Door
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L3** | Video | Warm air from a hair dryer on the DHT11 until Danger, then cooling: yellow, then red with a beep every $2\text{ seconds}$; heat index and trend shown; no `"FIRE"` and no change to the door; exactly one Danger $\rightarrow$ Warning and one Warning $\rightarrow$ Safe change, with Safe only at $30^{\circ}\text{C}$ or below | $\square$ |
| | Video | A candle flicked past the flame sensor for about $1\text{ second}$, and a touch on the panic pad shorter than $1\text{ second}$: nothing happens | $\square$ |
| | Video | Gentle warming: the trend shows Rising with no change to the light, the buzzer or the door | $\square$ |
| | Video | The DHT11 unplugged during Warning: `"quality"` shows `"bad"` and the status does not fall to Safe | $\square$ |
| | Write-up | The touch-pad baseline is calibrated at start-up | $\square$ |

### Level 4: Secure Platform, Roles and Alerts
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L4** | Video | All four roles logged in, each seeing only what it is allowed to see | $\square$ |
| | Video | The dashboard switched to Bangla: all menus and labels in Bangla on two pages, then in English again | $\square$ |
| | Video | 5 wrong passwords in a row: the correct password is refused for $30\text{ seconds}$, then accepted; the login history lists the attempts with their times | $\square$ |
| | Write-up | Passwords stored hashed (bcrypt or argon2) | $\square$ |
| | Write-up | A Teacher-only page opened by a Student, and another child's page opened by the Guardian, are refused with no data | $\square$ |

### Level 5: Remote Settings, OTA and Rollback
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L5** | Video | The DHT11 warmed past the new limit: Warning (yellow) appears after $3\text{ seconds}$ at or above it | $\square$ |
| | Video | A Warning limit of $45^{\circ}\text{C}$ refused with a clear message | $\square$ |
| | Video | The board switched off and on: the changed limit is still shown and used | $\square$ |
| | Video | The damaged firmware file rejected with the device keeping its current version, and the faulty firmware installing, then returning by itself to the previous version with an `"OTA_ROLLBACK"` event | $\square$ |
| | Write-up | A Teacher cannot open the settings page; the change log shows who changed what and when | $\square$ |

### Level 6: Weather, History and AI Assistant
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L6** | Video | The weather service blocked: the board shows the last data with its age and `"Data stale"` | $\square$ |
| | Video | The chart with the heat peak, the event history filtered to `"FIRE"` and `"PANIC"` with their times, and its CSV export matching the page | $\square$ |
| | Video | An assistant question about a date with no data, answered with `"no data"` and nothing invented | $\square$ |
| | Write-up | Students and Guardians cannot open the history, report or assistant pages | $\square$ |
| | Write-up | The weather service key never reaches the browser | $\square$ |

### Level 7: Card and Face Check-In
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L7** | Video | A second check-in by A: the door opens but no record is added | $\square$ |
| | Video | The card registered to C (another class): `"NOT IN THIS CLASS"`; the card unregistered: `"UNKNOWN CARD"` | $\square$ |
| | Video | The class start set to $31\text{ minutes}$ ago: D is marked Absent and the Guardian is notified within $60\text{ seconds}$; A stays Present, B stays Late, and the Guardian sees only B and D | $\square$ |
| | Write-up | Consent notice, at least 3 face images per student, embeddings stored and photos deleted | $\square$ |
| | Write-up | A tap with no class running is denied with `"NO CLASS NOW"` | $\square$ |

### Level 8: Spoofing, Lockout and Data Deletion
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L8** | Video | B tapping A's card until the third mismatch: `"CARD BLOCKED"`, even for A; after the Admin unblocks it, A gets in | $\square$ |
| | Video | Two Teacher windows pressing Ack on one alert at the same moment: exactly one acknowledgment, naming who and when | $\square$ |
| | Video | Student C deleted by the Admin: no face data, card link or snapshot of C remains; the audit log shows who deleted C and when | $\square$ |
| | Write-up | A Student cannot open a snapshot's address | $\square$ |
| | Write-up | The card stays blocked for $5\text{ minutes}$ unless the Admin unblocks it | $\square$ |

### Level 9: Occupancy, Live View, Night Watch and Tamper
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L9** | Video | Night mode, with a person in front of the camera for $5\text{ seconds}$: an `"INTRUDER"` alert with a snapshot reaches the Admin within $10\text{ seconds}$, and the local page shows `"INTRUDER"` | $\square$ |
| | Video | The camera turned about $45^{\circ}$ away: `"MOVED"` within $6\text{ seconds}$, and `"OK"` within $6\text{ seconds}$ after turning it back | $\square$ |
| | Video | A card tapped while the camera is covered at `"DOOR"`: denied with `"CAMERA FAULT"`; a card tapped at `"ROOM"`: denied with `"CAMERA NOT AT DOOR"` | $\square$ |
| | Write-up | Counting and live-view frames are not stored (only the `"INTRUDER"` snapshot is kept); Students cannot open the live view | $\square$ |
| | Write-up | People walking through the view cause no camera fault; the camera-health scores are shown live | $\square$ |

### Level 10: Own-Trained Hand-Raise Detector
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L10** | Video | The Model page: at least 100 images per class, at least $20\%$ held out, held-out accuracy, confusion matrix and time per prediction | $\square$ |
| | Write-up | The training code is in the repository; no ready-made hand-raise model is used | $\square$ |

### Level 11: Wi-Fi CSI Presence and Localization
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L11** | Video | The `"csi"` command showing at least 20 CSI packets per second, and the Model page with samples per zone and empty, held-out accuracy and confusion matrix | $\square$ |
| | Write-up | The camera is not used for any decision; at least $20\%$ of the CSI samples are held out | $\square$ |

### Level 12: Offline, Bluetooth and Power-Cut Recovery
| Lv | Type | Requirement | Check |
| :---: | :--- | :--- | :---: |
| **L12** | Video | The board switched off for $10\text{ seconds}$ during an outage: it keeps working offline, the waiting-records count is kept, and the records from before the cut are later sent once with their original times | $\square$ |
| | Video | The device put in a non-default state (for example the fire state latched) and the power cut twice in a row: after each restart the light, local page, door and dashboard agree within $20\text{ seconds}$ | $\square$ |
| | Write-up | At least $30\text{ minutes}$ of telemetry can be buffered; a half-written record is discarded | $\square$ |

---

## Score Summary

| Level | Name | Max | 2nd att. | Att. 1 P/F | Att. 2 P/F | Evidence OK | Marks |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | Dual-Core Tasks, Health and Watchdog | 35 | 24.5 | | | $\square$ | |
| **2** | Wi-Fi Setup and Secure Telemetry | 45 | 31.5 | | | $\square$ | |
| **3** | Heat, Fire, Panic and Fail-Safe Door | 50 | 35 | | | $\square$ | |
| **4** | Secure Platform, Roles and Alerts | 40 | 28 | | | $\square$ | |
| **5** | Remote Settings, OTA and Rollback | 30 | 21 | | | $\square$ | |
| **6** | Weather, History and AI Assistant | 40 | 28 | | | $\square$ | |
| **7** | Card and Face Check-In | 50 | 35 | | | $\square$ | |
| **8** | Spoofing, Lockout and Data Deletion | 30 | 21 | | | $\square$ | |
| **9** | Occupancy, Live View, Night Watch and Tamper | 50 | 35 | | | $\square$ | |
| **10** | Own-Trained Hand-Raise Detector | 40 | 28 | | | $\square$ | |
| **11** | Wi-Fi CSI Presence and Localization | 40 | 28 | | | $\square$ | |
| **12** | Offline, Bluetooth and Power-Cut Recovery | 50 | 35 | | | $\square$ | |
| | **Total** | **500** | | | | | |

**Time last level passed (tiebreaker):** _____________________ &emsp; **Judge signature:** _____________________________