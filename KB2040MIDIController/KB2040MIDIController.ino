/*
  Six Button USB MIDI CC Controller

  Board: Adafruit KB2040 using the Philhower RP2040 Arduino core.

  Wire each button between its input pin and GND. The sketch enables the
  internal pull-up resistor, so a pressed button reads LOW.
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <EEPROM.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

#if defined(ARDUINO_ARCH_RP2040) && !defined(USE_TINYUSB)
#error "Select Tools > USB Stack > Adafruit TinyUSB, or compile with :usbstack=tinyusb."
#endif

Adafruit_USBD_MIDI usbMidi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbMidi, MIDI);

const byte MIDI_CHANNEL_1 = 1; // Arduino MIDI library channels are encoded 1-16.
const unsigned long DEBOUNCE_MS = 25;
const unsigned long EDIT_HOLD_MS = 4000;
const unsigned long BUTTON_OUTPUT_SCREEN_MS = 5000;
const byte EDIT_PREVIOUS_PIN = 10;
const byte EDIT_NEXT_PIN = A0;
const unsigned long DISPLAY_MIN_UPDATE_MS = 80;
const uint32_t I2C_TIMEOUT_MS = 3;
const int EEPROM_SIGNATURE_ADDRESS = 0;
const int EEPROM_BUTTON_PRESETS_ADDRESS = 1;
const byte EEPROM_SIGNATURE = 0x4D;
const int EEPROM_SIZE = 256;

struct MidiButton {
  byte pin;
  byte controlChange;
  byte pressValue;
  byte presetIndex;
  bool stablePressed;
  bool lastReadingPressed;
  unsigned long lastChangeMs;
  unsigned long pressedAtMs;
  bool editHoldHandled;
};

MidiButton buttons[] = {
  {4, 69, 0, 17, false, false, 0, 0, false},
  {5, 69, 1, 18, false, false, 0, 0, false},
  {6, 69, 2, 19, false, false, 0, 0, false},
  {7, 52, 127, 3, false, false, 0, 0, false},
  {8, 53, 127, 4, false, false, 0, 0, false},
  {9, 25, 127, 255, false, false, 0, 0, false}
};

const byte BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0]);
const byte NO_BUTTON_SELECTED = 255;
const byte NO_PRESET_SELECTED = 255;

struct MidiPreset {
  char section[20];
  char function[24];
  byte controlChange;
  byte value;
};

const MidiPreset presets[] = {
  {"Footswitch Assign", "Emulates FS1", 49, 127},
  {"Footswitch Assign", "Emulates FS2", 50, 127},
  {"Footswitch Assign", "Emulates FS3", 51, 127},
  {"Footswitch Assign", "Emulates FS4", 52, 127},
  {"Footswitch Assign", "Emulates FS5", 53, 127},
  {"Looper Controls", "Looper Record", 60, 127},
  {"Looper Controls", "Looper Overdub", 60, 63},
  {"Looper Controls", "Looper Play", 61, 127},
  {"Looper Controls", "Looper Stop", 61, 63},
  {"Looper Controls", "Looper Play Once", 62, 127},
  {"Looper Controls", "Looper Undo/Redo", 63, 127},
  {"Looper Controls", "Looper Forward", 65, 63},
  {"Looper Controls", "Looper Reverse", 65, 127},
  {"Looper Controls", "Looper Full Speed", 66, 63},
  {"Looper Controls", "Looper Half Speed", 66, 127},
  {"Tempo", "Tap Tempo", 64, 127},
  {"Tuner", "Tuner screen on/off", 68, 127},
  {"Snapshot", "Snapshot 1", 69, 0},
  {"Snapshot", "Snapshot 2", 69, 1},
  {"Snapshot", "Snapshot 3", 69, 2},
  {"Snapshot", "Next Snapshot", 69, 8},
  {"Snapshot", "Previous Snapshot", 69, 9},
  {"All Bypass", "Bypass On", 70, 63},
  {"All Bypass", "Bypass Off", 70, 127},
  {"Footswitch Mode", "Stomp", 71, 0},
  {"Footswitch Mode", "Scroll", 71, 1},
  {"Footswitch Mode", "Preset", 71, 2},
  {"Footswitch Mode", "Snapshot", 71, 3},
  {"Footswitch Mode", "Next Footswitch", 71, 4},
  {"Footswitch Mode", "Previous Footswitch", 71, 5},
  {"Preset", "Previous Preset", 72, 63},
  {"Preset", "Next Preset", 72, 127}
};

const byte PRESET_COUNT = sizeof(presets) / sizeof(presets[0]);

U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
bool displayAvailable = false;
bool editMode = false;
byte editButtonIndex = NO_BUTTON_SELECTED;
unsigned long lastDisplayUpdateMs = 0;
bool pendingEditDisplayUpdate = false;
bool showingButtonOutput = false;
unsigned long buttonOutputShownAtMs = 0;

struct EditButton {
  byte pin;
  bool stablePressed;
  bool lastReadingPressed;
  unsigned long lastChangeMs;
};

EditButton editPreviousButton = {EDIT_PREVIOUS_PIN, false, false, 0};
EditButton editNextButton = {EDIT_NEXT_PIN, false, false, 0};

void copyPreset(byte presetIndex, MidiPreset &preset) {
  preset = presets[presetIndex];
}

void configureI2CTimeout() {
  Wire1.setTimeout(I2C_TIMEOUT_MS, true);
  Wire1.clearTimeoutFlag();
}

void clearI2CTimeoutFlag() {
  Wire1.clearTimeoutFlag();
}

bool getI2CTimeoutFlag() {
  return Wire1.getTimeoutFlag();
}

void commitEEPROM() {
  EEPROM.commit();
}

void sendControlChange(byte channel, byte control, byte value) {
  MIDI.sendControlChange(control, value, channel);
}

bool isValidPresetIndex(byte presetIndex) {
  return presetIndex == NO_PRESET_SELECTED || presetIndex < PRESET_COUNT;
}

void applyPresetToButton(byte buttonIndex, byte presetIndex) {
  if (!isValidPresetIndex(presetIndex)) {
    return;
  }

  buttons[buttonIndex].presetIndex = presetIndex;

  if (presetIndex == NO_PRESET_SELECTED) {
    return;
  }

  MidiPreset preset;
  copyPreset(presetIndex, preset);

  buttons[buttonIndex].controlChange = preset.controlChange;
  buttons[buttonIndex].pressValue = preset.value;
}

void saveButtonPreset(byte buttonIndex, bool commitChange = true) {
  EEPROM.update(EEPROM_BUTTON_PRESETS_ADDRESS + buttonIndex, buttons[buttonIndex].presetIndex);

  if (commitChange) {
    commitEEPROM();
  }
}

void loadButtonPresets() {
  if (EEPROM.read(EEPROM_SIGNATURE_ADDRESS) != EEPROM_SIGNATURE) {
    EEPROM.update(EEPROM_SIGNATURE_ADDRESS, EEPROM_SIGNATURE);

    for (byte i = 0; i < BUTTON_COUNT; i++) {
      saveButtonPreset(i, false);
    }

    commitEEPROM();
    return;
  }

  bool repairedSavedPreset = false;

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    const byte savedPresetIndex = EEPROM.read(EEPROM_BUTTON_PRESETS_ADDRESS + i);

    if (isValidPresetIndex(savedPresetIndex)) {
      applyPresetToButton(i, savedPresetIndex);
    } else {
      saveButtonPreset(i, false);
      repairedSavedPreset = true;
    }
  }

  if (repairedSavedPreset) {
    commitEEPROM();
  }
}

bool startDisplayUpdate() {
  if (!displayAvailable) {
    return false;
  }

  lastDisplayUpdateMs = millis();
  clearI2CTimeoutFlag();
  return true;
}

void finishDisplayUpdate() {
  display.sendBuffer();

  if (getI2CTimeoutFlag()) {
    displayAvailable = false;
    clearI2CTimeoutFlag();
  }
}

void drawClippedText(const char *text, byte x, byte baselineY, byte maxPixels) {
  char buffer[32];
  strncpy(buffer, text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  while (strlen(buffer) > 0 && display.getStrWidth(buffer) > maxPixels) {
    buffer[strlen(buffer) - 1] = '\0';
  }

  display.drawStr(x, baselineY, buffer);
}

void drawStatusFooter(byte controlChange, byte value) {
  char line[18];
  snprintf(line, sizeof(line), "CC %u  VAL %u", controlChange, value);
  display.drawRFrame(0, 51, 128, 13, 2);
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(4, 61, line);
}

void drawHeader(const char *text) {
  display.drawBox(0, 0, 128, 13);
  display.setDrawColor(0);
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(4, 10, text);
  display.setDrawColor(1);
}

void drawButtonCell(byte x, byte y, const char *label) {
  display.drawRFrame(x, y, 24, 13, 3);
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(x + 9, y + 10, label);
}

void drawCenteredText(byte baselineY, const char *text) {
  const int textWidth = display.getStrWidth(text);
  const int x = max(0, (128 - textWidth) / 2);
  display.drawStr(x, baselineY, text);
}

void showReadyScreen() {
  if (!startDisplayUpdate()) {
    return;
  }

  showingButtonOutput = false;
  display.clearBuffer();
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredText(12, "USB MIDI Ctrl");
  display.drawHLine(0, 16, 128);
  display.setFont(u8g2_font_6x10_tf);
  drawCenteredText(28, "Button Layout");
  drawButtonCell(4, 31, "1");
  drawButtonCell(52, 31, "2");
  drawButtonCell(100, 31, "3");
  drawButtonCell(4, 45, "4");
  drawButtonCell(52, 45, "5");
  drawButtonCell(100, 45, "6");
  display.setFont(u8g2_font_4x6_tf);
  drawCenteredText(64, "MIDI Channel 1");
  finishDisplayUpdate();
}

void showButtonOutput(byte buttonIndex) {
  if (!startDisplayUpdate()) {
    return;
  }

  display.clearBuffer();
  char line[20];
  MidiButton &button = buttons[buttonIndex];

  snprintf(line, sizeof(line), "Last button B%u", buttonIndex + 1);
  drawHeader(line);

  display.setFont(u8g2_font_7x14B_tf);
  if (button.presetIndex == NO_PRESET_SELECTED) {
    drawClippedText("Unlisted Preset", 0, 35, 128);
  } else {
    MidiPreset preset;
    copyPreset(button.presetIndex, preset);
    drawClippedText(preset.function, 0, 35, 128);
  }

  drawStatusFooter(button.controlChange, button.pressValue);
  showingButtonOutput = true;
  buttonOutputShownAtMs = millis();
  finishDisplayUpdate();
}

void showEditModeScreen() {
  if (editButtonIndex == NO_BUTTON_SELECTED || !startDisplayUpdate()) {
    return;
  }

  MidiButton &button = buttons[editButtonIndex];
  char line[20];

  display.clearBuffer();
  snprintf(line, sizeof(line), "EDIT B%u", editButtonIndex + 1);
  drawHeader(line);

  if (button.presetIndex == NO_PRESET_SELECTED) {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 28, "Unlisted Preset");
    display.setFont(u8g2_font_7x14B_tf);
    drawClippedText("Current button", 0, 43, 128);
  } else {
    MidiPreset preset;
    copyPreset(button.presetIndex, preset);
    display.setFont(u8g2_font_6x10_tf);
    drawClippedText(preset.section, 0, 28, 128);
    display.setFont(u8g2_font_7x14B_tf);
    drawClippedText(preset.function, 0, 43, 128);
  }

  drawStatusFooter(button.controlChange, button.pressValue);
  finishDisplayUpdate();
}

void requestEditDisplayUpdate() {
  pendingEditDisplayUpdate = true;
}

void serviceDisplayUpdates(unsigned long now) {
  if (!pendingEditDisplayUpdate || !displayAvailable) {
    return;
  }

  if (now - lastDisplayUpdateMs < DISPLAY_MIN_UPDATE_MS) {
    return;
  }

  pendingEditDisplayUpdate = false;
  showEditModeScreen();
}

void enterEditMode(byte buttonIndex) {
  editMode = true;
  editButtonIndex = buttonIndex;
  pendingEditDisplayUpdate = false;
  showingButtonOutput = false;
  showEditModeScreen();
}

void exitEditMode() {
  editMode = false;
  editButtonIndex = NO_BUTTON_SELECTED;
  pendingEditDisplayUpdate = false;
  showReadyScreen();
}

void changeEditedPreset(int delta) {
  if (!editMode || editButtonIndex == NO_BUTTON_SELECTED) {
    return;
  }

  MidiButton &button = buttons[editButtonIndex];
  int newPreset;

  if (button.presetIndex == NO_PRESET_SELECTED) {
    newPreset = delta > 0 ? 0 : PRESET_COUNT - 1;
  } else {
    newPreset = button.presetIndex + delta;
  }

  if (newPreset < 0) {
    newPreset = PRESET_COUNT - 1;
  } else if (newPreset >= PRESET_COUNT) {
    newPreset = 0;
  }

  if (buttons[editButtonIndex].presetIndex != newPreset) {
    applyPresetToButton(editButtonIndex, newPreset);
    saveButtonPreset(editButtonIndex);
  }

  requestEditDisplayUpdate();
}

bool updateEditButton(EditButton &button, unsigned long now) {
  const bool readingPressed = digitalRead(button.pin) == LOW;

  if (readingPressed != button.lastReadingPressed) {
    button.lastReadingPressed = readingPressed;
    button.lastChangeMs = now;
  }

  if ((now - button.lastChangeMs) >= DEBOUNCE_MS &&
      readingPressed != button.stablePressed) {
    button.stablePressed = readingPressed;
    return button.stablePressed;
  }

  return false;
}

void setupUSBMIDI() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  Serial.begin(115200);
  usbMidi.setStringDescriptor("KB2040 MIDI Ctrl");
  MIDI.begin(MIDI_CHANNEL_OMNI);

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void setup() {
  setupUSBMIDI();
  pinMode(LED_BUILTIN, OUTPUT);

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }

  pinMode(EDIT_PREVIOUS_PIN, INPUT_PULLUP);
  pinMode(EDIT_NEXT_PIN, INPUT_PULLUP);

  EEPROM.begin(EEPROM_SIZE);
  loadButtonPresets();

  delay(50);
  Wire1.begin();
  configureI2CTimeout();
  display.begin();
  display.setPowerSave(0);
  display.setFont(u8g2_font_6x10_tf);
  displayAvailable = !getI2CTimeoutFlag();

  if (getI2CTimeoutFlag()) {
    displayAvailable = false;
    clearI2CTimeoutFlag();
  }

  if (displayAvailable) {
    showReadyScreen();
  }
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
#endif

  MIDI.read();

  const unsigned long now = millis();
  bool anyButtonPressed = false;

  if (!editMode &&
      showingButtonOutput &&
      now - buttonOutputShownAtMs >= BUTTON_OUTPUT_SCREEN_MS) {
    showReadyScreen();
  }

  if (editMode) {
    if (updateEditButton(editPreviousButton, now)) {
      changeEditedPreset(-1);
    }

    if (updateEditButton(editNextButton, now)) {
      changeEditedPreset(1);
    }

    serviceDisplayUpdates(now);
  }

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    MidiButton &button = buttons[i];
    const bool readingPressed = digitalRead(button.pin) == LOW;

    if (readingPressed != button.lastReadingPressed) {
      button.lastReadingPressed = readingPressed;
      button.lastChangeMs = now;
    }

    if ((now - button.lastChangeMs) >= DEBOUNCE_MS &&
        readingPressed != button.stablePressed) {
      button.stablePressed = readingPressed;

      if (button.stablePressed) {
        button.pressedAtMs = now;
        button.editHoldHandled = false;

        if (editMode) {
          if (i == editButtonIndex) {
            exitEditMode();
          }
        } else {
          sendControlChange(MIDI_CHANNEL_1, button.controlChange, button.pressValue);
          showButtonOutput(i);
        }
      } else {
        button.pressedAtMs = 0;
        button.editHoldHandled = false;
      }
    }

    if (!editMode &&
        button.stablePressed &&
        !button.editHoldHandled &&
        button.pressedAtMs > 0 &&
        now - button.pressedAtMs >= EDIT_HOLD_MS) {
      button.editHoldHandled = true;
      enterEditMode(i);
    }

    if (button.stablePressed) {
      anyButtonPressed = true;
    }
  }

  digitalWrite(LED_BUILTIN, anyButtonPressed ? HIGH : LOW);
}
