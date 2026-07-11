#include "DeviceConfig.h"

#include <EEPROM.h>
#include <LittleFS.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t CONFIG_MAGIC = 0x4B425043;  // KBPC
constexpr uint16_t CONFIG_VERSION = 13;
constexpr uint16_t MACROS_PREVIOUS_VERSION = 12;
constexpr uint16_t CORRECTED_BUTTON_DEFAULTS_PREVIOUS_VERSION = 11;
constexpr uint16_t CORRECTED_BUTTON_DEFAULTS_INITIAL_VERSION = 10;
constexpr uint16_t STEMMA_QT_ENCODER_PREVIOUS_VERSION = 9;
constexpr uint16_t PRESET_TOGGLE_PREVIOUS_VERSION = 8;
constexpr uint16_t PC_DISPLAY_PREVIOUS_VERSION = 7;
constexpr uint16_t MESSAGE_TYPE_PREVIOUS_VERSION = 6;
constexpr uint16_t DEVICE_PROFILES_PREVIOUS_VERSION = 5;
constexpr uint16_t PREVIOUS_CONFIG_VERSION = 4;
constexpr uint16_t HARDWARELESS_CONFIG_VERSION = 3;
constexpr uint16_t LEGACY_PRESET_CONFIG_VERSION = 2;
constexpr uint32_t LEGACY_CONFIG_MAGIC = 0x4B423543;  // KB5C
constexpr uint16_t LEGACY_CONFIG_VERSION = 1;
constexpr uint8_t LEGACY_SIGNATURE = 0x4E;
constexpr uint8_t LEGACY_OLD_SIGNATURE = 0x4D;
constexpr uint8_t LEGACY_PRESET_OFFSET = 2;
constexpr uint8_t LEGACY_UNASSIGNED = 255;
constexpr char CONFIG_PATH[] = "/pedal-config.bin";
constexpr char TEMP_CONFIG_PATH[] = "/pedal-config.tmp";
constexpr uint8_t DEFAULT_QT_ENCODER_INTERRUPT_PIN = 10;
constexpr uint8_t DEFAULT_ENCODER_A_PIN = A1;
constexpr uint8_t DEFAULT_ENCODER_B_PIN = A2;
constexpr uint8_t DEFAULT_ENCODER_SWITCH_PIN = A3;

#pragma pack(push, 1)
struct LegacyAssignmentConfig {
  uint16_t id;
  AssignmentType type;
  uint8_t controlChange;
  uint8_t valueOn;
  uint8_t valueOff;
  char label[ASSIGNMENT_LABEL_LENGTH];
};

struct StoredAssignmentConfig {
  uint16_t id;
  AssignmentType type;
  uint8_t midiChannel;
  uint8_t controlChange;
  uint8_t valueOn;
  uint8_t valueOff;
  char label[ASSIGNMENT_LABEL_LENGTH];
};

struct StoredV6AssignmentConfig {
  uint16_t id;
  AssignmentType type;
  uint8_t controlChange;
  uint8_t valueOn;
  uint8_t valueOff;
  char label[ASSIGNMENT_LABEL_LENGTH];
};

struct LegacyDeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  uint8_t midiChannel;
  uint8_t assignmentCount;
  LegacyAssignmentConfig assignments[MAX_ASSIGNMENTS];
  uint16_t buttonAssignmentIds[LEGACY_BUTTON_COUNT];
  ExpressionConfig expression;
};

struct StoredPedalPresetConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  uint8_t midiChannel;
  uint8_t assignmentCount;
  StoredAssignmentConfig assignments[MAX_ASSIGNMENTS];
  uint16_t buttonAssignmentIds[MAX_BUTTON_COUNT];
  ExpressionConfig expression;
};

struct StoredNarrowPedalPresetConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  uint8_t midiChannel;
  uint8_t assignmentCount;
  StoredAssignmentConfig assignments[MAX_ASSIGNMENTS];
  uint16_t buttonAssignmentIds[LEGACY_BUTTON_COUNT];
  ExpressionConfig expression;
};

struct StoredHardwareConfig {
  uint8_t buttonCount;
  uint8_t buttonPins[MAX_BUTTON_COUNT];
  MenuControlMode menuControlMode;
  uint8_t previousButtonPin;
  uint8_t nextButtonPin;
  uint8_t encoderAPin;
  uint8_t encoderBPin;
  uint8_t encoderSwitchPin;
};

struct StoredButtonPresetConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  ButtonAssignmentRef buttonAssignments[MAX_BUTTON_COUNT];
};

struct StoredV12DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  HardwareConfig hardware;
  DeviceSettings settings;
  PresetToggleConfig presetToggle;
  uint16_t activePresetId;
  uint8_t profileCount;
  DeviceProfileConfig profiles[MAX_DEVICE_PROFILES];
  uint8_t presetCount;
  StoredButtonPresetConfig presets[MAX_PRESETS];
};

struct StoredV9DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  StoredHardwareConfig hardware;
  DeviceSettings settings;
  PresetToggleConfig presetToggle;
  uint16_t activePresetId;
  uint8_t profileCount;
  DeviceProfileConfig profiles[MAX_DEVICE_PROFILES];
  uint8_t presetCount;
  StoredButtonPresetConfig presets[MAX_PRESETS];
};

struct StoredV5DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  StoredHardwareConfig hardware;
  DeviceSettings settings;
  uint16_t activePresetId;
  uint8_t presetCount;
  StoredPedalPresetConfig presets[MAX_PRESETS];
};

struct StoredV6DeviceProfileConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  uint8_t midiChannel;
  uint8_t assignmentCount;
  StoredV6AssignmentConfig assignments[MAX_ASSIGNMENTS];
};

struct StoredV6PedalPresetConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  ButtonAssignmentRef buttonAssignments[MAX_BUTTON_COUNT];
};

struct StoredV6DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  StoredHardwareConfig hardware;
  DeviceSettings settings;
  uint16_t activePresetId;
  uint8_t profileCount;
  StoredV6DeviceProfileConfig profiles[MAX_DEVICE_PROFILES];
  uint8_t presetCount;
  StoredV6PedalPresetConfig presets[MAX_PRESETS];
};

struct StoredV7DeviceProfileConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  uint8_t midiChannel;
  uint8_t assignmentCount;
  AssignmentConfig assignments[MAX_ASSIGNMENTS];
};

struct StoredV7DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  StoredHardwareConfig hardware;
  DeviceSettings settings;
  uint16_t activePresetId;
  uint8_t profileCount;
  StoredV7DeviceProfileConfig profiles[MAX_DEVICE_PROFILES];
  uint8_t presetCount;
  StoredButtonPresetConfig presets[MAX_PRESETS];
};

struct StoredV8DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  StoredHardwareConfig hardware;
  DeviceSettings settings;
  uint16_t activePresetId;
  uint8_t profileCount;
  DeviceProfileConfig profiles[MAX_DEVICE_PROFILES];
  uint8_t presetCount;
  StoredButtonPresetConfig presets[MAX_PRESETS];
};

struct StoredV4DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  StoredHardwareConfig hardware;
  uint16_t activePresetId;
  uint8_t presetCount;
  StoredPedalPresetConfig presets[MAX_PRESETS];
};

struct StoredV3DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  uint16_t activePresetId;
  uint8_t presetCount;
  StoredNarrowPedalPresetConfig presets[MAX_PRESETS];
};

struct StoredV2PedalPresetConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  uint8_t midiChannel;
  uint8_t assignmentCount;
  LegacyAssignmentConfig assignments[MAX_ASSIGNMENTS];
  uint16_t buttonAssignmentIds[LEGACY_BUTTON_COUNT];
  ExpressionConfig expression;
};

struct StoredV2DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  uint16_t activePresetId;
  uint8_t presetCount;
  StoredV2PedalPresetConfig presets[MAX_PRESETS];
};

struct LegacyStoredDeviceConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t payloadSize;
  uint32_t checksum;
  LegacyDeviceConfigPayload payload;
};
#pragma pack(pop)

uint32_t checksumBytes(const uint8_t *bytes, size_t length) {
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < length; ++i) {
    value ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      value = (value >> 1) ^ (0xEDB88320 & (0 - (value & 1)));
    }
  }
  return ~value;
}

void copyTextValue(char *destination, size_t destinationSize, const char *source) {
  if (destinationSize == 0) return;
  strncpy(destination, source != nullptr ? source : "", destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

bool normalizeDeviceName(DeviceConfigPayload &config) {
  if (strcmp(config.deviceName, "HX Stomp Controller") == 0) {
    copyTextValue(config.deviceName, sizeof(config.deviceName), "MIDI Controller");
    return true;
  }
  return false;
}

void loadDefaultHardwareValues(HardwareConfig &hardware) {
  memset(&hardware, 0, sizeof(hardware));
  hardware.buttonCount = DEFAULT_BUTTON_COUNT;
  const uint8_t defaultButtonPins[MAX_BUTTON_COUNT] = {9, 8, 7, 6, 5, 4, A1, A2};
  memcpy(hardware.buttonPins, defaultButtonPins, sizeof(defaultButtonPins));
  hardware.menuControlMode = MenuControlMode::StemmaQtEncoder;
  hardware.previousButtonPin = 10;
  hardware.nextButtonPin = A0;
  hardware.encoderAPin = DEFAULT_ENCODER_A_PIN;
  hardware.encoderBPin = DEFAULT_ENCODER_B_PIN;
  hardware.encoderSwitchPin = DEFAULT_ENCODER_SWITCH_PIN;
  hardware.qtEncoderInterruptPin = DEFAULT_QT_ENCODER_INTERRUPT_PIN;
}

void loadDefaultSettingsValues(DeviceSettings &settings) {
  settings.holdToEditEnabled = false;
  settings.buttonScreenSeconds = 5;
  settings.midiOutputMode = MidiOutputMode::UsbAndDin;
}

void copyStoredHardware(const StoredHardwareConfig &source, HardwareConfig &target) {
  loadDefaultHardwareValues(target);
  target.buttonCount = source.buttonCount;
  memcpy(target.buttonPins, source.buttonPins, sizeof(target.buttonPins));
  target.menuControlMode = source.menuControlMode;
  target.previousButtonPin = source.previousButtonPin;
  target.nextButtonPin = source.nextButtonPin;
  target.encoderAPin = source.encoderAPin;
  target.encoderBPin = source.encoderBPin;
  target.encoderSwitchPin = source.encoderSwitchPin;
  target.qtEncoderInterruptPin = DEFAULT_QT_ENCODER_INTERRUPT_PIN;
}

bool isConfigurableInputPin(uint8_t pin) {
  return pin == 1 || (pin >= 4 && pin <= 10) || (pin >= A0 && pin <= A3);
}

bool rememberPinUse(bool usedPins[], uint8_t pin) {
  if (usedPins[pin]) return false;
  usedPins[pin] = true;
  return true;
}

bool activeHardwarePinsAreUnique(const HardwareConfig &hardware) {
  bool usedPins[A3 + 1]{};
  for (uint8_t i = 0; i < hardware.buttonCount; ++i) {
    if (!rememberPinUse(usedPins, hardware.buttonPins[i])) return false;
  }
  if (hardware.menuControlMode == MenuControlMode::TwoButton) {
    return rememberPinUse(usedPins, hardware.previousButtonPin) &&
           rememberPinUse(usedPins, hardware.nextButtonPin);
  }
  const bool encoderPinsUnique = rememberPinUse(usedPins, hardware.encoderAPin) &&
                                 rememberPinUse(usedPins, hardware.encoderBPin) &&
                                 rememberPinUse(usedPins, hardware.encoderSwitchPin);
  if (!encoderPinsUnique) return false;
  if (hardware.menuControlMode == MenuControlMode::StemmaQtEncoder) {
    return rememberPinUse(usedPins, hardware.qtEncoderInterruptPin);
  }
  return true;
}

bool referencesCommand(const ButtonAssignmentRef &reference) {
  return reference.profileId != UNASSIGNED_ID && reference.assignmentId != UNASSIGNED_ID;
}

uint16_t nextProfileId(const DeviceConfigPayload &config) {
  uint16_t highest = 0;
  for (uint8_t i = 0; i < config.profileCount; ++i) {
    highest = max<uint16_t>(highest, config.profiles[i].id);
  }
  return highest + 1;
}

bool profileIdExists(const DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.profileCount; ++i) {
    if (config.profiles[i].id == id) return true;
  }
  return false;
}

void copyStoredAssignment(const StoredAssignmentConfig &source, AssignmentConfig &target) {
  target.id = source.id;
  target.messageType = MidiMessageType::ControlChange;
  target.type = source.type;
  target.controlChange = source.controlChange;
  target.valueOn = source.valueOn;
  target.valueOff = source.valueOff;
  copyTextValue(target.label, sizeof(target.label), source.label);
}

void copyLegacyAssignment(const LegacyAssignmentConfig &source, AssignmentConfig &target) {
  target.id = source.id;
  target.messageType = MidiMessageType::ControlChange;
  target.type = source.type;
  target.controlChange = source.controlChange;
  target.valueOn = source.valueOn;
  target.valueOff = source.valueOff;
  copyTextValue(target.label, sizeof(target.label), source.label);
}

void copyV6Assignment(const StoredV6AssignmentConfig &source, AssignmentConfig &target) {
  target.id = source.id;
  target.messageType = MidiMessageType::ControlChange;
  target.type = source.type;
  target.controlChange = source.controlChange;
  target.valueOn = source.valueOn;
  target.valueOff = source.valueOff;
  copyTextValue(target.label, sizeof(target.label), source.label);
}

DeviceProfileConfig *findProfileByNameAndChannel(DeviceConfigPayload &config,
                                                  const char *name,
                                                  uint8_t midiChannel) {
  for (uint8_t i = 0; i < config.profileCount; ++i) {
    if (config.profiles[i].midiChannel == midiChannel &&
        strncmp(config.profiles[i].name, name, PRESET_NAME_LENGTH) == 0) {
      return &config.profiles[i];
    }
  }
  return nullptr;
}

void formatMigratedProfileName(char *output,
                               size_t outputSize,
                               const char *presetName,
                               uint8_t presetMidiChannel,
                               uint8_t assignmentMidiChannel) {
  const char *baseName = presetName != nullptr && presetName[0] != '\0' ? presetName : "MIDI Device";
  if (assignmentMidiChannel == presetMidiChannel) {
    copyTextValue(output, outputSize, baseName);
    return;
  }
  snprintf(output, outputSize, "%s Ch %u", baseName, assignmentMidiChannel);
  output[outputSize - 1] = '\0';
}

DeviceProfileConfig *addMigratedProfile(DeviceConfigPayload &config,
                                         const char *name,
                                         uint8_t midiChannel,
                                         uint16_t preferredId) {
  if (config.profileCount >= MAX_DEVICE_PROFILES) return nullptr;

  DeviceProfileConfig &profile = config.profiles[config.profileCount++];
  memset(&profile, 0, sizeof(profile));
  uint16_t id = preferredId != UNASSIGNED_ID ? preferredId : nextProfileId(config);
  while (id == UNASSIGNED_ID || profileIdExists(config, id)) ++id;
  profile.id = id;
  copyTextValue(profile.name, sizeof(profile.name), name);
  profile.midiChannel = midiChannel;
  profile.programChangeDisplayMode = ProgramChangeDisplayMode::ZeroBased;
  profile.assignmentCount = 0;
  return &profile;
}

DeviceProfileConfig *ensureMigratedProfile(DeviceConfigPayload &config,
                                           const char *presetName,
                                           uint16_t presetId,
                                           uint8_t presetMidiChannel,
                                           uint8_t assignmentMidiChannel) {
  char profileName[PRESET_NAME_LENGTH]{};
  formatMigratedProfileName(profileName,
                            sizeof(profileName),
                            presetName,
                            presetMidiChannel,
                            assignmentMidiChannel);
  DeviceProfileConfig *existing = findProfileByNameAndChannel(config, profileName, assignmentMidiChannel);
  if (existing != nullptr) return existing;

  const uint16_t preferredId = assignmentMidiChannel == presetMidiChannel ? presetId : nextProfileId(config);
  return addMigratedProfile(config, profileName, assignmentMidiChannel, preferredId);
}

const StoredAssignmentConfig *findStoredAssignment(const StoredPedalPresetConfig &preset, uint16_t id) {
  for (uint8_t i = 0; i < preset.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
    if (preset.assignments[i].id == id) return &preset.assignments[i];
  }
  return nullptr;
}

const StoredAssignmentConfig *findStoredAssignment(const StoredNarrowPedalPresetConfig &preset, uint16_t id) {
  for (uint8_t i = 0; i < preset.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
    if (preset.assignments[i].id == id) return &preset.assignments[i];
  }
  return nullptr;
}

const LegacyAssignmentConfig *findLegacyAssignment(const StoredV2PedalPresetConfig &preset, uint16_t id) {
  for (uint8_t i = 0; i < preset.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
    if (preset.assignments[i].id == id) return &preset.assignments[i];
  }
  return nullptr;
}

const LegacyAssignmentConfig *findLegacyAssignment(const LegacyDeviceConfigPayload &payload, uint16_t id) {
  for (uint8_t i = 0; i < payload.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
    if (payload.assignments[i].id == id) return &payload.assignments[i];
  }
  return nullptr;
}

bool addAssignmentIfMissing(DeviceProfileConfig &profile, const StoredAssignmentConfig &source) {
  for (uint8_t i = 0; i < profile.assignmentCount; ++i) {
    if (profile.assignments[i].id == source.id) return true;
  }
  if (profile.assignmentCount >= MAX_ASSIGNMENTS) return false;
  copyStoredAssignment(source, profile.assignments[profile.assignmentCount++]);
  return true;
}

bool addAssignmentIfMissing(DeviceProfileConfig &profile, const LegacyAssignmentConfig &source) {
  for (uint8_t i = 0; i < profile.assignmentCount; ++i) {
    if (profile.assignments[i].id == source.id) return true;
  }
  if (profile.assignmentCount >= MAX_ASSIGNMENTS) return false;
  copyLegacyAssignment(source, profile.assignments[profile.assignmentCount++]);
  return true;
}

template <typename SourcePreset>
void migrateStoredPresetAssignments(const SourcePreset &sourcePreset, DeviceConfigPayload &target) {
  for (uint8_t i = 0; i < sourcePreset.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
    const StoredAssignmentConfig &source = sourcePreset.assignments[i];
    const uint8_t channel = source.midiChannel >= 1 && source.midiChannel <= 16
                                ? source.midiChannel
                                : sourcePreset.midiChannel;
    DeviceProfileConfig *profile = ensureMigratedProfile(target,
                                                         sourcePreset.name,
                                                         sourcePreset.id,
                                                         sourcePreset.midiChannel,
                                                         channel);
    if (profile != nullptr) addAssignmentIfMissing(*profile, source);
  }
}

void migrateStoredPreset(const StoredPedalPresetConfig &sourcePreset,
                         PedalPresetConfig &targetPreset,
                         DeviceConfigPayload &target) {
  memset(&targetPreset, 0, sizeof(targetPreset));
  targetPreset.id = sourcePreset.id;
  copyTextValue(targetPreset.name, sizeof(targetPreset.name), sourcePreset.name);
  migrateStoredPresetAssignments(sourcePreset, target);

  for (uint8_t i = 0; i < MAX_BUTTON_COUNT; ++i) {
    const uint16_t assignmentId = sourcePreset.buttonAssignmentIds[i];
    const StoredAssignmentConfig *source = findStoredAssignment(sourcePreset, assignmentId);
    if (source == nullptr) continue;
    const uint8_t channel = source->midiChannel >= 1 && source->midiChannel <= 16
                                ? source->midiChannel
                                : sourcePreset.midiChannel;
    DeviceProfileConfig *profile = ensureMigratedProfile(target,
                                                         sourcePreset.name,
                                                         sourcePreset.id,
                                                         sourcePreset.midiChannel,
                                                         channel);
    if (profile != nullptr && addAssignmentIfMissing(*profile, *source)) {
      targetPreset.buttonAssignments[i] = {profile->id, assignmentId};
    }
  }
}

void migrateStoredPreset(const StoredNarrowPedalPresetConfig &sourcePreset,
                         PedalPresetConfig &targetPreset,
                         DeviceConfigPayload &target) {
  memset(&targetPreset, 0, sizeof(targetPreset));
  targetPreset.id = sourcePreset.id;
  copyTextValue(targetPreset.name, sizeof(targetPreset.name), sourcePreset.name);
  migrateStoredPresetAssignments(sourcePreset, target);

  for (uint8_t i = 0; i < LEGACY_BUTTON_COUNT; ++i) {
    const uint16_t assignmentId = sourcePreset.buttonAssignmentIds[i];
    const StoredAssignmentConfig *source = findStoredAssignment(sourcePreset, assignmentId);
    if (source == nullptr) continue;
    const uint8_t channel = source->midiChannel >= 1 && source->midiChannel <= 16
                                ? source->midiChannel
                                : sourcePreset.midiChannel;
    DeviceProfileConfig *profile = ensureMigratedProfile(target,
                                                         sourcePreset.name,
                                                         sourcePreset.id,
                                                         sourcePreset.midiChannel,
                                                         channel);
    if (profile != nullptr && addAssignmentIfMissing(*profile, *source)) {
      targetPreset.buttonAssignments[i] = {profile->id, assignmentId};
    }
  }
}

void migrateLegacyPreset(const StoredV2PedalPresetConfig &sourcePreset,
                         PedalPresetConfig &targetPreset,
                         DeviceConfigPayload &target) {
  memset(&targetPreset, 0, sizeof(targetPreset));
  targetPreset.id = sourcePreset.id;
  copyTextValue(targetPreset.name, sizeof(targetPreset.name), sourcePreset.name);
  DeviceProfileConfig *profile = ensureMigratedProfile(target,
                                                       sourcePreset.name,
                                                       sourcePreset.id,
                                                       sourcePreset.midiChannel,
                                                       sourcePreset.midiChannel);
  if (profile != nullptr) {
    for (uint8_t i = 0; i < sourcePreset.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
      addAssignmentIfMissing(*profile, sourcePreset.assignments[i]);
    }
  }

  for (uint8_t i = 0; i < LEGACY_BUTTON_COUNT; ++i) {
    const uint16_t assignmentId = sourcePreset.buttonAssignmentIds[i];
    const LegacyAssignmentConfig *source = findLegacyAssignment(sourcePreset, assignmentId);
    if (profile != nullptr && source != nullptr && addAssignmentIfMissing(*profile, *source)) {
      targetPreset.buttonAssignments[i] = {profile->id, assignmentId};
    }
  }
}

void copyStoredButtonPreset(const StoredButtonPresetConfig &source, PedalPresetConfig &target) {
  memset(&target, 0, sizeof(target));
  target.id = source.id;
  copyTextValue(target.name, sizeof(target.name), source.name);
  memcpy(target.buttonAssignments, source.buttonAssignments, sizeof(target.buttonAssignments));
}

void migrateV9Config(const StoredV9DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  copyStoredHardware(source.hardware, target.hardware);
  target.settings = source.settings;
  target.presetToggle = source.presetToggle;
  target.activePresetId = source.activePresetId;
  target.profileCount = min<uint8_t>(source.profileCount, MAX_DEVICE_PROFILES);
  for (uint8_t p = 0; p < target.profileCount; ++p) {
    target.profiles[p] = source.profiles[p];
  }
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    copyStoredButtonPreset(source.presets[p], target.presets[p]);
  }
}

void applyCorrectedButtonDefaultsIfFactory(DeviceConfigPayload &config) {
  const uint8_t oldPins[DEFAULT_BUTTON_COUNT] = {4, 5, 6, 7, 8, 9};
  const uint8_t correctedPins[DEFAULT_BUTTON_COUNT] = {9, 8, 7, 6, 5, 4};
  const ButtonAssignmentRef oldDefaults[DEFAULT_BUTTON_COUNT] = {
      {1, 18}, {1, 19}, {1, 20}, {1, 4}, {1, 5}, {1, 33}};
  const ButtonAssignmentRef correctedDefaults[DEFAULT_BUTTON_COUNT] = {
      {1, 31}, {1, 32}, {1, 17}, {1, 18}, {1, 19}, {1, 20}};

  if (config.hardware.buttonCount != DEFAULT_BUTTON_COUNT || config.presetCount == 0) return;
  bool hasOldPinOrder = true;
  bool hasOldAssignments = true;
  for (uint8_t i = 0; i < DEFAULT_BUTTON_COUNT; ++i) {
    if (config.hardware.buttonPins[i] != oldPins[i]) hasOldPinOrder = false;
    if (config.presets[0].buttonAssignments[i].profileId != oldDefaults[i].profileId ||
        config.presets[0].buttonAssignments[i].assignmentId != oldDefaults[i].assignmentId) {
      hasOldAssignments = false;
    }
  }

  if (hasOldPinOrder) memcpy(config.hardware.buttonPins, correctedPins, sizeof(correctedPins));
  if (hasOldAssignments) {
    memcpy(config.presets[0].buttonAssignments, correctedDefaults, sizeof(correctedDefaults));
  }
}

void migrateV12Config(const StoredV12DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  target.hardware = source.hardware;
  target.settings = source.settings;
  target.presetToggle = source.presetToggle;
  target.activePresetId = source.activePresetId;
  target.profileCount = min<uint8_t>(source.profileCount, MAX_DEVICE_PROFILES);
  for (uint8_t p = 0; p < target.profileCount; ++p) {
    target.profiles[p] = source.profiles[p];
  }
  target.macroCount = 0;
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    copyStoredButtonPreset(source.presets[p], target.presets[p]);
  }
}

void migrateV8Config(const StoredV8DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  copyStoredHardware(source.hardware, target.hardware);
  target.settings = source.settings;
  target.presetToggle = {false, UNASSIGNED_ID, UNASSIGNED_ID, 1};
  target.activePresetId = source.activePresetId;
  target.profileCount = min<uint8_t>(source.profileCount, MAX_DEVICE_PROFILES);
  for (uint8_t p = 0; p < target.profileCount; ++p) {
    target.profiles[p] = source.profiles[p];
  }
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    copyStoredButtonPreset(source.presets[p], target.presets[p]);
  }
}

void migrateV7Config(const StoredV7DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  copyStoredHardware(source.hardware, target.hardware);
  target.settings = source.settings;
  target.presetToggle = {false, UNASSIGNED_ID, UNASSIGNED_ID, 1};
  target.activePresetId = source.activePresetId;
  target.profileCount = min<uint8_t>(source.profileCount, MAX_DEVICE_PROFILES);
  for (uint8_t p = 0; p < target.profileCount; ++p) {
    const StoredV7DeviceProfileConfig &sourceProfile = source.profiles[p];
    DeviceProfileConfig &targetProfile = target.profiles[p];
    targetProfile.id = sourceProfile.id;
    copyTextValue(targetProfile.name, sizeof(targetProfile.name), sourceProfile.name);
    targetProfile.midiChannel = sourceProfile.midiChannel;
    targetProfile.programChangeDisplayMode = ProgramChangeDisplayMode::ZeroBased;
    targetProfile.assignmentCount = min<uint8_t>(sourceProfile.assignmentCount, MAX_ASSIGNMENTS);
    for (uint8_t i = 0; i < targetProfile.assignmentCount; ++i) {
      targetProfile.assignments[i] = sourceProfile.assignments[i];
    }
  }
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    copyStoredButtonPreset(source.presets[p], target.presets[p]);
  }
}

void migrateV6Config(const StoredV6DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  copyStoredHardware(source.hardware, target.hardware);
  target.settings = source.settings;
  target.activePresetId = source.activePresetId;
  target.profileCount = min<uint8_t>(source.profileCount, MAX_DEVICE_PROFILES);
  for (uint8_t p = 0; p < target.profileCount; ++p) {
    const StoredV6DeviceProfileConfig &sourceProfile = source.profiles[p];
    DeviceProfileConfig &targetProfile = target.profiles[p];
    targetProfile.id = sourceProfile.id;
    copyTextValue(targetProfile.name, sizeof(targetProfile.name), sourceProfile.name);
    targetProfile.midiChannel = sourceProfile.midiChannel;
    targetProfile.programChangeDisplayMode = ProgramChangeDisplayMode::ZeroBased;
    targetProfile.assignmentCount = min<uint8_t>(sourceProfile.assignmentCount, MAX_ASSIGNMENTS);
    for (uint8_t i = 0; i < targetProfile.assignmentCount; ++i) {
      copyV6Assignment(sourceProfile.assignments[i], targetProfile.assignments[i]);
    }
  }
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    target.presets[p].id = source.presets[p].id;
    copyTextValue(target.presets[p].name, sizeof(target.presets[p].name), source.presets[p].name);
    memcpy(target.presets[p].buttonAssignments,
           source.presets[p].buttonAssignments,
           sizeof(target.presets[p].buttonAssignments));
  }
}

void migrateV5Config(const StoredV5DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyStoredHardware(source.hardware, target.hardware);
  target.settings = source.settings;
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  target.activePresetId = source.activePresetId;
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    migrateStoredPreset(source.presets[p], target.presets[p], target);
  }
}

void migrateV4Config(const StoredV4DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  copyStoredHardware(source.hardware, target.hardware);
  loadDefaultSettingsValues(target.settings);
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  target.activePresetId = source.activePresetId;
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    migrateStoredPreset(source.presets[p], target.presets[p], target);
  }
}

void migrateV3Config(const StoredV3DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  loadDefaultHardwareValues(target.hardware);
  loadDefaultSettingsValues(target.settings);
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  target.activePresetId = source.activePresetId;
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    migrateStoredPreset(source.presets[p], target.presets[p], target);
  }
}

void migrateV2Config(const StoredV2DeviceConfigPayload &source, DeviceConfigPayload &target) {
  memset(&target, 0, sizeof(target));
  loadDefaultHardwareValues(target.hardware);
  loadDefaultSettingsValues(target.settings);
  copyTextValue(target.deviceName, sizeof(target.deviceName), source.deviceName);
  target.activePresetId = source.activePresetId;
  target.presetCount = min<uint8_t>(source.presetCount, MAX_PRESETS);
  for (uint8_t p = 0; p < target.presetCount; ++p) {
    migrateLegacyPreset(source.presets[p], target.presets[p], target);
  }
}
}

bool DeviceConfigStore::begin() {
  EEPROM.begin(LEGACY_EEPROM_SIZE);
  return LittleFS.begin();
}

bool DeviceConfigStore::load(DeviceConfigPayload &config) {
  if (LittleFS.exists(CONFIG_PATH)) {
    File file = LittleFS.open(CONFIG_PATH, "r");
    StoredDeviceConfigHeader header{};
    const size_t headerBytes = file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header));
    if (headerBytes == sizeof(header) && header.magic == CONFIG_MAGIC) {
      if (header.version == CONFIG_VERSION &&
          header.payloadSize == sizeof(DeviceConfigPayload)) {
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&config), sizeof(config));
        file.close();
        if (payloadBytes == sizeof(config) && header.checksum == checksum(config) && validate(config)) {
          if (normalizeDeviceName(config)) save(config);
          return true;
        }
      } else if (header.version == MACROS_PREVIOUS_VERSION &&
                 header.payloadSize == sizeof(StoredV12DeviceConfigPayload)) {
        StoredV12DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV12Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if ((header.version == CORRECTED_BUTTON_DEFAULTS_PREVIOUS_VERSION ||
                  header.version == CORRECTED_BUTTON_DEFAULTS_INITIAL_VERSION) &&
                 header.payloadSize == sizeof(StoredV12DeviceConfigPayload)) {
        StoredV12DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV12Config(previous, config);
          normalizeDeviceName(config);
          applyCorrectedButtonDefaultsIfFactory(config);
          if (validate(config)) {
            save(config);
            return true;
          }
        }
      } else if (header.version == STEMMA_QT_ENCODER_PREVIOUS_VERSION &&
                 header.payloadSize == sizeof(StoredV9DeviceConfigPayload)) {
        StoredV9DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV9Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == PRESET_TOGGLE_PREVIOUS_VERSION &&
                 header.payloadSize == sizeof(StoredV8DeviceConfigPayload)) {
        StoredV8DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV8Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == PC_DISPLAY_PREVIOUS_VERSION &&
                 header.payloadSize == sizeof(StoredV7DeviceConfigPayload)) {
        StoredV7DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV7Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == MESSAGE_TYPE_PREVIOUS_VERSION &&
                 header.payloadSize == sizeof(StoredV6DeviceConfigPayload)) {
        StoredV6DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV6Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == DEVICE_PROFILES_PREVIOUS_VERSION &&
                 header.payloadSize == sizeof(StoredV5DeviceConfigPayload)) {
        StoredV5DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV5Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == PREVIOUS_CONFIG_VERSION &&
                 header.payloadSize == sizeof(StoredV4DeviceConfigPayload)) {
        StoredV4DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV4Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == HARDWARELESS_CONFIG_VERSION &&
                 header.payloadSize == sizeof(StoredV3DeviceConfigPayload)) {
        StoredV3DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV3Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else if (header.version == LEGACY_PRESET_CONFIG_VERSION &&
                 header.payloadSize == sizeof(StoredV2DeviceConfigPayload)) {
        StoredV2DeviceConfigPayload previous{};
        const size_t payloadBytes = file.read(reinterpret_cast<uint8_t *>(&previous), sizeof(previous));
        file.close();
        const uint32_t previousChecksum = checksumBytes(
            reinterpret_cast<const uint8_t *>(&previous), sizeof(previous));
        if (payloadBytes == sizeof(previous) && header.checksum == previousChecksum) {
          migrateV2Config(previous, config);
          if (validate(config)) {
            normalizeDeviceName(config);
            save(config);
            return true;
          }
        }
      } else {
        file.close();
      }
    } else {
      file.close();
    }
  }

  loadDefaults(config);
  DeviceProfileConfig &profile = config.profiles[0];
  PedalPresetConfig &preset = config.presets[0];

  LegacyStoredDeviceConfig legacyStored{};
  EEPROM.get(0, legacyStored);
  const uint32_t legacyChecksum = checksumBytes(
      reinterpret_cast<const uint8_t *>(&legacyStored.payload), sizeof(legacyStored.payload));
  if (legacyStored.magic == LEGACY_CONFIG_MAGIC &&
      legacyStored.version == LEGACY_CONFIG_VERSION &&
      legacyStored.payloadSize == sizeof(LegacyDeviceConfigPayload) &&
      legacyStored.checksum == legacyChecksum) {
    copyText(config.deviceName, sizeof(config.deviceName), legacyStored.payload.deviceName);
    profile.midiChannel = legacyStored.payload.midiChannel;
    profile.assignmentCount = 0;
    for (uint8_t i = 0; i < legacyStored.payload.assignmentCount && i < MAX_ASSIGNMENTS; ++i) {
      addAssignmentIfMissing(profile, legacyStored.payload.assignments[i]);
    }
    for (uint8_t i = 0; i < LEGACY_BUTTON_COUNT; ++i) {
      const uint16_t assignmentId = legacyStored.payload.buttonAssignmentIds[i];
      if (findLegacyAssignment(legacyStored.payload, assignmentId) != nullptr) {
        preset.buttonAssignments[i] = {profile.id, assignmentId};
      }
    }
  } else {
    const uint8_t legacySignature = EEPROM.read(0);
    uint8_t legacyPresetIndices[LEGACY_BUTTON_COUNT]{};
    for (uint8_t i = 0; i < LEGACY_BUTTON_COUNT; ++i) {
      legacyPresetIndices[i] = EEPROM.read(1 + i);
    }
    migrateLegacyButtons(profile, preset, legacySignature, legacyPresetIndices);
  }

  if (!validate(config)) loadDefaults(config);
  normalizeDeviceName(config);
  save(config);
  return false;
}

bool DeviceConfigStore::save(DeviceConfigPayload &config) {
  if (!validate(config)) return false;

  StoredDeviceConfigHeader header{};
  header.magic = CONFIG_MAGIC;
  header.version = CONFIG_VERSION;
  header.payloadSize = sizeof(DeviceConfigPayload);
  header.checksum = checksum(config);

  LittleFS.remove(TEMP_CONFIG_PATH);
  File file = LittleFS.open(TEMP_CONFIG_PATH, "w");
  if (!file) return false;
  const size_t headerWritten = file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header));
  const size_t payloadWritten = file.write(reinterpret_cast<const uint8_t *>(&config), sizeof(config));
  file.flush();
  file.close();
  if (headerWritten != sizeof(header) || payloadWritten != sizeof(config)) {
    LittleFS.remove(TEMP_CONFIG_PATH);
    return false;
  }

  return LittleFS.rename(TEMP_CONFIG_PATH, CONFIG_PATH);
}

void DeviceConfigStore::loadDefaults(DeviceConfigPayload &config) const {
  memset(&config, 0, sizeof(config));
  copyText(config.deviceName, sizeof(config.deviceName), "MIDI Controller");
  loadDefaultHardware(config.hardware);
  loadDefaultSettingsValues(config.settings);
  config.presetToggle = {false, UNASSIGNED_ID, UNASSIGNED_ID, 1};
  config.profileCount = 1;
  loadDefaultProfile(config.profiles[0]);
  config.presetCount = 1;
  config.activePresetId = 1;
  loadDefaultPreset(config.presets[0]);
}

void DeviceConfigStore::loadDefaultHardware(HardwareConfig &hardware) {
  loadDefaultHardwareValues(hardware);
}

void DeviceConfigStore::loadDefaultProfile(DeviceProfileConfig &profile) {
  memset(&profile, 0, sizeof(profile));
  profile.id = 1;
  copyText(profile.name, sizeof(profile.name), "HX Stomp");
  profile.midiChannel = 1;
  profile.programChangeDisplayMode = ProgramChangeDisplayMode::ZeroBased;

  addDefaultAssignment(profile, 1, "Emulates FS1", 49, 127);
  addDefaultAssignment(profile, 2, "Emulates FS2", 50, 127);
  addDefaultAssignment(profile, 3, "Emulates FS3", 51, 127);
  addDefaultAssignment(profile, 4, "Emulates FS4", 52, 127);
  addDefaultAssignment(profile, 5, "Emulates FS5", 53, 127);
  addDefaultAssignment(profile, 6, "Looper Record", 60, 127);
  addDefaultAssignment(profile, 7, "Looper Overdub", 60, 63);
  addDefaultAssignment(profile, 8, "Looper Play", 61, 127);
  addDefaultAssignment(profile, 9, "Looper Stop", 61, 63);
  addDefaultAssignment(profile, 10, "Looper Play Once", 62, 127);
  addDefaultAssignment(profile, 11, "Looper Undo/Redo", 63, 127);
  addDefaultAssignment(profile, 12, "Looper Forward", 65, 63);
  addDefaultAssignment(profile, 13, "Looper Reverse", 65, 127);
  addDefaultAssignment(profile, 14, "Looper Full Speed", 66, 63);
  addDefaultAssignment(profile, 15, "Looper Half Speed", 66, 127);
  addDefaultAssignment(profile, 16, "Tap Tempo", 64, 127);
  addDefaultAssignment(profile, 17, "Tuner On/Off", 68, 127);
  addDefaultAssignment(profile, 18, "Snapshot 1", 69, 0);
  addDefaultAssignment(profile, 19, "Snapshot 2", 69, 1);
  addDefaultAssignment(profile, 20, "Snapshot 3", 69, 2);
  addDefaultAssignment(profile, 21, "Next Snapshot", 69, 8);
  addDefaultAssignment(profile, 22, "Previous Snapshot", 69, 9);
  addDefaultAssignment(profile, 23, "Bypass On", 70, 63);
  addDefaultAssignment(profile, 24, "Bypass Off", 70, 127);
  addDefaultAssignment(profile, 25, "Footswitch Stomp", 71, 0);
  addDefaultAssignment(profile, 26, "Footswitch Scroll", 71, 1);
  addDefaultAssignment(profile, 27, "Footswitch Preset", 71, 2);
  addDefaultAssignment(profile, 28, "Footswitch Snapshot", 71, 3);
  addDefaultAssignment(profile, 29, "Next Footswitch", 71, 4);
  addDefaultAssignment(profile, 30, "Previous Footswitch", 71, 5);
  addDefaultAssignment(profile, 31, "Previous Preset", 72, 63);
  addDefaultAssignment(profile, 32, "Next Preset", 72, 127);
  addDefaultAssignment(profile, 33, "Custom CC 25", 25, 127);
}

void DeviceConfigStore::loadDefaultPreset(PedalPresetConfig &preset) {
  memset(&preset, 0, sizeof(preset));
  preset.id = 1;
  copyText(preset.name, sizeof(preset.name), "HX Stomp");

  const ButtonAssignmentRef defaults[DEFAULT_BUTTON_COUNT] = {
      {1, 31}, {1, 32}, {1, 17}, {1, 18}, {1, 19}, {1, 20}};
  memcpy(preset.buttonAssignments, defaults, sizeof(defaults));
}

PedalPresetConfig *DeviceConfigStore::activePreset(DeviceConfigPayload &config) {
  return findPreset(config, config.activePresetId);
}

const PedalPresetConfig *DeviceConfigStore::activePreset(const DeviceConfigPayload &config) {
  return findPreset(config, config.activePresetId);
}

PedalPresetConfig *DeviceConfigStore::findPreset(DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.presetCount; ++i) {
    if (config.presets[i].id == id) return &config.presets[i];
  }
  return nullptr;
}

const PedalPresetConfig *DeviceConfigStore::findPreset(const DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.presetCount; ++i) {
    if (config.presets[i].id == id) return &config.presets[i];
  }
  return nullptr;
}

DeviceProfileConfig *DeviceConfigStore::findProfile(DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.profileCount; ++i) {
    if (config.profiles[i].id == id) return &config.profiles[i];
  }
  return nullptr;
}

const DeviceProfileConfig *DeviceConfigStore::findProfile(const DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.profileCount; ++i) {
    if (config.profiles[i].id == id) return &config.profiles[i];
  }
  return nullptr;
}

MacroConfig *DeviceConfigStore::findMacro(DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.macroCount; ++i) {
    if (config.macros[i].id == id) return &config.macros[i];
  }
  return nullptr;
}

const MacroConfig *DeviceConfigStore::findMacro(const DeviceConfigPayload &config, uint16_t id) {
  for (uint8_t i = 0; i < config.macroCount; ++i) {
    if (config.macros[i].id == id) return &config.macros[i];
  }
  return nullptr;
}

AssignmentConfig *DeviceConfigStore::findAssignment(DeviceProfileConfig &profile, uint16_t id) {
  for (uint8_t i = 0; i < profile.assignmentCount; ++i) {
    if (profile.assignments[i].id == id) return &profile.assignments[i];
  }
  return nullptr;
}

const AssignmentConfig *DeviceConfigStore::findAssignment(const DeviceProfileConfig &profile, uint16_t id) {
  for (uint8_t i = 0; i < profile.assignmentCount; ++i) {
    if (profile.assignments[i].id == id) return &profile.assignments[i];
  }
  return nullptr;
}

const AssignmentConfig *DeviceConfigStore::findAssignment(const DeviceConfigPayload &config,
                                                          const ButtonAssignmentRef &reference,
                                                          const DeviceProfileConfig **profile) {
  if (!referencesCommand(reference)) {
    if (profile != nullptr) *profile = nullptr;
    return nullptr;
  }
  const DeviceProfileConfig *resolvedProfile = findProfile(config, reference.profileId);
  if (profile != nullptr) *profile = resolvedProfile;
  if (resolvedProfile == nullptr) return nullptr;
  return findAssignment(*resolvedProfile, reference.assignmentId);
}

bool DeviceConfigStore::validate(DeviceConfigPayload &config) {
  if (config.profileCount == 0 || config.profileCount > MAX_DEVICE_PROFILES ||
      config.presetCount == 0 || config.presetCount > MAX_PRESETS) return false;
  config.deviceName[DEVICE_NAME_LENGTH - 1] = '\0';
  if (config.hardware.buttonCount == 0) return false;
  if (config.hardware.buttonCount > MAX_STOMP_SWITCH_COUNT) {
    config.hardware.buttonCount = MAX_STOMP_SWITCH_COUNT;
  }
  if (static_cast<uint8_t>(config.hardware.menuControlMode) >
      static_cast<uint8_t>(MenuControlMode::StemmaQtEncoder)) return false;
  for (uint8_t i = 0; i < MAX_BUTTON_COUNT; ++i) {
    if (!isConfigurableInputPin(config.hardware.buttonPins[i])) return false;
  }
  if (!isConfigurableInputPin(config.hardware.previousButtonPin) ||
      !isConfigurableInputPin(config.hardware.nextButtonPin) ||
      !isConfigurableInputPin(config.hardware.encoderAPin) ||
      !isConfigurableInputPin(config.hardware.encoderBPin) ||
      !isConfigurableInputPin(config.hardware.encoderSwitchPin) ||
      !isConfigurableInputPin(config.hardware.qtEncoderInterruptPin) ||
      !activeHardwarePinsAreUnique(config.hardware)) return false;
  if (config.settings.buttonScreenSeconds < 1 || config.settings.buttonScreenSeconds > 30) {
    config.settings.buttonScreenSeconds = 5;
  }
  if (static_cast<uint8_t>(config.settings.midiOutputMode) >
      static_cast<uint8_t>(MidiOutputMode::DinOnly)) {
    config.settings.midiOutputMode = MidiOutputMode::UsbAndDin;
  }

  for (uint8_t p = 0; p < config.profileCount; ++p) {
    DeviceProfileConfig &profile = config.profiles[p];
    profile.name[PRESET_NAME_LENGTH - 1] = '\0';
    if (profile.id == UNASSIGNED_ID || profile.midiChannel < 1 || profile.midiChannel > 16 ||
        static_cast<uint8_t>(profile.programChangeDisplayMode) >
            static_cast<uint8_t>(ProgramChangeDisplayMode::OneBased) ||
        profile.assignmentCount == 0 || profile.assignmentCount > MAX_ASSIGNMENTS) return false;
    for (uint8_t q = p + 1; q < config.profileCount; ++q) {
      if (profile.id == config.profiles[q].id) return false;
    }
    for (uint8_t i = 0; i < profile.assignmentCount; ++i) {
      AssignmentConfig &assignment = profile.assignments[i];
      assignment.label[ASSIGNMENT_LABEL_LENGTH - 1] = '\0';
      if (assignment.id == UNASSIGNED_ID ||
          static_cast<uint8_t>(assignment.messageType) > static_cast<uint8_t>(MidiMessageType::ProgramChange) ||
          static_cast<uint8_t>(assignment.type) > static_cast<uint8_t>(AssignmentType::Momentary) ||
          assignment.controlChange > 127 || assignment.valueOn > 127 || assignment.valueOff > 127) return false;
      for (uint8_t j = i + 1; j < profile.assignmentCount; ++j) {
        if (assignment.id == profile.assignments[j].id) return false;
      }
    }
  }

  if (config.macroCount > MAX_MACROS) return false;
  for (uint8_t m = 0; m < config.macroCount; ++m) {
    MacroConfig &macro = config.macros[m];
    macro.name[PRESET_NAME_LENGTH - 1] = '\0';
    if (macro.id == UNASSIGNED_ID || macro.stepCount == 0 || macro.stepCount > MAX_MACRO_STEPS) return false;
    for (uint8_t n = m + 1; n < config.macroCount; ++n) {
      if (macro.id == config.macros[n].id) return false;
    }
    for (uint8_t s = 0; s < macro.stepCount; ++s) {
      MacroStepConfig &step = macro.steps[s];
      if (step.delayAfterMs > MAX_MACRO_STEP_DELAY_MS ||
          findAssignment(config, step.command, nullptr) == nullptr) {
        return false;
      }
    }
    for (uint8_t s = macro.stepCount; s < MAX_MACRO_STEPS; ++s) {
      macro.steps[s] = {{UNASSIGNED_ID, UNASSIGNED_ID}, 0};
    }
  }
  for (uint8_t m = config.macroCount; m < MAX_MACROS; ++m) {
    memset(&config.macros[m], 0, sizeof(config.macros[m]));
  }

  for (uint8_t p = 0; p < config.presetCount; ++p) {
    PedalPresetConfig &preset = config.presets[p];
    preset.name[PRESET_NAME_LENGTH - 1] = '\0';
    if (preset.id == 0) return false;
    for (uint8_t q = p + 1; q < config.presetCount; ++q) {
      if (preset.id == config.presets[q].id) return false;
    }

    for (uint8_t i = 0; i < config.hardware.buttonCount; ++i) {
      ButtonAssignmentRef &reference = preset.buttonAssignments[i];
      if (!referencesCommand(reference)) {
        reference = {UNASSIGNED_ID, UNASSIGNED_ID};
      } else if (findAssignment(config, reference, nullptr) == nullptr) {
        reference = {UNASSIGNED_ID, UNASSIGNED_ID};
      }
      if (preset.buttonMacroIds[i] != UNASSIGNED_ID &&
          findMacro(config, preset.buttonMacroIds[i]) == nullptr) {
        preset.buttonMacroIds[i] = UNASSIGNED_ID;
      }
    }
    for (uint8_t i = config.hardware.buttonCount; i < MAX_BUTTON_COUNT; ++i) {
      preset.buttonAssignments[i] = {UNASSIGNED_ID, UNASSIGNED_ID};
      preset.buttonMacroIds[i] = UNASSIGNED_ID;
    }
  }

  if (config.presetToggle.switchNumber < 1 ||
      config.presetToggle.switchNumber > config.hardware.buttonCount) {
    config.presetToggle.switchNumber = 1;
  }
  if (config.presetCount >= 2) {
    if (findPreset(config, config.presetToggle.primaryPresetId) == nullptr) {
      config.presetToggle.primaryPresetId = config.presets[0].id;
    }
    if (findPreset(config, config.presetToggle.alternatePresetId) == nullptr ||
        config.presetToggle.alternatePresetId == config.presetToggle.primaryPresetId) {
      config.presetToggle.alternatePresetId = UNASSIGNED_ID;
      for (uint8_t p = 0; p < config.presetCount; ++p) {
        if (config.presets[p].id != config.presetToggle.primaryPresetId) {
          config.presetToggle.alternatePresetId = config.presets[p].id;
          break;
        }
      }
    }
  } else {
    config.presetToggle.primaryPresetId = UNASSIGNED_ID;
    config.presetToggle.alternatePresetId = UNASSIGNED_ID;
    config.presetToggle.enabled = false;
  }
  if (config.presetToggle.enabled &&
      (config.presetToggle.primaryPresetId == UNASSIGNED_ID ||
       config.presetToggle.alternatePresetId == UNASSIGNED_ID ||
       config.presetToggle.primaryPresetId == config.presetToggle.alternatePresetId)) {
    return false;
  }

  if (activePreset(config) == nullptr) config.activePresetId = config.presets[0].id;
  return true;
}

uint32_t DeviceConfigStore::checksum(const DeviceConfigPayload &config) {
  return checksumBytes(reinterpret_cast<const uint8_t *>(&config), sizeof(config));
}

void DeviceConfigStore::copyText(char *destination, size_t destinationSize, const char *source) {
  copyTextValue(destination, destinationSize, source);
}

void DeviceConfigStore::addDefaultAssignment(DeviceProfileConfig &profile,
                                             uint16_t id,
                                             const char *label,
                                             uint8_t controlChange,
                                             uint8_t valueOn) {
  if (profile.assignmentCount >= MAX_ASSIGNMENTS) return;
  AssignmentConfig &assignment = profile.assignments[profile.assignmentCount++];
  assignment = {id, MidiMessageType::ControlChange, AssignmentType::Fixed, controlChange, valueOn, 0, ""};
  copyText(assignment.label, sizeof(assignment.label), label);
}

void DeviceConfigStore::migrateLegacyButtons(const DeviceProfileConfig &profile,
                                             PedalPresetConfig &preset,
                                             uint8_t legacySignature,
                                             const uint8_t legacyPresetIndices[LEGACY_BUTTON_COUNT]) {
  if (legacySignature != LEGACY_SIGNATURE && legacySignature != LEGACY_OLD_SIGNATURE) return;
  for (uint8_t i = 0; i < LEGACY_BUTTON_COUNT; ++i) {
    uint8_t presetIndex = legacyPresetIndices[i];
    if (legacySignature == LEGACY_OLD_SIGNATURE && presetIndex != LEGACY_UNASSIGNED) {
      presetIndex += LEGACY_PRESET_OFFSET;
    }
    if (presetIndex == LEGACY_UNASSIGNED) {
      preset.buttonAssignments[i] = i == 5 ? ButtonAssignmentRef{profile.id, 33}
                                           : ButtonAssignmentRef{UNASSIGNED_ID, UNASSIGNED_ID};
      continue;
    }
    const uint16_t assignmentId = presetIndex >= LEGACY_PRESET_OFFSET
                                      ? presetIndex - LEGACY_PRESET_OFFSET + 1
                                      : UNASSIGNED_ID;
    preset.buttonAssignments[i] = findAssignment(profile, assignmentId) != nullptr
                                      ? ButtonAssignmentRef{profile.id, assignmentId}
                                      : ButtonAssignmentRef{UNASSIGNED_ID, UNASSIGNED_ID};
  }
}
