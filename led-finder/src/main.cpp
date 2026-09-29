#include <Arduino.h>

// Drives each GPIO that is safe to probe in turn, so we can find out which
// pins the breadboard LEDs are wired to. Each pin is driven HIGH, then LOW
// (for an LED wired to 3V3 instead of GND), then released.
//
// Press any button when an LED lights up. The board reports which pin was
// being driven at that moment, and the one before it in case the press came
// just after the step moved on:
//   FOUND GPIO <n> <HIGH|LOW> (previous: GPIO <m> <HIGH|LOW>)
//
// Left out: 0 (BOOT button), 3/4/5 (the push buttons), 19/20 (USB),
// 26-37 (flash and PSRAM), 48 (RGB LED).

static const uint8_t pins[] = {
  1, 2, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21,
  38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
};
static const size_t pinCount = sizeof(pins) / sizeof(pins[0]);
static const uint8_t buttonPins[] = {5, 4, 3};

static const uint32_t STEP_MS = 1500;
static const uint32_t LATE_MS = 700;    // a press this soon after a step may be for the last one
static const uint32_t DEBOUNCE_MS = 20;
static const int PASSES = 3;

static int step = -1;           // pin index * 2 + phase (0 = HIGH, 1 = LOW)
static int pass = 0;
static uint32_t stepAt = 0;

static void describe(int s, char *out, size_t len) {
  if (s < 0) snprintf(out, len, "none");
  else snprintf(out, len, "GPIO %u %s", pins[s / 2], s % 2 ? "LOW" : "HIGH");
}

static void startStep(int s) {
  if (step >= 0) pinMode(pins[step / 2], INPUT);
  step = s;
  stepAt = millis();
  uint8_t pin = pins[s / 2];
  pinMode(pin, OUTPUT);
  digitalWrite(pin, s % 2 ? LOW : HIGH);
  char d[24];
  describe(s, d, sizeof(d));
  Serial.printf("STEP %s\n", d);
}

void setup() {
  Serial.begin(115200);
  for (uint8_t p : buttonPins) pinMode(p, INPUT_PULLDOWN);
  delay(3000);
  Serial.println("led finder ready - press any button when an LED lights");
  startStep(0);
}

void loop() {
  static bool stable = false, lastRead = false;
  static uint32_t changedAt = 0;
  uint32_t now = millis();

  bool level = false;
  for (uint8_t p : buttonPins) level |= digitalRead(p);
  if (level != lastRead) {
    lastRead = level;
    changedAt = now;
  } else if (level != stable && now - changedAt >= DEBOUNCE_MS) {
    stable = level;
    if (level && step >= 0) {
      char cur[24], prev[24];
      describe(step, cur, sizeof(cur));
      describe(now - stepAt < LATE_MS ? step - 1 : -1, prev, sizeof(prev));
      Serial.printf("FOUND %s (previous: %s)\n", cur, prev);
    }
  }

  if (step >= 0 && now - stepAt >= STEP_MS) {
    int next = step + 1;
    if (next >= (int)pinCount * 2) {
      if (++pass >= PASSES) {
        pinMode(pins[step / 2], INPUT);
        step = -1;
        Serial.println("DONE");
        return;
      }
      Serial.printf("PASS %d\n", pass + 1);
      next = 0;
    }
    startStep(next);
  }
  delay(1);
}
