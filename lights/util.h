#ifndef UTIL_H
#define UTIL_H

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

void logf(const char *format, ...)
{
#if SERIAL_LOGGING
  va_list argptr;
  va_start(argptr, format);
  char *buf = (char *)calloc(strlen(format) + 200, sizeof(char));
  vsnprintf(buf, 200, format, argptr);
  va_end(argptr);
  Serial.println(buf);
  free(buf);
#endif
}

#define MOD_DISTANCE(a, b, m) (abs(m / 2. - fmod((3 * m) / 2 + a - b, m)))

inline int mod_wrap(int x, int m) {
  int result = x % m;
  return result < 0 ? result + m : result;
}

inline float fmod_wrap(float x, int m) {
  float result = fmod(x, m);
  return result < 0 ? result + m : result;
}

class FrameCounter {
  private:
    long lastPrint = 0;
    long frames = 0;
  public:
    long printInterval = 2000;
    void tick() {
      unsigned long mil = millis();
      long elapsed = mil - lastPrint;
      if (elapsed > printInterval) {
        if (lastPrint != 0) {
          logf("Framerate: %f", frames / (float)elapsed * 1000);
        }
        frames = 0;
        lastPrint = mil;
      }
      ++frames;
    }
};

#endif

