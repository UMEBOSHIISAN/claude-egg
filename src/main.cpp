// Claude-EGG firmware — Phase B (BLE NUS heartbeat + local keyboard debug)
//
// The Mac daemon (tools/claude_usage_digest.py) emits a single-line JSON
// heartbeat every 10 s over the Nordic UART Service. We parse it into
// PetState and recompute mood. The QWERTY keys still work as a debug
// override so you can force stage/mood without the daemon running.
//
// Heartbeat JSON:
//   { "type":"egg.heartbeat", "v":1,
//     "lifetime_min":12847, "today_min":412,
//     "active_now":true, "late_night_streak":2,
//     "silent_hours":0, "ts":"2026-04-20T14:33:02Z" }
//
// NUS UUIDs (standard Nordic):
//   Service : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
//   RX (write, host→device) : 6E400002-...
//   TX (notify, device→host): 6E400003-...

#include <M5Cardputer.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

#ifndef DEFAULT_BUDDY
#define DEFAULT_BUDDY "default"
#endif

// ---- BLE NUS UUIDs --------------------------------------------------------

static const char* NUS_SERVICE_UUID   = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_RX_CHAR_UUID   = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* NUS_TX_CHAR_UUID   = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* BLE_DEVICE_NAME    = "Claude-EGG";

// ---- pet state ------------------------------------------------------------

enum Stage {
  STAGE_EGG = 0,
  STAGE_CHILD,   // tadpole
  STAGE_TEEN,    // young frog
  STAGE_ADULT,   // toad
  STAGE_ELDER,   // pond-sage
  STAGE_COUNT
};

enum Mood {
  MOOD_HAPPY = 0,
  MOOD_EXCITED,
  MOOD_TIRED,
  MOOD_GRUMPY,
  MOOD_SICK,      // 3 a.m. belly-up, X eyes
  MOOD_LONELY,
  MOOD_ZEN,
  MOOD_COUNT
};

static const char* kStageName[STAGE_COUNT] = {
  "egg", "tadpole", "frog", "toad", "pond-sage"
};

static const char* kMoodName[MOOD_COUNT] = {
  "happy", "excited", "tired", "grumpy", "sick", "lonely", "zen"
};

enum Mode {
  MODE_NORMAL = 0,
  MODE_POSTMORTEM,
};

struct PetState {
  Stage stage;
  Mood  mood;
  int   today_min;
  int   today_late_min;
  int   yesterday_min;
  int   yesterday_late_min;
  int   lifetime_min;
  bool  active_now;
  int   late_night_streak;
  int   silent_hours;
  bool  ble_connected;
  bool  manual_override;     // true if the keyboard has taken over
  Mode  mode;
  unsigned long last_heartbeat_ms;
};

static PetState g_state = {
  .stage = STAGE_EGG,
  .mood  = MOOD_HAPPY,
  .today_min = 0,
  .today_late_min = 0,
  .yesterday_min = 0,
  .yesterday_late_min = 0,
  .lifetime_min = 0,
  .active_now = false,
  .late_night_streak = 0,
  .silent_hours = 0,
  .ble_connected = false,
  .manual_override = false,
  .mode = MODE_NORMAL,
  .last_heartbeat_ms = 0,
};

static bool g_dirty = true;

// Stage thresholds (mirror the default pack manifest).
static Stage stageFromLifetimeMinutes(int m) {
  if (m >= 18000) return STAGE_ELDER;
  if (m >= 3000)  return STAGE_ADULT;
  if (m >= 600)   return STAGE_TEEN;
  if (m >= 30)    return STAGE_CHILD;
  return STAGE_EGG;
}

static Mood moodFromState(const PetState& s) {
  if (s.late_night_streak >= 2) return MOOD_SICK;
  if (s.silent_hours >= 24 * 7) return MOOD_ZEN;
  if (s.silent_hours >= 24)     return MOOD_LONELY;
  if (s.today_min >= 480)       return MOOD_GRUMPY;  // 8h+
  if (s.today_min >= 240)       return MOOD_TIRED;   // 4–8h
  if (s.active_now)             return MOOD_EXCITED;
  return MOOD_HAPPY;
}

// ---- display constants ----------------------------------------------------

static const int kScreenW = 240;
static const int kScreenH = 135;
static const int kHeaderH = 16;
static const int kFooterH = 18;
static const int kBodyTop = kHeaderH;
static const int kBodyBot = kScreenH - kFooterH;
static const int kBodyCx  = kScreenW / 2;
static const int kBodyCy  = (kBodyTop + kBodyBot) / 2;

static const uint16_t COL_BG     = 0x0000;
static const uint16_t COL_BODY   = 0x4EA5;
static const uint16_t COL_BODY2  = 0x2D05;
static const uint16_t COL_EYE_W  = 0xFFFF;
static const uint16_t COL_EYE_B  = 0x0000;
static const uint16_t COL_SICK   = 0xA254;
static const uint16_t COL_GAUNT  = 0x7BCF;
static const uint16_t COL_TEXT   = 0xFFFF;
static const uint16_t COL_DIM    = 0x7BEF;
static const uint16_t COL_ACCENT = 0xFD20;
static const uint16_t COL_BLE_OK = 0x07E0;  // green
static const uint16_t COL_BLE_NO = 0x780F;  // magenta

// ---- rendering (same shape renderer as Phase A) ---------------------------

static uint16_t bodyTintForMood(Mood m) {
  switch (m) {
    case MOOD_SICK:   return COL_SICK;
    case MOOD_LONELY:
    case MOOD_ZEN:    return COL_GAUNT;
    case MOOD_GRUMPY: return COL_BODY2;
    default:          return COL_BODY;
  }
}

static void drawEye(int cx, int cy, int r, Mood mood) {
  if (mood == MOOD_SICK) {
    M5Cardputer.Display.drawLine(cx - r, cy - r, cx + r, cy + r, COL_EYE_B);
    M5Cardputer.Display.drawLine(cx - r, cy + r, cx + r, cy - r, COL_EYE_B);
    return;
  }
  if (mood == MOOD_TIRED || mood == MOOD_ZEN) {
    M5Cardputer.Display.drawLine(cx - r, cy, cx + r, cy, COL_EYE_B);
    return;
  }
  M5Cardputer.Display.fillCircle(cx, cy, r, COL_EYE_W);
  int px = cx;
  int py = cy;
  if (mood == MOOD_GRUMPY)  py -= 1;
  if (mood == MOOD_EXCITED) py -= 1;
  M5Cardputer.Display.fillCircle(px, py, r / 2, COL_EYE_B);
}

static void drawMouth(int cx, int cy, Mood mood, int w) {
  switch (mood) {
    case MOOD_HAPPY:
    case MOOD_EXCITED:
      M5Cardputer.Display.drawLine(cx - w,     cy,     cx - w / 2, cy + 3, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx - w / 2, cy + 3, cx + w / 2, cy + 3, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx + w / 2, cy + 3, cx + w,     cy,     COL_EYE_B);
      break;
    case MOOD_TIRED:
    case MOOD_ZEN:
      M5Cardputer.Display.drawLine(cx - w / 2, cy + 2, cx + w / 2, cy + 2, COL_EYE_B);
      break;
    case MOOD_GRUMPY:
      M5Cardputer.Display.drawLine(cx - w,     cy + 3, cx - w / 2, cy,     COL_EYE_B);
      M5Cardputer.Display.drawLine(cx - w / 2, cy,     cx + w / 2, cy,     COL_EYE_B);
      M5Cardputer.Display.drawLine(cx + w / 2, cy,     cx + w,     cy + 3, COL_EYE_B);
      break;
    case MOOD_SICK:
      M5Cardputer.Display.drawLine(cx - w,     cy + 1, cx - w / 3, cy - 1, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx - w / 3, cy - 1, cx + w / 3, cy + 1, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx + w / 3, cy + 1, cx + w,     cy - 1, COL_EYE_B);
      break;
    case MOOD_LONELY:
      M5Cardputer.Display.drawLine(cx - w / 3, cy + 2, cx + w / 3, cy + 2, COL_DIM);
      break;
    default: break;
  }
}

static void drawEgg() {
  int cx = kBodyCx, cy = kBodyCy;
  M5Cardputer.Display.fillEllipse(cx, cy, 22, 30, 0xF79E);
  M5Cardputer.Display.drawEllipse(cx, cy, 22, 30, COL_DIM);
  if (g_state.late_night_streak > 0) {
    M5Cardputer.Display.drawLine(cx - 4, cy - 20, cx,     cy - 16, COL_EYE_B);
    M5Cardputer.Display.drawLine(cx,     cy - 16, cx + 3, cy - 22, COL_EYE_B);
  }
}

static void drawTadpole(Mood mood) {
  int cx = kBodyCx, cy = kBodyCy;
  uint16_t tint = bodyTintForMood(mood);
  M5Cardputer.Display.fillCircle(cx, cy, 16, tint);
  M5Cardputer.Display.fillTriangle(cx - 14, cy, cx - 34, cy - 6, cx - 34, cy + 6, tint);
  drawEye(cx - 5, cy - 4, 3, mood);
  drawEye(cx + 5, cy - 4, 3, mood);
  drawMouth(cx, cy + 5, mood, 5);
}

static void drawFrog(Mood mood, int bodyW, int bodyH) {
  int cx = kBodyCx, cy = kBodyCy;
  uint16_t tint = bodyTintForMood(mood);

  if (mood == MOOD_SICK) {
    M5Cardputer.Display.fillEllipse(cx, cy + 4, bodyW, bodyH - 2, COL_EYE_W);
    M5Cardputer.Display.drawEllipse(cx, cy + 4, bodyW, bodyH - 2, COL_EYE_B);
    drawEye(cx - 8, cy - 2, 4, MOOD_SICK);
    drawEye(cx + 8, cy - 2, 4, MOOD_SICK);
    drawMouth(cx, cy + 8, MOOD_SICK, 8);
    M5Cardputer.Display.drawLine(cx - bodyW + 4, cy - bodyH, cx - bodyW + 2, cy - bodyH + 8, COL_SICK);
    M5Cardputer.Display.drawLine(cx + bodyW - 4, cy - bodyH, cx + bodyW - 2, cy - bodyH + 8, COL_SICK);
    return;
  }

  M5Cardputer.Display.fillEllipse(cx, cy + 4, bodyW, bodyH, tint);
  M5Cardputer.Display.fillEllipse(cx, cy + 10, bodyW - 8, bodyH - 6, 0xF79E);
  int eyeCy = cy - bodyH + 4;
  int eyeDx = bodyW / 2 - 2;
  M5Cardputer.Display.fillCircle(cx - eyeDx, eyeCy, 6, tint);
  M5Cardputer.Display.fillCircle(cx + eyeDx, eyeCy, 6, tint);
  drawEye(cx - eyeDx, eyeCy, 3, mood);
  drawEye(cx + eyeDx, eyeCy, 3, mood);
  drawMouth(cx, cy + 2, mood, 10);
  M5Cardputer.Display.fillCircle(cx - bodyW, cy + bodyH - 2, 5, tint);
  M5Cardputer.Display.fillCircle(cx + bodyW, cy + bodyH - 2, 5, tint);
}

static void drawPondSage() {
  int cx = kBodyCx, cy = kBodyCy;
  M5Cardputer.Display.fillEllipse(cx, cy + 2, 36, 24, COL_GAUNT);
  M5Cardputer.Display.drawEllipse(cx, cy + 2, 36, 24, COL_DIM);
  M5Cardputer.Display.drawLine(cx - 12, cy - 6, cx - 4,  cy - 6, COL_EYE_B);
  M5Cardputer.Display.drawLine(cx + 4,  cy - 6, cx + 12, cy - 6, COL_EYE_B);
  M5Cardputer.Display.drawLine(cx - 6,  cy + 6, cx + 6,  cy + 6, COL_EYE_B);
  M5Cardputer.Display.fillTriangle(cx - 40, cy - 20, cx - 34, cy - 24, cx - 32, cy - 18, COL_ACCENT);
  M5Cardputer.Display.fillTriangle(cx + 34, cy + 22, cx + 42, cy + 20, cx + 40, cy + 26, COL_ACCENT);
}

static const char* postmortemVerdict(int min, int late, int streak) {
  if (min == 0)                     return "A quiet day. Good.";
  if (late > 0 && streak >= 2)      return "Past 1 a.m. again. Please rest.";
  if (late > 0)                     return "Some minutes past 1 a.m.";
  if (min >= 480)                   return "You went hard. Drink water.";
  if (min >= 240)                   return "A long day.";
  if (min >= 60)                    return "Balanced.";
  return "Light usage.";
}

static void drawPostmortem() {
  M5Cardputer.Display.fillScreen(COL_BG);

  // Header strip
  M5Cardputer.Display.fillRect(0, 0, kScreenW, kHeaderH, 0x2104);
  M5Cardputer.Display.setTextColor(COL_ACCENT, 0x2104);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.print("POSTMORTEM :: yesterday");
  M5Cardputer.Display.setTextColor(COL_DIM, 0x2104);
  M5Cardputer.Display.setCursor(kScreenW - 36, 4);
  M5Cardputer.Display.print("p:back");

  // Big minute figure
  M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
  M5Cardputer.Display.setTextSize(3);
  char big[12];
  snprintf(big, sizeof(big), "%d", g_state.yesterday_min);
  int bigW = M5Cardputer.Display.textWidth(big);
  M5Cardputer.Display.setCursor((kScreenW - bigW) / 2, 28);
  M5Cardputer.Display.print(big);

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(COL_DIM, COL_BG);
  const char* unit = "active minutes";
  int uW = M5Cardputer.Display.textWidth(unit);
  M5Cardputer.Display.setCursor((kScreenW - uW) / 2, 54);
  M5Cardputer.Display.print(unit);

  // Metrics row
  M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
  M5Cardputer.Display.setCursor(16, 72);
  M5Cardputer.Display.printf("late 1am-5am : %dm", g_state.yesterday_late_min);
  M5Cardputer.Display.setCursor(16, 82);
  M5Cardputer.Display.printf("late streak  : %d day(s)", g_state.late_night_streak);
  M5Cardputer.Display.setCursor(16, 92);
  M5Cardputer.Display.printf("lifetime     : %dm", g_state.lifetime_min);

  // Verdict line
  const char* v = postmortemVerdict(
    g_state.yesterday_min, g_state.yesterday_late_min, g_state.late_night_streak);
  uint16_t vc = (g_state.yesterday_late_min > 0) ? COL_SICK : COL_BLE_OK;
  M5Cardputer.Display.setTextColor(vc, COL_BG);
  int vW = M5Cardputer.Display.textWidth(v);
  M5Cardputer.Display.setCursor((kScreenW - vW) / 2, 110);
  M5Cardputer.Display.print(v);

  // Footer hint
  M5Cardputer.Display.fillRect(0, kScreenH - kFooterH, kScreenW, kFooterH, 0x2104);
  M5Cardputer.Display.setTextColor(COL_DIM, 0x2104);
  M5Cardputer.Display.setCursor(4, kScreenH - kFooterH + 5);
  M5Cardputer.Display.print("today so far: ");
  M5Cardputer.Display.setTextColor(COL_TEXT, 0x2104);
  M5Cardputer.Display.printf("%dm (%dm late)", g_state.today_min, g_state.today_late_min);
}

static void drawHeader() {
  M5Cardputer.Display.fillRect(0, 0, kScreenW, kHeaderH, 0x2104);
  M5Cardputer.Display.setTextColor(COL_TEXT, 0x2104);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.print("Claude-EGG :: ");
  M5Cardputer.Display.print(DEFAULT_BUDDY);

  // BLE status badge
  uint16_t badge = g_state.ble_connected ? COL_BLE_OK : COL_BLE_NO;
  M5Cardputer.Display.fillCircle(kScreenW - 10, 8, 3, badge);
  if (g_state.manual_override) {
    M5Cardputer.Display.setTextColor(COL_ACCENT, 0x2104);
    M5Cardputer.Display.setCursor(kScreenW - 60, 4);
    M5Cardputer.Display.print("MANUAL");
  }
}

static void drawFooter() {
  int y = kScreenH - kFooterH;
  M5Cardputer.Display.fillRect(0, y, kScreenW, kFooterH, 0x2104);
  M5Cardputer.Display.setTextColor(COL_TEXT, 0x2104);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, y + 2);
  M5Cardputer.Display.printf("%s / %s", kStageName[g_state.stage], kMoodName[g_state.mood]);
  M5Cardputer.Display.setCursor(4, y + 10);
  M5Cardputer.Display.setTextColor(COL_DIM, 0x2104);
  M5Cardputer.Display.printf("today %dm  life %dm", g_state.today_min, g_state.lifetime_min);
}

static void drawPet() {
  M5Cardputer.Display.fillRect(0, kBodyTop, kScreenW, kBodyBot - kBodyTop, COL_BG);
  switch (g_state.stage) {
    case STAGE_EGG:   drawEgg(); break;
    case STAGE_CHILD: drawTadpole(g_state.mood); break;
    case STAGE_TEEN:  drawFrog(g_state.mood, 32, 20); break;
    case STAGE_ADULT: drawFrog(g_state.mood, 44, 26); break;
    case STAGE_ELDER: drawPondSage(); break;
    default: break;
  }
  if (g_state.active_now) {
    M5Cardputer.Display.fillCircle(kScreenW - 8, kBodyTop + 6, 3, COL_ACCENT);
  }
}

static void redrawAll() {
  if (g_state.mode == MODE_POSTMORTEM) {
    drawPostmortem();
    return;
  }
  M5Cardputer.Display.fillScreen(COL_BG);
  drawHeader();
  drawPet();
  drawFooter();
}

// ---- BLE NUS server -------------------------------------------------------

static NimBLECharacteristic* g_tx_char = nullptr;

static void applyHeartbeat(const JsonDocument& doc) {
  // Version / type gate.
  const char* type = doc["type"] | "";
  int v = doc["v"] | 0;
  if (strcmp(type, "egg.heartbeat") != 0 || v != 1) return;

  g_state.lifetime_min       = doc["lifetime_min"]       | g_state.lifetime_min;
  g_state.today_min          = doc["today_min"]          | g_state.today_min;
  g_state.today_late_min     = doc["today_late_min"]     | 0;
  g_state.yesterday_min      = doc["yesterday_min"]      | 0;
  g_state.yesterday_late_min = doc["yesterday_late_min"] | 0;
  g_state.active_now         = doc["active_now"]         | false;
  g_state.late_night_streak  = doc["late_night_streak"]  | 0;
  g_state.silent_hours       = doc["silent_hours"]       | 0;

  if (!g_state.manual_override) {
    g_state.stage = stageFromLifetimeMinutes(g_state.lifetime_min);
    g_state.mood  = moodFromState(g_state);
  }
  g_state.last_heartbeat_ms = millis();
  g_dirty = true;
}

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string payload = c->getValue();
    if (payload.empty()) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return;
    applyHeartbeat(doc);
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s) override {
    g_state.ble_connected = true;
    g_dirty = true;
  }
  void onDisconnect(NimBLEServer* s) override {
    g_state.ble_connected = false;
    g_dirty = true;
    NimBLEDevice::startAdvertising();
  }
};

static void startBle() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setMTU(517);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(NUS_SERVICE_UUID);

  NimBLECharacteristic* rx = svc->createCharacteristic(
    NUS_RX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(new RxCallbacks());

  g_tx_char = svc->createCharacteristic(
    NUS_TX_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setName(BLE_DEVICE_NAME);
  adv->setScanResponse(true);
  NimBLEDevice::startAdvertising();
}

// ---- input (debug override) -----------------------------------------------

static void handleKey(char c) {
  bool changed = true;
  switch (c) {
    case '1': g_state.stage = STAGE_EGG;   break;
    case '2': g_state.stage = STAGE_CHILD; break;
    case '3': g_state.stage = STAGE_TEEN;  break;
    case '4': g_state.stage = STAGE_ADULT; break;
    case '5': g_state.stage = STAGE_ELDER; break;
    case 'q': g_state.mood = MOOD_HAPPY;   break;
    case 'w': g_state.mood = MOOD_EXCITED; break;
    case 'e': g_state.mood = MOOD_TIRED;   break;
    case 'r': g_state.mood = MOOD_GRUMPY;  break;
    case 't': g_state.mood = MOOD_SICK;    break;
    case 'y': g_state.mood = MOOD_LONELY;  break;
    case 'a': g_state.active_now = !g_state.active_now; break;
    case '+': g_state.today_min += 30; g_state.lifetime_min += 30; break;
    case '-': g_state.today_min = max(0, g_state.today_min - 30); break;
    case '0':
      // exit manual override — heartbeat-driven state takes over again
      g_state.manual_override = false;
      break;
    case 'p':
      g_state.mode = (g_state.mode == MODE_POSTMORTEM) ? MODE_NORMAL : MODE_POSTMORTEM;
      break;
    default: changed = false; break;
  }
  if (changed) {
    if (c >= '1' && c <= '5')      g_state.manual_override = true;
    if (c == 'q' || c == 'w' || c == 'e' ||
        c == 'r' || c == 't' || c == 'y') g_state.manual_override = true;
    g_dirty = true;
  }
}

// ---- arduino entry points -------------------------------------------------

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(80);
  redrawAll();
  startBle();
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    for (auto c : status.word) handleKey(c);
  }

  static Mode last_mode = MODE_NORMAL;
  if (g_dirty) {
    bool mode_changed = (last_mode != g_state.mode);
    if (g_state.mode == MODE_POSTMORTEM) {
      drawPostmortem();                       // does its own fillScreen
    } else {
      if (mode_changed) {
        M5Cardputer.Display.fillScreen(COL_BG);
      }
      drawHeader();
      drawPet();
      drawFooter();
    }
    last_mode = g_state.mode;
    g_dirty = false;
  }

  delay(20);
}
