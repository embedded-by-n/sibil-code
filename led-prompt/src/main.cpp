#include <Arduino.h>

// Lights a prompt LED on command from the computer and waits for the
// matching button to be pressed to confirm it was seen.
//
// Commands, one per line over USB serial:
//   ON <colour>   light that colour's prompt LED (red, yellow or blue)
//   OFF           switch the prompt LEDs off without waiting for a press
//
// Replies:
//   LED ON <colour> / LED OFF    command accepted
//   ACK <colour> <ms>            the matching button was pressed; ms since it lit
//   WRONG <colour> <ms>          another button was pressed; the LED stays lit
//   PRESS <colour> <millis>      every counted press, as in button-logger
//
// A press counts once per 5 s per button; repeats within that span are ignored.
//
// Buttons connect their pins to 3V3, so a pressed button reads HIGH. Each
// prompt LED runs from its own pin through a resistor to GND, separate from
// the buttons.

struct Button {
  const char *name;
  uint8_t pin;
  uint8_t ledPin;       // this colour's prompt LED
  bool stable;          // debounced level: true = pressed (HIGH)
  bool lastRead;        // most recent raw reading
  uint32_t changedAt;   // when the raw reading last changed
  uint32_t pressedAt;   // when the last counted press happened
  bool counted;         // whether any press has been counted yet
};

static Button buttons[] = {
  {"red", 5, 10},
  {"yellow", 4, 11},
  {"blue", 3, 12},
};

static const uint32_t DEBOUNCE_MS = 20;
// Each counted press covers this span: further presses of the same button
// within it are ignored, so one experience is logged once.
static const uint32_t LOCKOUT_MS = 5000;

static Button *prompted = nullptr;  // colour currently lit, if any
static uint32_t litAt = 0;
static char line[32];
static size_t lineLen = 0;

static void ledsOff() {
  for (Button &b : buttons) digitalWrite(b.ledPin, LOW);
  prompted = nullptr;
}

static void handleCommand(char *cmd) {
  if (strncmp(cmd, "ON ", 3) == 0) {
    for (Button &b : buttons) {
      if (strcasecmp(cmd + 3, b.name) == 0) {
        ledsOff();
        digitalWrite(b.ledPin, HIGH);
        prompted = &b;
        litAt = millis();
        Serial.printf("LED ON %s\n", b.name);
        return;
      }
    }
    Serial.printf("ERR unknown colour: %s\n", cmd + 3);
  } else if (strcmp(cmd, "OFF") == 0) {
    ledsOff();
    Serial.println("LED OFF");
  } else if (cmd[0] != '\0') {
    Serial.printf("ERR unknown command: %s\n", cmd);
  }
}

static void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[lineLen] = '\0';
      handleCommand(line);
      lineLen = 0;
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = toupper(c);
    }
  }
}

void setup() {
  Serial.begin(115200);
  for (Button &b : buttons) {
    pinMode(b.ledPin, OUTPUT);
    pinMode(b.pin, INPUT_PULLDOWN);
    b.stable = b.lastRead = digitalRead(b.pin);
    b.changedAt = millis();
  }
  ledsOff();
  delay(1000);
  Serial.print("READY");
  for (Button &b : buttons) {
    Serial.printf(" %s=%s", b.name, b.stable ? "HIGH" : "LOW");
  }
  Serial.println();
}

void loop() {
  readSerial();
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
        if (prompted == &b) {
          ledsOff();
          Serial.printf("ACK %s %lu\n", b.name, (unsigned long)(now - litAt));
        } else if (prompted) {
          Serial.printf("WRONG %s %lu\n", b.name, (unsigned long)(now - litAt));
        }
      }
    }
  }
  delay(1);
}
