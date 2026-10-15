// Jarvis eyes - dual GC9D01 brightness self-test.
//
// This is deliberately NOT the animated eye code. Its only job is to answer the
// one question yesterday's sessions never settled: does image detail reach the
// outside of the domes at all?
//
// Colour tests through tinted domes returned no signal last time, and that
// non-signal was misread as faulty chip-select wiring. Brightness is the right
// instrument: full white vs full black is the boldest difference a panel can
// make. If the domes do not visibly change, no eye code will ever show through
// them.
//
// Three library traps found the hard way on 2026-10-14, all of which make the
// board look dead - each is avoided below and labelled:
//   (1) two SPI bus objects on one SPI port       -> one bus object, shared
//   (2) two display objects owning the same bus CS -> bus has no CS; we drive
//                                                     CS manually per panel
//   (3) constructing displays before setup() runs  -> heap-allocated in setup()
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board_pins.h"

// (1) ONE bus for the whole SPI port. GFX_NOT_DEFINED for CS: the bus must not
// own a chip-select, because two panels share it and we select them by hand.
static Arduino_DataBus *bus = nullptr;
static Arduino_GFX *eyeL = nullptr;
static Arduino_GFX *eyeR = nullptr;

// Every burst is wrapped: assert this panel's CS, draw, release. Nothing
// touches the bus outside one of these scopes.
struct EyeSelect {
  uint8_t pin;
  explicit EyeSelect(uint8_t cs) : pin(cs) { digitalWrite(pin, LOW); }
  ~EyeSelect() { digitalWrite(pin, HIGH); }
};

static void fillEye(Arduino_GFX *eye, uint8_t cs, uint16_t colour) {
  EyeSelect sel(cs);
  eye->fillScreen(colour);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n[eyes] GC9D01 brightness self-test"));

  // CS lines idle HIGH (deselected) before anything is created, so a
  // half-initialised bus cannot talk to either panel.
  pinMode(PIN_EYE_CS_L, OUTPUT);
  pinMode(PIN_EYE_CS_R, OUTPUT);
  digitalWrite(PIN_EYE_CS_L, HIGH);
  digitalWrite(PIN_EYE_CS_R, HIGH);

  // Backlights on. If these leads are floating, the panels stay dark no matter
  // what is drawn - which on its own explains dark domes.
  pinMode(PIN_EYE_BL_L, OUTPUT);
  pinMode(PIN_EYE_BL_R, OUTPUT);
  digitalWrite(PIN_EYE_BL_L, HIGH);
  digitalWrite(PIN_EYE_BL_R, HIGH);

  // (3) Everything below is constructed inside setup(), never at file scope.
  bus = new Arduino_ESP32SPI(PIN_EYE_DC, GFX_NOT_DEFINED, PIN_SPI_SCLK,
                             PIN_SPI_MOSI, PIN_SPI_MISO);

  // (2) Two display objects, one shared bus, a private reset line each.
  eyeL = new Arduino_GC9D01(bus, PIN_EYE_RST_L, 0 /* rotation */, true /* IPS */,
                            EYE_WIDTH, EYE_HEIGHT);
  eyeR = new Arduino_GC9D01(bus, PIN_EYE_RST_R, 0 /* rotation */, true /* IPS */,
                            EYE_WIDTH, EYE_HEIGHT);

  // Init one panel at a time with only that panel selected, or the second
  // init's command stream lands on both controllers.
  {
    EyeSelect sel(PIN_EYE_CS_L);
    Serial.println(eyeL->begin() ? F("[eyes] L init ok") : F("[eyes] L init FAILED"));
  }
  {
    EyeSelect sel(PIN_EYE_CS_R);
    Serial.println(eyeR->begin() ? F("[eyes] R init ok") : F("[eyes] R init FAILED"));
  }

  Serial.println(F("[eyes] alternating WHITE/BLACK every 2s - watch the domes"));
}

void loop() {
  static bool white = true;
  const uint16_t colour = white ? RGB565_WHITE : RGB565_BLACK;

  fillEye(eyeL, PIN_EYE_CS_L, colour);
  fillEye(eyeR, PIN_EYE_CS_R, colour);

  Serial.printf("[eyes] %s  heap=%u\n", white ? "WHITE" : "BLACK",
                (unsigned)ESP.getFreeHeap());
  white = !white;
  delay(2000);
}
