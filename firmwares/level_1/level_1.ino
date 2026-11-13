// ===== config (merged from config.h) =====
// ============================================================================
//  SmartClass — FastRobox 1.0 — build configuration
//  Level 1: Dual-Core Tasks, Health and Watchdog
// ============================================================================

#define FW_VERSION   "1.0.0"
#define TEAM_ID      "T17"            // <-- your team ID
#define DEVICE_ID    TEAM_ID "-D1"

// ---------------------------------------------------------------------------
//  PIN MAP  (Freenove-style ESP32-S3 WROOM CAM, DevKitC-1 header layout)
//  Camera uses 4-13,15-18; octal PSRAM uses 35-37; USB uses 19/20.
//  VERIFY against your own board's pinout before powering up.
//  Do NOT insert an SD card (38/39/40 are shared with the SD slot).
// ---------------------------------------------------------------------------
// RC522 RFID (SPI) — power from a 3V3 pin, never 5V
#define PIN_RFID_SCK    39
#define PIN_RFID_MISO   40
#define PIN_RFID_MOSI   41
#define PIN_RFID_SS     42   // "SDA" on the RC522 module
#define PIN_RFID_RST    47

// DHT11 data
#define PIN_DHT         1

// IR flame sensor DO (most LM393 modules pull DO LOW when they see a flame)
#define PIN_FLAME       2
#define FLAME_ACTIVE_LOW 1

// Active buzzer (+ leg)
#define PIN_BUZZER      21

// Touch panic pad (Level 3) — reserved now, a bare jumper end on this pin
#define PIN_TOUCH       14

// Onboard WS2812 RGB LED
#define PIN_RGB         48
#define RGB_BRIGHTNESS  40     // 0-255

// Helper "power pins": the board only has 2x 3V3 and 3x GND header pins,
// and no breadboard is allowed. So the flame module is powered from a GPIO
// driven HIGH, and the buzzer's (-) leg goes to a GPIO driven LOW.
// Set either to -1 if you wire that module to a real 3V3 / GND pin instead.
#define PIN_FLAME_PWR   38     // flame VCC  (driven HIGH, ~10 mA)
#define PIN_BUZZER_GND  3      // buzzer (-) (driven LOW,  ~25 mA)

// ---------------------------------------------------------------------------
//  Timing (from the question paper)
// ---------------------------------------------------------------------------
#define PERIOD_FLAME_MS     100
#define PERIOD_RFID_MS      200
#define PERIOD_DHT_S        5
#define PERIOD_STATUS_MS    1000
#define PERIOD_LED_MS        50
#define PERIOD_CONTROL_MS   100
#define PERIOD_SERIAL_MS    20
#define PERIOD_TOUCH_MS      20

#define WDT_TIMEOUT_MS      5000    // frozen task -> restart well inside 10 s

#define RFID_FAILS_FOR_ERROR   3    // 3 x 200 ms = 0.6 s  (limit 3 s)
#define RFID_OKS_FOR_CLEAR     3
#define DHT_FAILS_FOR_ERROR    2    // worst case ~10 s     (limit 12 s)
#define CARD_GONE_MS         600    // card must be away this long to count a new tap
#define FIRE_HOLD_MS        2000
#define PANIC_HOLD_MS       2000
#define PANIC_DURATION_MS  10000
#define MAX_TEACHER_TAGS       8

// ============================================================================
//  SmartClass — FastRobox 1.0 — Level 1
//  Dual-core FreeRTOS tasks, health, serial commands, sensor errors, watchdog
//  Board: ESP32-S3-CAM   Library: MFRC522 (GithubCommunity)
// ============================================================================
#include <Arduino.h>
#include <ctype.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <stdarg.h>
#include "esp_arduino_version.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "src/core/heat.h"
#include "src/core/status_machine.h"

using fastrobox::HeatStatus;
using fastrobox::Readings;
using fastrobox::StatusMachine;
using fastrobox::Trend;
using fastrobox::TrendWindow;

// ============================================================================
//  Shared device state (guarded by a spinlock — copy in/out only)
// ============================================================================
struct DeviceState {
  float tempC = NAN;
  float rh = NAN;
  float heatIndexC = NAN;
  bool  flame = false;
  bool  doorUnlocked = false;
  bool  dhtError = false;
  bool  rfidError = false;
  bool  qualityGood = false;
  bool  fireLatched = false;
  bool  fireSilenced = false;
  bool  fireActive = false;
  bool  panicActive = false;
  HeatStatus heatStatus = HeatStatus::Safe;
  Trend trend = Trend::Stable;
  uint32_t panicAtMs = 0;
  char  lastUid[32] = "";
};

static DeviceState       g;
static portMUX_TYPE      gMux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t gLogMtx;
static SemaphoreHandle_t gNvsMtx;

static const char*       gResetReason = "POWER_ON";
static volatile bool     gHangRequested = false;
static volatile float    gChipTemp = NAN;    // refreshed every second
static float             gTouchBaseline = 0;
static volatile bool     gTeacherEnrollRequested = false;
static volatile uint32_t gTeacherEnrollUntilMs = 0;

// Survives a software / watchdog reset (not a power cut)
RTC_NOINIT_ATTR static uint32_t gWdtFiredMagic;
#define WDT_MAGIC 0x5AFEC0DEu

static DeviceState snapshot() {
  DeviceState s;
  portENTER_CRITICAL(&gMux); s = g; portEXIT_CRITICAL(&gMux);
  return s;
}
#define STATE_SET(stmt) do { portENTER_CRITICAL(&gMux); stmt; portEXIT_CRITICAL(&gMux); } while (0)

static const char* visibleStatus(const DeviceState& s) {
  return s.fireLatched ? "FIRE" : fastrobox::statusName(s.heatStatus);
}

// ============================================================================
//  Logging — one whole line at a time, never interleaved between tasks
// ============================================================================
static void logLine(const char* fmt, ...) {
  char buf[320];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  bool locked = gLogMtx && xSemaphoreTake(gLogMtx, pdMS_TO_TICKS(50)) == pdTRUE;
  Serial.println(buf);
  if (locked) xSemaphoreGive(gLogMtx);
}

static bool storeFireLatch(bool latched) {
  if (!gNvsMtx || xSemaphoreTake(gNvsMtx, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
  Preferences prefs;
  bool ok = prefs.begin("fastrobox", false);
  if (ok) {
    ok = prefs.putBool("fire_latched", latched) == 1;
    prefs.end();
  }
  xSemaphoreGive(gNvsMtx);
  return ok;
}

static bool loadFireLatch(bool& latched) {
  if (!gNvsMtx || xSemaphoreTake(gNvsMtx, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
  Preferences prefs;
  bool ok = prefs.begin("fastrobox", true);
  if (ok) {
    latched = prefs.getBool("fire_latched", false);
    prefs.end();
  }
  xSemaphoreGive(gNvsMtx);
  return ok;
}

static bool normalizeUid(const char* input, char* output, size_t outputSize) {
  size_t digits = 0;
  if (outputSize < 3) return false;
  for (const char* p = input; *p; ++p) {
    if (*p == ':' || *p == '-' || *p == ' ' || *p == '\t') continue;
    char c = (char)toupper((unsigned char)*p);
    if (!isxdigit((unsigned char)c) || digits >= 20) return false;
    if (digits > 0 && digits % 2 == 0) {
      if (digits / 2 * 3 + 1 >= outputSize) return false;
      output[digits / 2 * 3 - 1] = ':';
    }
    size_t pos = digits / 2 * 3 + (digits % 2);
    if (pos + 1 >= outputSize) return false;
    output[pos] = c;
    ++digits;
  }
  if (digits < 8 || digits % 2 != 0) return false;
  size_t length = digits + digits / 2 - 1;
  output[length] = 0;
  return true;
}

static bool teacherTagExists(const char* uid, bool& found) {
  found = false;
  if (!gNvsMtx || xSemaphoreTake(gNvsMtx, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
  Preferences prefs;
  bool ok = prefs.begin("fastrobox", true);
  if (ok) {
    for (uint8_t i = 0; i < MAX_TEACHER_TAGS; ++i) {
      char key[12];
      snprintf(key, sizeof key, "teacher%u", (unsigned)i);
      if (prefs.getString(key, "") == uid) {
        found = true;
        break;
      }
    }
    prefs.end();
  }
  xSemaphoreGive(gNvsMtx);
  return ok;
}

static bool addTeacherTag(const char* uid) {
  if (!gNvsMtx || xSemaphoreTake(gNvsMtx, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
  Preferences prefs;
  bool added = false;
  if (prefs.begin("fastrobox", false)) {
    for (uint8_t i = 0; i < MAX_TEACHER_TAGS; ++i) {
      char key[12];
      snprintf(key, sizeof key, "teacher%u", (unsigned)i);
      String existing = prefs.getString(key, "");
      if (existing == uid) {
        added = true;
        break;
      }
      if (!existing.length()) {
        added = prefs.putString(key, uid) > 0;
        break;
      }
    }
    prefs.end();
  }
  xSemaphoreGive(gNvsMtx);
  return added;
}

static bool resetFireLatch() {
  DeviceState s = snapshot();
  if (!s.fireLatched) {
    logLine("[CMD] reset_fire: no fire latch is active");
    return true;
  }
  if (s.flame) {
    logLine("[ERROR] CANNOT RESET FIRE - FLAME PRESENT");
    return false;
  }
  if (!storeFireLatch(false)) {
    logLine("[ERROR] FIRE LATCH RESET NOT SAVED");
    return false;
  }
  STATE_SET({ g.fireLatched = false; g.fireSilenced = false; g.doorUnlocked = false; });
  logLine("[CMD] fire latch reset");
  return true;
}

// ============================================================================
//  Task registry (for the "tasks" command)
// ============================================================================
struct TaskInfo { const char* name; TaskHandle_t handle; BaseType_t core; uint32_t periodMs; };
static TaskInfo gTasks[10];
static int      gTaskCount = 0;

static void startTask(TaskFunction_t fn, const char* name, uint32_t stack,
                      UBaseType_t prio, BaseType_t core, uint32_t periodMs) {
  TaskInfo& t = gTasks[gTaskCount++];
  t.name = name; t.core = core; t.periodMs = periodMs; t.handle = nullptr;
  xTaskCreatePinnedToCore(fn, name, stack, nullptr, prio, &t.handle, core);
}

// ============================================================================
//  Watchdog
// ============================================================================
// Called by ESP-IDF from the task-watchdog interrupt, just before it resets.
extern "C" void IRAM_ATTR esp_task_wdt_isr_user_handler(void) { gWdtFiredMagic = WDT_MAGIC; }

static void watchdogInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WDT_TIMEOUT_MS;
  cfg.idle_core_mask = 0;          // watch our tasks, not the idle tasks
  cfg.trigger_panic = true;        // panic -> restart
  // Arduino 3.x already starts the TWDT: reconfigure it; init only if it is not running
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
#endif
}
static inline void wdtAdd()  { esp_task_wdt_add(NULL); }
static inline void wdtFeed() { esp_task_wdt_reset(); }

static const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_DEEPSLEEP: return "SOFTWARE";
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT:       return "WATCHDOG";
    case ESP_RST_PANIC:     return "CRASH";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    default:                return "POWER_ON";   // POWERON, EXT (EN button), USB, UNKNOWN
  }
}

// ============================================================================
//  RGB LED
// ============================================================================
static void rgbWrite(uint32_t c) {
  uint8_t r = ((c >> 16) & 0xFF) * RGB_BRIGHTNESS / 255;
  uint8_t gr = ((c >> 8) & 0xFF) * RGB_BRIGHTNESS / 255;
  uint8_t b = (c & 0xFF) * RGB_BRIGHTNESS / 255;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  rgbLedWrite(PIN_RGB, r, gr, b);
#else
  neopixelWrite(PIN_RGB, r, gr, b);
#endif
}

// Device Signals table colours
#define COLOR_RED     0xFF0000u
#define COLOR_GREEN   0x00FF00u
#define COLOR_BLUE    0x0000FFu
#define COLOR_MAGENTA 0xFF00FFu
#define COLOR_CYAN    0x00FFFFu
#define COLOR_PURPLE  0x8000FFu
#define COLOR_YELLOW  0xFFC000u

// ============================================================================
//  Flame sensor
// ============================================================================
static bool readFlameRaw() {
  int v = digitalRead(PIN_FLAME);
  return FLAME_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// ============================================================================
//  DHT11 driver (own implementation, timing-critical part ~5 ms)
// ============================================================================
static portMUX_TYPE dhtMux = portMUX_INITIALIZER_UNLOCKED;

static bool IRAM_ATTR dhtWaitLevel(int level, uint32_t timeoutUs) {
  int64_t start = esp_timer_get_time();
  while (gpio_get_level((gpio_num_t)PIN_DHT) != level) {
    if (esp_timer_get_time() - start > timeoutUs) return false;
  }
  return true;
}

static bool dht11Read(float& tempC, float& rh) {
  uint8_t d[5] = {0, 0, 0, 0, 0};
  // Start signal: hold the line LOW for 20 ms (task sleeps, no busy wait)
  pinMode(PIN_DHT, OUTPUT);
  digitalWrite(PIN_DHT, LOW);
  vTaskDelay(pdMS_TO_TICKS(20));
  pinMode(PIN_DHT, INPUT_PULLUP);              // release; sensor answers in 20-40 us

  bool ok = true;
  portENTER_CRITICAL(&dhtMux);
  if (!dhtWaitLevel(0, 200)) ok = false;       // sensor pulls LOW
  else if (!dhtWaitLevel(1, 120)) ok = false;  // 80 us LOW
  else if (!dhtWaitLevel(0, 120)) ok = false;  // 80 us HIGH
  for (int i = 0; ok && i < 40; i++) {
    if (!dhtWaitLevel(1, 100)) { ok = false; break; }   // 50 us LOW gap
    int64_t t0 = esp_timer_get_time();
    if (!dhtWaitLevel(0, 120)) { ok = false; break; }   // 26 us = 0, 70 us = 1
    if (esp_timer_get_time() - t0 > 45) d[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
  }
  portEXIT_CRITICAL(&dhtMux);

  if (!ok) return false;
  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;   // checksum
  if (d[0] == 0 && d[2] == 0) return false;                          // all-zero frame
  rh = d[0] + d[1] * 0.1f;
  tempC = (d[2] & 0x7F) + d[3] * 0.1f;
  if (d[3] & 0x80) tempC = -tempC;
  return true;
}

// ============================================================================
//  RC522
// ============================================================================
static MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);

// Real liveness test: write a random byte into the RC522 FIFO and read it back.
// A loose wire / floating SPI line cannot pass this.
static bool rfidAlive() {
  uint8_t v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  if (v == 0x00 || v == 0xFF) return false;
  uint8_t probe = (uint8_t)random(1, 255);
  rfid.PCD_WriteRegister(MFRC522::FIFOLevelReg, 0x80);       // flush FIFO
  rfid.PCD_WriteRegister(MFRC522::FIFODataReg, probe);
  uint8_t level = rfid.PCD_ReadRegister(MFRC522::FIFOLevelReg) & 0x7F;
  uint8_t back  = rfid.PCD_ReadRegister(MFRC522::FIFODataReg);
  rfid.PCD_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
  return level == 1 && back == probe;
}

// Detects a card even if it was halted (WUPA), so a held card is seen every poll.
static bool rfidReadCard(char* out, size_t n) {
  byte atqa[2]; byte sz = sizeof(atqa);
  MFRC522::StatusCode st = rfid.PICC_WakeupA(atqa, &sz);
  if (st != MFRC522::STATUS_OK && st != MFRC522::STATUS_COLLISION) return false;
  if (!rfid.PICC_ReadCardSerial()) return false;
  size_t p = 0; out[0] = 0;
  for (byte i = 0; i < rfid.uid.size && p + 4 < n; i++)
    p += snprintf(out + p, n - p, i ? ":%02X" : "%02X", rfid.uid.uidByte[i]);
  rfid.PICC_HaltA();
  return true;
}

// ============================================================================
// Status line (fixed format)
// ============================================================================
static void printStatusLine() {
  DeviceState s = snapshot();
  char t[12], rh[12], hi[12], chip[12];
  if (isnan(s.tempC)) strcpy(t, "-");  else snprintf(t, sizeof t, "%.1fC", s.tempC);
  if (isnan(s.rh))    strcpy(rh, "-"); else snprintf(rh, sizeof rh, "%.0f%%", s.rh);
  if (isnan(s.heatIndexC)) strcpy(hi, "-"); else snprintf(hi, sizeof hi, "%.1fC", s.heatIndexC);
  float ct = gChipTemp;
  if (isnan(ct))      strcpy(chip, "-"); else snprintf(chip, sizeof chip, "%.1fC", ct);
  logLine("[STATUS] up=%lus T=%s RH=%s HI=%s flame=%d door=%s state=%s trend=%s rssi=- "
          "heap=%luk psram=%luk chip=%s fw=%s quality=%s",
          (unsigned long)(esp_timer_get_time() / 1000000ULL), t, rh, hi, s.flame ? 1 : 0,
          s.doorUnlocked ? "UNLOCKED" : "LOCKED", visibleStatus(s), fastrobox::trendName(s.trend),
          (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getFreePsram() / 1024),
          chip, FW_VERSION, s.qualityGood ? "good" : "bad");
}

// ============================================================================
//  TASKS
// ============================================================================

// ---- Core 1: flame, every 100 ms ------------------------------------------
static void flameTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    bool f = readFlameRaw();
    STATE_SET(g.flame = f);
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_FLAME_MS));
  }
}

// ---- Core 1: RFID, every 200 ms -------------------------------------------
static void rfidTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  uint8_t fails = 0, oks = 0;
  bool err = false, onReader = false;
  char cur[32] = "";
  uint32_t lastSeen = 0;

  for (;;) {
    wdtFeed();
    if (rfidAlive()) {
      fails = 0;
      if (err) {
        if (++oks >= RFID_OKS_FOR_CLEAR) {
          rfid.PCD_Init();                       // restore antenna/timer settings
          err = false; oks = 0;
          STATE_SET(g.rfidError = false);
          logLine("[ERROR] SENSOR CLEARED: RFID");
        }
      } else {
        char uid[32];
        if (rfidReadCard(uid, sizeof uid)) {
          lastSeen = millis();
          if (!onReader || strcmp(uid, cur) != 0) {     // new tap
            onReader = true;
            strlcpy(cur, uid, sizeof cur);
            handleCardTap(uid);
          }
        } else if (onReader && millis() - lastSeen > CARD_GONE_MS) {
          onReader = false;                      // card removed -> next tap counts
        }
      }
    } else {
      oks = 0;
      if (!err && ++fails >= RFID_FAILS_FOR_ERROR) {
        err = true; onReader = false;
        STATE_SET(g.rfidError = true);
        logLine("[ERROR] SENSOR ERROR: RFID");
      }
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_RFID_MS));
  }
}

// ---- Core 1: DHT11, every 5 s (wakes every 1 s to feed the watchdog) -------
static void dhtTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  int tick = PERIOD_DHT_S - 2;           // first read ~2 s after boot
  uint8_t fails = 0;
  bool err = false;
  TrendWindow temperatures;
  for (;;) {
    wdtFeed();
    if (++tick >= PERIOD_DHT_S) {
      tick = 0;
      float t, h;
      if (dht11Read(t, h) && t >= 0.0f && t <= 50.0f && h >= 20.0f && h <= 90.0f) {
        fails = 0;
        temperatures.add(t);
        STATE_SET({
          g.tempC = t;
          g.rh = h;
          g.heatIndexC = fastrobox::heatIndexC(t, h);
          g.trend = temperatures.trend();
          g.qualityGood = true;
        });
        if (err) {
          err = false;
          STATE_SET(g.dhtError = false);
          logLine("[ERROR] SENSOR CLEARED: DHT11");
        }
      } else {
        if (fails < 255) fails++;
        STATE_SET({
          g.tempC = NAN;
          g.rh = NAN;
          g.heatIndexC = NAN;
          g.qualityGood = false;
        });
        if (!err && fails >= DHT_FAILS_FOR_ERROR) {
          err = true;
          STATE_SET(g.dhtError = true);
          logLine("[ERROR] SENSOR ERROR: DHT11");
        }
      }
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
  }
}

// ---- Core 1: control ("main task") -----------------------------------------
// Decides the device state and the light colour. "hang" freezes this task.
static void controlTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  StatusMachine statusMachine;
  bool flameTiming = false;
  uint32_t flameStartedAt = 0;
  for (;;) {
    if (gHangRequested) {
      logLine("[CMD] main task frozen - watchdog must restart the board within 10 s");
      volatile uint32_t spin = 0;
      for (;;) { spin++; }                      // stuck, never feeds the watchdog
    }
    DeviceState before = snapshot();
    char oldStatus[12];
    strlcpy(oldStatus, visibleStatus(before), sizeof oldStatus);
    uint32_t now = millis();
    if (before.flame) {
      if (!flameTiming) {
        flameTiming = true;
        flameStartedAt = now;
      } else if (!before.fireActive &&
                 static_cast<uint32_t>(now - flameStartedAt) >= FIRE_HOLD_MS) {
        if (!before.fireLatched && !storeFireLatch(true))
          logLine("[ERROR] FIRE LATCH SAVE FAILED");
        STATE_SET({
          g.fireLatched = true;
          g.fireSilenced = false;
          g.fireActive = true;
          g.doorUnlocked = true;
        });
        logLine("[EVENT] FIRE flame held for 2s");
      }
    } else {
      flameTiming = false;
      if (before.fireActive) {
        STATE_SET(g.fireActive = false);
        logLine("[EVENT] FIRE_CLEARED");
      }
    }

    before = snapshot();
    HeatStatus next = statusMachine.update(
        {before.tempC, before.rh, before.qualityGood}, now);
    STATE_SET(g.heatStatus = next);
    if (before.panicActive &&
        static_cast<uint32_t>(now - before.panicAtMs) >= PANIC_DURATION_MS)
      STATE_SET(g.panicActive = false);
    DeviceState after = snapshot();
    if (strcmp(oldStatus, visibleStatus(after)) != 0)
      logLine("[EVENT] STATUS %s->%s", oldStatus, visibleStatus(after));
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_CONTROL_MS));
  }
}

// ---- Core 0: light and buzzer signals -------------------------------------
static void ledTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    DeviceState s = snapshot();
    uint32_t now = millis();
    uint32_t color = COLOR_GREEN;
    uint32_t phaseMs = 500;
    bool ledOn = (now % phaseMs) < phaseMs / 2;
    bool buzzerOn = false;
    bool panicActive = s.panicActive &&
        static_cast<uint32_t>(now - s.panicAtMs) < PANIC_DURATION_MS;

    if (s.fireLatched) {
      color = COLOR_RED;
      phaseMs = 400;
      ledOn = (now % phaseMs) < 200;
      buzzerOn = !s.fireSilenced && (now % phaseMs) < 200;
    } else if (panicActive) {
      uint32_t panicPhase = now % 400;
      color = (panicPhase < 200) ? COLOR_RED : COLOR_BLUE;
      ledOn = (panicPhase % 200) < 100;
      buzzerOn = (panicPhase % 200) < 100;
    } else if (s.heatStatus == HeatStatus::Danger) {
      color = COLOR_RED;
      buzzerOn = (now % 2000) < 100;
    } else if (s.dhtError || s.rfidError) {
      color = COLOR_MAGENTA;
    } else if (s.heatStatus == HeatStatus::Warning) {
      color = COLOR_YELLOW;
    }

    rgbWrite(ledOn ? color : 0);
    digitalWrite(PIN_BUZZER, buzzerOn ? HIGH : LOW);
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_LED_MS));
  }
}

static void touchTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  bool touching = false;
  bool panicRaised = false;
  uint32_t touchStartedAt = 0;
  for (;;) {
    uint32_t now = millis();
    uint16_t value = touchRead(PIN_TOUCH);
    bool active = gTouchBaseline > 0 && value < gTouchBaseline * 0.70f;
    if (active) {
      if (!touching) {
        touching = true;
        panicRaised = false;
        touchStartedAt = now;
      } else if (!panicRaised &&
                 static_cast<uint32_t>(now - touchStartedAt) >= PANIC_HOLD_MS) {
        STATE_SET({ g.panicActive = true; g.panicAtMs = now; });
        panicRaised = true;
        logLine("[EVENT] PANIC touch held for 2s");
      }
    } else {
      touching = false;
      panicRaised = false;
    }
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_TOUCH_MS));
  }
}

static bool consumeTeacherEnrollRequest() {
  bool requested;
  portENTER_CRITICAL(&gMux);
  requested = gTeacherEnrollRequested &&
      static_cast<int32_t>(gTeacherEnrollUntilMs - millis()) > 0;
  gTeacherEnrollRequested = false;
  gTeacherEnrollUntilMs = 0;
  portEXIT_CRITICAL(&gMux);
  return requested;
}

static void handleCardTap(const char* uid) {
  STATE_SET(strlcpy(g.lastUid, uid, sizeof g.lastUid));
  logLine("[CARD] uid=%s", uid);
  if (consumeTeacherEnrollRequest()) {
    if (addTeacherTag(uid)) logLine("[EVENT] TEACHER_TAG_ADDED uid=%s", uid);
    else logLine("[ERROR] TEACHER TAG LIST FULL OR NVS WRITE FAILED");
    return;
  }
  bool teacher = false;
  if (!teacherTagExists(uid, teacher)) {
    logLine("[ERROR] TEACHER TAG STORAGE READ FAILED");
    return;
  }
  if (!teacher) return;

  DeviceState s = snapshot();
  if (!s.fireLatched) return;
  if (s.flame) {
    logLine("[ERROR] CANNOT SILENCE - FLAME PRESENT");
  } else if (!s.fireSilenced) {
    STATE_SET(g.fireSilenced = true);
    logLine("[EVENT] SILENCED");
  }
}

// ---- Core 0: status line every second -------------------------------------
static void statusTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    gChipTemp = temperatureRead();
    printStatusLine();
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_STATUS_MS));
  }
}

// ---- Core 0: serial commands -------------------------------------------------
static void cmdHelp() {
  logLine("[CMD] commands:");
  logLine("[CMD]   help    - this list");
  logLine("[CMD]   status  - print one status line now");
  logLine("[CMD]   health  - chip temp, heap, PSRAM, flash, CPU, RSSI, uptime, reset reason, fw");
  logLine("[CMD]   tasks   - each task with its core and free stack");
  logLine("[CMD]   ver     - firmware version and build date");
  logLine("[CMD]   hang    - freeze the main task (watchdog test)");
  logLine("[CMD]   teacher enroll - register the next RFID tag as a Teacher tag");
  logLine("[CMD]   teacher add <uid> | teacher list");
  logLine("[CMD]   reset_fire - clear the fire latch when no flame is present");
  logLine("[CMD]   reset   - clear Wi-Fi details, restart in setup mode (Level 2)");
  logLine("[CMD]   csi     - Wi-Fi CSI rate, subcarriers, channel (Level 11)");
}

static void cmdTeacherList() {
  if (!gNvsMtx || xSemaphoreTake(gNvsMtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
    logLine("[ERROR] TEACHER TAG STORAGE BUSY");
    return;
  }
  Preferences prefs;
  if (!prefs.begin("fastrobox", true)) {
    xSemaphoreGive(gNvsMtx);
    logLine("[ERROR] TEACHER TAG STORAGE UNAVAILABLE");
    return;
  }
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_TEACHER_TAGS; ++i) {
    char key[12];
    snprintf(key, sizeof key, "teacher%u", (unsigned)i);
    String uid = prefs.getString(key, "");
    if (uid.length()) {
      logLine("[CMD] teacher_tag=%s", uid.c_str());
      ++count;
    }
  }
  prefs.end();
  xSemaphoreGive(gNvsMtx);
  if (!count) logLine("[CMD] no Teacher tags registered");
}

static void cmdHealth() {
  float ct = temperatureRead();
  logLine("[CMD] health chip_temp=%.1fC", ct);
  logLine("[CMD] heap_free=%luk heap_min=%luk", (unsigned long)(ESP.getFreeHeap() / 1024),
          (unsigned long)(ESP.getMinFreeHeap() / 1024));
  logLine("[CMD] psram_free=%luk psram_size=%luk", (unsigned long)(ESP.getFreePsram() / 1024),
          (unsigned long)(ESP.getPsramSize() / 1024));
  logLine("[CMD] flash_size=%luMB", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
  logLine("[CMD] cpu_freq=%luMHz", (unsigned long)getCpuFrequencyMhz());
  logLine("[CMD] wifi_rssi=-");
  logLine("[CMD] uptime=%lus", (unsigned long)(esp_timer_get_time() / 1000000ULL));
  logLine("[CMD] reset_reason=%s", gResetReason);
  logLine("[CMD] fw=%s", FW_VERSION);
}

static void cmdTasks() {
  logLine("[CMD] tasks: %d running", gTaskCount);
  for (int i = 0; i < gTaskCount; i++) {
    TaskInfo& t = gTasks[i];
    logLine("[CMD]   %-8s core=%d period=%lums free_stack=%luB", t.name, (int)t.core,
            (unsigned long)t.periodMs,
            (unsigned long)(t.handle ? uxTaskGetStackHighWaterMark(t.handle) : 0));
  }
}

static void handleCommand(char* line) {
  // trim + lowercase
  while (*line == ' ' || *line == '\t') line++;
  size_t n = strlen(line);
  while (n && (line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = 0;
  for (size_t i = 0; i < n; i++) line[i] = (char)tolower((unsigned char)line[i]);
  if (!n) return;

  if      (!strcmp(line, "help"))   cmdHelp();
  else if (!strcmp(line, "status")) printStatusLine();
  else if (!strcmp(line, "health")) cmdHealth();
  else if (!strcmp(line, "tasks"))  cmdTasks();
  else if (!strcmp(line, "ver"))    logLine("[CMD] fw=%s build=%s %s device=%s", FW_VERSION, __DATE__, __TIME__, DEVICE_ID);
  else if (!strcmp(line, "hang"))   { logLine("[CMD] hang: freezing main task"); gHangRequested = true; }
  else if (!strcmp(line, "teacher enroll")) {
    STATE_SET({
      gTeacherEnrollRequested = true;
      gTeacherEnrollUntilMs = millis() + 30000U;
    });
    logLine("[CMD] tap the Teacher key tag within 30 seconds");
  }
  else if (!strcmp(line, "teacher list")) cmdTeacherList();
  else if (!strncmp(line, "teacher add ", 12)) {
    char uid[32];
    if (!normalizeUid(line + 12, uid, sizeof uid))
      logLine("[ERROR] INVALID TEACHER TAG UID");
    else if (addTeacherTag(uid))
      logLine("[CMD] Teacher tag registered: %s", uid);
    else
      logLine("[ERROR] TEACHER TAG LIST FULL OR NVS WRITE FAILED");
  }
  else if (!strcmp(line, "reset_fire")) resetFireLatch();
  else if (!strcmp(line, "reset"))  logLine("[CMD] reset: available from Level 2");
  else if (!strcmp(line, "csi"))    logLine("[CMD] csi: available from Level 11");
  else                              logLine("[CMD] unknown command '%s' - type help", line);
}

static void serialTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  char line[64]; size_t len = 0;
  uint32_t lastCharMs = 0;
  for (;;) {
    while (Serial.available() > 0) {          // never blocks
      int c = Serial.read();
      if (c < 0) continue;
      if (c == '\n' || c == '\r') {           // NL, CR or CR+LF all end a command
        if (len) { line[len] = 0; handleCommand(line); len = 0; }
      } else if (len < sizeof(line) - 1) {
        line[len++] = (char)c;
        lastCharMs = millis();
      }
    }
    // "No Line Ending" in the Serial Monitor: run the command after 150 ms of quiet
    if (len && millis() - lastCharMs > 150) { line[len] = 0; handleCommand(line); len = 0; }
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_SERIAL_MS));
  }
}

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  // --- helper power pins (flame VCC, buzzer GND) ---
  if (PIN_FLAME_PWR >= 0) {
    pinMode(PIN_FLAME_PWR, OUTPUT);
    gpio_set_drive_capability((gpio_num_t)PIN_FLAME_PWR, GPIO_DRIVE_CAP_3);
    digitalWrite(PIN_FLAME_PWR, HIGH);
  }
  if (PIN_BUZZER_GND >= 0) {
    pinMode(PIN_BUZZER_GND, OUTPUT);
    gpio_set_drive_capability((gpio_num_t)PIN_BUZZER_GND, GPIO_DRIVE_CAP_3);
    digitalWrite(PIN_BUZZER_GND, LOW);
  }
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_FLAME, INPUT_PULLUP);              // unplugged sensor reads HIGH = no flame
  delay(150);                                   // let the flame module power up

  // --- FIRST CHECK: flame at start-up decides the door ---
  int votes = 0;
  for (int i = 0; i < 5; i++) { if (readFlameRaw()) votes++; delay(5); }
  bool flameAtBoot = votes >= 3;
  gNvsMtx = xSemaphoreCreateMutex();
  bool fireLatchStored = false;
  bool fireLatchReadOk = loadFireLatch(fireLatchStored);
  g.flame = flameAtBoot;
  g.fireLatched = !fireLatchReadOk || fireLatchStored;
  g.doorUnlocked = flameAtBoot || g.fireLatched;

  // --- serial + logging ---
  gLogMtx = xSemaphoreCreateMutex();
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 5000) delay(10);   // native USB: wait for the monitor to reconnect
  delay(300);                                          // let it settle so the BOOT line is not lost

  // --- boot reason ---
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr != ESP_RST_POWERON && gWdtFiredMagic == WDT_MAGIC) gResetReason = "WATCHDOG";
  else gResetReason = resetReasonText(rr);
  gWdtFiredMagic = 0;

  logLine("[EVENT] BOOT reason=%s", gResetReason);
  if (!fireLatchReadOk) logLine("[ERROR] FIRE LATCH NVS READ FAILED");
  logLine("[EVENT] DOOR %s (startup flame=%s fire_latched=%s)",
          g.doorUnlocked ? "UNLOCKED" : "LOCKED", flameAtBoot ? "yes" : "no",
          g.fireLatched ? "yes" : "no");
  logLine("[CMD] SmartClass %s fw=%s - type help", DEVICE_ID, FW_VERSION);
  uint32_t touchTotal = 0;
  for (uint8_t i = 0; i < 32; ++i) {
    touchTotal += touchRead(PIN_TOUCH);
    delay(10);
  }
  gTouchBaseline = touchTotal / 32.0f;
  logLine("[EVENT] TOUCH_BASELINE value=%.1f threshold=%.1f",
          gTouchBaseline, gTouchBaseline * 0.70f);

  // --- LED ---
  rgbWrite(0);

  // --- RC522 ---
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();
  logLine("[CARD] reader version=0x%02X", rfid.PCD_ReadRegister(MFRC522::VersionReg));

  // --- watchdog, then tasks ---
  watchdogInit();
  //        function     name       stack prio core period
  startTask(flameTask,   "flame",   3072, 4,   1,   PERIOD_FLAME_MS);
  startTask(controlTask, "control", 4096, 3,   1,   PERIOD_CONTROL_MS);
  startTask(rfidTask,    "rfid",    4096, 2,   1,   PERIOD_RFID_MS);
  startTask(dhtTask,     "dht11",   3072, 2,   1,   PERIOD_DHT_S * 1000);
  startTask(ledTask,     "led",     3072, 3,   0,   PERIOD_LED_MS);
  startTask(touchTask,   "touch",   3072, 2,   0,   PERIOD_TOUCH_MS);
  startTask(statusTask,  "status",  4096, 2,   0,   PERIOD_STATUS_MS);
  startTask(serialTask,  "serial",  4096, 1,   0,   PERIOD_SERIAL_MS);
}

void loop() {
  vTaskDelete(NULL);   // all work runs in the FreeRTOS tasks above
}