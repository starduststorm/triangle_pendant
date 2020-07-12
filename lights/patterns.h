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

    virtual void stopCompleted() {
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

    virtual void stop() {
      if (isRunning()) {
        logf("Stopping %s", description());
        stopTime = millis();
      }
      if (subPattern) {
        subPattern->stop();
      }
      stopCompleted();
    }

    bool isRunning() {
      return startTime != -1;
    }

    bool isStopped() {
      return !isRunning();
    }

    long runTime() {
      return startTime == -1 ? 0 : millis() - startTime;
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


class PinkFlash : public Pattern {
  unsigned int fadeupStart[3] = {0};
  void setup() {
    for (int side = 0; side < 3; ++side) {
      fadeupStart[side] = 0;
    }
  }
  
  void update(CRGBArray<NUM_LEDS> &leds) {
    for (int side = 0; side < 3; ++side) {
      if (random8() == 0) {
        fadeupStart[side] = millis();
      }
      unsigned int fadeupDuration = millis() - fadeupStart[side];
      if (fadeupDuration < 100) {
        CRGB color = CRGB::DeepPink;
        color.nscale8(fadeupDuration * 0xFF/100);
        if (color.getLuma() > leds[side*STRIP_LENGTH].getLuma()) {
          leds(side * STRIP_LENGTH, (side+1) * STRIP_LENGTH - 1) = color;
        }
      }
    }
    
    leds.fadeToBlackBy(3);
    for(CRGB & pixel : leds) {
      if (pixel.blue == 0) {
        pixel = CRGB::Black;
      }
    }
  }

  const char *description() {
    return "Pink Flash";
  }
};

class Bits : public Pattern {
    enum BitColor {
      monotone, fromPalette, mix, white, pink
    };
    typedef struct _BitsPreset {
      unsigned int maxBits, bitLifespan, updateInterval, fadedown;
      BitColor color;
    } BitsPreset;

    BitsPreset presets[5] = {
//      { .maxBits = 4, .bitLifespan = 3000, .updateInterval = 35, .fadedown = 5, .color = white}, // dots enhancer
//      { .maxBits = 4, .bitLifespan = 3000, .updateInterval = 45, .fadedown = 5, .color = fromPalette}, // dots enhancer
      // little too frenetic, use as trigger patterns?
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = monotone }, // party streamers
      //      { .maxBits = 10, .bitLifespan = 3000, .updateInterval = 0, .fadedown = 20, .color = mix }, // multi-color party streamers
      { .maxBits = 5, .bitLifespan = 3000, .updateInterval = 8, .fadedown = 12, .color = pink}, // pink triangle
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
        } else {
          bit->reset(getBitColor());
          hasAliveBit = true;
        }
      }

      if (isRunning() && numBits < preset.maxBits && mils - lastBitCreation > preset.bitLifespan / preset.maxBits) {
        bits[numBits++] = Bit(getBitColor());
        lastBitCreation = mils;
      }
      leds.fadeToBlackBy(preset.fadedown);
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

#endif
