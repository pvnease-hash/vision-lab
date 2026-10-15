// Animated Jarvis eyes - dual GC9D01, one shared SPI bus, dual chip-select.
//
// Flash this only after the self-test (env:jarvis_eyes_c6) has proved that
// image detail actually reaches the outside of the domes. If white-vs-black is
// not visible through them, nothing here will be either.
//
// RAM budget, which is the whole reason this file is shaped the way it is:
// one 160x160 RGB565 framebuffer is 51,200 bytes. The C6 has 320 KB of SRAM and
// the Arduino/Wi-Fi stack wants a good share of it, so TWO canvases (102 KB)
// is a bad trade for no benefit. Instead there is ONE canvas: draw the left
// eye, push it to the left panel, redraw for the right eye, push it to the
// right panel. Both eyes still update every frame and the peak cost stays at
// one buffer. The allocation is checked - a silent canvas failure is
// indistinguishable from a wiring fault, which cost a whole session last time.
//
// The three Arduino_GFX traps (two bus objects on one port, two displays owning
// the bus CS, construction before setup()) are avoided here the same way they
// are in selftest.cpp.
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board_pins.h"

// ---------------------------------------------------------------------------
// Look. Tune these; they are the only knobs that change the character.
// ---------------------------------------------------------------------------
#define IRIS_COLOUR    0x3D7F  // cool Jarvis blue
#define IRIS_RIM       0x1B3B  // darker limbal ring
#define IRIS_SPOKE     0x5EBF  // lighter fibre streaks
#define SCLERA_COLOUR  0xD6BA  // off-white, never pure white
#define PUPIL_COLOUR   RGB565_BLACK
#define LID_COLOUR     RGB565_BLACK

static const int16_t CX = EYE_WIDTH / 2;
static const int16_t CY = EYE_HEIGHT / 2;
static const int16_t R_SCLERA = 78;
static const int16_t R_IRIS   = 44;
static const int16_t R_PUPIL  = 17;
static const int16_t GLANCE_R = 18;   // how far the iris wanders from centre
static const uint8_t IRIS_SPOKES = 24;

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX *eyeL = nullptr;
static Arduino_GFX *eyeR = nullptr;
static Arduino_Canvas *canvas = nullptr;

// Assert one panel's CS for the duration of a burst, release on scope exit.
// Nothing touches the bus outside one of these.
struct EyeSelect {
  uint8_t pin;
  explicit EyeSelect(uint8_t cs) : pin(cs) { digitalWrite(pin, LOW); }
  ~EyeSelect() { digitalWrite(pin, HIGH); }
};

// Integer sine, 256 units per turn, output scaled by 1024. Avoids float trig in
// the per-frame path.
static int32_t isin(uint8_t a) {
  static const int16_t q[65] = {
      0,   25,  50,  75,  100, 125, 150, 175, 200, 224, 249, 273, 297, 321, 345,
      369, 392, 415, 438, 460, 483, 505, 526, 548, 569, 590, 610, 630, 650, 669,
      688, 706, 724, 742, 759, 775, 792, 807, 822, 837, 851, 865, 878, 891, 903,
      915, 926, 936, 946, 955, 964, 972, 980, 987, 993, 999, 1004, 1009, 1013,
      1016, 1019, 1021, 1023, 1024, 1024};
  if (a < 64) return q[a];
  if (a < 128) return q[128 - a];
  if (a < 192) return -q[a - 128];
  return -q[256 - a];
}
static int32_t icos(uint8_t a) { return isin((uint8_t)(a + 64)); }

// One eye into the shared canvas. gx/gy are the iris offset; lid is 0 (open) to
// 255 (shut).
static void drawEye(int16_t gx, int16_t gy, uint8_t lid) {
  canvas->fillScreen(RGB565_BLACK);
  canvas->fillCircle(CX, CY, R_SCLERA, SCLERA_COLOUR);

  const int16_t ix = CX + gx;
  const int16_t iy = CY + gy;

  canvas->fillCircle(ix, iy, R_IRIS, IRIS_COLOUR);

  // Fibre streaks: cheap radial texture, 24 lines instead of per-pixel noise.
  for (uint8_t i = 0; i < IRIS_SPOKES; i++) {
    const uint8_t a = (uint8_t)((i * 256) / IRIS_SPOKES);
    const int16_t x0 = ix + (int16_t)((icos(a) * R_PUPIL) >> 10);
    const int16_t y0 = iy + (int16_t)((isin(a) * R_PUPIL) >> 10);
    const int16_t x1 = ix + (int16_t)((icos(a) * (R_IRIS - 2)) >> 10);
    const int16_t y1 = iy + (int16_t)((isin(a) * (R_IRIS - 2)) >> 10);
    canvas->drawLine(x0, y0, x1, y1, (i & 1) ? IRIS_SPOKE : IRIS_RIM);
  }

  canvas->drawCircle(ix, iy, R_IRIS, IRIS_RIM);       // limbal ring
  canvas->drawCircle(ix, iy, R_IRIS - 1, IRIS_RIM);
  canvas->fillCircle(ix, iy, R_PUPIL, PUPIL_COLOUR);
  canvas->fillCircle(ix - 7, iy - 8, 5, RGB565_WHITE); // specular highlight

  // Lids. Almond rather than a straight wipe: two filled rectangles plus a
  // rounded inner edge, closing from both top and bottom.
  if (lid) {
    const int16_t drop = (int16_t)(((int32_t)lid * (R_SCLERA + 4)) >> 8);
    canvas->fillRect(0, 0, EYE_WIDTH, CY - R_SCLERA + drop, LID_COLOUR);
    canvas->fillRect(0, CY + R_SCLERA - drop, EYE_WIDTH, EYE_HEIGHT, LID_COLOUR);
    canvas->drawFastHLine(0, CY - R_SCLERA + drop, EYE_WIDTH, 0x2124);
    canvas->drawFastHLine(0, CY + R_SCLERA - drop, EYE_WIDTH, 0x2124);
  }

  // Permanent soft corners so a round panel still reads as an eye, not a disc.
  canvas->fillTriangle(0, 0, 44, 0, 0, 44, LID_COLOUR);
  canvas->fillTriangle(EYE_WIDTH, 0, EYE_WIDTH - 44, 0, EYE_WIDTH, 44, LID_COLOUR);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n[eyes] animated GC9D01 eyes"));

  pinMode(PIN_EYE_CS_L, OUTPUT);
  pinMode(PIN_EYE_CS_R, OUTPUT);
  digitalWrite(PIN_EYE_CS_L, HIGH);
  digitalWrite(PIN_EYE_CS_R, HIGH);
  pinMode(PIN_EYE_BL_L, OUTPUT);
  pinMode(PIN_EYE_BL_R, OUTPUT);
  digitalWrite(PIN_EYE_BL_L, HIGH);
  digitalWrite(PIN_EYE_BL_R, HIGH);

  bus = new Arduino_ESP32SPI(PIN_EYE_DC, GFX_NOT_DEFINED, PIN_SPI_SCLK,
                             PIN_SPI_MOSI, PIN_SPI_MISO);
  eyeL = new Arduino_GC9D01(bus, PIN_EYE_RST_L, 0, true, EYE_WIDTH, EYE_HEIGHT);
  eyeR = new Arduino_GC9D01(bus, PIN_EYE_RST_R, 0, true, EYE_WIDTH, EYE_HEIGHT);

  {
    EyeSelect sel(PIN_EYE_CS_L);
    Serial.println(eyeL->begin() ? F("[eyes] L init ok") : F("[eyes] L init FAILED"));
  }
  {
    EyeSelect sel(PIN_EYE_CS_R);
    Serial.println(eyeR->begin() ? F("[eyes] R init ok") : F("[eyes] R init FAILED"));
  }

  // Output target is the left panel; the right panel gets the same buffer
  // pushed by hand, so only one framebuffer is ever allocated.
  canvas = new Arduino_Canvas(EYE_WIDTH, EYE_HEIGHT, eyeL);
  if (!canvas->begin(GFX_NOT_DEFINED)) {
    Serial.printf("[eyes] FATAL: canvas alloc failed, free heap=%u, largest block=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    return;  // better a clear log line than a board that merely looks dead
  }
  Serial.printf("[eyes] canvas ok, free heap=%u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  if (!canvas) { delay(1000); return; }

  static uint32_t nextGlance = 0;
  static uint32_t nextBlink = 0;
  static uint32_t blinkStart = 0;
  static int16_t gx = 0, gy = 0, tgx = 0, tgy = 0;
  static uint32_t frames = 0, fpsMark = 0;

  const uint32_t now = millis();

  // Saccades: pick a new target now and then, then ease toward it. Eyes that
  // glide look alive; eyes that teleport look broken.
  if (now >= nextGlance) {
    const uint8_t a = (uint8_t)random(256);
    const int16_t r = (int16_t)random(GLANCE_R + 1);
    tgx = (int16_t)((icos(a) * r) >> 10);
    tgy = (int16_t)((isin(a) * r) >> 10);
    nextGlance = now + 900 + random(2600);
  }
  gx += (tgx - gx + ((tgx > gx) ? 2 : -2)) / 3;
  gy += (tgy - gy + ((tgy > gy) ? 2 : -2)) / 3;

  // Blinks: ~170 ms, every 3-9 s, occasionally a double.
  if (now >= nextBlink) {
    blinkStart = now;
    nextBlink = now + 3000 + random(6000);
  }
  uint8_t lid = 0;
  const uint32_t dt = now - blinkStart;
  if (dt < 170) {
    lid = (dt < 85) ? (uint8_t)((dt * 255) / 85) : (uint8_t)(((170 - dt) * 255) / 85);
  }

  // One buffer, two panels. Each push is CS-wrapped.
  drawEye(gx, gy, lid);
  { EyeSelect sel(PIN_EYE_CS_L); canvas->flush(); }

  drawEye(-gx, gy, lid);  // mirrored gaze so both eyes converge, not track
  {
    EyeSelect sel(PIN_EYE_CS_R);
    eyeR->draw16bitRGBBitmap(0, 0, canvas->getFramebuffer(), EYE_WIDTH, EYE_HEIGHT);
  }

  frames++;
  if (now - fpsMark >= 5000) {
    Serial.printf("[eyes] %lu fps, free heap=%u\n",
                  (unsigned long)(frames * 1000 / (now - fpsMark)),
                  (unsigned)ESP.getFreeHeap());
    frames = 0;
    fpsMark = now;
  }
}
