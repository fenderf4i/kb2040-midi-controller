#include "WebConfigServer.h"

#include <ArduinoJson.h>
#include <NCMEthernetlwIP.h>
#include <USB.h>
#include <WebServer.h>
#include <pico/bootrom.h>
#include <pico/unique_id.h>
#include <string.h>

#include "WebConfigIndex.h"
#include "src/networking/dhserver.h"
#include "src/networking/dnserver.h"

namespace {
NCMEthernetlwIP usbNetwork;
WebServer httpServer(80);

constexpr uint8_t NETWORK_PREFIX_A = 192;
constexpr uint8_t NETWORK_PREFIX_B = 168;
constexpr uint8_t NETWORK_PREFIX_C = 7;
constexpr uint8_t DEVICE_ADDRESS = 1;

dhcp_entry_t dhcpEntries[3]{};
dhcp_config_t dhcpConfig{};
ip_addr_t dnsBindAddress{};
DeviceConfigPayload webConfigScratch{};

bool resolveDeviceName(const char *, ip4_addr_t *address) {
  IP4_ADDR(address, NETWORK_PREFIX_A, NETWORK_PREFIX_B, NETWORK_PREFIX_C, DEVICE_ADDRESS);
  return true;
}

void copyJsonText(char *destination, size_t destinationSize, const char *value) {
  if (destinationSize == 0) return;
  strncpy(destination, value != nullptr ? value : "", destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

bool jsonIntegerInRange(JsonVariantConst value, int minimum, int maximum) {
  if (!value.is<int>()) return false;
  const int parsed = value.as<int>();
  return parsed >= minimum && parsed <= maximum;
}

const char *menuModeName(MenuControlMode mode) {
  switch (mode) {
    case MenuControlMode::Encoder:
      return "encoder";
    case MenuControlMode::StemmaQtEncoder:
      return "stemmaQtEncoder";
    case MenuControlMode::TwoButton:
    default:
      return "twoButton";
  }
}

const char *outputModeName(MidiOutputMode mode) {
  switch (mode) {
    case MidiOutputMode::UsbOnly:
      return "usb";
    case MidiOutputMode::DinOnly:
      return "din";
    case MidiOutputMode::UsbAndDin:
    default:
      return "both";
  }
}

bool parseMenuMode(const char *value, MenuControlMode &mode) {
  if (strcmp(value, "encoder") == 0) {
    mode = MenuControlMode::Encoder;
    return true;
  }
  if (strcmp(value, "stemmaQtEncoder") == 0) {
    mode = MenuControlMode::StemmaQtEncoder;
    return true;
  }
  if (strcmp(value, "twoButton") == 0) {
    mode = MenuControlMode::TwoButton;
    return true;
  }
  return false;
}

bool parseOutputMode(const char *value, MidiOutputMode &mode) {
  if (strcmp(value, "usb") == 0) {
    mode = MidiOutputMode::UsbOnly;
    return true;
  }
  if (strcmp(value, "din") == 0) {
    mode = MidiOutputMode::DinOnly;
    return true;
  }
  if (strcmp(value, "both") == 0) {
    mode = MidiOutputMode::UsbAndDin;
    return true;
  }
  return false;
}

void formatPinName(uint8_t pin, char *output, size_t outputSize) {
  if (pin >= A0 && pin <= A3) {
    snprintf(output, outputSize, "A%u", pin - A0);
  } else {
    snprintf(output, outputSize, "D%u", pin);
  }
}

bool isConfigurableInputPin(uint8_t pin) {
  return pin == 1 || (pin >= 4 && pin <= 10) || (pin >= A0 && pin <= A3);
}

bool noteHardwarePinUse(bool usedPins[], uint8_t pin, String &error) {
  char pinName[8]{};
  formatPinName(pin, pinName, sizeof(pinName));
  if (!isConfigurableInputPin(pin)) {
    error = "Pin ";
    error += pinName;
    error += " is not available for controls";
    return false;
  }
  if (usedPins[pin]) {
    error = "Pin ";
    error += pinName;
    error += " is assigned to more than one active control";
    return false;
  }
  usedPins[pin] = true;
  return true;
}

bool validateHardwarePins(const HardwareConfig &hardware, String &error) {
  for (uint8_t i = 0; i < MAX_STOMP_SWITCH_COUNT; ++i) {
    if (!isConfigurableInputPin(hardware.buttonPins[i])) {
      char pinName[8]{};
      formatPinName(hardware.buttonPins[i], pinName, sizeof(pinName));
      error = "Switch pin ";
      error += pinName;
      error += " is not available";
      return false;
    }
  }
  if (!isConfigurableInputPin(hardware.previousButtonPin) ||
      !isConfigurableInputPin(hardware.nextButtonPin) ||
      !isConfigurableInputPin(hardware.encoderAPin) ||
      !isConfigurableInputPin(hardware.encoderBPin) ||
      !isConfigurableInputPin(hardware.encoderSwitchPin) ||
      !isConfigurableInputPin(hardware.qtEncoderInterruptPin)) {
    error = "Menu control pin is not available";
    return false;
  }

  bool usedPins[A3 + 1]{};
  for (uint8_t i = 0; i < hardware.buttonCount; ++i) {
    if (!noteHardwarePinUse(usedPins, hardware.buttonPins[i], error)) return false;
  }
  if (hardware.menuControlMode == MenuControlMode::TwoButton) {
    return noteHardwarePinUse(usedPins, hardware.previousButtonPin, error) &&
           noteHardwarePinUse(usedPins, hardware.nextButtonPin, error);
  }
  const bool encoderPinsValid = noteHardwarePinUse(usedPins, hardware.encoderAPin, error) &&
                                noteHardwarePinUse(usedPins, hardware.encoderBPin, error) &&
                                noteHardwarePinUse(usedPins, hardware.encoderSwitchPin, error);
  if (!encoderPinsValid) return false;
  if (hardware.menuControlMode == MenuControlMode::StemmaQtEncoder) {
    return noteHardwarePinUse(usedPins, hardware.qtEncoderInterruptPin, error);
  }
  return true;
}

void appendJsonString(String &output, const char *value) {
  output += '"';
  for (const char *cursor = value != nullptr ? value : ""; *cursor != '\0'; ++cursor) {
    const char character = *cursor;
    if (character == '"' || character == '\\') {
      output += '\\';
      output += character;
    } else if (character == '\n') {
      output += "\\n";
    } else if (character == '\r') {
      output += "\\r";
    } else if (character == '\t') {
      output += "\\t";
    } else if (static_cast<uint8_t>(character) >= 0x20) {
      output += character;
    }
  }
  output += '"';
}

void appendJsonNumber(String &output, unsigned int value) {
  output += String(value);
}

void appendJsonBool(String &output, bool value) {
  output += value ? "true" : "false";
}
}

WebConfigServer::WebConfigServer(DeviceConfigStore &store, DeviceConfigPayload &config)
    : _store(store), _config(config) {}

bool WebConfigServer::begin() {
  const IPAddress localIp(NETWORK_PREFIX_A, NETWORK_PREFIX_B, NETWORK_PREFIX_C, DEVICE_ADDRESS);
  const IPAddress noGateway(0, 0, 0, 0);
  const IPAddress subnet(255, 255, 255, 0);

  pico_unique_board_id_t boardId{};
  pico_get_unique_board_id(&boardId);
  uint8_t macAddress[6];
  memcpy(macAddress, &boardId.id[2], sizeof(macAddress));
  macAddress[0] = (macAddress[0] & 0xFC) | 0x02;

  USB.setManufacturer("KB2040 Stompbox");
  USB.setProduct("KB2040 Web Config");
  usbNetwork.config(localIp, noGateway, subnet, localIp);
  if (!usbNetwork.begin(macAddress)) {
    return false;
  }
  usbNetwork.setDefault(true);

  for (uint8_t i = 0; i < 3; ++i) {
    memset(dhcpEntries[i].mac, 0, sizeof(dhcpEntries[i].mac));
    IP4_ADDR(&dhcpEntries[i].addr,
             NETWORK_PREFIX_A,
             NETWORK_PREFIX_B,
             NETWORK_PREFIX_C,
             2 + i);
    dhcpEntries[i].lease = 24 * 60 * 60;
  }
  IP4_ADDR(&dhcpConfig.router, 0, 0, 0, 0);
  dhcpConfig.port = 67;
  IP4_ADDR(&dhcpConfig.dns,
           NETWORK_PREFIX_A,
           NETWORK_PREFIX_B,
           NETWORK_PREFIX_C,
           DEVICE_ADDRESS);
  dhcpConfig.domain = "stompbox";
  dhcpConfig.num_entry = 3;
  dhcpConfig.entries = dhcpEntries;
  dhserv_init(&dhcpConfig);

  IP_ADDR4(&dnsBindAddress,
           NETWORK_PREFIX_A,
           NETWORK_PREFIX_B,
           NETWORK_PREFIX_C,
           DEVICE_ADDRESS);
  dnserv_init(&dnsBindAddress, 53, resolveDeviceName);

  configureRoutes();
  httpServer.begin();
  _running = true;
  return true;
}

void WebConfigServer::process() {
  if (!_running) return;
  httpServer.handleClient();
  if (_rebootAt != 0 && static_cast<long>(millis() - _rebootAt) >= 0) {
    rp2040.reboot();
  }
  if (_bootloaderAt != 0 && static_cast<long>(millis() - _bootloaderAt) >= 0) {
    reset_usb_boot(0, 0);
  }
}

bool WebConfigServer::isRunning() const {
  return _running;
}

void WebConfigServer::configureRoutes() {
  httpServer.on("/", HTTP_GET, [this]() { handleIndex(); });
  httpServer.on("/index.html", HTTP_GET, [this]() { handleIndex(); });
  httpServer.on("/default-config.json", HTTP_GET, [this]() { handleGetDefaults(); });
  httpServer.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
  httpServer.on("/api/config", HTTP_PUT, [this]() { handlePutConfig(); });
  httpServer.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  httpServer.on("/api/bootloader", HTTP_POST, [this]() { handleBootloader(); });
  httpServer.onNotFound([this]() { handleNotFound(); });
}

void WebConfigServer::handleIndex() {
  httpServer.sendHeader("Content-Encoding", "gzip");
  httpServer.sendHeader("Cache-Control", "no-store");
  httpServer.send_P(200,
                    PSTR("text/html; charset=utf-8"),
                    reinterpret_cast<PGM_P>(WEB_CONFIG_INDEX_GZ),
                    WEB_CONFIG_INDEX_GZ_SIZE);
}

void WebConfigServer::handleGetConfig() {
  String response;
  serializeConfig(_config, response);
  httpServer.send(200, "application/json", response);
}

void WebConfigServer::handleGetDefaults() {
  _store.loadDefaults(webConfigScratch);
  String response;
  serializeConfig(webConfigScratch, response);
  httpServer.send(200, "application/json", response);
}

void WebConfigServer::handlePutConfig() {
  String error;
  if (!parseConfig(httpServer.arg("plain"), webConfigScratch, error)) {
    JsonDocument response;
    response["ok"] = false;
    response["error"] = error;
    String body;
    serializeJson(response, body);
    httpServer.send(400, "application/json", body);
    return;
  }

  if (!_store.save(webConfigScratch)) {
    httpServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Flash write failed\"}");
    return;
  }

  _config = webConfigScratch;
  httpServer.send(200, "application/json", "{\"ok\":true}");
}

void WebConfigServer::handleReboot() {
  httpServer.send(200, "application/json", "{\"ok\":true,\"mode\":\"normal\"}");
  _rebootAt = millis() + 500;
}

void WebConfigServer::handleBootloader() {
  httpServer.send(200, "application/json", "{\"ok\":true,\"mode\":\"bootloader\"}");
  _bootloaderAt = millis() + 750;
}

void WebConfigServer::handleNotFound() {
  if (httpServer.method() == HTTP_GET) {
    handleIndex();
    return;
  }
  httpServer.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
}

void WebConfigServer::serializeConfig(const DeviceConfigPayload &config, String &output) {
  output = "";
  output.reserve(4096 + config.profileCount * 4096 + config.presetCount * 1024);

  output += "{\"schemaVersion\":12,\"device\":{\"name\":";
  appendJsonString(output, config.deviceName);
  output += ",\"activePresetId\":";
  appendJsonNumber(output, config.activePresetId);
  output += "},\"hardware\":{\"buttonCount\":";
  appendJsonNumber(output, config.hardware.buttonCount);
  output += ",\"buttonPins\":[";
  for (uint8_t i = 0; i < MAX_STOMP_SWITCH_COUNT; ++i) {
    if (i > 0) output += ',';
    appendJsonNumber(output, config.hardware.buttonPins[i]);
  }
  output += "],\"menuControls\":{\"mode\":";
  appendJsonString(output, menuModeName(config.hardware.menuControlMode));
  output += ",\"previousButtonPin\":";
  appendJsonNumber(output, config.hardware.previousButtonPin);
  output += ",\"nextButtonPin\":";
  appendJsonNumber(output, config.hardware.nextButtonPin);
  output += ",\"encoderAPin\":";
  appendJsonNumber(output, config.hardware.encoderAPin);
  output += ",\"encoderBPin\":";
  appendJsonNumber(output, config.hardware.encoderBPin);
  output += ",\"encoderSwitchPin\":";
  appendJsonNumber(output, config.hardware.encoderSwitchPin);
  output += ",\"qtEncoderInterruptPin\":";
  appendJsonNumber(output, config.hardware.qtEncoderInterruptPin);
  output += "}},\"settings\":{\"holdToEditEnabled\":";
  appendJsonBool(output, config.settings.holdToEditEnabled);
  output += ",\"buttonScreenSeconds\":";
  appendJsonNumber(output, config.settings.buttonScreenSeconds);
  output += ",\"midiOutputMode\":";
  appendJsonString(output, outputModeName(config.settings.midiOutputMode));
  output += "},\"presetToggle\":{\"enabled\":";
  appendJsonBool(output, config.presetToggle.enabled);
  output += ",\"primaryPresetId\":";
  appendJsonNumber(output, config.presetToggle.primaryPresetId);
  output += ",\"alternatePresetId\":";
  appendJsonNumber(output, config.presetToggle.alternatePresetId);
  output += ",\"switchNumber\":";
  appendJsonNumber(output, config.presetToggle.switchNumber);
  output += "},\"deviceProfiles\":[";

  for (uint8_t p = 0; p < config.profileCount; ++p) {
    if (p > 0) output += ',';
    const DeviceProfileConfig &profile = config.profiles[p];
    output += "{\"id\":";
    appendJsonNumber(output, profile.id);
    output += ",\"name\":";
    appendJsonString(output, profile.name);
    output += ",\"midiChannel\":";
    appendJsonNumber(output, profile.midiChannel);
    output += ",\"programChangeDisplayMode\":";
    appendJsonString(output, programChangeDisplayModeName(profile.programChangeDisplayMode));
    output += ",\"commands\":[";
    for (uint8_t i = 0; i < profile.assignmentCount; ++i) {
      if (i > 0) output += ',';
      const AssignmentConfig &source = profile.assignments[i];
      output += "{\"id\":";
      appendJsonNumber(output, source.id);
      output += ",\"messageType\":";
      appendJsonString(output, messageTypeName(source.messageType));
      output += ",\"type\":";
      appendJsonString(output, typeName(source.type));
      output += ",\"label\":";
      appendJsonString(output, source.label);
      output += ",\"number\":";
      appendJsonNumber(output, source.controlChange);
      output += ",\"valueOn\":";
      appendJsonNumber(output, source.valueOn);
      output += ",\"valueOff\":";
      appendJsonNumber(output, source.valueOff);
      output += '}';
    }
    output += "]}";
  }

  output += "],\"presets\":[";
  for (uint8_t p = 0; p < config.presetCount; ++p) {
    if (p > 0) output += ',';
    const PedalPresetConfig &sourcePreset = config.presets[p];
    output += "{\"id\":";
    appendJsonNumber(output, sourcePreset.id);
    output += ",\"name\":";
    appendJsonString(output, sourcePreset.name);
    output += ",\"buttons\":[";

    for (uint8_t i = 0; i < config.hardware.buttonCount; ++i) {
      if (i > 0) output += ',';
      char pinName[4]{};
      formatPinName(config.hardware.buttonPins[i], pinName, sizeof(pinName));
      output += "{\"number\":";
      appendJsonNumber(output, i + 1);
      output += ",\"pin\":";
      appendJsonString(output, pinName);
      output += ",\"deviceProfileId\":";
      appendJsonNumber(output, sourcePreset.buttonAssignments[i].profileId);
      output += ",\"commandId\":";
      appendJsonNumber(output, sourcePreset.buttonAssignments[i].assignmentId);
      output += '}';
    }

    output += "]}";
  }

  output += "]}";
}

bool WebConfigServer::parseConfig(const String &body,
                                  DeviceConfigPayload &candidate,
                                  String &error) {
  JsonDocument document;
  const DeserializationError parseError = deserializeJson(document, body);
  if (parseError) {
    error = "Invalid JSON";
    return false;
  }

  JsonObjectConst device = document["device"];
  JsonObjectConst hardware = document["hardware"];
  JsonObjectConst settings = document["settings"];
  JsonObjectConst presetToggle = document["presetToggle"];
  JsonArrayConst profiles = document["deviceProfiles"];
  JsonArrayConst presets = document["presets"];
  if (device.isNull() || profiles.isNull() || presets.isNull()) {
    error = "Missing configuration section";
    return false;
  }
  if (profiles.size() == 0 || profiles.size() > MAX_DEVICE_PROFILES ||
      presets.size() == 0 || presets.size() > MAX_PRESETS ||
      !jsonIntegerInRange(device["activePresetId"], 1, 65535)) {
    error = "Invalid profile, preset, or active preset count";
    return false;
  }

  memset(&candidate, 0, sizeof(candidate));
  copyJsonText(candidate.deviceName,
               sizeof(candidate.deviceName),
               device["name"] | "MIDI Controller");
  candidate.activePresetId = device["activePresetId"].as<uint16_t>();
  candidate.profileCount = profiles.size();
  candidate.presetCount = presets.size();
  DeviceConfigStore::loadDefaultHardware(candidate.hardware);
  candidate.settings.holdToEditEnabled = false;
  candidate.settings.buttonScreenSeconds = 5;
  candidate.settings.midiOutputMode = MidiOutputMode::UsbAndDin;
  candidate.presetToggle = {false, UNASSIGNED_ID, UNASSIGNED_ID, 1};

  if (!hardware.isNull()) {
    JsonArrayConst buttonPins = hardware["buttonPins"];
    JsonObjectConst menuControls = hardware["menuControls"];
    if (!jsonIntegerInRange(hardware["buttonCount"], 1, MAX_STOMP_SWITCH_COUNT) ||
        buttonPins.isNull() || buttonPins.size() < MAX_STOMP_SWITCH_COUNT ||
        buttonPins.size() > MAX_BUTTON_COUNT ||
        menuControls.isNull()) {
      error = "Invalid hardware configuration";
      return false;
    }
    candidate.hardware.buttonCount = hardware["buttonCount"].as<uint8_t>();
    for (uint8_t i = 0; i < buttonPins.size(); ++i) {
      JsonVariantConst pin = buttonPins[i];
      if (!jsonIntegerInRange(pin, 0, A3)) {
        error = "Invalid button pin";
        return false;
      }
      candidate.hardware.buttonPins[i] = pin.as<uint8_t>();
    }
    if (!parseMenuMode(menuControls["mode"] | "", candidate.hardware.menuControlMode) ||
        !jsonIntegerInRange(menuControls["previousButtonPin"], 0, A3) ||
        !jsonIntegerInRange(menuControls["nextButtonPin"], 0, A3) ||
        !jsonIntegerInRange(menuControls["encoderAPin"], 0, A3) ||
        !jsonIntegerInRange(menuControls["encoderBPin"], 0, A3) ||
        !jsonIntegerInRange(menuControls["encoderSwitchPin"], 0, A3)) {
      error = "Invalid menu control configuration";
      return false;
    }
    candidate.hardware.previousButtonPin = menuControls["previousButtonPin"].as<uint8_t>();
    candidate.hardware.nextButtonPin = menuControls["nextButtonPin"].as<uint8_t>();
    candidate.hardware.encoderAPin = menuControls["encoderAPin"].as<uint8_t>();
    candidate.hardware.encoderBPin = menuControls["encoderBPin"].as<uint8_t>();
    candidate.hardware.encoderSwitchPin = menuControls["encoderSwitchPin"].as<uint8_t>();
    if (!menuControls["qtEncoderInterruptPin"].isNull()) {
      if (!jsonIntegerInRange(menuControls["qtEncoderInterruptPin"], 0, A3)) {
        error = "Invalid menu control configuration";
        return false;
      }
      candidate.hardware.qtEncoderInterruptPin = menuControls["qtEncoderInterruptPin"].as<uint8_t>();
    }
    if (!validateHardwarePins(candidate.hardware, error)) {
      return false;
    }
  }
  if (!settings.isNull()) {
    MidiOutputMode outputMode = MidiOutputMode::UsbAndDin;
    if (!jsonIntegerInRange(settings["buttonScreenSeconds"], 1, 30) ||
        !parseOutputMode(settings["midiOutputMode"] | "both", outputMode)) {
      error = "Invalid device settings";
      return false;
    }
    candidate.settings.holdToEditEnabled = settings["holdToEditEnabled"] | false;
    candidate.settings.buttonScreenSeconds = settings["buttonScreenSeconds"].as<uint8_t>();
    candidate.settings.midiOutputMode = outputMode;
  }
  if (!presetToggle.isNull()) {
    candidate.presetToggle.enabled = presetToggle["enabled"] | false;
    if (candidate.presetToggle.enabled) {
      if (!jsonIntegerInRange(presetToggle["primaryPresetId"], 1, 65535) ||
          !jsonIntegerInRange(presetToggle["alternatePresetId"], 1, 65535) ||
          !jsonIntegerInRange(presetToggle["switchNumber"], 1, MAX_STOMP_SWITCH_COUNT)) {
        error = "Invalid preset toggle";
        return false;
      }
      candidate.presetToggle.primaryPresetId = presetToggle["primaryPresetId"].as<uint16_t>();
      candidate.presetToggle.alternatePresetId = presetToggle["alternatePresetId"].as<uint16_t>();
      candidate.presetToggle.switchNumber = presetToggle["switchNumber"].as<uint8_t>();
    } else if (jsonIntegerInRange(presetToggle["switchNumber"], 1, MAX_STOMP_SWITCH_COUNT)) {
      candidate.presetToggle.switchNumber = presetToggle["switchNumber"].as<uint8_t>();
    }
  }

  uint8_t profileIndex = 0;
  for (JsonObjectConst sourceProfile : profiles) {
    JsonArrayConst commands = sourceProfile["commands"];
    if (commands.isNull() || commands.size() == 0 || commands.size() > MAX_ASSIGNMENTS ||
        !jsonIntegerInRange(sourceProfile["id"], 1, 65535) ||
        !jsonIntegerInRange(sourceProfile["midiChannel"], 1, 16)) {
      error = "Invalid device profile configuration";
      return false;
    }

    DeviceProfileConfig &targetProfile = candidate.profiles[profileIndex++];
    targetProfile.id = sourceProfile["id"].as<uint16_t>();
    targetProfile.midiChannel = sourceProfile["midiChannel"].as<uint8_t>();
    if (!parseProgramChangeDisplayMode(sourceProfile["programChangeDisplayMode"] | "zeroBased",
                                       targetProfile.programChangeDisplayMode)) {
      error = "Invalid Program Change display mode";
      return false;
    }
    targetProfile.assignmentCount = commands.size();
    copyJsonText(targetProfile.name, sizeof(targetProfile.name), sourceProfile["name"] | "Device");

    uint8_t assignmentIndex = 0;
    for (JsonObjectConst source : commands) {
      JsonVariantConst number = source["number"].isNull() ? source["cc"] : source["number"];
      if (!jsonIntegerInRange(source["id"], 1, 65535) ||
          !jsonIntegerInRange(number, 0, 127) ||
          !jsonIntegerInRange(source["valueOn"], 0, 127) ||
          !jsonIntegerInRange(source["valueOff"], 0, 127)) {
        error = "Command values are out of range";
        return false;
      }
      AssignmentConfig &target = targetProfile.assignments[assignmentIndex++];
      target.id = source["id"].as<uint16_t>();
      if (!parseMessageType(source["messageType"] | "cc", target.messageType)) {
        error = "Unknown MIDI message type";
        return false;
      }
      if (!parseType(source["type"] | "", target.type)) {
        error = "Unknown command type";
        return false;
      }
      target.controlChange = number.as<uint8_t>();
      target.valueOn = source["valueOn"].as<uint8_t>();
      target.valueOff = source["valueOff"].as<uint8_t>();
      copyJsonText(target.label, sizeof(target.label), source["label"] | "Command");
    }
  }

  uint8_t presetIndex = 0;
  for (JsonObjectConst sourcePreset : presets) {
    JsonArrayConst buttons = sourcePreset["buttons"];
    if (buttons.isNull() || buttons.size() != candidate.hardware.buttonCount ||
        !jsonIntegerInRange(sourcePreset["id"], 1, 65535)) {
      error = "Invalid preset configuration";
      return false;
    }

    PedalPresetConfig &targetPreset = candidate.presets[presetIndex++];
    targetPreset.id = sourcePreset["id"].as<uint16_t>();
    copyJsonText(targetPreset.name, sizeof(targetPreset.name), sourcePreset["name"] | "Preset");

    uint8_t buttonIndex = 0;
    for (JsonObjectConst source : buttons) {
      JsonVariantConst commandId = source["commandId"].isNull()
                                       ? source["assignmentId"]
                                       : source["commandId"];
      if (!jsonIntegerInRange(source["deviceProfileId"], 0, 65535) ||
          !jsonIntegerInRange(commandId, 0, 65535)) {
        error = "Invalid button command";
        return false;
      }
      targetPreset.buttonAssignments[buttonIndex++] = {
          source["deviceProfileId"].as<uint16_t>(),
          commandId.as<uint16_t>()};
    }
  }

  if (!DeviceConfigStore::validate(candidate)) {
    error = "Configuration validation failed";
    return false;
  }
  return true;
}

const char *WebConfigServer::messageTypeName(MidiMessageType type) {
  switch (type) {
    case MidiMessageType::ProgramChange:
      return "pc";
    case MidiMessageType::ControlChange:
    default:
      return "cc";
  }
}

bool WebConfigServer::parseMessageType(const char *value, MidiMessageType &type) {
  if (strcmp(value, "cc") == 0 || strcmp(value, "controlChange") == 0) {
    type = MidiMessageType::ControlChange;
    return true;
  }
  if (strcmp(value, "pc") == 0 || strcmp(value, "programChange") == 0) {
    type = MidiMessageType::ProgramChange;
    return true;
  }
  return false;
}

const char *WebConfigServer::programChangeDisplayModeName(ProgramChangeDisplayMode mode) {
  switch (mode) {
    case ProgramChangeDisplayMode::OneBased:
      return "oneBased";
    case ProgramChangeDisplayMode::ZeroBased:
    default:
      return "zeroBased";
  }
}

bool WebConfigServer::parseProgramChangeDisplayMode(const char *value,
                                                    ProgramChangeDisplayMode &mode) {
  if (strcmp(value, "zeroBased") == 0 || strcmp(value, "0-127") == 0) {
    mode = ProgramChangeDisplayMode::ZeroBased;
    return true;
  }
  if (strcmp(value, "oneBased") == 0 || strcmp(value, "1-128") == 0) {
    mode = ProgramChangeDisplayMode::OneBased;
    return true;
  }
  return false;
}

const char *WebConfigServer::typeName(AssignmentType type) {
  switch (type) {
    case AssignmentType::Toggle:
      return "toggle";
    case AssignmentType::Momentary:
      return "momentary";
    case AssignmentType::Fixed:
    default:
      return "fixed";
  }
}

bool WebConfigServer::parseType(const char *value, AssignmentType &type) {
  if (strcmp(value, "fixed") == 0) {
    type = AssignmentType::Fixed;
    return true;
  }
  if (strcmp(value, "toggle") == 0) {
    type = AssignmentType::Toggle;
    return true;
  }
  if (strcmp(value, "momentary") == 0) {
    type = AssignmentType::Momentary;
    return true;
  }
  return false;
}
