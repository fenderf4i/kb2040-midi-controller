#pragma once

#include <Arduino.h>

constexpr uint8_t MAX_BUTTON_COUNT = 8;
constexpr uint8_t MAX_STOMP_SWITCH_COUNT = 6;
constexpr uint8_t DEFAULT_BUTTON_COUNT = 6;
constexpr uint8_t LEGACY_BUTTON_COUNT = 6;
constexpr uint8_t MAX_ASSIGNMENTS = 40;
constexpr uint8_t MAX_DEVICE_PROFILES = 8;
constexpr uint8_t MAX_PRESETS = 8;
constexpr uint8_t ASSIGNMENT_LABEL_LENGTH = 21;
constexpr uint8_t PRESET_NAME_LENGTH = 25;
constexpr uint8_t DEVICE_NAME_LENGTH = 33;
constexpr uint16_t LEGACY_EEPROM_SIZE = 2048;
constexpr uint16_t UNASSIGNED_ID = 0;

enum class AssignmentType : uint8_t {
  Fixed = 0,
  Toggle = 1,
  Momentary = 2
};

enum class MidiMessageType : uint8_t {
  ControlChange = 0,
  ProgramChange = 1
};

enum class MenuControlMode : uint8_t {
  TwoButton = 0,
  Encoder = 1,
  StemmaQtEncoder = 2
};

enum class MidiOutputMode : uint8_t {
  UsbAndDin = 0,
  UsbOnly = 1,
  DinOnly = 2
};

enum class ProgramChangeDisplayMode : uint8_t {
  ZeroBased = 0,
  OneBased = 1
};

#pragma pack(push, 1)
struct AssignmentConfig {
  uint16_t id;
  MidiMessageType messageType;
  AssignmentType type;
  uint8_t controlChange;
  uint8_t valueOn;
  uint8_t valueOff;
  char label[ASSIGNMENT_LABEL_LENGTH];
};

struct ExpressionConfig {
  bool enabled;
  uint8_t controlChange;
  uint16_t rawMinimum;
  uint16_t rawMaximum;
  uint8_t deadband;
  bool invert;
  char label[ASSIGNMENT_LABEL_LENGTH];
};

struct DeviceProfileConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  uint8_t midiChannel;
  ProgramChangeDisplayMode programChangeDisplayMode;
  uint8_t assignmentCount;
  AssignmentConfig assignments[MAX_ASSIGNMENTS];
};

struct ButtonAssignmentRef {
  uint16_t profileId;
  uint16_t assignmentId;
};

struct PedalPresetConfig {
  uint16_t id;
  char name[PRESET_NAME_LENGTH];
  ButtonAssignmentRef buttonAssignments[MAX_BUTTON_COUNT];
};

struct HardwareConfig {
  uint8_t buttonCount;
  uint8_t buttonPins[MAX_BUTTON_COUNT];
  MenuControlMode menuControlMode;
  uint8_t previousButtonPin;
  uint8_t nextButtonPin;
  uint8_t encoderAPin;
  uint8_t encoderBPin;
  uint8_t encoderSwitchPin;
  uint8_t qtEncoderInterruptPin;
};

struct DeviceSettings {
  bool holdToEditEnabled;
  uint8_t buttonScreenSeconds;
  MidiOutputMode midiOutputMode;
};

struct PresetToggleConfig {
  bool enabled;
  uint16_t primaryPresetId;
  uint16_t alternatePresetId;
  uint8_t switchNumber;
};

struct DeviceConfigPayload {
  char deviceName[DEVICE_NAME_LENGTH];
  HardwareConfig hardware;
  DeviceSettings settings;
  PresetToggleConfig presetToggle;
  uint16_t activePresetId;
  uint8_t profileCount;
  DeviceProfileConfig profiles[MAX_DEVICE_PROFILES];
  uint8_t presetCount;
  PedalPresetConfig presets[MAX_PRESETS];
};

struct StoredDeviceConfigHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t payloadSize;
  uint32_t checksum;
};
#pragma pack(pop)

class DeviceConfigStore {
 public:
  bool begin();
  bool load(DeviceConfigPayload &config);
  bool save(DeviceConfigPayload &config);
  void loadDefaults(DeviceConfigPayload &config) const;

  static PedalPresetConfig *activePreset(DeviceConfigPayload &config);
  static const PedalPresetConfig *activePreset(const DeviceConfigPayload &config);
  static PedalPresetConfig *findPreset(DeviceConfigPayload &config, uint16_t id);
  static const PedalPresetConfig *findPreset(const DeviceConfigPayload &config, uint16_t id);
  static DeviceProfileConfig *findProfile(DeviceConfigPayload &config, uint16_t id);
  static const DeviceProfileConfig *findProfile(const DeviceConfigPayload &config, uint16_t id);
  static AssignmentConfig *findAssignment(DeviceProfileConfig &profile, uint16_t id);
  static const AssignmentConfig *findAssignment(const DeviceProfileConfig &profile, uint16_t id);
  static const AssignmentConfig *findAssignment(const DeviceConfigPayload &config,
                                                const ButtonAssignmentRef &reference,
                                                const DeviceProfileConfig **profile = nullptr);
  static bool validate(DeviceConfigPayload &config);
  static void loadDefaultHardware(HardwareConfig &hardware);

 private:
  static uint32_t checksum(const DeviceConfigPayload &config);
  static void copyText(char *destination, size_t destinationSize, const char *source);
  static void loadDefaultProfile(DeviceProfileConfig &profile);
  static void loadDefaultPreset(PedalPresetConfig &preset);
  static void addDefaultAssignment(DeviceProfileConfig &profile,
                                   uint16_t id,
                                   const char *label,
                                   uint8_t controlChange,
                                   uint8_t valueOn);
  static void migrateLegacyButtons(const DeviceProfileConfig &profile,
                                   PedalPresetConfig &preset,
                                   uint8_t legacySignature,
                                   const uint8_t legacyPresetIndices[LEGACY_BUTTON_COUNT]);
};
