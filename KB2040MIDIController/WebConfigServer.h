#pragma once

#include <Arduino.h>

#include "DeviceConfig.h"

class WebConfigServer {
 public:
  WebConfigServer(DeviceConfigStore &store, DeviceConfigPayload &config);

  bool begin();
  void process();
  bool isRunning() const;

 private:
  DeviceConfigStore &_store;
  DeviceConfigPayload &_config;
  bool _running = false;
  unsigned long _rebootAt = 0;
  unsigned long _bootloaderAt = 0;

  void configureRoutes();
  void handleIndex();
  void handleGetConfig();
  void handleGetDefaults();
  void handlePutConfig();
  void handleReboot();
  void handleBootloader();
  void handleNotFound();

  static void serializeConfig(const DeviceConfigPayload &config, String &output);
  static bool parseConfig(const String &body, DeviceConfigPayload &candidate, String &error);
  static const char *messageTypeName(MidiMessageType type);
  static bool parseMessageType(const char *value, MidiMessageType &type);
  static const char *programChangeDisplayModeName(ProgramChangeDisplayMode mode);
  static bool parseProgramChangeDisplayMode(const char *value, ProgramChangeDisplayMode &mode);
  static const char *typeName(AssignmentType type);
  static bool parseType(const char *value, AssignmentType &type);
};
