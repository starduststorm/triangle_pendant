#include <FastLED.h>

#define SERIAL_LOGGING 1
#define STRIP_LENGTH 16
#define STRIP_COUNT 3
#define NUM_LEDS (STRIP_LENGTH * STRIP_COUNT)
#define UNCONNECTED_PIN 14
#define TOUCH_PIN 33

#include "util.h"
#include "patterns.h"

/* ---- Options ---- */
// FIXME: should have options here for mounting, e.g. side-down vs. corner down, which strand is top/bottom, etc.
// though changing these flags in the field is not very practical if it requires a recompile.
/* ---- ------------*/

CRGBArray<NUM_LEDS> leds;

StandingWaves standingWavesPattern;
Droplets dropletsPattern;
Bits pinkBits(0);
PinkFlash pinkFlash;
Bits bitsPattern;
SmoothPalettes smoothPalettes;

Pattern *idlePatterns[] = {
//  &centerPulsePattern, // looks awful on small triangle
//  &standingWavesPattern, 
  &pinkBits,
  &pinkFlash,
  &dropletsPattern,
  &bitsPattern,
  &smoothPalettes
  };
const unsigned int kIdlePatternsCount = ARRAY_SIZE(idlePatterns);

Pattern *activePattern = NULL;
int activePatternIndex = -1;
Pattern *lastPattern = NULL;

/* ---- Test Options ---- */
const bool kTestPatternTransitions = false;
const int kIdlePatternTimeout = -1;//1000 * (kTestPatternTransitions ? 15 : 60 * 2);

Pattern *testIdlePattern = NULL;//&smoothPalettes;//&dropletsPattern;

/* ---------------------- */

FrameCounter fc;

bool contactTouchDown = false;
unsigned long touchDownStart = 0;

uint8_t brightness = 127;
uint8_t lastBrightnessPhase = 0;

void setup() {

  Serial.begin(57600);
  Serial.println("begin");
  delay(200);

  randomSeed(analogRead(UNCONNECTED_PIN));
  random16_add_entropy( analogRead(UNCONNECTED_PIN) );

  FastLED.addLeds<APA102, BGR>(leds, NUM_LEDS);
  LEDS.setBrightness(brightness);

  fc.tick();
}

void nextPattern() {
  if (testIdlePattern != NULL) {
    activePattern = testIdlePattern;
  } else {
    if (activePattern) {
      activePattern->stop();
      lastPattern = activePattern;
      activePattern = NULL;
    }
    activePattern = idlePatterns[++activePatternIndex % kIdlePatternsCount];
  }
  if (!activePattern->isRunning()) {
    activePattern->start();
  }
}

void loop() {
  for (unsigned i = 0; i < kIdlePatternsCount; ++i) {
    Pattern *pattern = idlePatterns[i];
    if (pattern->isRunning()) {
      pattern->loop(leds);
    }
  }

  // clear out patterns that have stopped themselves
  if (activePattern != NULL && !activePattern->isRunning()) {
    logf("Clearing inactive pattern %s", activePattern->description());
    activePattern = NULL;
  }

  // time out idle patterns
  if (activePattern != NULL && activePattern->isRunning() && (kIdlePatternTimeout != -1 && activePattern->runTime() > kIdlePatternTimeout)) {
    if (activePattern != testIdlePattern && activePattern->wantsToIdleStop()) {
      nextPattern();
    }
  }

  int readVal = touchRead(TOUCH_PIN);
//  for (int x = 0; x < readVal / 100; ++x) {
//    if (x > 0 && x < NUM_LEDS) {
//      leds[x] = CRGB(0,0xFF, 0);
//    }
//  }

  if (readVal > 800) {
    if (!contactTouchDown) {
      touchDownStart = millis();
    }
    contactTouchDown = true;
  }
  unsigned long touchDownDuration = millis() - touchDownStart;
  const int brightnessFaderDelay = 500;
  if (contactTouchDown) {
    uint8_t phase = (touchDownDuration - brightnessFaderDelay) * 256 / 4000 + lastBrightnessPhase;
    if (touchDownDuration > brightnessFaderDelay) {  
      brightness = sin8(phase);
      LEDS.setBrightness(brightness);
    }
    
    if (touchRead(TOUCH_PIN) < 500) {
      contactTouchDown = false;
      if (touchDownDuration < brightnessFaderDelay) {
        nextPattern();
      } else {
        lastBrightnessPhase = phase % 0x100;
      }
    }
  }

  // start a new idle pattern
  if (activePattern == NULL) {
    nextPattern();
  }

  FastLED.show();

  fc.tick();
  fc.clampToFramerate(400);
}
