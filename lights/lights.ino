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

CenterPulse centerPulsePattern;
StandingWaves standingWavesPattern;
Droplets dropletsPattern;
Bits bitsPattern;
SmoothPalettes smoothPalettes;

Pattern *idlePatterns[] = {
//  &centerPulsePattern, // looks awful on small triangle
  &standingWavesPattern, 
  &dropletsPattern,
  &bitsPattern,
  &smoothPalettes
  };
const unsigned int kIdlePatternsCount = ARRAY_SIZE(idlePatterns);

RunInTriangles runInTrianglesPattern;
SparklyFlash sparklyFlashPattern;
CornerFlash cornerFlashPattern;
ColorTornado colorTornadoPattern;

Pattern *triggerPatterns[] = {
//  &runInTrianglesPattern, // FIXME: breathe portion doesn't start from correct led on small triangle
  &sparklyFlashPattern,
  &cornerFlashPattern,
  &colorTornadoPattern
};
const unsigned int kTriggerPatternsCount = ARRAY_SIZE(triggerPatterns);

Pattern *activePattern = NULL;
Pattern *lastPattern = NULL;

bool triggerIsActive = false;

/* ---- Test Options ---- */
const bool kTestPatternTransitions = true;
const int kIdlePatternTimeout = 1000 * (kTestPatternTransitions ? 15 : 60 * 2);
const unsigned long kTestTriggerAtInterval = 0;//10000;//1000 * 35;//1000 * 10; // 0 for no test

Pattern *testIdlePattern = &smoothPalettes;//&dropletsPattern;
Pattern *testTriggerPattern = NULL;//&cornerFlashPattern;

/* ---------------------- */

unsigned long lastTrigger = 0;
FrameCounter fc;

bool triggerTouchDown = false;

void setup() {

  Serial.begin(57600);
  Serial.println("begin");
  delay(200);

  randomSeed(analogRead(UNCONNECTED_PIN));
  random16_add_entropy( analogRead(UNCONNECTED_PIN) );

  FastLED.addLeds<APA102, BGR>(leds, NUM_LEDS);
  LEDS.setBrightness(127);

  fc.tick();
}

void loop() {
  for (unsigned i = 0; i < kIdlePatternsCount; ++i) {
    Pattern *pattern = idlePatterns[i];
    if (pattern->isRunning() || pattern->isStopping()) {
      pattern->loop(leds);
    }
  }
  for (unsigned i = 0; i < kTriggerPatternsCount; ++i) {
    Pattern *pattern = triggerPatterns[i];
    if (pattern->isRunning() || pattern->isStopping()) {
      pattern->loop(leds);
    }
  }

  // clear out patterns that have stopped themselves
  if (activePattern != NULL && !activePattern->isRunning()) {
    logf("Clearing inactive pattern %s", activePattern->description());
    activePattern = NULL;
    triggerIsActive = false; // TODO: better way to detect?
  }

  // time out idle patterns
  if (!triggerIsActive && activePattern != NULL && activePattern->isRunning() && activePattern->runTime() > kIdlePatternTimeout) {
    if (activePattern != testIdlePattern && activePattern->wantsToIdleStop()) {
      activePattern->lazyStop();
      lastPattern = activePattern;
      activePattern = NULL;
    }
  }

  // check for trigger
  if (Serial.available() > 0) {
    int incoming = Serial.read();
    if (incoming == 't') {
      triggerPattern();
    }
  }
  int readVal = touchRead(TOUCH_PIN);
//  for (int x = 0; x < readVal / 100; ++x) {
//    if (x > 0 && x < NUM_LEDS) {
//      leds[x] = CRGB(0,0xFF, 0);
//    }
//  }

  if (readVal > 800) {
    triggerTouchDown = true;
  }
  if (triggerTouchDown == true && touchRead(TOUCH_PIN) < 500) {
    triggerTouchDown = false;
    triggerPattern();
  }


  if (kTestTriggerAtInterval > 0 && millis() - lastTrigger > kTestTriggerAtInterval) {
    triggerPattern();
  }

  // start a new idle pattern
  if (activePattern == NULL) {
    Pattern *nextPattern;
    if (testIdlePattern != NULL) {
      nextPattern = testIdlePattern;
    } else {
      int choice = (int)random8(kIdlePatternsCount);
      nextPattern = idlePatterns[choice];
    }
    if ((nextPattern != lastPattern || nextPattern == testIdlePattern) && !nextPattern->isRunning() && !nextPattern->isStopping() && nextPattern->wantsToRun()) {
      nextPattern->start();
      activePattern = nextPattern;
    }
  }

  FastLED.show();

  fc.tick();

}

void triggerPattern() {
  if (triggerIsActive) {
    logf("Skipping repeat trigger");
    return;
  }
  logf("Trigger!");
  if (activePattern != NULL) {
    activePattern->stop();
    lastPattern = activePattern;
    activePattern = NULL;
  }

  Pattern *nextPattern;
  if (testTriggerPattern != NULL) {
    nextPattern = testTriggerPattern;
  } else {
    int choice = (int)random8(kTriggerPatternsCount);
    logf("picked trigger %i", choice);
    nextPattern = triggerPatterns[choice];
  }
  if (nextPattern) {
    nextPattern->start();
    activePattern = nextPattern;
  }
  lastTrigger = millis();
  triggerIsActive = true;
}

/* patterns:

    idle:
    pulsing white/color outward from side centers
    twinkling: bits
    undulating: 2-3 parity standing waves, varying colors?
    streaking: bits but 10-20px long
    popping: droplets of color with box blur
    noise: fill_raw_noise8
    palettes: various palette fades with slowly shifting parameters

    on trigger:
    follow around entire triangle very quickly
    blink entire triangle couple times
    blink chunks, e.g. 10px solid random colors

    breathe:
    pulse: takes a deep breathe by fading up to max, then down into a pattern
    lift&fall:

*/

/* Optimizations:

    1. millis into seconds using div1024_32_16
    2. replace any remaining float math with integer or with q44 or q62

*/
