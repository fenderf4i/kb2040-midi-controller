/*
  KB2040 USB and DIN MIDI CC Controller

  Board: Adafruit KB2040 using the Philhower RP2040 Arduino core.

  Wire each button between its input pin and GND. The sketch enables the
  internal pull-up resistor, so a pressed button reads LOW.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_seesaw.h>
#include <U8g2lib.h>
#include <MIDIUSB.h>
#include <USB.h>
#include <MIDI.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include "DeviceConfig.h"
#include "WebConfigServer.h"

#if defined(USE_TINYUSB)
#error "Select the Pico SDK USB stack for MIDI and web configuration support."
#endif

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, DIN_MIDI);

const unsigned long DEBOUNCE_MS = 25;
const unsigned long EDIT_HOLD_MS = 4000;
const unsigned long PRESET_SCREEN_MS = 2000;
const unsigned long MENU_HOLD_MS = 800;
const unsigned long MENU_TIMEOUT_MS = 10000;
const unsigned long MARQUEE_STEP_MS = 350;
const unsigned long MARQUEE_START_PAUSE_MS = 2000;
const byte DIN_MIDI_TX_PIN = 0; // KB2040 D0 / GPIO0 / UART0 TX.
const byte WEB_CONFIG_RECOVERY_PIN = 1;
const byte DEFAULT_EDIT_PREVIOUS_PIN = 10;
const byte DEFAULT_EDIT_NEXT_PIN = A0;
const unsigned long DISPLAY_MIN_UPDATE_MS = 80;
const byte DISPLAY_CONTENT_WIDTH = 128;
const byte MARQUEE_GAP_CHARS = 3;
const uint32_t I2C_TIMEOUT_MS = 20;
const uint32_t I2C_CLOCK_HZ = 400000;
const uint32_t WEB_CONFIG_BOOT_MAGIC = 0x57454243;
const uint8_t STEMMA_QT_ENCODER_ADDRESS = 0x36;
const uint8_t STEMMA_QT_ENCODER_SWITCH_PIN = 24;

struct MidiButton {
  byte pin;
  ButtonAssignmentRef assignment;
  uint16_t macroId;
  bool toggleActive;
  byte lastSentValue;
  bool stablePressed;
  bool lastReadingPressed;
  unsigned long lastChangeMs;
  unsigned long pressedAtMs;
  bool editHoldHandled;
  bool presetToggleHandled;
};

MidiButton buttons[] = {
  {4, {1, 18}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {5, {1, 19}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {6, {1, 20}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {7, {1, 4}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {8, {1, 5}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {9, {UNASSIGNED_ID, UNASSIGNED_ID}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {A1, {UNASSIGNED_ID, UNASSIGNED_ID}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false},
  {A2, {UNASSIGNED_ID, UNASSIGNED_ID}, UNASSIGNED_ID, false, 0, false, false, 0, 0, false, false}
};

struct MacroPlaybackState {
  bool active;
  uint16_t macroId;
  uint8_t nextStepIndex;
  unsigned long nextStepAtMs;
};

const byte BUTTON_STORAGE_COUNT = sizeof(buttons) / sizeof(buttons[0]);
const byte NO_BUTTON_SELECTED = 255;

DeviceConfigStore configStore;
DeviceConfigPayload deviceConfig{};
WebConfigServer webConfigServer(configStore, deviceConfig);
MacroPlaybackState macroPlayback{};

U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
bool displayAvailable = false;
bool editMode = false;
byte editButtonIndex = NO_BUTTON_SELECTED;
bool editAssignmentDirty = false;
unsigned long lastDisplayUpdateMs = 0;
unsigned long lastMarqueeUpdateMs = 0;
unsigned long marqueeStartedAtMs = 0;
uint8_t lastMenuMarqueeScreen = 255;
int8_t lastMenuMarqueeIndex = -128;
bool pendingEditDisplayUpdate = false;
bool showingButtonOutput = false;
bool showingPresetChange = false;
bool showingDefaultsRestoreResult = false;
bool defaultsRestoreSucceeded = false;
unsigned long buttonOutputShownAtMs = 0;
unsigned long presetShownAtMs = 0;
unsigned long defaultsRestoreShownAtMs = 0;
byte lastButtonOutputIndex = 0;
bool webConfigMode = false;

struct EditButton {
  byte pin;
  bool stablePressed;
  bool lastReadingPressed;
  unsigned long lastChangeMs;
  unsigned long pressedAtMs;
  bool holdHandled;
};

EditButton editPreviousButton = {DEFAULT_EDIT_PREVIOUS_PIN, false, false, 0, 0, false};
EditButton editNextButton = {DEFAULT_EDIT_NEXT_PIN, false, false, 0, 0, false};

struct EncoderControl {
  byte pinA;
  byte pinB;
  byte switchPin;
  uint8_t lastState;
  int8_t accumulator;
  bool stableSwitchPressed;
  bool lastSwitchReadingPressed;
  unsigned long switchLastChangeMs;
  unsigned long switchPressedAtMs;
  bool switchHoldHandled;
};

EncoderControl encoderControl = {A1, A2, A3, 0, 0, false, false, 0, 0, false};

struct StemmaQtEncoderControl {
  byte interruptPin;
  bool available;
  bool stableSwitchPressed;
  bool lastSwitchReadingPressed;
  unsigned long switchLastChangeMs;
  unsigned long switchPressedAtMs;
  bool switchHoldHandled;
};

Adafruit_seesaw qtEncoder;
StemmaQtEncoderControl qtEncoderControl = {10, false, false, false, 0, 0, false};

enum class MenuScreen : uint8_t {
  Main,
  Presets,
  PresetToggle,
  PresetToggleEnabled,
  PresetTogglePrimary,
  PresetToggleAlternate,
  PresetToggleSwitch,
  Switches,
  Assignments,
  MidiOutput,
  HomeDelay,
  HoldEdit,
  System,
  ConfirmResetState,
  ConfirmWebConfig,
  ConfirmBootloader,
  ConfirmRestoreDefaults
};

bool menuMode = false;
MenuScreen menuScreen = MenuScreen::Main;
int8_t menuIndex = 0;
byte menuSwitchIndex = 0;
unsigned long lastMenuInputMs = 0;

const uint8_t HOME_DELAY_OPTIONS[] = {1, 3, 5, 10, 15, 30};
const uint8_t HOME_DELAY_OPTION_COUNT = sizeof(HOME_DELAY_OPTIONS) / sizeof(HOME_DELAY_OPTIONS[0]);

const char *assignmentTypeLabel(AssignmentType type) {
  switch (type) {
    case AssignmentType::Toggle:
      return "Toggle";
    case AssignmentType::Momentary:
      return "Momentary";
    case AssignmentType::Fixed:
    default:
      return "Fixed Value";
  }
}

const char *messageTypeLabel(MidiMessageType type) {
  return type == MidiMessageType::ProgramChange ? "Program Change" : "Control Change";
}

uint16_t displayNumberForAssignment(const DeviceProfileConfig &profile,
                                    const AssignmentConfig &assignment) {
  if (assignment.messageType == MidiMessageType::ProgramChange &&
      profile.programChangeDisplayMode == ProgramChangeDisplayMode::OneBased) {
    return static_cast<uint16_t>(assignment.controlChange) + 1;
  }
  return assignment.controlChange;
}

PedalPresetConfig &activePreset() {
  return *DeviceConfigStore::activePreset(deviceConfig);
}

const PedalPresetConfig &activePresetConst() {
  return *DeviceConfigStore::activePreset(deviceConfig);
}

bool assignmentRefsMatch(const ButtonAssignmentRef &left, const ButtonAssignmentRef &right) {
  return left.profileId == right.profileId && left.assignmentId == right.assignmentId;
}

bool assignmentRefIsAssigned(const ButtonAssignmentRef &reference) {
  return reference.profileId != UNASSIGNED_ID && reference.assignmentId != UNASSIGNED_ID;
}

const AssignmentConfig *assignmentForButton(byte buttonIndex,
                                            const DeviceProfileConfig **profile = nullptr) {
  if (buttonIndex >= deviceConfig.hardware.buttonCount) return nullptr;
  return DeviceConfigStore::findAssignment(deviceConfig, buttons[buttonIndex].assignment, profile);
}

const MacroConfig *macroForButton(byte buttonIndex) {
  if (buttonIndex >= deviceConfig.hardware.buttonCount) return nullptr;
  if (buttons[buttonIndex].macroId == UNASSIGNED_ID) return nullptr;
  return DeviceConfigStore::findMacro(deviceConfig, buttons[buttonIndex].macroId);
}

void applyHardwareConfiguration() {
  byte count = deviceConfig.hardware.buttonCount;
  if (count == 0 || count > MAX_STOMP_SWITCH_COUNT) count = DEFAULT_BUTTON_COUNT;
  deviceConfig.hardware.buttonCount = count;

  for (byte i = 0; i < BUTTON_STORAGE_COUNT; ++i) {
    buttons[i].pin = deviceConfig.hardware.buttonPins[i];
    buttons[i].assignment = {UNASSIGNED_ID, UNASSIGNED_ID};
    buttons[i].macroId = UNASSIGNED_ID;
    buttons[i].toggleActive = false;
    buttons[i].stablePressed = false;
    buttons[i].lastReadingPressed = false;
    buttons[i].lastChangeMs = 0;
    buttons[i].pressedAtMs = 0;
    buttons[i].editHoldHandled = false;
    buttons[i].presetToggleHandled = false;
  }

  editPreviousButton.pin = deviceConfig.hardware.previousButtonPin;
  editPreviousButton.stablePressed = false;
  editPreviousButton.lastReadingPressed = false;
  editPreviousButton.lastChangeMs = 0;
  editPreviousButton.pressedAtMs = 0;
  editPreviousButton.holdHandled = false;
  editNextButton.pin = deviceConfig.hardware.nextButtonPin;
  editNextButton.stablePressed = false;
  editNextButton.lastReadingPressed = false;
  editNextButton.lastChangeMs = 0;
  editNextButton.pressedAtMs = 0;
  editNextButton.holdHandled = false;

  encoderControl.pinA = deviceConfig.hardware.encoderAPin;
  encoderControl.pinB = deviceConfig.hardware.encoderBPin;
  encoderControl.switchPin = deviceConfig.hardware.encoderSwitchPin;
  encoderControl.lastState = 0;
  encoderControl.accumulator = 0;
  encoderControl.stableSwitchPressed = false;
  encoderControl.lastSwitchReadingPressed = false;
  encoderControl.switchLastChangeMs = 0;
  encoderControl.switchPressedAtMs = 0;
  encoderControl.switchHoldHandled = false;

  qtEncoderControl.interruptPin = deviceConfig.hardware.qtEncoderInterruptPin;
  qtEncoderControl.available = false;
  qtEncoderControl.stableSwitchPressed = false;
  qtEncoderControl.lastSwitchReadingPressed = false;
  qtEncoderControl.switchLastChangeMs = 0;
  qtEncoderControl.switchPressedAtMs = 0;
  qtEncoderControl.switchHoldHandled = false;
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

void sendControlChange(byte channel, byte control, byte value) {
  const midiEventPacket_t packet = {
    0x0B,
    static_cast<uint8_t>(0xB0 | ((channel - 1) & 0x0F)),
    control,
    value
  };
  if (deviceConfig.settings.midiOutputMode != MidiOutputMode::DinOnly) {
    MidiUSB.sendMIDI(packet);
    MidiUSB.flush();
  }
  if (deviceConfig.settings.midiOutputMode != MidiOutputMode::UsbOnly) {
    DIN_MIDI.sendControlChange(control, value, channel);
  }
}

void sendProgramChange(byte channel, byte program) {
  const midiEventPacket_t packet = {
    0x0C,
    static_cast<uint8_t>(0xC0 | ((channel - 1) & 0x0F)),
    program,
    0
  };
  if (deviceConfig.settings.midiOutputMode != MidiOutputMode::DinOnly) {
    MidiUSB.sendMIDI(packet);
    MidiUSB.flush();
  }
  if (deviceConfig.settings.midiOutputMode != MidiOutputMode::UsbOnly) {
    DIN_MIDI.sendProgramChange(program, channel);
  }
}

const char *midiOutputModeLabel(MidiOutputMode mode) {
  switch (mode) {
    case MidiOutputMode::UsbOnly:
      return "USB only";
    case MidiOutputMode::DinOnly:
      return "DIN only";
    case MidiOutputMode::UsbAndDin:
    default:
      return "USB + DIN";
  }
}

MidiOutputMode midiOutputModeForOption(int option) {
  switch (option) {
    case 1:
      return MidiOutputMode::DinOnly;
    case 2:
      return MidiOutputMode::UsbOnly;
    case 0:
    default:
      return MidiOutputMode::UsbAndDin;
  }
}

int optionForMidiOutputMode(MidiOutputMode mode) {
  switch (mode) {
    case MidiOutputMode::DinOnly:
      return 1;
    case MidiOutputMode::UsbOnly:
      return 2;
    case MidiOutputMode::UsbAndDin:
    default:
      return 0;
  }
}

int optionForHomeDelay(uint8_t seconds) {
  for (uint8_t i = 0; i < HOME_DELAY_OPTION_COUNT; ++i) {
    if (HOME_DELAY_OPTIONS[i] == seconds) return i;
  }
  return 2;
}

void applyButtonTarget(byte buttonIndex, ButtonAssignmentRef assignment, uint16_t macroId) {
  if (buttonIndex >= deviceConfig.hardware.buttonCount) return;
  PedalPresetConfig &preset = activePreset();
  if (assignmentRefIsAssigned(assignment) &&
      DeviceConfigStore::findAssignment(deviceConfig, assignment, nullptr) == nullptr) {
    assignment = {UNASSIGNED_ID, UNASSIGNED_ID};
  }
  if (macroId != UNASSIGNED_ID && DeviceConfigStore::findMacro(deviceConfig, macroId) == nullptr) {
    macroId = UNASSIGNED_ID;
  }
  buttons[buttonIndex].assignment = assignment;
  buttons[buttonIndex].macroId = macroId;
  buttons[buttonIndex].toggleActive = false;
  const AssignmentConfig *resolved = DeviceConfigStore::findAssignment(deviceConfig, assignment, nullptr);
  buttons[buttonIndex].lastSentValue = resolved != nullptr
                                           ? (resolved->messageType == MidiMessageType::ProgramChange
                                                  ? resolved->controlChange
                                                  : resolved->type == AssignmentType::Toggle
                                                  ? resolved->valueOff
                                                  : resolved->valueOn)
                                           : 0;
  preset.buttonAssignments[buttonIndex] = assignment;
  preset.buttonMacroIds[buttonIndex] = macroId;
}

void applyAssignmentToButton(byte buttonIndex, ButtonAssignmentRef assignment) {
  applyButtonTarget(buttonIndex, assignment, UNASSIGNED_ID);
}

void applyMacroToButton(byte buttonIndex, uint16_t macroId) {
  if (buttonIndex >= deviceConfig.hardware.buttonCount) return;
  applyButtonTarget(buttonIndex, buttons[buttonIndex].assignment, macroId);
}

void saveButtonAssignment(byte buttonIndex) {
  if (buttonIndex >= deviceConfig.hardware.buttonCount) return;
  activePreset().buttonAssignments[buttonIndex] = buttons[buttonIndex].assignment;
  activePreset().buttonMacroIds[buttonIndex] = buttons[buttonIndex].macroId;
  configStore.save(deviceConfig);
}

void applyActivePreset() {
  PedalPresetConfig &preset = activePreset();
  for (byte i = 0; i < deviceConfig.hardware.buttonCount; ++i) {
    applyButtonTarget(i, preset.buttonAssignments[i], preset.buttonMacroIds[i]);
  }
}

bool setActivePresetId(uint16_t presetId, bool saveConfig) {
  if (DeviceConfigStore::findPreset(deviceConfig, presetId) == nullptr) return false;
  deviceConfig.activePresetId = presetId;
  applyActivePreset();
  if (saveConfig) configStore.save(deviceConfig);
  return true;
}

void loadDeviceConfiguration() {
  configStore.begin();
  configStore.load(deviceConfig);
  applyHardwareConfiguration();
  applyActivePreset();
}

bool sendReferencedCommandPress(const ButtonAssignmentRef &reference, byte *lastSentValue = nullptr) {
  const DeviceProfileConfig *profile = nullptr;
  const AssignmentConfig *assignment = DeviceConfigStore::findAssignment(deviceConfig, reference, &profile);
  if (assignment == nullptr || profile == nullptr) return false;

  if (assignment->messageType == MidiMessageType::ProgramChange) {
    if (lastSentValue != nullptr) *lastSentValue = assignment->controlChange;
    sendProgramChange(profile->midiChannel, assignment->controlChange);
    return true;
  }

  if (lastSentValue != nullptr) *lastSentValue = assignment->valueOn;
  sendControlChange(profile->midiChannel, assignment->controlChange, assignment->valueOn);
  return true;
}

bool startMacroPlayback(byte buttonIndex) {
  const MacroConfig *macro = macroForButton(buttonIndex);
  if (macro == nullptr || macro->stepCount == 0) return false;
  macroPlayback = {true, macro->id, 0, millis()};
  return true;
}

void serviceMacroPlayback(unsigned long now) {
  if (!macroPlayback.active ||
      static_cast<long>(now - macroPlayback.nextStepAtMs) < 0) {
    return;
  }

  const MacroConfig *macro = DeviceConfigStore::findMacro(deviceConfig, macroPlayback.macroId);
  if (macro == nullptr || macroPlayback.nextStepIndex >= macro->stepCount) {
    macroPlayback.active = false;
    return;
  }

  const MacroStepConfig &step = macro->steps[macroPlayback.nextStepIndex++];
  sendReferencedCommandPress(step.command);
  if (macroPlayback.nextStepIndex >= macro->stepCount) {
    macroPlayback.active = false;
  } else {
    macroPlayback.nextStepAtMs = now + step.delayAfterMs;
  }
}

void sendButtonPress(byte buttonIndex) {
  MidiButton &button = buttons[buttonIndex];
  if (macroForButton(buttonIndex) != nullptr) {
    startMacroPlayback(buttonIndex);
    return;
  }
  const DeviceProfileConfig *profile = nullptr;
  const AssignmentConfig *assignment = assignmentForButton(buttonIndex, &profile);
  if (assignment == nullptr) return;

  if (assignment->messageType == MidiMessageType::ProgramChange) {
    button.lastSentValue = assignment->controlChange;
    sendProgramChange(profile->midiChannel, assignment->controlChange);
    return;
  }

  byte value = assignment->valueOn;
  if (assignment->type == AssignmentType::Toggle) {
    button.toggleActive = !button.toggleActive;
    value = button.toggleActive ? assignment->valueOn : assignment->valueOff;
  }
  button.lastSentValue = value;
  sendControlChange(profile->midiChannel, assignment->controlChange, value);
}

void sendButtonRelease(byte buttonIndex) {
  if (macroForButton(buttonIndex) != nullptr) return;
  MidiButton &button = buttons[buttonIndex];
  const DeviceProfileConfig *profile = nullptr;
  const AssignmentConfig *assignment = assignmentForButton(buttonIndex, &profile);
  if (assignment == nullptr ||
      assignment->messageType != MidiMessageType::ControlChange ||
      assignment->type != AssignmentType::Momentary) return;
  button.lastSentValue = assignment->valueOff;
  sendControlChange(profile->midiChannel, assignment->controlChange, assignment->valueOff);
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

void drawScrollingText(const char *text, byte x, byte baselineY, byte maxPixels) {
  if (text == nullptr || text[0] == '\0') return;
  if (display.getStrWidth(text) <= maxPixels) {
    display.drawStr(x, baselineY, text);
    return;
  }

  const size_t textLength = strlen(text);
  const size_t cycleLength = textLength + MARQUEE_GAP_CHARS;
  const unsigned long elapsedMs = millis() - marqueeStartedAtMs;
  const size_t start = elapsedMs < MARQUEE_START_PAUSE_MS
                           ? 0
                           : ((elapsedMs - MARQUEE_START_PAUSE_MS) / MARQUEE_STEP_MS) % cycleLength;
  char buffer[48] = "";
  size_t outputLength = 0;

  while (outputLength < sizeof(buffer) - 1) {
    const size_t sourceIndex = (start + outputLength) % cycleLength;
    buffer[outputLength] = sourceIndex < textLength ? text[sourceIndex] : ' ';
    buffer[outputLength + 1] = '\0';
    if (display.getStrWidth(buffer) > maxPixels) {
      buffer[outputLength] = '\0';
      break;
    }
    ++outputLength;
  }

  display.drawStr(x, baselineY, buffer);
}

void resetMarqueePhase() {
  marqueeStartedAtMs = millis();
  lastMarqueeUpdateMs = marqueeStartedAtMs;
}

void resetMenuMarqueeTracking() {
  lastMenuMarqueeScreen = 255;
  lastMenuMarqueeIndex = -128;
}

void drawStatusFooter(MidiMessageType messageType,
                      byte midiChannel,
                      uint16_t number,
                      byte value,
                      bool variableValue = false) {
  char line[20];
  if (messageType == MidiMessageType::ProgramChange) {
    snprintf(line, sizeof(line), "CH%u PC%u", midiChannel, number);
  } else if (variableValue) {
    snprintf(line, sizeof(line), "CH%u CC%u 0-127", midiChannel, number);
  } else {
    snprintf(line, sizeof(line), "CH%u CC%u VAL%u", midiChannel, number, value);
  }
  display.drawRFrame(0, 51, DISPLAY_CONTENT_WIDTH, 13, 2);
  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(4, 61, line);
}

void drawHeader(const char *text) {
  display.drawBox(0, 0, DISPLAY_CONTENT_WIDTH, 13);
  display.setDrawColor(0);
  display.setFont(u8g2_font_6x10_tf);
  drawScrollingText(text, 4, 10, DISPLAY_CONTENT_WIDTH - 8);
  display.setDrawColor(1);
}

void drawMenuPositionIndicator(int selectedIndex, int itemCount) {
  if (itemCount <= 1) return;
  constexpr byte trackX = DISPLAY_CONTENT_WIDTH - 2;
  constexpr byte trackY = 16;
  constexpr byte trackHeight = 45;
  byte thumbHeight = trackHeight / itemCount;
  if (thumbHeight < 5) thumbHeight = 5;
  const int travel = trackHeight - thumbHeight;
  const byte thumbY = trackY + ((travel * selectedIndex) / (itemCount - 1));

  for (byte y = trackY; y < trackY + trackHeight; y += 4) {
    display.drawPixel(trackX + 1, y);
  }
  display.drawBox(trackX, thumbY, 2, thumbHeight);
}

void drawButtonCell(byte x, byte y, byte width, byte height, const char *label) {
  display.drawRFrame(x, y, width, height, 4);
  display.setFont(height >= 30 ? u8g2_font_logisoso16_tf : u8g2_font_7x14B_tf);
  const int textWidth = display.getStrWidth(label);
  const int ascent = display.getAscent();
  const int descent = display.getDescent();
  const int textHeight = ascent - descent;
  const int textX = x + max(0, (static_cast<int>(width) - textWidth) / 2);
  const int baselineY = y + max(0, (static_cast<int>(height) - textHeight) / 2) + ascent;
  display.drawStr(textX, baselineY, label);
}

const PedalPresetConfig *presetToggleTargetPreset() {
  if (!deviceConfig.presetToggle.enabled) return nullptr;
  const uint16_t activeId = deviceConfig.activePresetId;
  const uint16_t targetId = activeId == deviceConfig.presetToggle.primaryPresetId
                                ? deviceConfig.presetToggle.alternatePresetId
                                : activeId == deviceConfig.presetToggle.alternatePresetId
                                ? deviceConfig.presetToggle.primaryPresetId
                                : UNASSIGNED_ID;
  return targetId == UNASSIGNED_ID ? nullptr : DeviceConfigStore::findPreset(deviceConfig, targetId);
}

bool presetToggleAppliesToButton(byte buttonIndex) {
  return deviceConfig.presetToggle.enabled &&
         deviceConfig.presetToggle.switchNumber == buttonIndex + 1 &&
         presetToggleTargetPreset() != nullptr;
}

void formatPresetToggleTargetLine(byte buttonIndex, char *output, size_t outputSize) {
  if (outputSize == 0) return;
  output[0] = '\0';
  const PedalPresetConfig *targetPreset = presetToggleAppliesToButton(buttonIndex)
                                             ? presetToggleTargetPreset()
                                             : nullptr;
  if (targetPreset == nullptr) return;
  snprintf(output, outputSize, "B%u to %s", buttonIndex + 1, targetPreset->name);
  output[outputSize - 1] = '\0';
}

void formatHomeButtonLabel(byte buttonIndex, char *output, size_t outputSize) {
  if (outputSize == 0) return;
  const PedalPresetConfig *targetPreset = presetToggleAppliesToButton(buttonIndex)
                                             ? presetToggleTargetPreset()
                                             : nullptr;
  if (targetPreset != nullptr) {
    snprintf(output, outputSize, "Page %s", targetPreset->name);
    output[outputSize - 1] = '\0';
    return;
  }

  const MacroConfig *macro = macroForButton(buttonIndex);
  if (macro != nullptr) {
    snprintf(output, outputSize, "Macro %s", macro->name);
    output[outputSize - 1] = '\0';
    return;
  }

  const AssignmentConfig *assignment = assignmentForButton(buttonIndex, nullptr);
  if (assignment == nullptr) {
    strncpy(output, "Unassigned", outputSize);
  } else {
    strncpy(output, assignment->label, outputSize);
  }
  output[outputSize - 1] = '\0';
}

void drawButtonAssignmentMap() {
  const byte count = deviceConfig.hardware.buttonCount;
  if (count == 0) return;

  const byte columns = 1;
  const byte rows = count;
  const byte areaY = 16;
  const byte cellWidth = DISPLAY_CONTENT_WIDTH / columns;
  const byte rowHeight = (64 - areaY) / rows;
  char number[4];
  char label[DEVICE_NAME_LENGTH + 6];

  display.setFont(u8g2_font_5x8_tf);
  for (byte i = 0; i < count; ++i) {
    const byte column = 0;
    const byte row = i;
    const byte x = column * cellWidth + 1;
    const byte textOffset = rowHeight > 9 ? 8 : rowHeight - 1;
    const byte baselineY = areaY + row * rowHeight + textOffset;
    formatHomeButtonLabel(i, label, sizeof(label));
    snprintf(number, sizeof(number), "%u", i + 1);
    display.drawStr(x, baselineY, number);
    drawScrollingText(label, x + 9, baselineY, cellWidth - 12);
  }
}

void drawCenteredText(byte baselineY, const char *text) {
  const int textWidth = display.getStrWidth(text);
  const int x = max(0, (DISPLAY_CONTENT_WIDTH - textWidth) / 2);
  display.drawStr(x, baselineY, text);
}

void drawCenteredClippedText(byte baselineY, const char *text) {
  char buffer[DEVICE_NAME_LENGTH];
  strncpy(buffer, text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  while (strlen(buffer) > 0 && display.getStrWidth(buffer) > DISPLAY_CONTENT_WIDTH) {
    buffer[strlen(buffer) - 1] = '\0';
  }
  drawCenteredText(baselineY, buffer);
}

void drawCenteredScrollingText(byte baselineY, const char *text) {
  if (text == nullptr) return;
  if (display.getStrWidth(text) <= DISPLAY_CONTENT_WIDTH) {
    drawCenteredText(baselineY, text);
  } else {
    drawScrollingText(text, 0, baselineY, DISPLAY_CONTENT_WIDTH);
  }
}

void showReadyScreen() {
  if (!startDisplayUpdate()) {
    return;
  }

  showingButtonOutput = false;
  showingPresetChange = false;
  showingDefaultsRestoreResult = false;
  display.clearBuffer();
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredScrollingText(12, activePresetConst().name);
  drawButtonAssignmentMap();
  finishDisplayUpdate();
}

void showPresetChangeScreen() {
  if (!startDisplayUpdate()) return;
  showingDefaultsRestoreResult = false;
  display.clearBuffer();
  drawHeader("PRESET");
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredClippedText(37, activePresetConst().name);
  display.setFont(u8g2_font_6x10_tf);
  char line[18];
  uint8_t activeIndex = 0;
  for (uint8_t i = 0; i < deviceConfig.presetCount; ++i) {
    if (deviceConfig.presets[i].id == deviceConfig.activePresetId) activeIndex = i;
  }
  snprintf(line, sizeof(line), "%u of %u", activeIndex + 1, deviceConfig.presetCount);
  drawCenteredText(54, line);
  showingButtonOutput = false;
  showingPresetChange = true;
  presetShownAtMs = millis();
  finishDisplayUpdate();
}

bool handlePresetTogglePress(byte buttonIndex) {
  if (!presetToggleAppliesToButton(buttonIndex)) return false;
  const PedalPresetConfig *targetPreset = presetToggleTargetPreset();
  if (targetPreset == nullptr) return false;
  if (!setActivePresetId(targetPreset->id, false)) return false;
  showReadyScreen();
  return true;
}

void showPresetStateResetScreen() {
  if (!startDisplayUpdate()) return;
  showingDefaultsRestoreResult = false;
  display.clearBuffer();
  drawHeader("RESET");
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredClippedText(35, activePresetConst().name);
  display.setFont(u8g2_font_6x10_tf);
  drawCenteredText(54, "State cleared");
  showingButtonOutput = false;
  showingPresetChange = true;
  presetShownAtMs = millis();
  finishDisplayUpdate();
}

void drawDefaultsRestoreResultScreen(bool saved) {
  display.clearBuffer();
  drawHeader("DEFAULTS");
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredText(35, saved ? "Restored" : "Save failed");
  display.setFont(u8g2_font_6x10_tf);
  drawCenteredText(54, saved ? "Returning home" : "Try web config");
}

void showDefaultsRestoredScreen() {
  if (!startDisplayUpdate()) return;
  drawDefaultsRestoreResultScreen(true);
  showingButtonOutput = false;
  showingPresetChange = false;
  showingDefaultsRestoreResult = true;
  defaultsRestoreSucceeded = true;
  defaultsRestoreShownAtMs = millis();
  finishDisplayUpdate();
}

void showDefaultsRestoreFailedScreen() {
  if (!startDisplayUpdate()) return;
  drawDefaultsRestoreResultScreen(false);
  showingButtonOutput = false;
  showingPresetChange = false;
  showingDefaultsRestoreResult = true;
  defaultsRestoreSucceeded = false;
  defaultsRestoreShownAtMs = millis();
  finishDisplayUpdate();
}

void showWebConfigScreen() {
  if (!startDisplayUpdate()) return;
  display.clearBuffer();
  drawHeader("WEB CONFIG");
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredText(34, "192.168.7.1");
  display.setFont(u8g2_font_6x10_tf);
  drawCenteredText(49, webConfigServer.isRunning() ? "USB connected" : "Network error");
  display.setFont(u8g2_font_4x6_tf);
  drawCenteredText(62, "Configuration mode");
  finishDisplayUpdate();
}

void showWebConfigStartingScreen() {
  if (!startDisplayUpdate()) return;
  display.clearBuffer();
  drawHeader("WEB CONFIG");
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredText(36, "Starting...");
  display.setFont(u8g2_font_6x10_tf);
  drawCenteredText(54, "USB network");
  finishDisplayUpdate();
}

void showButtonOutput(byte buttonIndex, bool resetOutputTimer = true) {
  if (!startDisplayUpdate()) {
    return;
  }

  showingDefaultsRestoreResult = false;
  display.clearBuffer();
  char line[20];
  MidiButton &button = buttons[buttonIndex];
  const DeviceProfileConfig *profile = nullptr;
  const AssignmentConfig *assignment = assignmentForButton(buttonIndex, &profile);
  const MacroConfig *macro = macroForButton(buttonIndex);

  snprintf(line, sizeof(line), "Last button B%u", buttonIndex + 1);
  drawHeader(line);

  display.setFont(u8g2_font_7x14B_tf);
  if (macro != nullptr) {
    drawClippedText(macro->name, 0, 35, DISPLAY_CONTENT_WIDTH);
    display.drawRFrame(0, 51, DISPLAY_CONTENT_WIDTH, 13, 2);
    display.setFont(u8g2_font_6x10_tf);
    char footer[18];
    snprintf(footer, sizeof(footer), "Macro / %u steps", macro->stepCount);
    display.drawStr(4, 61, footer);
  } else if (assignment == nullptr) {
    drawClippedText("Unassigned", 0, 35, DISPLAY_CONTENT_WIDTH);
    display.drawRFrame(0, 51, DISPLAY_CONTENT_WIDTH, 13, 2);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(4, 61, "No MIDI message");
  } else {
    drawClippedText(assignment->label, 0, 35, DISPLAY_CONTENT_WIDTH);
    drawStatusFooter(assignment->messageType,
                     profile->midiChannel,
                     displayNumberForAssignment(*profile, *assignment),
                     button.lastSentValue);
  }

  showingButtonOutput = true;
  lastButtonOutputIndex = buttonIndex;
  if (resetOutputTimer) {
    buttonOutputShownAtMs = millis();
  }
  finishDisplayUpdate();
}

void showEditModeScreen() {
  if (editButtonIndex == NO_BUTTON_SELECTED || !startDisplayUpdate()) {
    return;
  }

  MidiButton &button = buttons[editButtonIndex];
  const DeviceProfileConfig *profile = nullptr;
  const AssignmentConfig *assignment = assignmentForButton(editButtonIndex, &profile);
  const MacroConfig *macro = macroForButton(editButtonIndex);
  char line[20];

  display.clearBuffer();
  snprintf(line, sizeof(line), "EDIT B%u", editButtonIndex + 1);
  drawHeader(line);

  if (macro != nullptr) {
    display.setFont(u8g2_font_6x10_tf);
    drawClippedText("Macro", 0, 28, DISPLAY_CONTENT_WIDTH);
    display.setFont(u8g2_font_7x14B_tf);
    drawScrollingText(macro->name, 0, 43, DISPLAY_CONTENT_WIDTH);
    display.setFont(u8g2_font_6x10_tf);
    char footer[18];
    snprintf(footer, sizeof(footer), "%u steps", macro->stepCount);
    drawCenteredText(61, footer);
  } else if (assignment == nullptr) {
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 28, "Unassigned");
    display.setFont(u8g2_font_7x14B_tf);
    drawClippedText("No MIDI message", 0, 43, DISPLAY_CONTENT_WIDTH);
    display.drawRFrame(0, 51, DISPLAY_CONTENT_WIDTH, 13, 2);
  } else {
    display.setFont(u8g2_font_6x10_tf);
    drawClippedText(assignment->messageType == MidiMessageType::ProgramChange
                        ? messageTypeLabel(assignment->messageType)
                        : assignmentTypeLabel(assignment->type),
                    0,
                    28,
                    DISPLAY_CONTENT_WIDTH);
    display.setFont(u8g2_font_7x14B_tf);
    drawScrollingText(assignment->label, 0, 43, DISPLAY_CONTENT_WIDTH);
    drawStatusFooter(assignment->messageType,
                     profile->midiChannel,
                     displayNumberForAssignment(*profile, *assignment),
                     assignment->valueOn);
  }

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
  editAssignmentDirty = false;
  pendingEditDisplayUpdate = false;
  showingButtonOutput = false;
  showingPresetChange = false;
  showingDefaultsRestoreResult = false;
  resetMarqueePhase();
  showEditModeScreen();
}

void exitEditMode() {
  if (editAssignmentDirty && editButtonIndex != NO_BUTTON_SELECTED) {
    saveButtonAssignment(editButtonIndex);
  }
  editMode = false;
  editButtonIndex = NO_BUTTON_SELECTED;
  editAssignmentDirty = false;
  pendingEditDisplayUpdate = false;
  showReadyScreen();
}

void refreshCurrentScreen() {
  if (editMode) {
    showEditModeScreen();
  } else if (showingButtonOutput) {
    showButtonOutput(lastButtonOutputIndex, false);
  } else if (showingDefaultsRestoreResult) {
    if (startDisplayUpdate()) {
      drawDefaultsRestoreResultScreen(defaultsRestoreSucceeded);
      finishDisplayUpdate();
    }
  } else if (showingPresetChange) {
    showPresetChangeScreen();
  } else {
    showReadyScreen();
  }
}

void serviceMarqueeUpdates(unsigned long now) {
  if (!displayAvailable || now - lastMarqueeUpdateMs < MARQUEE_STEP_MS) return;
  if (showingButtonOutput) return;
  if (showingDefaultsRestoreResult) return;

  lastMarqueeUpdateMs = now;
  if (editMode) {
    showEditModeScreen();
  } else if (menuMode) {
    drawMenuScreen();
  } else if (showingPresetChange) {
    showPresetChangeScreen();
  } else {
    showReadyScreen();
  }
}

int assignmentLibraryCount() {
  int count = 0;
  for (uint8_t p = 0; p < deviceConfig.profileCount; ++p) {
    count += deviceConfig.profiles[p].assignmentCount;
  }
  return count;
}

int assignmentOptionCount() {
  return assignmentLibraryCount() + deviceConfig.macroCount;
}

bool assignmentOptionAt(int option,
                        ButtonAssignmentRef &reference,
                        const DeviceProfileConfig **profile,
                        const AssignmentConfig **assignment) {
  if (profile != nullptr) *profile = nullptr;
  if (assignment != nullptr) *assignment = nullptr;
  reference = {UNASSIGNED_ID, UNASSIGNED_ID};
  if (option <= 0) return option == 0;

  int cursor = 1;
  for (uint8_t p = 0; p < deviceConfig.profileCount; ++p) {
    const DeviceProfileConfig &candidateProfile = deviceConfig.profiles[p];
    for (uint8_t a = 0; a < candidateProfile.assignmentCount; ++a) {
      if (cursor == option) {
        reference = {candidateProfile.id, candidateProfile.assignments[a].id};
        if (profile != nullptr) *profile = &candidateProfile;
        if (assignment != nullptr) *assignment = &candidateProfile.assignments[a];
        return true;
      }
      ++cursor;
    }
  }
  return false;
}

bool macroOptionAt(int option, const MacroConfig **macro) {
  if (macro != nullptr) *macro = nullptr;
  const int macroIndex = option - assignmentLibraryCount() - 1;
  if (macroIndex < 0 || macroIndex >= deviceConfig.macroCount) return false;
  if (macro != nullptr) *macro = &deviceConfig.macros[macroIndex];
  return true;
}

int assignmentOptionForReference(const ButtonAssignmentRef &reference) {
  if (!assignmentRefIsAssigned(reference)) return 0;
  int cursor = 1;
  for (uint8_t p = 0; p < deviceConfig.profileCount; ++p) {
    const DeviceProfileConfig &profile = deviceConfig.profiles[p];
    for (uint8_t a = 0; a < profile.assignmentCount; ++a) {
      if (profile.id == reference.profileId && profile.assignments[a].id == reference.assignmentId) {
        return cursor;
      }
      ++cursor;
    }
  }
  return 0;
}

int assignmentOptionForMacro(uint16_t macroId) {
  if (macroId == UNASSIGNED_ID) return 0;
  const int firstMacroOption = assignmentLibraryCount() + 1;
  for (uint8_t i = 0; i < deviceConfig.macroCount; ++i) {
    if (deviceConfig.macros[i].id == macroId) return firstMacroOption + i;
  }
  return 0;
}

ButtonAssignmentRef assignmentRefForOption(int option) {
  ButtonAssignmentRef reference{UNASSIGNED_ID, UNASSIGNED_ID};
  assignmentOptionAt(option, reference, nullptr, nullptr);
  return reference;
}

void applyAssignmentOptionToButton(byte buttonIndex, int option) {
  const MacroConfig *macro = nullptr;
  if (macroOptionAt(option, &macro) && macro != nullptr) {
    applyMacroToButton(buttonIndex, macro->id);
    return;
  }
  applyAssignmentToButton(buttonIndex, assignmentRefForOption(option));
}

void assignmentLabelForOption(int option, char *output, size_t outputSize) {
  if (option <= 0) {
    strncpy(output, "Unassigned", outputSize);
    output[outputSize - 1] = '\0';
    return;
  }

  const MacroConfig *macro = nullptr;
  if (macroOptionAt(option, &macro) && macro != nullptr) {
    snprintf(output, outputSize, "Macro: %s", macro->name);
    output[outputSize - 1] = '\0';
    return;
  }

  ButtonAssignmentRef reference{};
  const DeviceProfileConfig *profile = nullptr;
  const AssignmentConfig *assignment = nullptr;
  if (!assignmentOptionAt(option, reference, &profile, &assignment) ||
      profile == nullptr || assignment == nullptr) {
    strncpy(output, "Back", outputSize);
    output[outputSize - 1] = '\0';
    return;
  }
  snprintf(output, outputSize, "%s: %s", profile->name, assignment->label);
  output[outputSize - 1] = '\0';
}

void changeEditedAssignment(int delta) {
  if (!editMode || editButtonIndex == NO_BUTTON_SELECTED) {
    return;
  }

  MidiButton &button = buttons[editButtonIndex];
  const int optionCount = assignmentOptionCount() + 1;
  int currentOption = assignmentOptionForReference(button.assignment);
  if (button.macroId != UNASSIGNED_ID) currentOption = assignmentOptionForMacro(button.macroId);

  int newOption = (currentOption + delta) % optionCount;
  if (newOption < 0) newOption += optionCount;

  if (newOption != currentOption) {
    applyAssignmentOptionToButton(editButtonIndex, newOption);
    editAssignmentDirty = true;
  }

  pendingEditDisplayUpdate = false;
  resetMarqueePhase();
  showEditModeScreen();
}

int8_t updateEditButton(EditButton &button, unsigned long now) {
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
      button.holdHandled = false;
    } else {
      button.pressedAtMs = 0;
    }
    return button.stablePressed ? 1 : -1;
  }

  return 0;
}

void changeActivePreset(int delta) {
  if (deviceConfig.presetCount < 2) return;
  int currentIndex = 0;
  for (uint8_t i = 0; i < deviceConfig.presetCount; ++i) {
    if (deviceConfig.presets[i].id == deviceConfig.activePresetId) {
      currentIndex = i;
      break;
    }
  }
  int nextIndex = (currentIndex + delta) % deviceConfig.presetCount;
  if (nextIndex < 0) nextIndex += deviceConfig.presetCount;
  if (setActivePresetId(deviceConfig.presets[nextIndex].id, true)) {
    showPresetChangeScreen();
  }
}

uint8_t readEncoderState() {
  const uint8_t a = digitalRead(encoderControl.pinA) == LOW ? 1 : 0;
  const uint8_t b = digitalRead(encoderControl.pinB) == LOW ? 1 : 0;
  return (a << 1) | b;
}

int8_t updateEncoderRotation() {
  static const int8_t transitionTable[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
  };
  const uint8_t state = readEncoderState();
  if (state == encoderControl.lastState) return 0;
  encoderControl.accumulator += transitionTable[(encoderControl.lastState << 2) | state];
  encoderControl.lastState = state;
  if (encoderControl.accumulator >= 4) {
    encoderControl.accumulator = 0;
    return 1;
  }
  if (encoderControl.accumulator <= -4) {
    encoderControl.accumulator = 0;
    return -1;
  }
  return 0;
}

int8_t updateEncoderSwitch(unsigned long now) {
  const bool readingPressed = digitalRead(encoderControl.switchPin) == LOW;

  if (readingPressed != encoderControl.lastSwitchReadingPressed) {
    encoderControl.lastSwitchReadingPressed = readingPressed;
    encoderControl.switchLastChangeMs = now;
  }

  if ((now - encoderControl.switchLastChangeMs) >= DEBOUNCE_MS &&
      readingPressed != encoderControl.stableSwitchPressed) {
    encoderControl.stableSwitchPressed = readingPressed;
    if (encoderControl.stableSwitchPressed) {
      encoderControl.switchPressedAtMs = now;
      encoderControl.switchHoldHandled = false;
    } else {
      encoderControl.switchPressedAtMs = 0;
    }
    return encoderControl.stableSwitchPressed ? 1 : -1;
  }

  return 0;
}

int8_t updateQtEncoderRotation() {
  if (!qtEncoderControl.available) return 0;
  if (digitalRead(qtEncoderControl.interruptPin) == HIGH) return 0;
  const int32_t delta = qtEncoder.getEncoderDelta();
  if (delta > 0) return 1;
  if (delta < 0) return -1;
  return 0;
}

int8_t updateQtEncoderSwitch(unsigned long now) {
  if (!qtEncoderControl.available) return 0;
  if (digitalRead(qtEncoderControl.interruptPin) == HIGH &&
      !qtEncoderControl.stableSwitchPressed &&
      !qtEncoderControl.lastSwitchReadingPressed) {
    return 0;
  }

  const bool readingPressed = qtEncoder.digitalRead(STEMMA_QT_ENCODER_SWITCH_PIN) == LOW;

  if (readingPressed != qtEncoderControl.lastSwitchReadingPressed) {
    qtEncoderControl.lastSwitchReadingPressed = readingPressed;
    qtEncoderControl.switchLastChangeMs = now;
  }

  if ((now - qtEncoderControl.switchLastChangeMs) >= DEBOUNCE_MS &&
      readingPressed != qtEncoderControl.stableSwitchPressed) {
    qtEncoderControl.stableSwitchPressed = readingPressed;
    if (qtEncoderControl.stableSwitchPressed) {
      qtEncoderControl.switchPressedAtMs = now;
      qtEncoderControl.switchHoldHandled = false;
    } else {
      qtEncoderControl.switchPressedAtMs = 0;
    }
    return qtEncoderControl.stableSwitchPressed ? 1 : -1;
  }

  return 0;
}

bool usingGpioEncoderControls() {
  return deviceConfig.hardware.menuControlMode == MenuControlMode::Encoder;
}

bool usingQtEncoderControls() {
  return deviceConfig.hardware.menuControlMode == MenuControlMode::StemmaQtEncoder;
}

bool usingEncoderControls() {
  return usingGpioEncoderControls() || usingQtEncoderControls();
}

int8_t updateActiveEncoderRotation() {
  if (usingGpioEncoderControls()) return updateEncoderRotation();
  if (usingQtEncoderControls()) return updateQtEncoderRotation();
  return 0;
}

int8_t updateActiveEncoderSwitch(unsigned long now) {
  if (usingGpioEncoderControls()) return updateEncoderSwitch(now);
  if (usingQtEncoderControls()) return updateQtEncoderSwitch(now);
  return 0;
}

bool activeEncoderSwitchPressed() {
  return usingQtEncoderControls() ? qtEncoderControl.stableSwitchPressed : encoderControl.stableSwitchPressed;
}

bool activeEncoderSwitchHoldHandled() {
  return usingQtEncoderControls() ? qtEncoderControl.switchHoldHandled : encoderControl.switchHoldHandled;
}

unsigned long activeEncoderSwitchPressedAt() {
  return usingQtEncoderControls() ? qtEncoderControl.switchPressedAtMs : encoderControl.switchPressedAtMs;
}

void setActiveEncoderSwitchHoldHandled(bool handled) {
  if (usingQtEncoderControls()) {
    qtEncoderControl.switchHoldHandled = handled;
  } else {
    encoderControl.switchHoldHandled = handled;
  }
}

void setAllEncoderSwitchHoldHandled(bool handled) {
  encoderControl.switchHoldHandled = handled;
  qtEncoderControl.switchHoldHandled = handled;
}

void resetStompButtonStates() {
  const unsigned long now = millis();
  for (byte i = 0; i < deviceConfig.hardware.buttonCount; ++i) {
    const bool pressed = digitalRead(buttons[i].pin) == LOW;
    buttons[i].stablePressed = pressed;
    buttons[i].lastReadingPressed = pressed;
    buttons[i].lastChangeMs = now;
    buttons[i].pressedAtMs = pressed ? now : 0;
    buttons[i].editHoldHandled = false;
  }
}

uint8_t activePresetIndex() {
  for (uint8_t i = 0; i < deviceConfig.presetCount; ++i) {
    if (deviceConfig.presets[i].id == deviceConfig.activePresetId) return i;
  }
  return 0;
}

void setActivePresetIndex(uint8_t index) {
  if (index >= deviceConfig.presetCount) return;
  setActivePresetId(deviceConfig.presets[index].id, true);
}

int presetIndexForId(uint16_t presetId) {
  for (uint8_t i = 0; i < deviceConfig.presetCount; ++i) {
    if (deviceConfig.presets[i].id == presetId) return i;
  }
  return -1;
}

bool presetTogglePairAvailable() {
  return deviceConfig.presetCount >= 2;
}

const char *presetNameForId(uint16_t presetId) {
  const PedalPresetConfig *preset = DeviceConfigStore::findPreset(deviceConfig, presetId);
  return preset != nullptr ? preset->name : "Not set";
}

uint16_t firstPresetIdExcept(uint16_t excludedPresetId) {
  for (uint8_t i = 0; i < deviceConfig.presetCount; ++i) {
    if (deviceConfig.presets[i].id != excludedPresetId) return deviceConfig.presets[i].id;
  }
  return UNASSIGNED_ID;
}

bool normalizePresetToggleSelection() {
  if (deviceConfig.presetToggle.switchNumber < 1 ||
      deviceConfig.presetToggle.switchNumber > deviceConfig.hardware.buttonCount) {
    deviceConfig.presetToggle.switchNumber = 1;
  }

  if (!presetTogglePairAvailable()) {
    deviceConfig.presetToggle.enabled = false;
    deviceConfig.presetToggle.primaryPresetId = UNASSIGNED_ID;
    deviceConfig.presetToggle.alternatePresetId = UNASSIGNED_ID;
    return false;
  }

  if (DeviceConfigStore::findPreset(deviceConfig, deviceConfig.presetToggle.primaryPresetId) == nullptr) {
    deviceConfig.presetToggle.primaryPresetId = deviceConfig.presets[0].id;
  }
  if (DeviceConfigStore::findPreset(deviceConfig, deviceConfig.presetToggle.alternatePresetId) == nullptr ||
      deviceConfig.presetToggle.alternatePresetId == deviceConfig.presetToggle.primaryPresetId) {
    deviceConfig.presetToggle.alternatePresetId =
        firstPresetIdExcept(deviceConfig.presetToggle.primaryPresetId);
  }
  if (deviceConfig.presetToggle.alternatePresetId == UNASSIGNED_ID) {
    deviceConfig.presetToggle.enabled = false;
    return false;
  }
  return true;
}

bool savePresetToggleSettings() {
  normalizePresetToggleSelection();
  return configStore.save(deviceConfig);
}

void resetCurrentPresetState() {
  applyActivePreset();
  resetStompButtonStates();
  menuMode = false;
  editMode = false;
  showPresetStateResetScreen();
}

int assignmentOptionForSwitch(byte switchIndex) {
  if (switchIndex >= deviceConfig.hardware.buttonCount) return 0;
  if (buttons[switchIndex].macroId != UNASSIGNED_ID) {
    return assignmentOptionForMacro(buttons[switchIndex].macroId);
  }
  return assignmentOptionForReference(buttons[switchIndex].assignment);
}

int menuItemCount() {
  switch (menuScreen) {
    case MenuScreen::Presets:
      return deviceConfig.presetCount + 2;
    case MenuScreen::PresetToggle:
      return 5;
    case MenuScreen::PresetToggleEnabled:
      return 3;
    case MenuScreen::PresetTogglePrimary:
    case MenuScreen::PresetToggleAlternate:
      return deviceConfig.presetCount + 1;
    case MenuScreen::PresetToggleSwitch:
      return deviceConfig.hardware.buttonCount + 1;
    case MenuScreen::Switches:
      return deviceConfig.hardware.buttonCount + 1;
    case MenuScreen::Assignments:
      return assignmentOptionCount() + 2;
    case MenuScreen::MidiOutput:
      return 4;
    case MenuScreen::HomeDelay:
      return HOME_DELAY_OPTION_COUNT + 1;
    case MenuScreen::HoldEdit:
      return 3;
    case MenuScreen::System:
      return 5;
    case MenuScreen::ConfirmResetState:
    case MenuScreen::ConfirmWebConfig:
    case MenuScreen::ConfirmBootloader:
    case MenuScreen::ConfirmRestoreDefaults:
      return 2;
    case MenuScreen::Main:
    default:
      return 7;
  }
}

void clampMenuIndex() {
  const int count = menuItemCount();
  if (menuIndex < 0) menuIndex = count - 1;
  if (menuIndex >= count) menuIndex = 0;
}

void drawMenuScreen() {
  if (!startDisplayUpdate()) return;

  const uint8_t currentScreen = static_cast<uint8_t>(menuScreen);
  if (lastMenuMarqueeScreen != currentScreen || lastMenuMarqueeIndex != menuIndex) {
    resetMarqueePhase();
    lastMenuMarqueeScreen = currentScreen;
    lastMenuMarqueeIndex = menuIndex;
  }

  char primary[32] = "";
  char secondary[32] = "";
  char tertiary[24] = "";
  char headerBuffer[32] = "MENU";
  const char *header = headerBuffer;

  switch (menuScreen) {
    case MenuScreen::Main:
      switch (menuIndex) {
        case 0:
          strncpy(primary, "Presets", sizeof(primary));
          strncpy(secondary, activePresetConst().name, sizeof(secondary) - 1);
          break;
        case 1:
          strncpy(primary, "Switch Assign", sizeof(primary));
          strncpy(secondary, "Edit Assignments", sizeof(secondary) - 1);
          break;
        case 2:
          strncpy(primary, "MIDI Output", sizeof(primary));
          strncpy(secondary, midiOutputModeLabel(deviceConfig.settings.midiOutputMode), sizeof(secondary) - 1);
          break;
        case 3:
          strncpy(primary, "Home Delay", sizeof(primary));
          snprintf(secondary, sizeof(secondary), "%u seconds", deviceConfig.settings.buttonScreenSeconds);
          break;
        case 4:
          strncpy(primary, "Hold Edit", sizeof(primary));
          strncpy(secondary, deviceConfig.settings.holdToEditEnabled ? "On" : "Off", sizeof(secondary) - 1);
          break;
        case 5:
          strncpy(primary, "System", sizeof(primary));
          strncpy(secondary, "Config & reset", sizeof(secondary) - 1);
          break;
        default:
          strncpy(primary, "Exit", sizeof(primary));
          strncpy(secondary, "Close menu", sizeof(secondary) - 1);
          break;
      }
      break;

    case MenuScreen::Presets:
      header = "PRESETS";
      strncpy(secondary, activePresetConst().name, sizeof(secondary) - 1);
      if (menuIndex < deviceConfig.presetCount) {
        strncpy(primary, deviceConfig.presets[menuIndex].name, sizeof(primary) - 1);
      } else if (menuIndex == deviceConfig.presetCount) {
        strncpy(primary, "Page Toggle", sizeof(primary));
        if (presetTogglePairAvailable()) {
          snprintf(secondary,
                   sizeof(secondary),
                   "B%u / %s",
                   deviceConfig.presetToggle.switchNumber,
                   deviceConfig.presetToggle.enabled ? "On" : "Off");
        } else {
          strncpy(secondary, "Need 2 presets", sizeof(secondary) - 1);
        }
      } else {
        strncpy(primary, "Back", sizeof(primary));
        strncpy(secondary, "Return", sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::PresetToggle:
      header = "PAGE TOGGLE";
      normalizePresetToggleSelection();
      switch (menuIndex) {
        case 0:
          strncpy(primary, "Enable", sizeof(primary));
          strncpy(secondary,
                  presetTogglePairAvailable()
                      ? (deviceConfig.presetToggle.enabled ? "On" : "Off")
                      : "Need 2 presets",
                  sizeof(secondary) - 1);
          break;
        case 1:
          strncpy(primary, "Primary Preset", sizeof(primary));
          strncpy(secondary, presetNameForId(deviceConfig.presetToggle.primaryPresetId), sizeof(secondary) - 1);
          break;
        case 2:
          strncpy(primary, "Alternate Preset", sizeof(primary));
          strncpy(secondary, presetNameForId(deviceConfig.presetToggle.alternatePresetId), sizeof(secondary) - 1);
          break;
        case 3:
          strncpy(primary, "Toggle Switch", sizeof(primary));
          snprintf(secondary, sizeof(secondary), "Switch %u", deviceConfig.presetToggle.switchNumber);
          break;
        default:
          strncpy(primary, "Back", sizeof(primary));
          strncpy(secondary, "Return", sizeof(secondary) - 1);
          break;
      }
      break;

    case MenuScreen::PresetToggleEnabled:
      header = "TOGGLE";
      if (menuIndex >= 2) {
        strncpy(primary, "Back", sizeof(primary));
        strncpy(secondary, "Return", sizeof(secondary) - 1);
      } else {
        const bool optionEnabled = menuIndex == 1;
        strncpy(primary, optionEnabled ? "On" : "Off", sizeof(primary));
        if (optionEnabled && !presetTogglePairAvailable()) {
          strncpy(secondary, "Need 2 presets", sizeof(secondary) - 1);
        } else {
          strncpy(secondary,
                  optionEnabled == deviceConfig.presetToggle.enabled ? "Current" : "Press to save",
                  sizeof(secondary) - 1);
        }
      }
      break;

    case MenuScreen::PresetTogglePrimary:
      header = "PRIMARY";
      if (menuIndex >= deviceConfig.presetCount) {
        strncpy(primary, "Back", sizeof(primary));
        strncpy(secondary, "Return", sizeof(secondary) - 1);
      } else {
        strncpy(primary, deviceConfig.presets[menuIndex].name, sizeof(primary) - 1);
        strncpy(secondary,
                deviceConfig.presets[menuIndex].id == deviceConfig.presetToggle.primaryPresetId
                    ? "Current"
                    : "Press to save",
                sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::PresetToggleAlternate:
      header = "ALTERNATE";
      if (menuIndex >= deviceConfig.presetCount) {
        strncpy(primary, "Back", sizeof(primary));
        strncpy(secondary, "Return", sizeof(secondary) - 1);
      } else {
        strncpy(primary, deviceConfig.presets[menuIndex].name, sizeof(primary) - 1);
        strncpy(secondary,
                deviceConfig.presets[menuIndex].id == deviceConfig.presetToggle.alternatePresetId
                    ? "Current"
                    : "Press to save",
                sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::PresetToggleSwitch:
      header = "SWITCH";
      if (menuIndex >= deviceConfig.hardware.buttonCount) {
        strncpy(primary, "Back", sizeof(primary));
        strncpy(secondary, "Return", sizeof(secondary) - 1);
      } else {
        snprintf(primary, sizeof(primary), "Switch %d", menuIndex + 1);
        strncpy(secondary,
                menuIndex + 1 == deviceConfig.presetToggle.switchNumber ? "Current" : "Press to save",
                sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::Switches: {
      header = "SWITCHES";
      if (menuIndex >= deviceConfig.hardware.buttonCount) {
        strncpy(primary, "Back", sizeof(primary));
      } else {
        snprintf(primary, sizeof(primary), "Switch %d", menuIndex + 1);
        if (presetToggleAppliesToButton(menuIndex)) {
          strncpy(secondary, "Page toggle override", sizeof(secondary) - 1);
          formatPresetToggleTargetLine(menuIndex, tertiary, sizeof(tertiary));
        } else {
          assignmentLabelForOption(assignmentOptionForSwitch(menuIndex), secondary, sizeof(secondary));
        }
      }
      break;
    }

    case MenuScreen::Assignments: {
      header = activePresetConst().name;
      const bool pageToggleOverride = menuSwitchIndex != NO_BUTTON_SELECTED &&
                                      presetToggleAppliesToButton(menuSwitchIndex);
      if (menuIndex > assignmentOptionCount()) {
        strncpy(primary, "Back", sizeof(primary));
        secondary[0] = '\0';
      } else if (menuIndex <= 0) {
        strncpy(primary, "Unassigned", sizeof(primary));
        if (pageToggleOverride) {
          strncpy(secondary, "Page toggle override", sizeof(secondary) - 1);
          formatPresetToggleTargetLine(menuSwitchIndex, tertiary, sizeof(tertiary));
        } else {
          snprintf(secondary, sizeof(secondary), "Switch %u", menuSwitchIndex + 1);
        }
      } else {
        const MacroConfig *macro = nullptr;
        if (macroOptionAt(menuIndex, &macro) && macro != nullptr) {
          strncpy(primary, macro->name, sizeof(primary) - 1);
          if (pageToggleOverride) {
            strncpy(secondary, "Page toggle override", sizeof(secondary) - 1);
            formatPresetToggleTargetLine(menuSwitchIndex, tertiary, sizeof(tertiary));
          } else {
            snprintf(secondary, sizeof(secondary), "Macro / B%u", menuSwitchIndex + 1);
          }
          break;
        }
        ButtonAssignmentRef reference{};
        const DeviceProfileConfig *profile = nullptr;
        const AssignmentConfig *assignment = nullptr;
        if (assignmentOptionAt(menuIndex, reference, &profile, &assignment) &&
            profile != nullptr && assignment != nullptr) {
          strncpy(primary, assignment->label, sizeof(primary) - 1);
          if (pageToggleOverride) {
            strncpy(secondary, "Page toggle override", sizeof(secondary) - 1);
            formatPresetToggleTargetLine(menuSwitchIndex, tertiary, sizeof(tertiary));
          } else {
            snprintf(secondary, sizeof(secondary), "Switch %u", menuSwitchIndex + 1);
          }
        } else {
          strncpy(primary, "Back", sizeof(primary));
          secondary[0] = '\0';
        }
      }
      break;
    }

    case MenuScreen::MidiOutput:
      header = "OUTPUT";
      if (menuIndex >= 3) {
        strncpy(primary, "Back", sizeof(primary));
      } else {
        const MidiOutputMode optionMode = midiOutputModeForOption(menuIndex);
        strncpy(primary, midiOutputModeLabel(optionMode), sizeof(primary) - 1);
        strncpy(secondary,
                optionMode == deviceConfig.settings.midiOutputMode ? "Current" : "Press to save",
                sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::HomeDelay:
      header = "HOME DELAY";
      if (menuIndex >= HOME_DELAY_OPTION_COUNT) {
        strncpy(primary, "Back", sizeof(primary));
      } else {
        const uint8_t seconds = HOME_DELAY_OPTIONS[menuIndex];
        snprintf(primary, sizeof(primary), "%u seconds", seconds);
        strncpy(secondary,
                seconds == deviceConfig.settings.buttonScreenSeconds ? "Current" : "Press to save",
                sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::HoldEdit:
      header = "HOLD EDIT";
      if (menuIndex >= 2) {
        strncpy(primary, "Back", sizeof(primary));
        strncpy(secondary, "Return", sizeof(secondary));
      } else {
        const bool optionEnabled = menuIndex == 1;
        strncpy(primary, optionEnabled ? "On" : "Off", sizeof(primary));
        strncpy(secondary,
                optionEnabled == deviceConfig.settings.holdToEditEnabled ? "Current" : "Press to save",
                sizeof(secondary) - 1);
      }
      break;

    case MenuScreen::System:
      header = "SYSTEM";
      switch (menuIndex) {
        case 0:
          strncpy(primary, "Web Config", sizeof(primary));
          strncpy(secondary, "USB network", sizeof(secondary) - 1);
          strncpy(tertiary, "192.168.7.1", sizeof(tertiary) - 1);
          break;
        case 1:
          strncpy(primary, "Reset State", sizeof(primary));
          strncpy(secondary, activePresetConst().name, sizeof(secondary) - 1);
          break;
        case 2:
          strncpy(primary, "Firmware Update", sizeof(primary));
          strncpy(secondary, "RPI-RP2 drive", sizeof(secondary) - 1);
          break;
        case 3:
          strncpy(primary, "Restore to default settings", sizeof(primary));
          strncpy(secondary, "Factory defaults", sizeof(secondary) - 1);
          break;
        default:
          strncpy(primary, "Back", sizeof(primary));
          strncpy(secondary, "Return", sizeof(secondary));
          break;
      }
      break;

    case MenuScreen::ConfirmResetState:
      header = "RESET";
      strncpy(primary, menuIndex == 0 ? "Reset state" : "Back", sizeof(primary));
      strncpy(secondary, menuIndex == 0 ? "No MIDI sent" : "Cancel", sizeof(secondary));
      break;

    case MenuScreen::ConfirmWebConfig:
      header = "WEB CONFIG";
      strncpy(primary, menuIndex == 0 ? "Start web config" : "Back", sizeof(primary));
      strncpy(secondary, "Press to choose", sizeof(secondary));
      break;

    case MenuScreen::ConfirmBootloader:
      header = "UPDATE";
      strncpy(primary, menuIndex == 0 ? "Firmware update" : "Back", sizeof(primary));
      strncpy(secondary, "Press to choose", sizeof(secondary));
      break;

    case MenuScreen::ConfirmRestoreDefaults:
      header = "ARE YOU SURE?";
      strncpy(primary, menuIndex == 0 ? "Restore to default settings" : "Back", sizeof(primary));
      strncpy(secondary, menuIndex == 0 ? "Erase saved config" : "Cancel", sizeof(secondary));
      break;
  }

  display.clearBuffer();
  drawHeader(header);
  display.setFont(u8g2_font_7x14B_tf);
  drawCenteredScrollingText(36, primary);
  display.setFont(u8g2_font_6x10_tf);
  if (secondary[0] != '\0') {
    drawCenteredClippedText(tertiary[0] != '\0' ? 51 : 55, secondary);
  }
  if (tertiary[0] != '\0') {
    display.setFont(u8g2_font_5x8_tf);
    drawCenteredClippedText(63, tertiary);
  }
  drawMenuPositionIndicator(menuIndex, menuItemCount());
  finishDisplayUpdate();
}

void enterMenu() {
  editMode = false;
  menuMode = true;
  menuScreen = MenuScreen::Main;
  menuIndex = 0;
  lastMenuInputMs = millis();
  editPreviousButton.holdHandled = true;
  editNextButton.holdHandled = true;
  setAllEncoderSwitchHoldHandled(true);
  showingButtonOutput = false;
  showingPresetChange = false;
  showingDefaultsRestoreResult = false;
  resetMenuMarqueeTracking();
  drawMenuScreen();
}

void exitMenu() {
  menuMode = false;
  resetStompButtonStates();
  showReadyScreen();
}

int mainMenuIndexForScreen(MenuScreen screen) {
  switch (screen) {
    case MenuScreen::Presets:
    case MenuScreen::PresetToggle:
    case MenuScreen::PresetToggleEnabled:
    case MenuScreen::PresetTogglePrimary:
    case MenuScreen::PresetToggleAlternate:
    case MenuScreen::PresetToggleSwitch:
      return 0;
    case MenuScreen::Switches:
    case MenuScreen::Assignments:
      return 1;
    case MenuScreen::MidiOutput:
      return 2;
    case MenuScreen::HomeDelay:
      return 3;
    case MenuScreen::ConfirmResetState:
    case MenuScreen::HoldEdit:
      return 4;
    case MenuScreen::System:
    case MenuScreen::ConfirmWebConfig:
    case MenuScreen::ConfirmBootloader:
    case MenuScreen::ConfirmRestoreDefaults:
      return 5;
    case MenuScreen::Main:
    default:
      return 0;
  }
}

int presetToggleMenuIndexForScreen(MenuScreen screen) {
  switch (screen) {
    case MenuScreen::PresetToggleEnabled:
      return 0;
    case MenuScreen::PresetTogglePrimary:
      return 1;
    case MenuScreen::PresetToggleAlternate:
      return 2;
    case MenuScreen::PresetToggleSwitch:
      return 3;
    case MenuScreen::PresetToggle:
    default:
      return 0;
  }
}

int systemMenuIndexForScreen(MenuScreen screen) {
  switch (screen) {
    case MenuScreen::ConfirmWebConfig:
      return 0;
    case MenuScreen::ConfirmResetState:
      return 1;
    case MenuScreen::ConfirmBootloader:
      return 2;
    case MenuScreen::ConfirmRestoreDefaults:
      return 3;
    case MenuScreen::System:
    default:
      return 0;
  }
}

void menuBack() {
  if (menuScreen == MenuScreen::Main) {
    exitMenu();
    return;
  }
  if (menuScreen == MenuScreen::Assignments) {
    menuScreen = MenuScreen::Switches;
    menuIndex = menuSwitchIndex;
  } else if (menuScreen == MenuScreen::PresetToggle) {
    menuScreen = MenuScreen::Presets;
    menuIndex = deviceConfig.presetCount;
  } else if (menuScreen == MenuScreen::PresetToggleEnabled ||
             menuScreen == MenuScreen::PresetTogglePrimary ||
             menuScreen == MenuScreen::PresetToggleAlternate ||
             menuScreen == MenuScreen::PresetToggleSwitch) {
    const int parentIndex = presetToggleMenuIndexForScreen(menuScreen);
    menuScreen = MenuScreen::PresetToggle;
    menuIndex = parentIndex;
  } else if (menuScreen == MenuScreen::System) {
    menuScreen = MenuScreen::Main;
    menuIndex = mainMenuIndexForScreen(MenuScreen::System);
  } else if (menuScreen == MenuScreen::ConfirmResetState ||
             menuScreen == MenuScreen::ConfirmWebConfig ||
             menuScreen == MenuScreen::ConfirmBootloader ||
             menuScreen == MenuScreen::ConfirmRestoreDefaults) {
    const int parentIndex = systemMenuIndexForScreen(menuScreen);
    menuScreen = MenuScreen::System;
    menuIndex = parentIndex;
  } else {
    const int parentIndex = mainMenuIndexForScreen(menuScreen);
    menuScreen = MenuScreen::Main;
    menuIndex = parentIndex;
  }
  drawMenuScreen();
}

void startWebConfigReboot() {
  watchdog_hw->scratch[0] = WEB_CONFIG_BOOT_MAGIC;
  if (displayAvailable && startDisplayUpdate()) {
    display.clearBuffer();
    drawHeader("WEB CONFIG");
    display.setFont(u8g2_font_7x14B_tf);
    drawCenteredText(39, "Restarting");
    finishDisplayUpdate();
  }
  delay(150);
  rp2040.reboot();
}

void startBootloaderMode() {
  if (displayAvailable && startDisplayUpdate()) {
    display.clearBuffer();
    drawHeader("UPDATE");
    display.setFont(u8g2_font_7x14B_tf);
    drawCenteredText(39, "Bootloader");
    finishDisplayUpdate();
  }
  delay(150);
  reset_usb_boot(0, 0);
}

void restoreDefaultSettings() {
  configStore.loadDefaults(deviceConfig);
  const bool saved = configStore.save(deviceConfig);
  applyActivePreset();
  resetStompButtonStates();
  menuMode = false;
  editMode = false;
  if (saved) {
    showDefaultsRestoredScreen();
  } else {
    showDefaultsRestoreFailedScreen();
  }
}

void selectMenuItem() {
  lastMenuInputMs = millis();
  switch (menuScreen) {
    case MenuScreen::Main:
      switch (menuIndex) {
        case 0:
          menuScreen = MenuScreen::Presets;
          menuIndex = activePresetIndex();
          break;
        case 1:
          menuScreen = MenuScreen::Switches;
          menuIndex = 0;
          break;
        case 2:
          menuScreen = MenuScreen::MidiOutput;
          menuIndex = optionForMidiOutputMode(deviceConfig.settings.midiOutputMode);
          break;
        case 3:
          menuScreen = MenuScreen::HomeDelay;
          menuIndex = optionForHomeDelay(deviceConfig.settings.buttonScreenSeconds);
          break;
        case 4:
          menuScreen = MenuScreen::HoldEdit;
          menuIndex = deviceConfig.settings.holdToEditEnabled ? 1 : 0;
          break;
        case 5:
          menuScreen = MenuScreen::System;
          menuIndex = 0;
          break;
        default:
          exitMenu();
          return;
      }
      break;

    case MenuScreen::Presets:
      if (menuIndex < deviceConfig.presetCount) {
        setActivePresetIndex(menuIndex);
      } else if (menuIndex == deviceConfig.presetCount) {
        normalizePresetToggleSelection();
        menuScreen = MenuScreen::PresetToggle;
        menuIndex = 0;
      } else {
        menuBack();
        return;
      }
      break;

    case MenuScreen::PresetToggle:
      switch (menuIndex) {
        case 0:
          menuScreen = MenuScreen::PresetToggleEnabled;
          menuIndex = deviceConfig.presetToggle.enabled ? 1 : 0;
          break;
        case 1: {
          normalizePresetToggleSelection();
          const int primaryIndex = presetIndexForId(deviceConfig.presetToggle.primaryPresetId);
          menuScreen = MenuScreen::PresetTogglePrimary;
          menuIndex = primaryIndex >= 0 ? primaryIndex : 0;
          break;
        }
        case 2: {
          normalizePresetToggleSelection();
          const int alternateIndex = presetIndexForId(deviceConfig.presetToggle.alternatePresetId);
          menuScreen = MenuScreen::PresetToggleAlternate;
          menuIndex = alternateIndex >= 0 ? alternateIndex : 0;
          break;
        }
        case 3:
          menuScreen = MenuScreen::PresetToggleSwitch;
          menuIndex = deviceConfig.presetToggle.switchNumber > 0
                          ? deviceConfig.presetToggle.switchNumber - 1
                          : 0;
          break;
        default:
          menuBack();
          return;
      }
      break;

    case MenuScreen::PresetToggleEnabled:
      if (menuIndex >= 2) {
        menuBack();
        return;
      }
      normalizePresetToggleSelection();
      if (menuIndex == 1 && !presetTogglePairAvailable()) {
        deviceConfig.presetToggle.enabled = false;
      } else {
        deviceConfig.presetToggle.enabled = menuIndex == 1;
      }
      savePresetToggleSettings();
      break;

    case MenuScreen::PresetTogglePrimary:
      if (menuIndex >= deviceConfig.presetCount) {
        menuBack();
        return;
      }
      deviceConfig.presetToggle.primaryPresetId = deviceConfig.presets[menuIndex].id;
      if (deviceConfig.presetToggle.alternatePresetId == deviceConfig.presetToggle.primaryPresetId) {
        deviceConfig.presetToggle.alternatePresetId =
            firstPresetIdExcept(deviceConfig.presetToggle.primaryPresetId);
      }
      savePresetToggleSettings();
      menuScreen = MenuScreen::PresetToggle;
      menuIndex = presetToggleMenuIndexForScreen(MenuScreen::PresetTogglePrimary);
      break;

    case MenuScreen::PresetToggleAlternate:
      if (menuIndex >= deviceConfig.presetCount) {
        menuBack();
        return;
      }
      deviceConfig.presetToggle.alternatePresetId = deviceConfig.presets[menuIndex].id;
      if (deviceConfig.presetToggle.primaryPresetId == deviceConfig.presetToggle.alternatePresetId) {
        deviceConfig.presetToggle.primaryPresetId =
            firstPresetIdExcept(deviceConfig.presetToggle.alternatePresetId);
      }
      savePresetToggleSettings();
      menuScreen = MenuScreen::PresetToggle;
      menuIndex = presetToggleMenuIndexForScreen(MenuScreen::PresetToggleAlternate);
      break;

    case MenuScreen::PresetToggleSwitch:
      if (menuIndex >= deviceConfig.hardware.buttonCount) {
        menuBack();
        return;
      }
      deviceConfig.presetToggle.switchNumber = menuIndex + 1;
      savePresetToggleSettings();
      menuScreen = MenuScreen::PresetToggle;
      menuIndex = presetToggleMenuIndexForScreen(MenuScreen::PresetToggleSwitch);
      break;

    case MenuScreen::Switches:
      if (menuIndex >= deviceConfig.hardware.buttonCount) {
        menuBack();
        return;
      }
      menuSwitchIndex = menuIndex;
      menuScreen = MenuScreen::Assignments;
      menuIndex = assignmentOptionForSwitch(menuSwitchIndex);
      break;

    case MenuScreen::Assignments:
      if (menuIndex > assignmentOptionCount()) {
        menuBack();
        return;
      }
      applyAssignmentOptionToButton(menuSwitchIndex, menuIndex);
      saveButtonAssignment(menuSwitchIndex);
      menuScreen = MenuScreen::Switches;
      menuIndex = menuSwitchIndex;
      break;

    case MenuScreen::MidiOutput:
      if (menuIndex >= 3) {
        menuBack();
        return;
      }
      deviceConfig.settings.midiOutputMode = midiOutputModeForOption(menuIndex);
      configStore.save(deviceConfig);
      break;

    case MenuScreen::HomeDelay:
      if (menuIndex >= HOME_DELAY_OPTION_COUNT) {
        menuBack();
        return;
      }
      deviceConfig.settings.buttonScreenSeconds = HOME_DELAY_OPTIONS[menuIndex];
      configStore.save(deviceConfig);
      break;

    case MenuScreen::HoldEdit:
      if (menuIndex >= 2) {
        menuBack();
        return;
      }
      deviceConfig.settings.holdToEditEnabled = menuIndex == 1;
      configStore.save(deviceConfig);
      break;

    case MenuScreen::System:
      switch (menuIndex) {
        case 0:
          menuScreen = MenuScreen::ConfirmWebConfig;
          menuIndex = 0;
          break;
        case 1:
          menuScreen = MenuScreen::ConfirmResetState;
          menuIndex = 0;
          break;
        case 2:
          menuScreen = MenuScreen::ConfirmBootloader;
          menuIndex = 0;
          break;
        case 3:
          menuScreen = MenuScreen::ConfirmRestoreDefaults;
          menuIndex = 1;
          break;
        default:
          menuBack();
          return;
      }
      break;

    case MenuScreen::ConfirmResetState:
      if (menuIndex == 0) {
        resetCurrentPresetState();
        return;
      }
      menuBack();
      return;

    case MenuScreen::ConfirmWebConfig:
      if (menuIndex == 0) startWebConfigReboot();
      menuBack();
      return;

    case MenuScreen::ConfirmBootloader:
      if (menuIndex == 0) startBootloaderMode();
      menuBack();
      return;

    case MenuScreen::ConfirmRestoreDefaults:
      if (menuIndex == 0) {
        restoreDefaultSettings();
        return;
      }
      menuBack();
      return;
  }
  clampMenuIndex();
  drawMenuScreen();
}

void moveMenuSelection(int delta) {
  menuIndex += delta;
  clampMenuIndex();
  lastMenuInputMs = millis();
  drawMenuScreen();
}

void serviceMenuControls(int8_t previousEvent,
                         int8_t nextEvent,
                         int8_t encoderRotation,
                         int8_t encoderSwitchEvent,
                         unsigned long now) {
  if (now - lastMenuInputMs >= MENU_TIMEOUT_MS) {
    exitMenu();
    return;
  }

  if (usingEncoderControls()) {
    if (encoderRotation != 0) moveMenuSelection(encoderRotation);
    if (activeEncoderSwitchPressed() &&
        !activeEncoderSwitchHoldHandled() &&
        activeEncoderSwitchPressedAt() > 0 &&
        now - activeEncoderSwitchPressedAt() >= MENU_HOLD_MS) {
      setActiveEncoderSwitchHoldHandled(true);
      lastMenuInputMs = now;
      menuBack();
      return;
    }
    if (encoderSwitchEvent < 0 && !activeEncoderSwitchHoldHandled()) {
      selectMenuItem();
    }
    return;
  }

  if (editPreviousButton.stablePressed &&
      !editPreviousButton.holdHandled &&
      editPreviousButton.pressedAtMs > 0 &&
      now - editPreviousButton.pressedAtMs >= MENU_HOLD_MS) {
    editPreviousButton.holdHandled = true;
    lastMenuInputMs = now;
    menuBack();
    return;
  }

  if (editNextButton.stablePressed &&
      !editNextButton.holdHandled &&
      editNextButton.pressedAtMs > 0 &&
      now - editNextButton.pressedAtMs >= MENU_HOLD_MS) {
    editNextButton.holdHandled = true;
    lastMenuInputMs = now;
    selectMenuItem();
    return;
  }

  if (previousEvent < 0 && !editPreviousButton.holdHandled) {
    moveMenuSelection(-1);
  }
  if (nextEvent < 0 && !editNextButton.holdHandled) {
    moveMenuSelection(1);
  }
}

void servicePerformanceControls(unsigned long now) {
  if (usingEncoderControls()) {
    if (activeEncoderSwitchPressed() &&
        !activeEncoderSwitchHoldHandled() &&
        activeEncoderSwitchPressedAt() > 0 &&
        now - activeEncoderSwitchPressedAt() >= MENU_HOLD_MS) {
      setActiveEncoderSwitchHoldHandled(true);
      enterMenu();
    }
    return;
  }

  if (editNextButton.stablePressed &&
      !editNextButton.holdHandled &&
      editNextButton.pressedAtMs > 0 &&
      now - editNextButton.pressedAtMs >= MENU_HOLD_MS) {
    editNextButton.holdHandled = true;
    enterMenu();
    return;
  }
}

void setupUSBMIDI() {
  USB.setManufacturer("KB2040 Stompbox");
  USB.setProduct("KB2040 MIDI Ctrl");
  MidiUSB.setName("KB2040 MIDI Ctrl");
  MidiUSB.begin();
}

void setupDINMIDI() {
  Serial1.setTX(DIN_MIDI_TX_PIN);
  DIN_MIDI.begin(MIDI_CHANNEL_OMNI);
}

void setupDisplay() {
  delay(50);
  Wire1.begin();
  Wire1.setClock(I2C_CLOCK_HZ);
  configureI2CTimeout();
  display.setBusClock(I2C_CLOCK_HZ);
  display.begin();
  display.setPowerSave(0);
  display.setFont(u8g2_font_6x10_tf);
  displayAvailable = !getI2CTimeoutFlag();

  if (getI2CTimeoutFlag()) {
    displayAvailable = false;
    clearI2CTimeoutFlag();
  }
}

void setupQtEncoder() {
  qtEncoderControl.available = false;
  if (!usingQtEncoderControls()) return;

  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);
  if (!qtEncoder.begin(STEMMA_QT_ENCODER_ADDRESS)) return;

  qtEncoder.pinMode(STEMMA_QT_ENCODER_SWITCH_PIN, INPUT_PULLUP);
  qtEncoder.setGPIOInterrupts((uint32_t)1 << STEMMA_QT_ENCODER_SWITCH_PIN, 1);
  qtEncoder.enableEncoderInterrupt();
  qtEncoder.setEncoderPosition(0);
  qtEncoderControl.available = true;
}

bool qtEncoderBootShortcutPressed() {
  return qtEncoderControl.available &&
         qtEncoder.digitalRead(STEMMA_QT_ENCODER_SWITCH_PIN) == LOW;
}

void setup() {
  loadDeviceConfiguration();
  pinMode(LED_BUILTIN, OUTPUT);
  analogReadResolution(12);

  for (byte i = 0; i < deviceConfig.hardware.buttonCount; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }

  pinMode(editPreviousButton.pin, INPUT_PULLUP);
  pinMode(editNextButton.pin, INPUT_PULLUP);
  pinMode(encoderControl.pinA, INPUT_PULLUP);
  pinMode(encoderControl.pinB, INPUT_PULLUP);
  pinMode(encoderControl.switchPin, INPUT_PULLUP);
  pinMode(qtEncoderControl.interruptPin, INPUT_PULLUP);
  pinMode(WEB_CONFIG_RECOVERY_PIN, INPUT_PULLUP);

  delay(30);
  encoderControl.lastState = readEncoderState();
  setupQtEncoder();
  webConfigMode = watchdog_hw->scratch[0] == WEB_CONFIG_BOOT_MAGIC ||
                  digitalRead(WEB_CONFIG_RECOVERY_PIN) == LOW ||
                  qtEncoderBootShortcutPressed() ||
                  (digitalRead(editPreviousButton.pin) == LOW &&
                   digitalRead(editNextButton.pin) == LOW);
  watchdog_hw->scratch[0] = 0;

  setupDisplay();
  if (webConfigMode) {
    digitalWrite(LED_BUILTIN, HIGH);
    const bool webDisplayDetected = displayAvailable;
    if (webDisplayDetected) {
      showWebConfigStartingScreen();
    }
    webConfigServer.begin();
    if (webDisplayDetected) {
      display.setPowerSave(1);
      displayAvailable = true;
      clearI2CTimeoutFlag();
    }
    showWebConfigScreen();
    if (webDisplayDetected) {
      displayAvailable = true;
      clearI2CTimeoutFlag();
      delay(5);
      showWebConfigScreen();
      displayAvailable = true;
      clearI2CTimeoutFlag();
      display.setPowerSave(0);
      if (getI2CTimeoutFlag()) {
        displayAvailable = false;
        clearI2CTimeoutFlag();
      }
    }
    return;
  }

  setupUSBMIDI();
  setupDINMIDI();

  if (displayAvailable) {
    showReadyScreen();
  }
}

void loop() {
  if (webConfigMode) {
    webConfigServer.process();
    return;
  }

  const unsigned long now = millis();
  bool anyButtonPressed = false;
  const int8_t previousEditEvent = updateEditButton(editPreviousButton, now);
  const int8_t nextEditEvent = updateEditButton(editNextButton, now);
  const int8_t encoderRotation = updateActiveEncoderRotation();
  const int8_t encoderSwitchEvent = updateActiveEncoderSwitch(now);

  serviceMacroPlayback(now);

  if (menuMode) {
    serviceMenuControls(previousEditEvent, nextEditEvent, encoderRotation, encoderSwitchEvent, now);
    serviceMarqueeUpdates(now);
    digitalWrite(LED_BUILTIN, LOW);
    return;
  }

  if (!editMode &&
      showingButtonOutput &&
      now - buttonOutputShownAtMs >= static_cast<unsigned long>(deviceConfig.settings.buttonScreenSeconds) * 1000UL) {
    showReadyScreen();
  }

  if (!editMode &&
      showingPresetChange &&
      now - presetShownAtMs >= PRESET_SCREEN_MS) {
    showReadyScreen();
  }

  if (!editMode &&
      showingDefaultsRestoreResult &&
      now - defaultsRestoreShownAtMs >= PRESET_SCREEN_MS) {
    showReadyScreen();
  }

  serviceMarqueeUpdates(now);

  if (editMode) {
    if (usingEncoderControls()) {
      if (encoderRotation != 0) {
        changeEditedAssignment(encoderRotation);
      }

      if (encoderSwitchEvent < 0 && !activeEncoderSwitchHoldHandled()) {
        exitEditMode();
      }
    } else {
      if (previousEditEvent > 0) {
        changeEditedAssignment(-1);
      }

      if (nextEditEvent > 0) {
        changeEditedAssignment(1);
      }
    }

    serviceDisplayUpdates(now);
  } else {
    servicePerformanceControls(now);
    if (menuMode) {
      digitalWrite(LED_BUILTIN, LOW);
      return;
    }
  }

  for (byte i = 0; i < deviceConfig.hardware.buttonCount; i++) {
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
        button.presetToggleHandled = false;

        if (editMode) {
          if (i == editButtonIndex) {
            exitEditMode();
          }
        } else {
          button.presetToggleHandled = handlePresetTogglePress(i);
          if (!button.presetToggleHandled) {
            sendButtonPress(i);
            showButtonOutput(i);
          }
        }
      } else {
        if (!editMode && !button.presetToggleHandled) {
          sendButtonRelease(i);
        }
        button.pressedAtMs = 0;
        button.editHoldHandled = false;
        button.presetToggleHandled = false;
      }
    }

    if (!editMode &&
        deviceConfig.settings.holdToEditEnabled &&
        button.stablePressed &&
        !button.editHoldHandled &&
        !button.presetToggleHandled &&
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
