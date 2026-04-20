// Claude-EGG firmware — Phase A (local demo, no BLE yet)
//
// Renders the tartan reference pack as simple shapes + ASCII so we can see
// the pet on the Cardputer before any sprite art exists. QWERTY keys let
// you cycle stage and mood by hand while the BLE NUS side is offline.
//
//   Stage:  1 egg   2 tadpole   3 frog   4 toad   5 elder
//   Mood:   q happy  w excited  e tired  r grumpy  t sick  y lonely
//
// Phase B will replace the manual keys with a heartbeat JSON received over
// BLE NUS from the Mac daemon. Phase C will swap the shape renderer for
// LittleFS-backed GIFs.

#include <M5Cardputer.h>

#ifndef DEFAULT_BUDDY
#define DEFAULT_BUDDY "default"
#endif

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
  MOOD_COUNT
};

static const char* kStageName[STAGE_COUNT] = {
  "egg", "tadpole", "frog", "toad", "pond-sage"
};

static const char* kMoodName[MOOD_COUNT] = {
  "happy", "excited", "tired", "grumpy", "sick", "lonely"
};

// Rough canvas geometry for 240x135.
static const int kScreenW = 240;
static const int kScreenH = 135;
static const int kHeaderH = 16;
static const int kFooterH = 18;
static const int kBodyTop = kHeaderH;
static const int kBodyBot = kScreenH - kFooterH;
static const int kBodyCx  = kScreenW / 2;
static const int kBodyCy  = (kBodyTop + kBodyBot) / 2;

// Color palette (RGB565)
static const uint16_t COL_BG     = 0x0000;  // black
static const uint16_t COL_BODY   = 0x4EA5;  // frog green
static const uint16_t COL_BODY2  = 0x2D05;  // darker belly
static const uint16_t COL_EYE_W  = 0xFFFF;  // white
static const uint16_t COL_EYE_B  = 0x0000;  // pupil
static const uint16_t COL_SICK   = 0xA254;  // pale sick green
static const uint16_t COL_CHONK  = 0x7F08;  // puffed lime
static const uint16_t COL_GAUNT  = 0x7BCF;  // dusty gray-green
static const uint16_t COL_TEXT   = 0xFFFF;
static const uint16_t COL_DIM    = 0x7BEF;
static const uint16_t COL_ACCENT = 0xFD20;  // amber

struct PetState {
  Stage stage;
  Mood  mood;
  int   today_min;       // today's Claude active minutes
  int   lifetime_min;    // cumulative lifetime minutes
  bool  active_now;      // session live right now
  int   late_night_streak;
};

static PetState g_state = {
  .stage = STAGE_TEEN,
  .mood  = MOOD_HAPPY,
  .today_min = 0,
  .lifetime_min = 0,
  .active_now = false,
  .late_night_streak = 0,
};

static bool g_dirty = true;

// ---- rendering ------------------------------------------------------------

static uint16_t bodyTintForMood(Mood m) {
  switch (m) {
    case MOOD_SICK:   return COL_SICK;
    case MOOD_LONELY: return COL_GAUNT;
    case MOOD_GRUMPY: return COL_BODY2;
    default:          return COL_BODY;
  }
}

static void drawEye(int cx, int cy, int r, Mood mood) {
  if (mood == MOOD_SICK) {
    // X eyes
    M5Cardputer.Display.drawLine(cx - r, cy - r, cx + r, cy + r, COL_EYE_B);
    M5Cardputer.Display.drawLine(cx - r, cy + r, cx + r, cy - r, COL_EYE_B);
    return;
  }
  if (mood == MOOD_TIRED) {
    // half-closed
    M5Cardputer.Display.drawLine(cx - r, cy, cx + r, cy, COL_EYE_B);
    return;
  }
  M5Cardputer.Display.fillCircle(cx, cy, r, COL_EYE_W);
  int px = cx;
  int py = cy;
  if (mood == MOOD_GRUMPY) py -= 1;
  if (mood == MOOD_EXCITED) { py -= 1; }
  M5Cardputer.Display.fillCircle(px, py, r / 2, COL_EYE_B);
}

static void drawMouth(int cx, int cy, Mood mood, int w) {
  switch (mood) {
    case MOOD_HAPPY:
    case MOOD_EXCITED:
      // smile (arc via two chords)
      M5Cardputer.Display.drawLine(cx - w, cy,     cx - w / 2, cy + 3, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx - w / 2, cy + 3, cx + w / 2, cy + 3, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx + w / 2, cy + 3, cx + w, cy,     COL_EYE_B);
      break;
    case MOOD_TIRED:
      M5Cardputer.Display.drawLine(cx - w / 2, cy + 2, cx + w / 2, cy + 2, COL_EYE_B);
      break;
    case MOOD_GRUMPY:
      // frown
      M5Cardputer.Display.drawLine(cx - w, cy + 3, cx - w / 2, cy,     COL_EYE_B);
      M5Cardputer.Display.drawLine(cx - w / 2, cy, cx + w / 2, cy,     COL_EYE_B);
      M5Cardputer.Display.drawLine(cx + w / 2, cy, cx + w, cy + 3,     COL_EYE_B);
      break;
    case MOOD_SICK:
      // wavy mouth
      M5Cardputer.Display.drawLine(cx - w, cy + 1, cx - w / 3, cy - 1, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx - w / 3, cy - 1, cx + w / 3, cy + 1, COL_EYE_B);
      M5Cardputer.Display.drawLine(cx + w / 3, cy + 1, cx + w, cy - 1, COL_EYE_B);
      break;
    case MOOD_LONELY:
      // tiny flat line
      M5Cardputer.Display.drawLine(cx - w / 3, cy + 2, cx + w / 3, cy + 2, COL_DIM);
      break;
    default: break;
  }
}

static void drawEgg() {
  int cx = kBodyCx;
  int cy = kBodyCy;
  // off-white egg
  M5Cardputer.Display.fillEllipse(cx, cy, 22, 30, 0xF79E);
  M5Cardputer.Display.drawEllipse(cx, cy, 22, 30, COL_DIM);
  // tiny crack at the top when late-night streak bites
  if (g_state.late_night_streak > 0) {
    M5Cardputer.Display.drawLine(cx - 4, cy - 20, cx, cy - 16, COL_EYE_B);
    M5Cardputer.Display.drawLine(cx,     cy - 16, cx + 3, cy - 22, COL_EYE_B);
  }
}

static void drawTadpole(Mood mood) {
  int cx = kBodyCx;
  int cy = kBodyCy;
  uint16_t tint = bodyTintForMood(mood);
  // head
  M5Cardputer.Display.fillCircle(cx, cy, 16, tint);
  // tail
  M5Cardputer.Display.fillTriangle(cx - 14, cy, cx - 34, cy - 6, cx - 34, cy + 6, tint);
  // eyes
  drawEye(cx - 5, cy - 4, 3, mood);
  drawEye(cx + 5, cy - 4, 3, mood);
  drawMouth(cx, cy + 5, mood, 5);
}

static void drawFrog(Mood mood, int bodyW, int bodyH) {
  int cx = kBodyCx;
  int cy = kBodyCy;
  uint16_t tint = bodyTintForMood(mood);

  if (mood == MOOD_SICK) {
    // belly-up: body flipped, belly color on top
    M5Cardputer.Display.fillEllipse(cx, cy + 4, bodyW, bodyH - 2, COL_EYE_W);
    M5Cardputer.Display.drawEllipse(cx, cy + 4, bodyW, bodyH - 2, COL_EYE_B);
    // X eyes in the middle
    drawEye(cx - 8, cy - 2, 4, MOOD_SICK);
    drawEye(cx + 8, cy - 2, 4, MOOD_SICK);
    // crooked mouth
    drawMouth(cx, cy + 8, MOOD_SICK, 8);
    // legs sticking up
    M5Cardputer.Display.drawLine(cx - bodyW + 4, cy - bodyH, cx - bodyW + 2, cy - bodyH + 8, COL_SICK);
    M5Cardputer.Display.drawLine(cx + bodyW - 4, cy - bodyH, cx + bodyW - 2, cy - bodyH + 8, COL_SICK);
    return;
  }

  // body
  M5Cardputer.Display.fillEllipse(cx, cy + 4, bodyW, bodyH, tint);
  // belly
  M5Cardputer.Display.fillEllipse(cx, cy + 10, bodyW - 8, bodyH - 6, 0xF79E);
  // head bumps with eyes
  int eyeCy = cy - bodyH + 4;
  int eyeDx = bodyW / 2 - 2;
  M5Cardputer.Display.fillCircle(cx - eyeDx, eyeCy, 6, tint);
  M5Cardputer.Display.fillCircle(cx + eyeDx, eyeCy, 6, tint);
  drawEye(cx - eyeDx, eyeCy, 3, mood);
  drawEye(cx + eyeDx, eyeCy, 3, mood);
  // mouth
  drawMouth(cx, cy + 2, mood, 10);
  // legs
  M5Cardputer.Display.fillCircle(cx - bodyW,     cy + bodyH - 2, 5, tint);
  M5Cardputer.Display.fillCircle(cx + bodyW,     cy + bodyH - 2, 5, tint);
}

static void drawPondSage() {
  int cx = kBodyCx;
  int cy = kBodyCy;
  // mummified, slightly shriveled
  M5Cardputer.Display.fillEllipse(cx, cy + 2, 36, 24, COL_GAUNT);
  M5Cardputer.Display.drawEllipse(cx, cy + 2, 36, 24, COL_DIM);
  // closed zen eyes
  M5Cardputer.Display.drawLine(cx - 12, cy - 6, cx - 4,  cy - 6, COL_EYE_B);
  M5Cardputer.Display.drawLine(cx + 4,  cy - 6, cx + 12, cy - 6, COL_EYE_B);
  // serene flat mouth
  M5Cardputer.Display.drawLine(cx - 6, cy + 6, cx + 6, cy + 6, COL_EYE_B);
  // floating leaves
  M5Cardputer.Display.fillTriangle(cx - 40, cy - 20, cx - 34, cy - 24, cx - 32, cy - 18, COL_ACCENT);
  M5Cardputer.Display.fillTriangle(cx + 34, cy + 22, cx + 42, cy + 20, cx + 40, cy + 26, COL_ACCENT);
}

static void drawHeader() {
  M5Cardputer.Display.fillRect(0, 0, kScreenW, kHeaderH, 0x2104);
  M5Cardputer.Display.setTextColor(COL_TEXT, 0x2104);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.print("Claude-EGG :: ");
  M5Cardputer.Display.print(DEFAULT_BUDDY);
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
  // clear body region only, keep header/footer stable
  M5Cardputer.Display.fillRect(0, kBodyTop, kScreenW, kBodyBot - kBodyTop, COL_BG);

  switch (g_state.stage) {
    case STAGE_EGG:   drawEgg(); break;
    case STAGE_CHILD: drawTadpole(g_state.mood); break;
    case STAGE_TEEN:  drawFrog(g_state.mood, 32, 20); break;
    case STAGE_ADULT: drawFrog(g_state.mood, 44, 26); break;
    case STAGE_ELDER: drawPondSage(); break;
    default: break;
  }

  // active-now pulse dot
  if (g_state.active_now) {
    M5Cardputer.Display.fillCircle(kScreenW - 8, kBodyTop + 6, 3, COL_ACCENT);
  }
}

static void redrawAll() {
  M5Cardputer.Display.fillScreen(COL_BG);
  drawHeader();
  drawPet();
  drawFooter();
}

// ---- input ----------------------------------------------------------------

static void handleKey(char c) {
  switch (c) {
    case '1': g_state.stage = STAGE_EGG;   g_dirty = true; break;
    case '2': g_state.stage = STAGE_CHILD; g_dirty = true; break;
    case '3': g_state.stage = STAGE_TEEN;  g_dirty = true; break;
    case '4': g_state.stage = STAGE_ADULT; g_dirty = true; break;
    case '5': g_state.stage = STAGE_ELDER; g_dirty = true; break;
    case 'q': g_state.mood = MOOD_HAPPY;   g_dirty = true; break;
    case 'w': g_state.mood = MOOD_EXCITED; g_dirty = true; break;
    case 'e': g_state.mood = MOOD_TIRED;   g_dirty = true; break;
    case 'r': g_state.mood = MOOD_GRUMPY;  g_dirty = true; break;
    case 't': g_state.mood = MOOD_SICK;    g_dirty = true; break;
    case 'y': g_state.mood = MOOD_LONELY;  g_dirty = true; break;
    case 'a':  // toggle active_now indicator
      g_state.active_now = !g_state.active_now;
      g_dirty = true;
      break;
    case '+':
      g_state.today_min += 30;
      g_state.lifetime_min += 30;
      g_dirty = true;
      break;
    case '-':
      g_state.today_min = max(0, g_state.today_min - 30);
      g_dirty = true;
      break;
    default: break;
  }
}

// ---- arduino entry points -------------------------------------------------

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(80);
  redrawAll();
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    for (auto c : status.word) {
      handleKey(c);
    }
  }

  if (g_dirty) {
    drawPet();
    drawFooter();
    g_dirty = false;
  }

  delay(20);
}
