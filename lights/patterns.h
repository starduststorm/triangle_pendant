#ifndef PATTERN_H
#define PATTERN_H

#include <FastLED.h>
#include "util.h"
#include "palettes.h"

class Pattern {
  protected:
    long startTime = -1;
    long stopTime = -1;
    Pattern *subPattern = NULL;

    void stopCompleted() {
      if (!readyToStop()) {
        logf("WARNING: stopped %s before subPattern was stopped", description());
      }
      logf("Stopped %s", description());
      stopTime = -1;
      startTime = -1;
      if (subPattern) {
        subPattern->stop();
        delete subPattern;
        subPattern = NULL;
      }
    }

    virtual Pattern *makeSubPattern() {
      return NULL;
    }

    bool readyToStop() {
      return subPattern == NULL || subPattern->isStopped();
    }

  public:
    virtual ~Pattern() { }

    virtual bool wantsToRun() {
      // for idle patterns that require microphone input and may opt not to run if there is no sound
      return true;
    }

    void start() {
      logf("Starting %s", description());
      startTime = millis();
      stopTime = -1;
      setup();
      subPattern = makeSubPattern();
      if (subPattern) {
        subPattern->start();
      }
    }

    void loop(CRGBArray<NUM_LEDS> &leds) {
      update(leds);
      if (subPattern) {
        subPattern->update(leds);
      }
    }

    virtual void setup() { }

    virtual bool wantsToIdleStop() {
      return true;
    }

    virtual void lazyStop() {
      if (isRunning()) {
        logf("Stopping %s", description());
        stopTime = millis();
      }
      if (subPattern) {
        subPattern->lazyStop();
      }
    }

    void stop() {
      if (subPattern) {
        subPattern->stop();
      }
      stopCompleted();
    }

    bool isRunning() {
      return startTime != -1 && isStopping() == false;
    }

    bool isStopped() {
      return !isRunning() && !isStopping();
    }

    long runTime() {
      return startTime == -1 ? 0 : millis() - startTime;
    }

    bool isStopping() {
      return stopTime != -1;
    }

    virtual void update(CRGBArray<NUM_LEDS> &leds) = 0;
    virtual const char *description() = 0;

    // Sub patterns (for pattern mixing)
    void setSubPattern(Pattern *pattern) {
      subPattern = pattern;
      if (isRunning()) {
        subPattern->start();
      }
    }
};


/* --------------------------- */


class Bits : public Pattern {
    enum BitColor {
      monotone, fromPalette, mix, white, pink
    };
    typedef struct _BitsPreset {
      unsigned int maxBits, bitLifespan, updateInterval, fadedown;
      BitColor color;
    } BitsPreset;

    BitsPreset presets[8] = {
      { .maxBits = 4, .bitLifespan = 3000, .updateInterval = 35, .fadedown = 5, .color = white}, // dots enhancer
      { .maxBits = 4, .bitLifespan = 3000, .updateInterval = 45, .fadedown = 5, .color = fromPalette}, // dots enhancer
      // little too frenetic, use as trigger patterns?
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = monotone }, // party streamers
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = mix }, // multi-color party streamers
//      { .maxBits = 4, .bitLifespan = 3000, .updateInterval = 1, .fadedown = 15, .color = pink}, // pink triangle
      { .maxBits = 5, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 5, .color = monotone }, // chill streamers
      { .maxBits = 5, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 5, .color = fromPalette}, // palette chill streamers
      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 16, .fadedown = 30, .color = monotone }, // moving dots
//      { .maxBits = 14, .bitLifespan = 3000, .updateInterval = 350, .fadedown = 5, .color = monotone }, // OG bits pattern
      { .maxBits = 3, .bitLifespan = 3000, .updateInterval = 8, .fadedown = 50, .color = monotone }, // chase
    };

    class Bit {
        int8_t direction;
        unsigned long birthdate;
      public:
        unsigned int pos;
        bool alive;
        unsigned long lastTick;
        CRGB color;
        Bit(CRGB color) {
          reset(color);
        }
        void reset(CRGB color) {
          birthdate = millis();
          alive = true;
          pos = random16() % NUM_LEDS;
          direction = random8(2) == 0 ? 1 : -1;
          this->color = color;
        }
        unsigned int age() {
          return millis() - birthdate;
        }
        fract8 ageBrightness() {
          // FIXME: assumes 3000ms lifespan
          float theAge = age();
          if (theAge < 500) {
            return theAge * 0xFF / 500;
          } else if (theAge > 2500) {
            return (3000 - theAge) * 0xFF / 500;
          }
          return 0xFF;
        }
        void tick() {
          pos = mod_wrap(pos + direction, NUM_LEDS);
          lastTick = millis();
        }
    };

    Bit *bits;
    unsigned int numBits;
    unsigned int lastBitCreation;
    BitsPreset preset;
    uint8_t constPreset;

    CRGB color;
    CRGBPalette16 palette;
  public:
    Bits(int constPreset = -1) {
      this->constPreset = constPreset;
    }
  private:

    CRGB getBitColor() {
      switch (preset.color) {
        case monotone:
          return color; break;
        case fromPalette:
          return ColorFromPalette(palette, random8()); break;
        case mix:
          return CHSV(random8(), random8(200, 255), 255); break;
        case white:
          return CRGB::White;
        case pink:
        default:
          return CRGB::DeepPink;
      }
    }

    void setup() {
      uint8_t pick;
      if (constPreset != -1 && (uint16_t)constPreset < ARRAY_SIZE(presets)) {
        pick = constPreset;
        logf("Using const Bits preset %u", pick);
      } else {
        pick = random8(ARRAY_SIZE(presets));
        logf("Picked Bits preset %u", pick);
      }
      preset = presets[pick];


      unsigned int paletteChoice = random8(5);
      switch (paletteChoice) {
        case 0: palette = OceanColors_p; break;
        case 1: palette = LavaColors_p; break;
        case 2: palette = ForestColors_p; break;
        case 3: palette = PartyColors_p; break;
        case 4: palette = gGradientPalettes[random16(ARRAY_SIZE(gGradientPalettes))];
      }
      // for monotone
      color = CHSV(random8(), random8(8) == 0 ? 0 : random8(200, 255), 255);

      bits = (Bit *)calloc(preset.maxBits, sizeof(Bit));
      numBits = 0;
    }

    void update(CRGBArray<NUM_LEDS> &leds) {
      unsigned long mils = millis();
      bool hasAliveBit = false;
      for (unsigned int i = 0; i < numBits; ++i) {
        Bit *bit = &bits[i];
        if (bit->age() > preset.bitLifespan) {
          bit->alive = false;
        }
        if (bit->alive) {
          leds[bit->pos] = blend(CRGB::Black, bit->color, bit->ageBrightness());
          if (mils - bit->lastTick > preset.updateInterval) {
            bit->tick();
          }
          hasAliveBit = true;
        } else if (!isStopping()) {
          bit->reset(getBitColor());
          hasAliveBit = true;
        }
      }

      if (isRunning() && numBits < preset.maxBits && mils - lastBitCreation > preset.bitLifespan / preset.maxBits) {
        bits[numBits++] = Bit(getBitColor());
        lastBitCreation = mils;
      }
      if (!isStopping()) {
        leds.fadeToBlackBy(preset.fadedown);
      } else if (!hasAliveBit) {
        stopCompleted();
      }
    }

    void stopCompleted() {
      Pattern::stopCompleted();
      if (bits) {
        free(bits);
        bits = NULL;
        numBits = 0;
      }
    }

    const char *description() {
      return "Bits pattern";
    }
};


// FIXME: looks awful on small device

class CenterPulse : public Pattern {
    unsigned int prevOffset;

    Pattern *makeSubPattern() {
      if (random8(2) == 0) {
        return new Bits(1);
      }
      return NULL;
    }

    void update(CRGBArray<NUM_LEDS> &leds) {
      float offset = min(STRIP_LENGTH / 2 - 1, sin(fmod(runTime() / 800., PI / 2)) * STRIP_LENGTH / 2);
      if (isStopping() && offset < prevOffset) {
        if (readyToStop()) {
          stopCompleted();
        }
      } else {
        prevOffset = offset;
        for (int s = 0; s < STRIP_COUNT; ++s) {
          leds[STRIP_LENGTH * s + STRIP_LENGTH / 2 - (int)offset - 1] = CRGB::White;
          leds[STRIP_LENGTH * s + STRIP_LENGTH / 2 + (int)offset] = CRGB::White;
        }
      }
      if (!isStopping()) {
        leds.fadeToBlackBy(1);
      }
    }

    const char *description() {
      return "CenterPulse";
    }
};

class StandingWaves : public Pattern {
    const unsigned waveSize = 6;
    float initialPhase;
    int initialHue1;
    int initialHue2;
    int direction;

    Pattern *makeSubPattern() {
      if (true || random8(2) == 0) {
        return new Bits(0);
      }
      return NULL;
    }

    void setup() {
      initialPhase = random8(waveSize);
      initialHue1 = random8(0xFF);
      initialHue2 = random8(0xFF);
      direction = random8(2) == 0 ? 1 : -1;
    }

    void update(CRGBArray<NUM_LEDS> &leds) {
      float phase = 0;//initialPhase - direction * runTime() / 1000. * 2;

      uint8_t fadeSpeed = beatsin8(24, 0, 255, phase);

      int hue1 = mod_wrap(initialHue1 + direction * runTime() / 1000. * 8, 0xFF);
      int hue2 = mod_wrap(initialHue2 + direction * runTime() / 1000. * 8 + 120, 0xFF);

      // FIXME: is this a better technique? fps 6xx instead of ~130, but looks different & phase needs fixing
      //      CRGB topColor = blend(CHSV(hue1, 255, 255), CRGB::Black, fadeSpeed);
      //      CRGB bottomColor = blend(CRGB::Black, CHSV(hue2, 255, 255), fadeSpeed);
      //
      //      for (int segment = 0; segment < NUM_LEDS/waveSize; ++segment) {
      //        fill_gradient_RGB(leds, mod_wrap(segment * waveSize + phase, NUM_LEDS), bottomColor,
      //                                mod_wrap((segment  + 0.5) * waveSize + phase, NUM_LEDS), topColor);
      //        fill_gradient_RGB(leds, mod_wrap(ceil((segment + 0.5) * waveSize) + phase, NUM_LEDS), topColor,
      //                                mod_wrap((segment + 1) * waveSize - 1 + phase, NUM_LEDS), bottomColor);
      //      }

      float startBlend = min(runTime() / 1000. * 255, 255);
      float sin8Ratio = 0xFF / waveSize;
      for (int i = 0; i < NUM_LEDS; ++i) {
        float offset = fmod_wrap(i + phase, 255) * sin8Ratio;
        int brightness1 = sin8(offset);
        brightness1 = brightness1 < 40 ? 0 : brightness1;
        int brightness2 = sin8(offset + 0x7F);
        brightness2 = brightness2 < 40 ? 0 : brightness2;

        CHSV c1 = CHSV(hue1, 255, brightness1);
        CHSV c2 = CHSV(hue2, 255, brightness2);

        CRGB mix = blend(c1, c2, fadeSpeed);
        // TODO: fix single-subpixel aliasing?
        leds[i] = blend(leds[i], mix, startBlend);
      }
      if (isStopping() && readyToStop()) {
        stopCompleted();
      }
    }
    const char *description() {
      return "StandingWaves";
    }
};

class Droplets : public Pattern {
  private:
    unsigned long lastDrop;
    unsigned long lastFlow;
    CRGB cs[NUM_LEDS];
    CRGBPalette16 palette;
    bool usePalette;

    const unsigned int dropInterval = 450;
    unsigned int nextDropInterval = 0; // vary the drops

    void setup() {
      usePalette = (random(3) > 0);
      if (usePalette) {
        palette = gGradientPalettes[random16(gGradientPaletteCount)];
      }
      nextDropInterval = dropInterval;
    }
    
    void update(CRGBArray<NUM_LEDS> &leds) {
      const unsigned int flowInterval = 30;
      const float kFlow = 0.2;
      const float kEff = 0.97;
      const int minLoss = 1;

      unsigned long mils = millis();
      if (mils - lastDrop > nextDropInterval) {
        nextDropInterval = dropInterval + (dropInterval * 0.5) * (random8(2) ? -1 : 1);
        int center = random16(NUM_LEDS);
        CRGB color;
        if (usePalette) {
          color = ColorFromPalette(palette, random8());
        } else {
          color = CHSV(random8(), 255, 255);
        }
        for (int i = -2; i < 3; ++i) {
          leds[mod_wrap(center + i, NUM_LEDS)] = color;
        }
        lastDrop = mils;
      }
      if (mils - lastFlow > flowInterval) {
        for (int i = 0; i < NUM_LEDS; ++i) {
          cs[i] = leds[i];
        }
        for (int i = 0; i < NUM_LEDS; ++i) {
          int i2 = (i + 1) % NUM_LEDS;
          // calculate flows from og leds, set in scratch
          CRGB led1 = leds[i];
          CRGB led2 = leds[i2];
          for (uint8_t sp = 0; sp < 3; ++sp) { // each subpixel
            uint8_t *refSp = NULL;
            uint8_t *srcSp = NULL;
            uint8_t *dstSp = NULL;
            if (led1[sp] < led2[sp]) {
              refSp = &led2[sp];
              srcSp = &cs[i2][sp];
              dstSp = &cs[i][sp];
            } else if (led1[sp] > led2[sp] ) {
              refSp = &led1[sp];
              srcSp = &cs[i][sp];
              dstSp = &cs[i2][sp];
            }
            if (srcSp && dstSp) {
              uint8_t flow = min(*srcSp, min((int)(kFlow * *refSp), 0xFF - *dstSp));
              *dstSp += kEff * flow;
              if (*srcSp > flow && *srcSp > minLoss) {
                *srcSp -= max(minLoss, flow);
              } else {
                *srcSp = 0;
              }
            }
          }
        }
        for (int i = 0; i < NUM_LEDS; ++i) {
          leds[i] = cs[i];
        }
        lastFlow  = mils;
      }
      if (isStopping()) {
        stopCompleted();
      }
    }

    const char *description() {
      return "Droplets";
    }
};


#define SECONDS_PER_PALETTE 20
uint8_t gCurrentPaletteNumber = 0;
CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

class SmoothPalettes : public Pattern {
    void setup() {
      gTargetPalette = gGradientPalettes[random16(gGradientPaletteCount)];
    }
    void update(CRGBArray<NUM_LEDS> &leds) {
      EVERY_N_MILLISECONDS(20) {
        draw(leds);
      }
      if (isStopping()) {
        stopCompleted();
      }
    }

    void draw(CRGBArray<NUM_LEDS> &leds) {
      // ColorWavesWithPalettes
      // Animated shifting color waves, with several cross-fading color palettes.
      // by Mark Kriegsman, August 2015
      CRGBPalette16& palette = gCurrentPalette;
      EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
        gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, random8(16), gGradientPaletteCount);
        gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
      }

      EVERY_N_MILLISECONDS(40) {
        nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
      }

      uint16_t numleds = NUM_LEDS;
      static uint16_t sPseudotime = 0;
      static uint16_t sLastMillis = 0;
      static uint16_t sHue16 = 0;

      //      uint8_t sat8 = beatsin88( 87, 220, 250);
      uint8_t brightdepth = beatsin88( 341, 96, 224);
      uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
      uint8_t msmultiplier = beatsin88(147, 23, 60);

      uint16_t hue16 = sHue16;//gHue * 256;
      uint16_t hueinc16 = beatsin88(113, 300, 1500);

      uint16_t ms = millis();
      uint16_t deltams = ms - sLastMillis ;
      sLastMillis  = ms;
      sPseudotime += deltams * msmultiplier;
      sHue16 += deltams * beatsin88( 400, 5, 9);
      uint16_t brightnesstheta16 = sPseudotime;

      for ( uint16_t i = 0 ; i < numleds; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;
        uint16_t h16_128 = hue16 >> 7;
        if ( h16_128 & 0x100) {
          hue8 = 255 - (h16_128 >> 1);
        } else {
          hue8 = h16_128 >> 1;
        }

        brightnesstheta16  += brightnessthetainc16;
        uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        uint8_t index = hue8;
        //index = triwave8( index);
        index = scale8( index, 240);

        CRGB newcolor = ColorFromPalette( palette, index, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (numleds - 1) - pixelnumber;

        uint8_t blendAmt = runTime() < 2000 ? runTime() / 15 : 128;
        nblend( leds[pixelnumber], newcolor, blendAmt);
      }
    }
    const char *description() {
      return "Smooth palettes";
    }
};


class PowerTest : public Pattern {
    void update(CRGBArray<NUM_LEDS> &leds) {
      int bright = min(0xFF, beatsin16(10, 0, 400));
      logf("set brightness %i", bright);
      /* MY EYES */
      LEDS.setBrightness(bright);
      leds.fill_solid(CRGB::White);
    }
    const char *description() {
      return "Test max power draw";
    }
};



/* ----------------------------------------- Trigger Patterns -------------------------------------------- */



class RunInTriangles : public Pattern {
    enum Phase {zoom, blink, breathe};
    Phase phase;

    int lead;
    unsigned long lastUpdate;
    unsigned long phaseStart;
    unsigned rev;

    CRGB color;

    void setup() {
      rev = 0;
      lastUpdate = millis();
      color = CRGB::White;
      setPhase(zoom);
    }

    void setPhase(Phase p) {
      phase = p;
      phaseStart = millis();
      lead = 0;
    }

    void update(CRGBArray<NUM_LEDS> &leds) {
      unsigned long mils = millis();

      switch (phase) {
        case zoom:
          for (unsigned long i = 0; i < 2 * (mils - lastUpdate); ++i) {
            leds[lead] = color;
            lead = (lead + 1) % NUM_LEDS;
            if (lead == 0) {
              if (++rev == 3) {
                setPhase(blink);
                break;
              }
            }
          }
          leds.fadeToBlackBy(20);
          break;
        case blink: {
            const unsigned blinkLength = 120;
            leds.fill_solid(CRGB::White);
            unsigned long phaseDuration = mils - phaseStart;
            if (phaseDuration / blinkLength % 2 == 0) {
              leds.fill_solid(blend(color, CRGB::Black, 0x7F));
            } else {
              leds.fill_solid(CRGB::Black);
            }
            if (phaseDuration >= blinkLength * 5.5) {
              setPhase(breathe);
            }
            break;
          }
        case breathe: {
            const unsigned breathDuration = 1500;
            unsigned long phaseDuration = mils - phaseStart;
            if (phaseDuration < 2 * breathDuration) {
              uint8_t progress8 = ease8InOutCubic(phaseDuration * 0xFF / breathDuration);
              if (phaseDuration * 0xFF / breathDuration > 0xFF) {
                progress8 = 0xFF - progress8;
              }
              leds.fill_solid(CRGB::Black);
              int progress = progress8 * NUM_LEDS / 2 / 0xFF;
              // These are 0x7F rather than 0xFF because the full bright was starving the teensy of power,
              // not enough to cause it to power cycle, but apparently enough to kill its usb connection.
              // The teensy is powered directly from the first strand
              // so we should avoid making that strand full white until we give the teensy its own power tap.
              leds(NUM_LEDS - 1 - progress, NUM_LEDS - 1) = CHSV(0, 0, 0x7F);
              leds(0, progress) = CHSV(0, 0, 0x7F);
            } else {
              leds.fill_solid(CRGB::Black);
              delay(100);
              stop();
            }
            break;
          }
      }

      lastUpdate = mils;
    }

    const char *description() {
      return "Run in triangles";
    }
};

class SparklyFlash : public Pattern {
    void update(CRGBArray<NUM_LEDS> &leds) {
      if (runTime() < 3000) {
        for (CRGB &pixel : leds) {
          uint8_t sat = min(0xFF, (runTime() + 100) / 2);
          uint8_t bright = 0xFF - runTime() / 12;
          pixel = CHSV(random8(), sat, bright);
        }
        int flash = random16(NUM_LEDS - 3);
        leds(flash, flash + 3) = CRGB::White;
        delay(20);
      } else if (runTime() < 3500) {
        int extra = runTime() - 3000;
        leds.fill_solid(CHSV(0, 0, 0xFF - extra / 2));
      } else {
        leds.fill_solid(CRGB::Black);
        delay(100);
        stop();
      }
    }
    const char *description() {
      return "Sparkly flash";
    }
};

class CornerFlash : public Pattern {
    unsigned int phase;
    uint16_t phaseStart;
    int dotStart = 0; // phase hack. I mean this whole pattern is a hack. need to write a storyboarding harness.
    void setup() {
      phase = 0;
      phaseStart = runTime();
      dotStart = 0;
    }

    void nextPhase() {
      phase++;
      phaseStart = runTime();
    }

    void update(CRGBArray<NUM_LEDS> &leds) {
      const unsigned flashSize = STRIP_LENGTH / 4;
      const uint8_t dimBright = 0x4F;
      const unsigned maxMotion = STRIP_LENGTH / 2 - flashSize;

      int phaseTime = runTime() - phaseStart;

      switch (phase) {
        case 0: {
            leds.fill_solid(CRGB::Black);
            // flash, then dim, corners
            CRGB flashColor = CHSV(0, 0, lerp8by8(0xFF, dimBright, min(0xFF, runTime() / 2)));
            int motionTime = runTime() - 400;

            unsigned motion = (motionTime < 0 ? 0 : ease8InOutQuad(min(0x7F, motionTime / 2)) * maxMotion / 0x7F); // 0x7F to skip the ease out

            if (motionTime / 2 > 0x7F) {
              uint8_t dimmingFrac = min(0xFF, motionTime / 2 - 0x7F);
              if (dimmingFrac == 0xFF) {
                nextPhase();
              }
              flashColor = CHSV(0, 0, lerp8by8(0xFF, dimBright, dimmingFrac));
            }
            for (unsigned s = 0; s < STRIP_COUNT; ++s) {
              const unsigned strip = s * STRIP_LENGTH;
              const unsigned stripEnd = (s + 1) * STRIP_LENGTH - 1;

              leds(strip + motion, strip + motion + flashSize) = flashColor;
              leds(stripEnd - flashSize - motion, stripEnd - motion) = flashColor;
            }
            break;
          }
        case 1: {

            int rainbowLead = min(STRIP_LENGTH / 2, ease8InOutQuad(min(0xFF, phaseTime / 4)) * STRIP_LENGTH / 2 / 0xFF);

            for (unsigned s = 0; s < STRIP_COUNT; ++s) {
              const unsigned strip = s * STRIP_LENGTH;
              const unsigned stripEnd = (s + 1) * STRIP_LENGTH - 1;

              // fade out last bit of previous phase
              int phase0End = lerp8by8(0, flashSize, min(0xFF, phaseTime));
              leds.fadeToBlackBy(1);
              if (phase0End != flashSize) {
                leds(strip + maxMotion + phase0End, strip + maxMotion + flashSize) = CHSV(0, 0, dimBright);
                logf("setting leds %i to %i", stripEnd - flashSize - maxMotion, stripEnd - maxMotion - phase0End);
                leds(stripEnd - flashSize - maxMotion, stripEnd - maxMotion - phase0End) = CHSV(0, 0, dimBright);
              }

              leds(strip, strip + rainbowLead).fill_rainbow(-phaseTime / 5);
              leds(stripEnd - rainbowLead, stripEnd) = leds(strip + rainbowLead, strip);

              if (rainbowLead == STRIP_LENGTH / 2) {
                if (dotStart == 0) {
                  dotStart = phaseTime;
                }
                const int dotSize = 3;
                int dotTime = phaseTime - dotStart;
                leds(strip, stripEnd).fadeToBlackBy(dotTime / 5);
                int dotLead = lerp8by8(STRIP_LENGTH / 2, dotSize, dotTime / 4);
                leds(strip + dotLead - dotSize, strip + dotLead) = CRGB::White;
                leds(stripEnd - dotLead - dotSize, stripEnd - dotLead) = CRGB::White;
                if (dotLead == dotSize && phase == 1) {
                  nextPhase();
                }
              }
            }
            break;
          }
        case 2:
          leds.fill_solid(CHSV(0, 0, 0xFF - phaseTime / 2));
          if (phaseTime / 2 >= 0xFF) {
            nextPhase();
          }
          break;
        case 3:
          leds.fill_solid(CRGB::Black);
          delay(100);
          stop();
          break;
      }
    }
    const char *description() {
      return "Corner flash";
    }
};

class ColorTornado : public Pattern {
    enum Phase {discrete, continuous};
    Phase phase;
    uint32_t phaseStart;

    void setPhase(Phase p) {
      phase = p;
      phaseStart = runTime();
    }

    int pixelRotator;
    unsigned sideRotator;
    uint32_t lastTick;

    void setup() {
      phase = discrete;
      lastTick = 0;
      pixelRotator = 0;
      sideRotator = random8(3);
    }

    void update(CRGBArray<NUM_LEDS> &leds) {
      static const CRGB colors[] = {CRGB(0xFF, 0xFF, 0), CRGB(0, 0xFF, 0xFF), CRGB(0xFF, 0, 0xFF)};

      switch (phase) {
        case discrete: {
            float x = runTime() / 180.;
            if ((int)(runTime() - lastTick) > x * x) {
              sideRotator++;
              lastTick = runTime();
            }

            for (int s = 0; s < STRIP_COUNT; ++s) {
              unsigned sideStart = ((s + sideRotator) * STRIP_LENGTH) % NUM_LEDS;
              leds(sideStart, sideStart + STRIP_LENGTH - 1) = colors[s];
            }

            if (runTime() > 3000) {
              FastLED.show();
              delay(500);
              setPhase(continuous);

            }
            break;
          }
        case continuous: {
            uint32_t phaseTime = runTime() - phaseStart;
            unsigned index = 0;
            for (CRGB& led : leds) {
              CRGB color = colors[mod_wrap(index - sideRotator * STRIP_LENGTH + pixelRotator, NUM_LEDS) / STRIP_LENGTH];
              if (runTime() > 5500) {
                nblend(color, CRGB::Black, ease8InOutQuad((runTime() - 5500) * 0xFF / 500));
              }
              led = color;
              index++;
            }
            float x = phaseTime / 60.;
            float a = -2.5;
            float b = -10;
            float c = -81;
            pixelRotator = (x + a) * (x + a) * (x + a) + (x + b) * (x + b) + c;
            if (runTime() > 6000) {
              leds.fill_solid(CRGB::Black);
              stop();
            }
            break;
          }
      }
    }
    const char *description() {
      return "Color tornado";
    }
};

#endif

/* breathe up to solid and back

           const unsigned breathDuration = 2000;
        unsigned long phaseDuration = mils - phaseStart;
        if (phaseDuration < 2 * breathDuration) {
          CRGB start = (phaseDuration < breathDuration ? CRGB::Black : color);
          CRGB end = (phaseDuration < breathDuration ? color : CRGB::Black);
          leds.fill_solid(blend(start, end, phaseDuration % breathDuration * 255 / breathDuration));
        } else {
          stop();
        }
        break;
*/
