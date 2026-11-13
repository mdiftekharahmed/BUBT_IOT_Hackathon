// ============================================================================
//  SmartClass — FastRobox 1.0 — Level 2: Wi-Fi Setup and Secure Telemetry
//  Built on the Level 1 sketch (firmwares/level_1/level_1.ino). Everything from
//  Level 1 keeps running; Level 2 adds: setup access point + setup page, saved
//  details in NVS, reconnection with growing gaps, local status page (LittleFS)
//  at the IP and at http://smartclass-t17.local, NTP time, HTTPS telemetry every
//  5 s and BOOT / CARD_TAP events with the server certificate checked, a seq that
//  never repeats, and the serial commands `reset` and `replay`.
//
//  Board: ESP32-S3-CAM   Libraries: MFRC522 (GithubCommunity) + built-in ESP32 ones
//  Arduino IDE: Tools > Partition Scheme must include a SPIFFS/LittleFS partition.
// ============================================================================

// ===== config ================================================================
#define FW_VERSION   "2.0.0"
#define TEAM_ID      "T17"            // <-- your team ID
#define DEVICE_ID    TEAM_ID "-D1"

// Platform address. Public, NOT a secret — the device token is typed on the setup page.
// For the judged run this MUST be the public https:// address (certificate is checked).
// An http:// address switches to TEST MODE: plain HTTP on the local network, no certificate.
#define SERVER_BASE  "http://192.168.68.59:8010"    // <-- TEST MODE: this PC on the same Wi-Fi

#define AP_SSID      "SmartClass-" TEAM_ID
#define MDNS_HOST    "smartclass-t17"               // http://smartclass-t17.local

// ---------------------------------------------------------------------------
//  PIN MAP  (Freenove-style ESP32-S3 WROOM CAM, DevKitC-1 header layout)
//  Camera uses 4-13,15-18; octal PSRAM uses 35-37; USB uses 19/20.
//  Do NOT insert an SD card (38/39/40 are shared with the SD slot).
// ---------------------------------------------------------------------------
#define PIN_RFID_SCK    39
#define PIN_RFID_MISO   40
#define PIN_RFID_MOSI   41
#define PIN_RFID_SS     42   // "SDA" on the RC522 module
#define PIN_RFID_RST    47
#define PIN_DHT         1
#define PIN_FLAME       2
#define FLAME_ACTIVE_LOW 1
#define PIN_BUZZER      21
#define PIN_TOUCH       14
#define PIN_RGB         48
#undef  RGB_BRIGHTNESS
#define RGB_BRIGHTNESS  40     // 0-255
#define PIN_FLAME_PWR   38     // flame VCC  (driven HIGH)
#define PIN_BUZZER_GND  3      // buzzer (-) (driven LOW)
#define PIN_BOOT_BTN    0      // onboard BOOT button (LOW when pressed)

// ---------------------------------------------------------------------------
//  Timing
// ---------------------------------------------------------------------------
#define PERIOD_FLAME_MS     100
#define PERIOD_RFID_MS      200
#define PERIOD_DHT_S        5
#define PERIOD_STATUS_MS    1000
#define PERIOD_LED_MS       250     // 250 on / 250 off = 2 Hz
#define PERIOD_CONTROL_MS   100
#define PERIOD_SERIAL_MS    20
#define PERIOD_NET_MS       100
#define PERIOD_WEB_MS       5

#define WDT_TIMEOUT_MS      5000

#define RFID_FAILS_FOR_ERROR   3
#define RFID_OKS_FOR_CLEAR     3
#define DHT_FAILS_FOR_ERROR    2
#define CARD_GONE_MS         600

// Level 2
#define TELEMETRY_EVERY_S      5      // one record every 5th status tick
#define BOOT_HOLD_MS        5000      // BOOT button held this long -> clear Wi-Fi details
#define SETUP_FAIL_MS      13000      // new details that do not connect -> setup mode (limit 15 s)
#define RECONNECT_TRY_MS    8000      // one reconnect attempt lasts at most this long
#define AP_LINGER_MS       10000      // keep the setup AP this long after connecting (page shows the IP)
#define OUTBOX_CAP            64      // records waiting to be sent
#define BATCH_MAX             10      // records per HTTPS request
#define SEQ_BLOCK            100      // seq numbers reserved in NVS at a time
#define HTTP_TIMEOUT_MS     4000
#define LIVE_WINDOW_MS     15000      // "LIVE" = a send succeeded within this window

// ============================================================================
//  Server root certificate (public — safe to keep in the repository).
//  Paste the root CA(s) of SERVER_BASE's certificate chain between the markers.
//  For Tailscale Funnel / Let's Encrypt: "ISRG Root X1" followed by "ISRG Root X2"
//  from https://letsencrypt.org/certificates/ . Check the chain with:
//    openssl s_client -connect <host>:443 -servername <host> -showcerts </dev/null
//  Below: ISRG Root X1 then ISRG Root X2 (Let's Encrypt), which Tailscale Funnel uses.
// ============================================================================
static const char ROOT_CA[] = R"CA(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw
CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg
R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00
MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT
ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw
EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW
+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9
ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T
AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI
zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW
tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1
/q4AaOeMSQ+2b1tbFfLn
-----END CERTIFICATE-----
)CA";

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <stdarg.h>
#include <time.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_arduino_version.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// ============================================================================
//  Types (all here, before any function, so Arduino's prototypes compile)
// ============================================================================
struct DeviceState {
  float tempC = NAN;
  float rh = NAN;
  bool  flame = false;
  bool  doorUnlocked = false;
  bool  dhtError = false;
  bool  rfidError = false;
  char  lastUid[32] = "";
};

struct TaskInfo { const char* name; TaskHandle_t handle; BaseType_t core; uint32_t periodMs; };

enum NetState : uint8_t { NET_SETUP, NET_CONNECTING, NET_CONNECTED, NET_RECONNECTING };

struct NetConfig {
  char ssid[33];
  char pass[64];
  char room[17];
  char token[65];
  bool verified;          // these details have connected at least once
};

enum RecKind : uint8_t { REC_TELEMETRY, REC_EVENT };

struct OutRecord {
  RecKind  kind;
  uint32_t seq;
  uint32_t uptimeMs;      // when the reading was taken
  int64_t  ts;            // UTC seconds; 0 = clock not set yet (filled in before sending)
  float    tempC, rh, hiC, chipC;
  bool     flame, doorUnlocked, hasRssi, qualityGood;
  int      rssi;
  uint32_t heapKb;
  char     type[16];
  char     detail[40];
};

// ============================================================================
//  Shared state
// ============================================================================
static DeviceState       g;
static portMUX_TYPE      gMux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t gLogMtx;

static const char*       gResetReason = "POWER_ON";
static volatile bool     gHangRequested = false;
static volatile uint32_t gLedColor = 0;
static volatile float    gChipTemp = NAN;

RTC_NOINIT_ATTR static uint32_t gWdtFiredMagic;
#define WDT_MAGIC 0x5AFEC0DEu

// Level 2
static SemaphoreHandle_t gCfgMtx, gObMtx, gScanMtx;
static NetConfig         gCfg;                 // guarded by gCfgMtx
static NetConfig         gPending;             // filled by the setup page
static volatile bool     gSetupSubmitted = false;
static volatile NetState gNet = NET_SETUP;
static char              gApPass[9] = "";
static char              gSetupError[64] = "";
static char              gIp[16] = "";
static volatile bool     gApUp = false;
static String            gScanJson = "{\"networks\":[]}";   // guarded by gScanMtx
static volatile uint16_t gDiscCount = 0;                    // disconnect events (ours excluded)
static volatile int      gLastDiscReason = 0;

static OutRecord         gOutbox[OUTBOX_CAP];  // guarded by gObMtx
static int               gObHead = 0, gObCount = 0;
static uint32_t          gObDropped = 0;
static uint32_t          gSeqNext = 0, gSeqEnd = 0;   // guarded by gObMtx

static volatile bool     gServerOk = false, gServerEverOk = false;
static volatile uint32_t gLastOkMs = 0, gServerDownSinceMs = 0;
static volatile bool     gTimeSynced = false;
static volatile bool     gReplayRequested = false;
static char              gLastTelemetryJson[420] = "";   // owned by the cloud task
static uint32_t          gLastTelemetrySeq = 0;

static bool              gFsOk = false;
static WebServer         server(80);
static WiFiClientSecure  tls;
static WiFiClient        plainClient;              // TEST MODE only (http:// SERVER_BASE)

static TaskInfo gTasks[12];
static int      gTaskCount = 0;

static DeviceState snapshot() {
  DeviceState s;
  portENTER_CRITICAL(&gMux); s = g; portEXIT_CRITICAL(&gMux);
  return s;
}
#define STATE_SET(stmt) do { portENTER_CRITICAL(&gMux); stmt; portEXIT_CRITICAL(&gMux); } while (0)

static NetConfig cfgCopy() {
  NetConfig c;
  xSemaphoreTake(gCfgMtx, portMAX_DELAY); c = gCfg; xSemaphoreGive(gCfgMtx);
  return c;
}

// ============================================================================
//  Logging — one whole line at a time
// ============================================================================
static void logLine(const char* fmt, ...) {
  char buf[320];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  bool locked = gLogMtx && xSemaphoreTake(gLogMtx, pdMS_TO_TICKS(50)) == pdTRUE;
  Serial.println(buf);
  if (locked) xSemaphoreGive(gLogMtx);
}

// ============================================================================
//  Task registry
// ============================================================================
static void startTask(TaskFunction_t fn, const char* name, uint32_t stack,
                      UBaseType_t prio, BaseType_t core, uint32_t periodMs) {
  TaskInfo& t = gTasks[gTaskCount++];
  t.name = name; t.core = core; t.periodMs = periodMs; t.handle = nullptr;
  xTaskCreatePinnedToCore(fn, name, stack, nullptr, prio, &t.handle, core);
}

// ============================================================================
//  Watchdog
// ============================================================================
extern "C" void IRAM_ATTR esp_task_wdt_isr_user_handler(void) { gWdtFiredMagic = WDT_MAGIC; }

static void watchdogInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WDT_TIMEOUT_MS;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
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
    default:                return "POWER_ON";
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
//  DHT11 driver
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
  pinMode(PIN_DHT, OUTPUT);
  digitalWrite(PIN_DHT, LOW);
  vTaskDelay(pdMS_TO_TICKS(20));
  pinMode(PIN_DHT, INPUT_PULLUP);

  bool ok = true;
  portENTER_CRITICAL(&dhtMux);
  if (!dhtWaitLevel(0, 200)) ok = false;
  else if (!dhtWaitLevel(1, 120)) ok = false;
  else if (!dhtWaitLevel(0, 120)) ok = false;
  for (int i = 0; ok && i < 40; i++) {
    if (!dhtWaitLevel(1, 100)) { ok = false; break; }
    int64_t t0 = esp_timer_get_time();
    if (!dhtWaitLevel(0, 120)) { ok = false; break; }
    if (esp_timer_get_time() - t0 > 45) d[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
  }
  portEXIT_CRITICAL(&dhtMux);

  if (!ok) return false;
  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;
  if (d[0] == 0 && d[2] == 0) return false;
  rh = d[0] + d[1] * 0.1f;
  tempC = (d[2] & 0x7F) + d[3] * 0.1f;
  if (d[3] & 0x80) tempC = -tempC;
  return true;
}

// ============================================================================
//  RC522
// ============================================================================
static MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);

static bool rfidAlive() {
  uint8_t v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  if (v == 0x00 || v == 0xFF) return false;
  uint8_t probe = (uint8_t)random(1, 255);
  rfid.PCD_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
  rfid.PCD_WriteRegister(MFRC522::FIFODataReg, probe);
  uint8_t level = rfid.PCD_ReadRegister(MFRC522::FIFOLevelReg) & 0x7F;
  uint8_t back  = rfid.PCD_ReadRegister(MFRC522::FIFODataReg);
  rfid.PCD_WriteRegister(MFRC522::FIFOLevelReg, 0x80);
  return level == 1 && back == probe;
}

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
//  Small helpers (Level 2)
// ============================================================================
static bool epochValid(time_t t) { return t >= 1704067200; }   // 2024-01-01

// Heat index in C (same formula as the simulator); below 26.7 C it equals the temperature.
static float heatIndexC(float tC, float rh) {
  if (isnan(tC) || isnan(rh)) return NAN;
  float t = tC * 9.0f / 5.0f + 32.0f;
  if (t < 80.0f) return roundf(tC * 10.0f) / 10.0f;
  float hi = -42.379f + 2.04901523f * t + 10.14333127f * rh - 0.22475541f * t * rh
             - 0.00683783f * t * t - 0.05481717f * rh * rh + 0.00122874f * t * t * rh
             + 0.00085282f * t * rh * rh - 0.00000199f * t * t * rh * rh;
  return roundf((hi - 32.0f) * 5.0f / 9.0f * 10.0f) / 10.0f;
}

// Retry gaps after a Wi-Fi drop: 1, 2, 4, 8, 15, 15, ... seconds
static uint32_t reconnectGapMs(uint8_t attempt) {
  static const uint32_t gaps[] = {1000, 2000, 4000, 8000, 15000};
  return attempt < 5 ? gaps[attempt] : 15000;
}

// Writes s as a JSON string (with quotes), escaping " \ and control characters.
static size_t jsonStr(char* out, size_t n, const char* s) {
  size_t p = 0;
  auto put = [&](char c) { if (p + 1 < n) out[p] = c; p++; };
  put('"');
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { put('\\'); put((char)c); }
    else if (c < 0x20) { char e[7]; snprintf(e, sizeof e, "\\u%04x", c); for (char* q = e; *q; q++) put(*q); }
    else put((char)c);
  }
  put('"');
  if (n) out[p < n ? p : n - 1] = 0;
  return p;
}

static void jsonNum(char* out, size_t n, float v, const char* fmt) {
  if (isnan(v)) snprintf(out, n, "null"); else snprintf(out, n, fmt, v);
}

// ============================================================================
//  NVS: saved details and seq
// ============================================================================
static bool loadConfig(NetConfig& c) {
  memset(&c, 0, sizeof c);
  Preferences p;
  if (!p.begin("smartclass", true)) return false;
  p.getString("ssid", c.ssid, sizeof c.ssid);
  p.getString("pass", c.pass, sizeof c.pass);
  p.getString("room", c.room, sizeof c.room);
  p.getString("token", c.token, sizeof c.token);
  c.verified = p.getBool("verified", false);
  p.end();
  return c.ssid[0] != 0;
}

static void saveConfig(const NetConfig& c) {
  Preferences p;
  p.begin("smartclass", false);
  p.putString("ssid", c.ssid);
  p.putString("pass", c.pass);
  p.putString("room", c.room);
  p.putString("token", c.token);
  p.putBool("verified", c.verified);
  p.end();
}

static void markVerified() {
  Preferences p;
  p.begin("smartclass", false);
  p.putBool("verified", true);
  p.end();
  xSemaphoreTake(gCfgMtx, portMAX_DELAY); gCfg.verified = true; xSemaphoreGive(gCfgMtx);
}

static void clearConfig() {           // keeps seq_end so seq never repeats
  Preferences p;
  p.begin("smartclass", false);
  p.remove("ssid"); p.remove("pass"); p.remove("room"); p.remove("token"); p.remove("verified");
  p.end();
}

// seq is reserved in blocks: NVS always holds a number larger than any seq issued.
static void seqInit() {
  Preferences p;
  p.begin("smartclass", false);
  uint32_t stored = p.getUInt("seq_end", 0);
  gSeqNext = stored ? stored : 1;
  gSeqEnd = gSeqNext + SEQ_BLOCK;
  p.putUInt("seq_end", gSeqEnd);
  p.end();
}

static uint32_t seqTake() {            // call with gObMtx held
  if (gSeqNext >= gSeqEnd) {
    gSeqEnd += SEQ_BLOCK;
    Preferences p;
    p.begin("smartclass", false);
    p.putUInt("seq_end", gSeqEnd);
    p.end();
  }
  return gSeqNext++;
}

// ============================================================================
//  Outbox: records waiting to be sent (oldest dropped when full)
// ============================================================================
static void pushRecord(OutRecord& r) {
  r.uptimeMs = millis();
  time_t now = time(nullptr);
  r.ts = epochValid(now) ? (int64_t)now : 0;
  xSemaphoreTake(gObMtx, portMAX_DELAY);
  r.seq = seqTake();                   // seq and position taken together: outbox stays in seq order
  if (gObCount == OUTBOX_CAP) { gObHead = (gObHead + 1) % OUTBOX_CAP; gObCount--; gObDropped++; }
  gOutbox[(gObHead + gObCount) % OUTBOX_CAP] = r;
  gObCount++;
  xSemaphoreGive(gObMtx);
}

static int outboxSize() {
  xSemaphoreTake(gObMtx, portMAX_DELAY); int n = gObCount; xSemaphoreGive(gObMtx);
  return n;
}

// Every event the device sends is also printed: [EVENT] <type> <detail>
static void enqueueEvent(const char* type, const char* detail) {
  logLine("[EVENT] %s %s", type, detail);
  OutRecord r = {};
  r.kind = REC_EVENT;
  strlcpy(r.type, type, sizeof r.type);
  strlcpy(r.detail, detail, sizeof r.detail);
  pushRecord(r);
}

static void enqueueTelemetry() {
  DeviceState s = snapshot();
  OutRecord r = {};
  r.kind = REC_TELEMETRY;
  r.tempC = s.dhtError ? NAN : s.tempC;
  r.rh = s.dhtError ? NAN : s.rh;
  r.hiC = heatIndexC(r.tempC, r.rh);
  r.chipC = gChipTemp;
  r.flame = s.flame;
  r.doorUnlocked = s.doorUnlocked;
  r.hasRssi = WiFi.status() == WL_CONNECTED;
  r.rssi = r.hasRssi ? WiFi.RSSI() : 0;
  r.heapKb = ESP.getFreeHeap() / 1024;
  r.qualityGood = !s.dhtError && !s.rfidError && !isnan(r.tempC);
  pushRecord(r);
}

// One record as JSON, exactly the fields of the paper's section 6.
static size_t recordJson(const OutRecord& r, const char* room, char* out, size_t n) {
  char roomJ[40], a[16], b[16], c[16], d[16];
  jsonStr(roomJ, sizeof roomJ, room);
  if (r.kind == REC_EVENT) {
    char typeJ[40], detJ[100];
    jsonStr(typeJ, sizeof typeJ, r.type);
    jsonStr(detJ, sizeof detJ, r.detail);
    return snprintf(out, n,
      "{\"device_id\":\"" DEVICE_ID "\",\"room_id\":%s,\"seq\":%lu,\"ts\":%lld,\"type\":%s,\"detail\":%s}",
      roomJ, (unsigned long)r.seq, (long long)r.ts, typeJ, detJ);
  }
  jsonNum(a, sizeof a, r.tempC, "%.1f");
  jsonNum(b, sizeof b, r.rh, "%.0f");
  jsonNum(c, sizeof c, r.hiC, "%.1f");
  jsonNum(d, sizeof d, r.chipC, "%.1f");
  char rssi[12];
  if (r.hasRssi) snprintf(rssi, sizeof rssi, "%d", r.rssi); else strcpy(rssi, "null");
  return snprintf(out, n,
    "{\"device_id\":\"" DEVICE_ID "\",\"room_id\":%s,\"seq\":%lu,\"ts\":%lld,\"uptime_s\":%lu,"
    "\"temp_c\":%s,\"rh\":%s,\"heat_index_c\":%s,\"flame\":%s,\"door\":\"%s\",\"status\":\"SAFE\","
    "\"heat_trend\":null,\"rssi\":%s,\"heap_kb\":%lu,\"chip_temp_c\":%s,\"fw\":\"" FW_VERSION "\","
    "\"quality\":\"%s\"}",
    roomJ, (unsigned long)r.seq, (long long)r.ts, (unsigned long)(r.uptimeMs / 1000),
    a, b, c, r.flame ? "true" : "false", r.doorUnlocked ? "UNLOCKED" : "LOCKED",
    rssi, (unsigned long)r.heapKb, d, r.qualityGood ? "good" : "bad");
}

// ============================================================================
//  Wi-Fi: setup mode, connecting, reconnecting
// ============================================================================
static uint32_t gConnectStartMs = 0, gNextTryMs = 0, gTryStartMs = 0, gDownSinceMs = 0, gApOffAtMs = 0;
static uint8_t  gAttempt = 0;
static bool     gTrying = false;
static uint16_t gDiscAtTry = 0;
static uint32_t gScanAtMs = 0;

static void onWifiDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
  int r = info.wifi_sta_disconnected.reason;
  if (r == 8) return;                  // ASSOC_LEAVE: we asked for it
  gLastDiscReason = r;
  gDiscCount++;
}

static bool reasonIsFatal(int r) {     // wrong password / network not there
  return r == 2 || r == 15 || r == 201 || r == 202 || r == 204;
}

static const char* reasonText(int r) {
  if (r == 201) return "Network not found";
  if (r == 2 || r == 15 || r == 202 || r == 204) return "Wrong password";
  return "Could not connect";
}

static void makeApPassword(char out[9]) {
  static const char cs[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
  for (int i = 0; i < 8; i++) out[i] = cs[esp_random() % (sizeof(cs) - 1)];
  out[8] = 0;
}

static void enterSetup() {
  if (!gApPass[0]) makeApPassword(gApPass);      // one password per start-up
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, gApPass);
  gApUp = true;
  gApOffAtMs = 0;
  gScanAtMs = 0;
  gNet = NET_SETUP;
  logLine("[NET] setup mode: access point %s password=%s page=http://192.168.4.1", AP_SSID, gApPass);
}

static void startConnect(const NetConfig& c) {
  gLastDiscReason = 0;
  gDiscAtTry = gDiscCount;
  WiFi.begin(c.ssid, c.pass[0] ? c.pass : nullptr);
  gConnectStartMs = millis();
  gNet = NET_CONNECTING;
  logLine("[NET] connecting to \"%s\"", c.ssid);
}

static void startMdns() {
  MDNS.end();
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", 80);
    logLine("[NET] mDNS name: http://%s.local", MDNS_HOST);
  } else {
    logLine("[NET] mDNS failed to start");
  }
}

static void onConnected(bool wasReconnect) {
  strlcpy(gIp, WiFi.localIP().toString().c_str(), sizeof gIp);
  NetConfig c = cfgCopy();
  if (!c.verified) markVerified();
  if (wasReconnect)
    logLine("[NET] reconnected to \"%s\" ip=%s after %lus (no restart)", c.ssid, gIp,
            (unsigned long)((millis() - gDownSinceMs) / 1000));
  else
    logLine("[NET] connected to \"%s\" ip=%s rssi=%d", c.ssid, gIp, WiFi.RSSI());
  startMdns();
  gSetupError[0] = 0;
  if (gApUp && !gApOffAtMs) gApOffAtMs = millis() + AP_LINGER_MS;
  gNet = NET_CONNECTED;
}

static void setupFailed(int reason, bool timedOut) {
  NetConfig c = cfgCopy();
  const char* why = timedOut && !reasonIsFatal(reason) ? "No answer within 15 s" : reasonText(reason);
  snprintf(gSetupError, sizeof gSetupError, "%s (\"%s\")", why, c.ssid);
  logLine("[NET] setup error: %s", gSetupError);
  WiFi.disconnect(false);              // stop the station; the access point stays up
  enterSetup();
}

static void netTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  uint32_t btnSince = 0;
  for (;;) {
    wdtFeed();
    uint32_t now = millis();

    // BOOT button held 5 s -> clear details, restart in setup mode
    if (digitalRead(PIN_BOOT_BTN) == LOW) {
      if (!btnSince) btnSince = now ? now : 1;
      else if (now - btnSince >= BOOT_HOLD_MS) {
        logLine("[NET] BOOT button held 5 s: clearing Wi-Fi details, restarting in setup mode");
        clearConfig();
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_restart();
      }
    } else {
      btnSince = 0;
    }

    // New details from the setup page
    if (gSetupSubmitted) {
      gSetupSubmitted = false;
      xSemaphoreTake(gCfgMtx, portMAX_DELAY); gCfg = gPending; gCfg.verified = false; xSemaphoreGive(gCfgMtx);
      saveConfig(cfgCopy());
      logLine("[NET] setup saved: ssid=\"%s\" room=%s (details stored in NVS)", gPending.ssid, gPending.room);
      gSetupError[0] = 0;
      startConnect(cfgCopy());
    }

    switch (gNet) {
      case NET_SETUP: {
        int sc = WiFi.scanComplete();
        if (sc >= 0) {
          String js = "{\"networks\":[";
          for (int i = 0; i < sc; i++) {
            char ss[80];
            jsonStr(ss, sizeof ss, WiFi.SSID(i).c_str());
            if (i) js += ",";
            js += "{\"ssid\":"; js += ss;
            js += ",\"rssi\":"; js += WiFi.RSSI(i);
            js += ",\"open\":"; js += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "true" : "false";
            js += "}";
          }
          js += "]}";
          WiFi.scanDelete();
          xSemaphoreTake(gScanMtx, portMAX_DELAY); gScanJson = js; xSemaphoreGive(gScanMtx);
        }
        if (now >= gScanAtMs && sc != WIFI_SCAN_RUNNING) {
          WiFi.scanNetworks(true);
          gScanAtMs = now + 10000;
        }
        break;
      }

      case NET_CONNECTING: {
        if (WiFi.status() == WL_CONNECTED) { onConnected(false); break; }
        NetConfig c = cfgCopy();
        if (!c.verified) {
          // New details: wrong password / missing network -> back to setup within 15 s
          bool failedEvent = gDiscCount != gDiscAtTry && reasonIsFatal(gLastDiscReason);
          bool timedOut = now - gConnectStartMs > SETUP_FAIL_MS;
          if (failedEvent || timedOut) setupFailed(gLastDiscReason, timedOut);
        } else if (now - gConnectStartMs > RECONNECT_TRY_MS) {
          // Known-good details (e.g. after a power cut): keep retrying, never back to setup
          logLine("[NET] \"%s\" not reachable yet, retrying", c.ssid);
          gDownSinceMs = gConnectStartMs;
          gAttempt = 0; gTrying = false;
          gNextTryMs = now + reconnectGapMs(0);
          gNet = NET_RECONNECTING;
        }
        break;
      }

      case NET_CONNECTED: {
        if (WiFi.status() != WL_CONNECTED) {
          logLine("[NET] disconnected from Wi-Fi (reason %d), reconnecting", gLastDiscReason);
          gDownSinceMs = now;
          gAttempt = 0; gTrying = false;
          gNextTryMs = now + reconnectGapMs(0);
          gNet = NET_RECONNECTING;
          break;
        }
        if (gApOffAtMs && now >= gApOffAtMs) {
          WiFi.softAPdisconnect(true);
          WiFi.mode(WIFI_STA);
          gApUp = false; gApOffAtMs = 0;
          logLine("[NET] setup access point closed");
        }
        break;
      }

      case NET_RECONNECTING: {
        if (WiFi.status() == WL_CONNECTED) { onConnected(true); break; }
        if (gTrying) {
          if (gDiscCount != gDiscAtTry || now - gTryStartMs > RECONNECT_TRY_MS) {
            gTrying = false;
            uint32_t gap = reconnectGapMs(gAttempt);
            gNextTryMs = now + gap;
            logLine("[NET] reconnect attempt %u failed, next try in %lus", gAttempt + 1, (unsigned long)(gap / 1000));
            if (gAttempt < 255) gAttempt++;
          }
        } else if (now >= gNextTryMs) {
          NetConfig c = cfgCopy();
          gDiscAtTry = gDiscCount;
          WiFi.begin(c.ssid, c.pass[0] ? c.pass : nullptr);
          gTrying = true;
          gTryStartMs = now;
          logLine("[NET] reconnecting to \"%s\" (attempt %u)", c.ssid, gAttempt + 1);
        }
        break;
      }
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_NET_MS));
  }
}

// ============================================================================
//  Server connection (HTTPS, certificate checked) — cloud task
// ============================================================================
static const char* connLabel() {
  switch (gNet) {
    case NET_SETUP:        return "SETUP";
    case NET_CONNECTING:   return "CONNECTING";
    case NET_RECONNECTING: return "OFFLINE";
    default:               return (gServerOk && millis() - gLastOkMs < LIVE_WINDOW_MS) ? "LIVE" : "NO SERVER";
  }
}

static bool serverIsPlainHttp() { return strncmp(SERVER_BASE, "http://", 7) == 0; }
static bool caPresent() { return serverIsPlainHttp() || strstr(ROOT_CA, "BEGIN CERTIFICATE") != nullptr; }

static void serverResult(bool ok, const char* why) {
  if (ok) {
    if (!gServerEverOk)  logLine("[NET] server connected: %s (%s)", SERVER_BASE,
                                 serverIsPlainHttp() ? "TEST MODE: plain HTTP, no certificate" : "certificate checked");
    else if (!gServerOk) logLine("[NET] server reconnected after %lus, %d records waiting",
                                 (unsigned long)((millis() - gServerDownSinceMs) / 1000), outboxSize());
    gServerOk = true; gServerEverOk = true; gLastOkMs = millis();
  } else {
    if (gServerOk || !gServerDownSinceMs) {
      logLine("[NET] server unreachable: %s", why);
      gServerDownSinceMs = millis();
    }
    gServerOk = false;
  }
}

static int postJson(const char* path, const char* body, const char* token, String& resp) {
  String url = String(SERVER_BASE) + path;
  HTTPClient http;
  http.setReuse(true);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  bool begun = serverIsPlainHttp() ? http.begin(plainClient, url) : http.begin(tls, url);
  if (!begun) return -100;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + token);
  http.addHeader("X-Device-Id", DEVICE_ID);
  int code = http.POST((uint8_t*)body, strlen(body));
  resp = code > 0 ? http.getString() : HTTPClient::errorToString(code);
  http.end();
  return code;
}

static void doReplay(const char* token) {
  if (!gLastTelemetryJson[0]) { logLine("[CMD] replay: nothing has been sent yet"); return; }
  String resp;
  int code = postJson("/api/device/telemetry", gLastTelemetryJson, token, resp);
  logLine("[CMD] replay seq=%lu -> HTTP %d (%s)", (unsigned long)gLastTelemetrySeq, code,
          code == 409 ? "duplicate - rejected by the server, nothing stored"
                      : code == 200 ? "stored - NOT expected" : resp.c_str());
}

static void cloudTask(void*) {
  // Not on the task watchdog: a TLS handshake on a bad link may take longer than 5 s.
  static char body[BATCH_MAX * 420 + 4];
  bool ntpStarted = false, caWarned = false, tokenWarned = false;
  tls.setCACert(ROOT_CA);
  tls.setHandshakeTimeout(8);
  for (;;) {
    if (gNet != NET_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(250)); continue; }

    if (!ntpStarted) {
      configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      ntpStarted = true;
    }
    if (!gTimeSynced && epochValid(time(nullptr))) {
      gTimeSynced = true;
      time_t t = time(nullptr); struct tm u; gmtime_r(&t, &u);
      char iso[24]; strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &u);
      logLine("[NET] time synced (NTP): %s", iso);
    }
    if (!caPresent()) {
      if (!caWarned) { logLine("[NET] server: ROOT_CA not set in the sketch - nothing is sent"); caWarned = true; }
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    NetConfig c = cfgCopy();
    if (gReplayRequested) { gReplayRequested = false; doReplay(c.token); }

    // Take the oldest records of one kind whose time is known (fill it in if the clock was set later).
    size_t n = 0, len = 0;
    RecKind kind = REC_TELEMETRY;
    uint32_t lastSeq = 0;
    char one[420];
    time_t now = time(nullptr);
    xSemaphoreTake(gObMtx, portMAX_DELAY);
    for (int i = 0; i < gObCount && n < BATCH_MAX; i++) {
      OutRecord& r = gOutbox[(gObHead + i) % OUTBOX_CAP];
      if (r.ts == 0) {
        if (!epochValid(now)) break;
        r.ts = (int64_t)now - (int64_t)((millis() - r.uptimeMs) / 1000);   // back-date to the reading time
      }
      if (n == 0) kind = r.kind; else if (r.kind != kind) break;
      size_t l = recordJson(r, c.room, one, sizeof one);
      if (len + l + 2 >= sizeof body) break;
      body[len++] = n == 0 ? '[' : ',';
      memcpy(body + len, one, l); len += l;
      lastSeq = r.seq;
      n++;
    }
    xSemaphoreGive(gObMtx);
    if (n == 0) { vTaskDelay(pdMS_TO_TICKS(250)); continue; }
    body[len++] = ']'; body[len] = 0;

    String resp;
    const char* path = kind == REC_TELEMETRY ? "/api/device/telemetry" : "/api/device/events";
    int code = postJson(path, body, c.token, resp);

    if (code == 200 || code == 409 || code == 422) {
      if (code == 422 || resp.indexOf("rejected") >= 0)
        logLine("[NET] server rejected a record (up to seq %lu): %s", (unsigned long)lastSeq, resp.substring(0, 160).c_str());
      // remember the newest telemetry record sent, for the `replay` command
      if (kind == REC_TELEMETRY) {
        xSemaphoreTake(gObMtx, portMAX_DELAY);
        for (int i = 0; i < gObCount; i++) {
          OutRecord& r = gOutbox[(gObHead + i) % OUTBOX_CAP];
          if (r.seq == lastSeq) { recordJson(r, c.room, gLastTelemetryJson, sizeof gLastTelemetryJson); gLastTelemetrySeq = r.seq; break; }
        }
        xSemaphoreGive(gObMtx);
      }
      // drop what was sent (the head may have moved if the outbox overflowed meanwhile)
      xSemaphoreTake(gObMtx, portMAX_DELAY);
      while (gObCount && gOutbox[gObHead].seq <= lastSeq) { gObHead = (gObHead + 1) % OUTBOX_CAP; gObCount--; }
      xSemaphoreGive(gObMtx);
      serverResult(true, "");
      tokenWarned = false;
    } else if (code == 401) {
      serverResult(true, "");          // the server answered; the token is the problem
      if (!tokenWarned) { logLine("[NET] server refused the device token (401) - check the token on the setup page"); tokenWarned = true; }
      vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
      serverResult(false, resp.c_str());
      tls.stop();
      plainClient.stop();
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

// ============================================================================
//  Local web pages — stored in LittleFS (written from the sketch at start-up)
// ============================================================================
static const char STATUS_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartClass T17</title>
<style>
body{font-family:system-ui,Arial,sans-serif;margin:0;background:#f4f6f8;color:#1d2433}
header{background:#1d2433;color:#fff;padding:14px 16px;display:flex;justify-content:space-between;align-items:center}
h1{font-size:18px;margin:0}#conn{padding:4px 10px;border-radius:12px;font-weight:700;font-size:13px;background:#888}
.LIVE{background:#1a9b4b!important}.OFFLINE,.NO{background:#c0392b!important}.SETUP,.CONNECTING{background:#2d6cdf!important}
main{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;padding:16px}
.c{background:#fff;border-radius:10px;padding:12px;box-shadow:0 1px 3px #0002}
.k{font-size:12px;color:#667}.v{font-size:20px;font-weight:700;margin-top:4px;word-break:break-all}
footer{padding:0 16px 16px;font-size:12px;color:#667}
</style></head><body>
<header><h1>SmartClass T17 · Room <span id="room">—</span></h1><span id="conn">—</span></header>
<main>
<div class="c"><div class="k">Local time (Asia/Dhaka)</div><div class="v" id="time">—</div></div>
<div class="c"><div class="k">Temperature</div><div class="v" id="temp">—</div></div>
<div class="c"><div class="k">Humidity</div><div class="v" id="rh">—</div></div>
<div class="c"><div class="k">Status</div><div class="v" id="status">—</div></div>
<div class="c"><div class="k">Flame</div><div class="v" id="flame">—</div></div>
<div class="c"><div class="k">Door</div><div class="v" id="door">—</div></div>
<div class="c"><div class="k">Last card UID</div><div class="v" id="uid">—</div></div>
<div class="c"><div class="k">Wi-Fi signal</div><div class="v" id="rssi">—</div></div>
<div class="c"><div class="k">Chip temperature</div><div class="v" id="chip">—</div></div>
<div class="c"><div class="k">Firmware</div><div class="v" id="fw">—</div></div>
<div class="c"><div class="k">Records waiting</div><div class="v" id="wait">—</div></div>
<div class="c"><div class="k">Uptime</div><div class="v" id="up">—</div></div>
</main>
<footer>Updates every second · <span id="age">—</span></footer>
<script>
const $=id=>document.getElementById(id);
const set=(id,v)=>{$(id).textContent=(v===null||v===undefined||v==='')?'—':v};
let last=0;
const tick=async()=>{
 try{
  const s=await (await fetch('/api/state',{cache:'no-store'})).json();
  set('room',s.room);set('temp',s.temp_c===null?null:s.temp_c.toFixed(1)+' °C');
  set('rh',s.rh===null?null:Math.round(s.rh)+' %');set('status',s.status);
  set('flame',s.flame?'FLAME':'none');set('door',s.door);set('uid',s.last_uid);
  set('rssi',s.rssi===null?null:s.rssi+' dBm');set('chip',s.chip_c===null?null:s.chip_c.toFixed(1)+' °C');
  set('fw',s.fw);set('wait',s.waiting);set('up',s.uptime_s+' s');
  set('time',s.epoch===null?null:new Date(s.epoch*1000).toLocaleString('en-GB',{timeZone:'Asia/Dhaka'}));
  const c=$('conn');c.textContent=s.conn;c.className=s.conn.split(' ')[0];
  last=Date.now();
 }catch(e){}
 set('age',last?('last update '+Math.round((Date.now()-last)/1000)+' s ago'):'waiting');
};
tick();setInterval(tick,1000);
</script></body></html>)HTML";

static const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartClass T17 setup</title>
<style>
body{font-family:system-ui,Arial,sans-serif;margin:0;background:#f4f6f8;color:#1d2433}
header{background:#2d6cdf;color:#fff;padding:14px 16px}h1{font-size:18px;margin:0}
main{max-width:460px;margin:0 auto;padding:16px}
.box{background:#fff;border-radius:10px;padding:14px;margin-bottom:12px;box-shadow:0 1px 3px #0002}
label{display:block;font-size:13px;margin:10px 0 4px}input{width:100%;box-sizing:border-box;padding:9px;font-size:15px;border:1px solid #bbc;border-radius:6px}
button{margin-top:14px;width:100%;padding:11px;font-size:16px;border:0;border-radius:6px;background:#2d6cdf;color:#fff}
ul{list-style:none;padding:0;margin:0}li{padding:8px 4px;border-bottom:1px solid #eee;cursor:pointer;display:flex;justify-content:space-between}
li:hover{background:#eef3ff}.err{color:#c0392b;font-weight:700}.ok{color:#1a9b4b;font-weight:700}
</style></head><body>
<header><h1>SmartClass T17 · Wi-Fi setup</h1></header>
<main>
<div class="box"><b>Nearby networks</b> <small>(tap one)</small><ul id="nets"><li>Scanning…</li></ul></div>
<div class="box">
<form id="f">
<label>Wi-Fi name (or type a hidden one)</label><input name="ssid" id="ssid" maxlength="32" required>
<label>Wi-Fi password</label><input name="pass" type="password" maxlength="63">
<label>Room ID</label><input name="room" maxlength="16" required placeholder="e.g. R-305">
<label>Device token</label><input name="token" maxlength="64" required autocomplete="off">
<button>Save and connect</button>
</form>
<p id="msg"></p>
</div>
</main>
<script>
const $=id=>document.getElementById(id);
const bars=r=>r>-55?'▂▄▆█':r>-67?'▂▄▆':r>-78?'▂▄':'▂';
const scan=async()=>{
 try{
  const d=await (await fetch('/api/scan',{cache:'no-store'})).json();
  const ul=$('nets');ul.textContent='';
  d.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
   const li=document.createElement('li');
   const a=document.createElement('span');a.textContent=n.ssid+(n.open?' (open)':'');
   const b=document.createElement('span');b.textContent=bars(n.rssi)+' '+n.rssi+' dBm';
   li.append(a,b);li.onclick=()=>{$('ssid').value=n.ssid};ul.append(li);
  });
  if(!d.networks.length)ul.innerHTML='<li>Scanning…</li>';
 }catch(e){}
};
const msg=(t,cls)=>{const m=$('msg');m.textContent=t;m.className=cls||''};
let polling=null;
const poll=async()=>{
 try{
  const s=await (await fetch('/api/setup-state',{cache:'no-store'})).json();
  if(s.state==='CONNECTED'){msg('Connected to '+s.ssid+'. IP address: '+s.ip+' — join the same Wi-Fi and open http://'+s.ip+' or http://smartclass-t17.local','ok');clearInterval(polling);}
  else if(s.error){msg('Error: '+s.error,'err');clearInterval(polling);}
  else if(s.state==='CONNECTING'){msg('Connecting to '+s.ssid+'…');}
 }catch(e){msg('Reconnecting to the device…');}
};
$('f').onsubmit=async e=>{
 e.preventDefault();
 msg('Saving…');
 try{
  const r=await fetch('/api/setup',{method:'POST',body:new URLSearchParams(new FormData($('f')))});
  const d=await r.json();
  if(!r.ok){msg('Error: '+d.error,'err');return;}
  msg('Connecting…');clearInterval(polling);polling=setInterval(poll,1000);
 }catch(e){msg('Could not reach the device','err');}
};
scan();setInterval(scan,5000);poll();
</script></body></html>)HTML";

static void installPage(const char* path, const char* html) {
  File f = LittleFS.open(path, "w");
  if (!f) { logLine("[ERROR] LittleFS: cannot write %s", path); return; }
  f.write((const uint8_t*)html, strlen_P(html));
  f.close();
}

static void fsInit() {
  gFsOk = LittleFS.begin(true);        // formats the partition on first use
  if (!gFsOk) { logLine("[ERROR] LittleFS not available - check Tools > Partition Scheme"); return; }
  installPage("/index.html", STATUS_HTML);
  installPage("/setup.html", SETUP_HTML);
  logLine("[NET] web pages stored in LittleFS (/index.html, /setup.html)");
}

// ============================================================================
//  Web server — web task
// ============================================================================
static void sendPage(const char* path, const char* fallback) {
  if (gFsOk && LittleFS.exists(path)) {
    File f = LittleFS.open(path, "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send_P(200, "text/html", fallback);
  }
}

static void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static const char* validateSetup(const String& ssid, const String& pass, const String& room, const String& token) {
  if (!ssid.length()) return "Wi-Fi name is empty";
  if (ssid.length() > 32) return "Wi-Fi name is longer than 32 characters";
  if (pass.length() && (pass.length() < 8 || pass.length() > 63)) return "Password must be empty or 8-63 characters";
  if (!room.length() || room.length() > 16) return "Room ID: 1-16 letters, digits, - or _";
  for (size_t i = 0; i < room.length(); i++) {
    char ch = room[i];
    if (!isalnum((unsigned char)ch) && ch != '-' && ch != '_') return "Room ID: 1-16 letters, digits, - or _";
  }
  if (!token.length()) return "Device token is empty";
  if (token.length() > 64) return "Device token is longer than 64 characters";
  for (size_t i = 0; i < token.length(); i++) {
    char ch = token[i];
    if (!isalnum((unsigned char)ch) && !strchr("-_.~+/=", ch)) return "Device token has characters that are not allowed";
  }
  return nullptr;
}

static void handleState() {
  DeviceState s = snapshot();
  NetConfig c = cfgCopy();
  char room[40], uid[48], t[16], rh[16], chip[16], rssi[12], epoch[16];
  jsonStr(room, sizeof room, c.room);
  jsonStr(uid, sizeof uid, s.lastUid);
  jsonNum(t, sizeof t, s.dhtError ? NAN : s.tempC, "%.1f");
  jsonNum(rh, sizeof rh, s.dhtError ? NAN : s.rh, "%.0f");
  jsonNum(chip, sizeof chip, gChipTemp, "%.1f");
  if (WiFi.status() == WL_CONNECTED) snprintf(rssi, sizeof rssi, "%d", WiFi.RSSI()); else strcpy(rssi, "null");
  time_t now = time(nullptr);
  if (epochValid(now)) snprintf(epoch, sizeof epoch, "%lld", (long long)now); else strcpy(epoch, "null");
  char buf[640];
  snprintf(buf, sizeof buf,
    "{\"room\":%s,\"epoch\":%s,\"temp_c\":%s,\"rh\":%s,\"status\":\"SAFE\",\"flame\":%s,\"door\":\"%s\","
    "\"last_uid\":%s,\"rssi\":%s,\"chip_c\":%s,\"fw\":\"" FW_VERSION "\",\"conn\":\"%s\",\"uptime_s\":%lu,"
    "\"waiting\":%d,\"ip\":\"%s\",\"sensor_error\":%s}",
    room, epoch, t, rh, s.flame ? "true" : "false", s.doorUnlocked ? "UNLOCKED" : "LOCKED",
    uid, rssi, chip, connLabel(), (unsigned long)(esp_timer_get_time() / 1000000ULL),
    outboxSize(), gIp, (s.dhtError || s.rfidError) ? "true" : "false");
  sendJson(200, buf);
}

static void handleScan() {
  xSemaphoreTake(gScanMtx, portMAX_DELAY); String js = gScanJson; xSemaphoreGive(gScanMtx);
  sendJson(200, js);
}

static void handleSetup() {
  // Only from a laptop joined to the device's own access point.
  if (!gApUp || server.client().localIP() != WiFi.softAPIP()) {
    sendJson(403, "{\"error\":\"Setup is only possible through the SmartClass access point\"}");
    return;
  }
  String ssid = server.arg("ssid"), pass = server.arg("pass"), room = server.arg("room"), token = server.arg("token");
  ssid.trim(); room.trim(); token.trim();
  const char* err = validateSetup(ssid, pass, room, token);
  if (err) {
    char e[100]; jsonStr(e, sizeof e, err);
    sendJson(400, String("{\"error\":") + e + "}");
    return;
  }
  memset(&gPending, 0, sizeof gPending);
  strlcpy(gPending.ssid, ssid.c_str(), sizeof gPending.ssid);
  strlcpy(gPending.pass, pass.c_str(), sizeof gPending.pass);
  strlcpy(gPending.room, room.c_str(), sizeof gPending.room);
  strlcpy(gPending.token, token.c_str(), sizeof gPending.token);
  gSetupSubmitted = true;
  sendJson(200, "{\"ok\":true}");
}

static void handleSetupState() {
  NetConfig c = cfgCopy();
  const char* st = gNet == NET_CONNECTED ? "CONNECTED" : gNet == NET_SETUP ? "SETUP" : "CONNECTING";
  if (gSetupSubmitted) st = "CONNECTING";
  char ssid[80], err[100];
  jsonStr(ssid, sizeof ssid, c.ssid);
  jsonStr(err, sizeof err, gSetupError);
  char buf[256];
  snprintf(buf, sizeof buf, "{\"state\":\"%s\",\"ssid\":%s,\"error\":%s,\"ip\":\"%s\"}", st, ssid, err, gIp);
  sendJson(200, buf);
}

static void webTask(void*) {
  // Not on the task watchdog: a slow browser connection can hold handleClient() for seconds.
  server.on("/", HTTP_GET, []() {
    if (gNet == NET_CONNECTED || gNet == NET_RECONNECTING) sendPage("/index.html", STATUS_HTML);
    else sendPage("/setup.html", SETUP_HTML);
  });
  server.on("/status", HTTP_GET, []() { sendPage("/index.html", STATUS_HTML); });
  server.on("/setup", HTTP_GET, []() { sendPage("/setup.html", SETUP_HTML); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/setup", HTTP_POST, handleSetup);
  server.on("/api/setup-state", HTTP_GET, handleSetupState);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(PERIOD_WEB_MS));
  }
}

// ============================================================================
//  Status line (fixed format; trend comes in Level 3)
// ============================================================================
static void printStatusLine() {
  DeviceState s = snapshot();
  char t[12], rh[12], hi[12], chip[12], rssi[12];
  float tc = s.dhtError ? NAN : s.tempC, h = s.dhtError ? NAN : s.rh;
  if (isnan(tc)) strcpy(t, "-");  else snprintf(t, sizeof t, "%.1fC", tc);
  if (isnan(h))  strcpy(rh, "-"); else snprintf(rh, sizeof rh, "%.0f%%", h);
  float hic = heatIndexC(tc, h);
  if (isnan(hic)) strcpy(hi, "-"); else snprintf(hi, sizeof hi, "%.1fC", hic);
  float ct = gChipTemp;
  if (isnan(ct)) strcpy(chip, "-"); else snprintf(chip, sizeof chip, "%.1fC", ct);
  if (WiFi.status() == WL_CONNECTED) snprintf(rssi, sizeof rssi, "%d", WiFi.RSSI()); else strcpy(rssi, "-");
  logLine("[STATUS] up=%lus T=%s RH=%s HI=%s flame=%d door=%s state=SAFE trend=- rssi=%s "
          "heap=%luk psram=%luk chip=%s fw=%s",
          (unsigned long)(esp_timer_get_time() / 1000000ULL), t, rh, hi, s.flame ? 1 : 0,
          s.doorUnlocked ? "UNLOCKED" : "LOCKED", rssi,
          (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getFreePsram() / 1024),
          chip, FW_VERSION);
}

// ============================================================================
//  TASKS (Level 1, with Level 2 hooks)
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
          rfid.PCD_Init();
          err = false; oks = 0;
          STATE_SET(g.rfidError = false);
          logLine("[ERROR] SENSOR CLEARED: RFID");
        }
      } else {
        char uid[32];
        if (rfidReadCard(uid, sizeof uid)) {
          lastSeen = millis();
          if (!onReader || strcmp(uid, cur) != 0) {
            onReader = true;
            strlcpy(cur, uid, sizeof cur);
            STATE_SET(strlcpy(g.lastUid, uid, sizeof g.lastUid));
            logLine("[CARD] uid=%s", uid);
            enqueueEvent("CARD_TAP", uid);          // Level 2: sent at once
          }
        } else if (onReader && millis() - lastSeen > CARD_GONE_MS) {
          onReader = false;
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
  int tick = PERIOD_DHT_S - 2;
  uint8_t fails = 0;
  bool err = false;
  for (;;) {
    wdtFeed();
    if (++tick >= PERIOD_DHT_S) {
      tick = 0;
      float t, h;
      if (dht11Read(t, h)) {
        fails = 0;
        STATE_SET({ g.tempC = t; g.rh = h; });
        if (err) {
          err = false;
          STATE_SET(g.dhtError = false);
          logLine("[ERROR] SENSOR CLEARED: DHT11");
        }
      } else {
        if (fails < 255) fails++;
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
static void controlTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    if (gHangRequested) {
      logLine("[CMD] main task frozen - watchdog must restart the board within 10 s");
      volatile uint32_t spin = 0;
      for (;;) { spin++; }
    }
    DeviceState s = snapshot();
    // Device Signals priority (lower number wins): 6 Sensor error, 10 Setup/offline, 11 Safe.
    uint32_t c = COLOR_GREEN;
    if (s.dhtError || s.rfidError) c = COLOR_MAGENTA;
    else if (gNet != NET_CONNECTED) c = COLOR_BLUE;
    gLedColor = c;
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_CONTROL_MS));
  }
}

// ---- Core 0: LED heartbeat, 250 ms on / 250 ms off --------------------------
static void ledTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  bool on = false;
  for (;;) {
    on = !on;
    rgbWrite(on ? gLedColor : 0);
    wdtFeed();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PERIOD_LED_MS));
  }
}

// ---- Core 0: status line every second + telemetry record every 5 s ---------
static void statusTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  uint32_t tick = 0;
  for (;;) {
    gChipTemp = temperatureRead();
    printStatusLine();
    if (++tick % TELEMETRY_EVERY_S == 0) {
      NetConfig c = cfgCopy();
      if (c.token[0]) enqueueTelemetry();       // only once the device has been set up
    }
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
  logLine("[CMD]   reset   - clear Wi-Fi details, restart in setup mode");
  logLine("[CMD]   replay  - send the last telemetry record again (same seq) to show it is rejected");
  logLine("[CMD]   csi     - Wi-Fi CSI rate, subcarriers, channel (Level 11)");
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
  if (WiFi.status() == WL_CONNECTED) logLine("[CMD] wifi_rssi=%d", WiFi.RSSI());
  else logLine("[CMD] wifi_rssi=-");
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
  else if (!strcmp(line, "reset"))  {
    logLine("[CMD] reset: clearing Wi-Fi details, restarting in setup mode");
    clearConfig();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
  }
  else if (!strcmp(line, "replay")) { logLine("[CMD] replay: sending the last telemetry record again"); gReplayRequested = true; }
  else if (!strcmp(line, "csi"))    logLine("[CMD] csi: available from Level 11");
  else                              logLine("[CMD] unknown command '%s' - type help", line);
}

static void serialTask(void*) {
  wdtAdd();
  TickType_t last = xTaskGetTickCount();
  char line[64]; size_t len = 0;
  uint32_t lastCharMs = 0;
  for (;;) {
    while (Serial.available() > 0) {
      int c = Serial.read();
      if (c < 0) continue;
      if (c == '\n' || c == '\r') {
        if (len) { line[len] = 0; handleCommand(line); len = 0; }
      } else if (len < sizeof(line) - 1) {
        line[len++] = (char)c;
        lastCharMs = millis();
      }
    }
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
  pinMode(PIN_FLAME, INPUT_PULLUP);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  delay(150);

  // --- FIRST CHECK: flame at start-up decides the door ---
  int votes = 0;
  for (int i = 0; i < 5; i++) { if (readFlameRaw()) votes++; delay(5); }
  bool flameAtBoot = votes >= 3;
  g.flame = flameAtBoot;
  g.doorUnlocked = flameAtBoot;

  // --- serial + logging ---
  gLogMtx = xSemaphoreCreateMutex();
  gCfgMtx = xSemaphoreCreateMutex();
  gObMtx = xSemaphoreCreateMutex();
  gScanMtx = xSemaphoreCreateMutex();
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 5000) delay(10);
  delay(300);

  // --- boot reason ---
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr != ESP_RST_POWERON && gWdtFiredMagic == WDT_MAGIC) gResetReason = "WATCHDOG";
  else gResetReason = resetReasonText(rr);
  gWdtFiredMagic = 0;

  // --- saved details + seq, then the BOOT event (queued until the server is reachable) ---
  seqInit();
  bool haveCfg = loadConfig(gCfg);
  char bootDetail[32];
  snprintf(bootDetail, sizeof bootDetail, "reason=%s", gResetReason);
  enqueueEvent("BOOT", bootDetail);
  logLine("[EVENT] DOOR %s (start-up flame check: %s)",
          flameAtBoot ? "UNLOCKED" : "LOCKED", flameAtBoot ? "flame" : "no flame");
  logLine("[CMD] SmartClass %s fw=%s - type help", DEVICE_ID, FW_VERSION);

  // --- LED ---
  gLedColor = COLOR_BLUE;
  rgbWrite(0);

  // --- RC522 ---
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();
  logLine("[CARD] reader version=0x%02X", rfid.PCD_ReadRegister(MFRC522::VersionReg));

  // --- flash file system (web pages) ---
  fsInit();

  // --- Wi-Fi ---
  WiFi.persistent(false);              // our own NVS keys hold the details
  WiFi.setAutoReconnect(false);        // we control the retry gaps
  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.setHostname(MDNS_HOST);
  if (haveCfg) {
    WiFi.mode(WIFI_STA);
    logLine("[NET] saved details found (room %s) - connecting, no setup needed", gCfg.room);
    startConnect(gCfg);
  } else {
    logLine("[NET] no saved network");
    enterSetup();
  }

  // --- watchdog, then tasks ---
  watchdogInit();
  //        function     name       stack prio core period
  startTask(flameTask,   "flame",   3072, 4,   1,   PERIOD_FLAME_MS);
  startTask(controlTask, "control", 4096, 3,   1,   PERIOD_CONTROL_MS);
  startTask(rfidTask,    "rfid",    4096, 2,   1,   PERIOD_RFID_MS);
  startTask(dhtTask,     "dht11",   3072, 2,   1,   PERIOD_DHT_S * 1000);
  startTask(ledTask,     "led",     3072, 3,   0,   PERIOD_LED_MS);
  startTask(statusTask,  "status",  4096, 2,   0,   PERIOD_STATUS_MS);
  startTask(serialTask,  "serial",  4096, 1,   0,   PERIOD_SERIAL_MS);
  startTask(netTask,     "net",     6144, 2,   0,   PERIOD_NET_MS);
  startTask(webTask,     "web",     8192, 1,   0,   PERIOD_WEB_MS);
  startTask(cloudTask,   "cloud",   12288, 1,  0,   0);
}

void loop() {
  vTaskDelete(NULL);
}
