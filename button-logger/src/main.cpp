#include <Arduino.h>

// Watches three push buttons and reports each press over USB serial as
//   PRESS <colour> <millis since boot>
// Each button connects its pin to 3V3, so a pressed button reads HIGH.
// A press counts once per 5 s per button; repeats within that span are ignored.

struct Button {
  const char *name;
  uint8_t pin;
  bool stable;          // debounced level: true = pressed (HIGH)
  bool lastRead;        // most recent raw reading
  uint32_t changedAt;   // when the raw reading last changed
  uint32_t pressedAt;   // when the last counted press happened
  bool counted;         // whether any press has been counted yet
};

static Button buttons[] = {
  {"red", 5},
  {"yellow", 4},
  {"blue", 3},
};

static const uint32_t DEBOUNCE_MS = 20;
// Each counted press covers this span: further presses of the same button
// within it are ignored, so one experience is logged once.
static const uint32_t LOCKOUT_MS = 5000;

void setup() {
  Serial.begin(115200);
  for (Button &b : buttons) {
    pinMode(b.pin, INPUT_PULLDOWN);
    b.stable = b.lastRead = digitalRead(b.pin);
    b.changedAt = millis();
  }
  delay(1000);
  // Report the idle level of each pin; every button should read LOW when
  // untouched. A HIGH here means a button is stuck or wired differently.
  Serial.print("READY");
  for (Button &b : buttons) {
    Serial.printf(" %s=%s", b.name, b.stable ? "HIGH" : "LOW");
  }
  Serial.println();
}

void loop() {
  uint32_t now = millis();
  for (Button &b : buttons) {
    bool level = digitalRead(b.pin);
    if (level != b.lastRead) {
      b.lastRead = level;
      b.changedAt = now;
    } else if (level != b.stable && now - b.changedAt >= DEBOUNCE_MS) {
      b.stable = level;
      if (level && (!b.counted || now - b.pressedAt >= LOCKOUT_MS)) {
        b.counted = true;
        b.pressedAt = now;
        Serial.printf("PRESS %s %lu\n", b.name, (unsigned long)now);
      }
    }
  }
  delay(1);
}
