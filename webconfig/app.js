const MAX_COMMANDS = 40;
const MAX_DEVICE_PROFILES = 8;
const MAX_PRESETS = 8;
const MAX_BUTTONS = 6;
const MAX_LABEL_LENGTH = 20;
const MAX_PRESET_NAME_LENGTH = 24;
const MAX_DEVICE_NAME_LENGTH = 32;
const THEME_STORAGE_KEY = "kb2040-webconfig-theme";
const DRAFT_STORAGE_KEY = "kb2040-webconfig-draft";
const DEVICE_CONFIG_LOAD_ATTEMPTS = 8;
const DEVICE_CONFIG_RETRY_DELAY_MS = 1250;
const SAVE_BUTTON_LABEL = "Save & restart";
const THEMES = new Set(["light", "dark", "black"]);
const OUTPUT_MODES = new Set(["both", "usb", "din"]);
const PC_DISPLAY_MODES = new Set(["zeroBased", "oneBased"]);
const MENU_MODES = new Set(["twoButton", "encoder", "stemmaQtEncoder"]);
const DEFAULT_BUTTON_PINS = [9, 8, 7, 6, 5, 4];
const commandTypes = {
  fixed: "Fixed value",
  toggle: "Toggle",
  momentary: "Momentary"
};
const messageTypes = {
  cc: "Control Change",
  pc: "Program Change"
};
const pcDisplayModes = {
  zeroBased: "0-127",
  oneBased: "1-128"
};
const pinOptions = [
  ["D1", 1],
  ["D4", 4],
  ["D5", 5],
  ["D6", 6],
  ["D7", 7],
  ["D8", 8],
  ["D9", 9],
  ["D10", 10],
  ["A0", 26],
  ["A1", 27],
  ["A2", 28],
  ["A3", 29]
];
const allowedPinValues = new Set(pinOptions.map(([, value]) => value));
const pinLabel = (pin) => pinOptions.find(([, value]) => value === Number(pin))?.[0] || `GPIO ${pin}`;
const pinSelectOptions = (selectedPin) => pinOptions.map(([label, value]) => `<option value="${value}"${value === Number(selectedPin) ? " selected" : ""}>${label}</option>`).join("");
const normalizePin = (value, fallback) => {
  const pin = clampNumber(value, 0, 29, fallback);
  return allowedPinValues.has(pin) ? pin : fallback;
};

const HX_STOMP_COMMANDS = [
  { id: 1, type: "fixed", label: "Emulates FS1", cc: 49, valueOn: 127, valueOff: 0 },
  { id: 2, type: "fixed", label: "Emulates FS2", cc: 50, valueOn: 127, valueOff: 0 },
  { id: 3, type: "fixed", label: "Emulates FS3", cc: 51, valueOn: 127, valueOff: 0 },
  { id: 4, type: "fixed", label: "Emulates FS4", cc: 52, valueOn: 127, valueOff: 0 },
  { id: 5, type: "fixed", label: "Emulates FS5", cc: 53, valueOn: 127, valueOff: 0 },
  { id: 6, type: "fixed", label: "Looper Record", cc: 60, valueOn: 127, valueOff: 0 },
  { id: 7, type: "fixed", label: "Looper Overdub", cc: 60, valueOn: 63, valueOff: 0 },
  { id: 8, type: "fixed", label: "Looper Play", cc: 61, valueOn: 127, valueOff: 0 },
  { id: 9, type: "fixed", label: "Looper Stop", cc: 61, valueOn: 63, valueOff: 0 },
  { id: 10, type: "fixed", label: "Looper Play Once", cc: 62, valueOn: 127, valueOff: 0 },
  { id: 11, type: "fixed", label: "Looper Undo/Redo", cc: 63, valueOn: 127, valueOff: 0 },
  { id: 12, type: "fixed", label: "Looper Forward", cc: 65, valueOn: 63, valueOff: 0 },
  { id: 13, type: "fixed", label: "Looper Reverse", cc: 65, valueOn: 127, valueOff: 0 },
  { id: 14, type: "fixed", label: "Looper Full Speed", cc: 66, valueOn: 63, valueOff: 0 },
  { id: 15, type: "fixed", label: "Looper Half Speed", cc: 66, valueOn: 127, valueOff: 0 },
  { id: 16, type: "fixed", label: "Tap Tempo", cc: 64, valueOn: 127, valueOff: 0 },
  { id: 17, type: "fixed", label: "Tuner On/Off", cc: 68, valueOn: 127, valueOff: 0 },
  { id: 18, type: "fixed", label: "Snapshot 1", cc: 69, valueOn: 0, valueOff: 0 },
  { id: 19, type: "fixed", label: "Snapshot 2", cc: 69, valueOn: 1, valueOff: 0 },
  { id: 20, type: "fixed", label: "Snapshot 3", cc: 69, valueOn: 2, valueOff: 0 },
  { id: 21, type: "fixed", label: "Next Snapshot", cc: 69, valueOn: 8, valueOff: 0 },
  { id: 22, type: "fixed", label: "Previous Snapshot", cc: 69, valueOn: 9, valueOff: 0 },
  { id: 23, type: "fixed", label: "Bypass On", cc: 70, valueOn: 63, valueOff: 0 },
  { id: 24, type: "fixed", label: "Bypass Off", cc: 70, valueOn: 127, valueOff: 0 },
  { id: 25, type: "fixed", label: "Footswitch Stomp", cc: 71, valueOn: 0, valueOff: 0 },
  { id: 26, type: "fixed", label: "Footswitch Scroll", cc: 71, valueOn: 1, valueOff: 0 },
  { id: 27, type: "fixed", label: "Footswitch Preset", cc: 71, valueOn: 2, valueOff: 0 },
  { id: 28, type: "fixed", label: "Footswitch Snapshot", cc: 71, valueOn: 3, valueOff: 0 },
  { id: 29, type: "fixed", label: "Next Footswitch", cc: 71, valueOn: 4, valueOff: 0 },
  { id: 30, type: "fixed", label: "Previous Footswitch", cc: 71, valueOn: 5, valueOff: 0 },
  { id: 31, type: "fixed", label: "Previous Preset", cc: 72, valueOn: 63, valueOff: 0 },
  { id: 32, type: "fixed", label: "Next Preset", cc: 72, valueOn: 127, valueOff: 0 },
  { id: 33, type: "fixed", label: "Custom CC 25", cc: 25, valueOn: 127, valueOff: 0 }
];

const EMBEDDED_DEFAULT_CONFIG = {
  schemaVersion: 12,
  device: { name: "MIDI Controller", activePresetId: 1 },
  hardware: {
    buttonCount: 6,
    buttonPins: [9, 8, 7, 6, 5, 4],
    menuControls: {
      mode: "stemmaQtEncoder",
      previousButtonPin: 10,
      nextButtonPin: 26,
      encoderAPin: 27,
      encoderBPin: 28,
      encoderSwitchPin: 29,
      qtEncoderInterruptPin: 10
    }
  },
  settings: {
    holdToEditEnabled: false,
    buttonScreenSeconds: 5,
    midiOutputMode: "both"
  },
  presetToggle: {
    enabled: false,
    primaryPresetId: 0,
    alternatePresetId: 0,
    switchNumber: 1
  },
  deviceProfiles: [{
    id: 1,
    name: "HX Stomp",
    midiChannel: 1,
    programChangeDisplayMode: "zeroBased",
    commands: HX_STOMP_COMMANDS
  }],
  presets: [{
    id: 1,
    name: "HX Stomp",
    buttons: [
      { number: 1, pin: "D9", deviceProfileId: 1, commandId: 31 },
      { number: 2, pin: "D8", deviceProfileId: 1, commandId: 32 },
      { number: 3, pin: "D7", deviceProfileId: 1, commandId: 17 },
      { number: 4, pin: "D6", deviceProfileId: 1, commandId: 18 },
      { number: 5, pin: "D5", deviceProfileId: 1, commandId: 19 },
      { number: 6, pin: "D4", deviceProfileId: 1, commandId: 20 }
    ]
  }]
};

let config = null;
let defaults = null;
let connected = false;
let editingPresetId = 0;
let editingProfileId = 0;
let toastTimer = null;
let saveInProgress = false;

function applyTheme(theme, persist = true) {
  const selectedTheme = THEMES.has(theme) ? theme : "light";
  document.documentElement.dataset.theme = selectedTheme;
  document.querySelector('meta[name="theme-color"]').content = {
    light: "#f4f6f5",
    dark: "#171a1c",
    black: "#000000"
  }[selectedTheme];
  document.querySelectorAll('input[name="theme"]').forEach((input) => {
    input.checked = input.value === selectedTheme;
  });
  if (persist) localStorage.setItem(THEME_STORAGE_KEY, selectedTheme);
}

const clone = (value) => JSON.parse(JSON.stringify(value));
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const clampNumber = (value, minimum, maximum, fallback = minimum) => {
  const parsed = Number.parseInt(value, 10);
  return Number.isFinite(parsed) ? Math.min(maximum, Math.max(minimum, parsed)) : fallback;
};

async function fetchWithTimeout(url, options = {}, timeoutMs = 6000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    clearTimeout(timer);
  }
}

function showToast(message) {
  const toast = document.querySelector("#toast");
  toast.textContent = message;
  toast.classList.add("is-visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove("is-visible"), 2600);
}

function setConnection(isConnected, label) {
  connected = isConnected;
  document.querySelector("#connection-dot").classList.toggle("is-connected", isConnected);
  document.querySelector("#connection-status").textContent = label;
  if (config) renderValidation();
}

function channelOptions(selectedChannel) {
  return Array.from({ length: 16 }, (_, index) => {
    const channel = index + 1;
    return `<option value="${channel}"${channel === Number(selectedChannel) ? " selected" : ""}>${channel}</option>`;
  }).join("");
}

function normalizeCommand(command, index) {
  return {
    id: clampNumber(command?.id, 1, 65535, index + 1),
    messageType: messageTypes[command?.messageType] ? command.messageType : "cc",
    type: commandTypes[command?.type] ? command.type : "fixed",
    label: String(command?.label || command?.name || `Command ${index + 1}`).slice(0, MAX_LABEL_LENGTH),
    number: clampNumber(command?.number ?? command?.cc, 0, 127, 0),
    valueOn: clampNumber(command?.valueOn, 0, 127, 127),
    valueOff: clampNumber(command?.valueOff, 0, 127, 0)
  };
}

function normalizeProfile(profile, index) {
  const sourceCommands = Array.isArray(profile?.commands) ? profile.commands : profile?.assignments;
  const pcDisplayMode = PC_DISPLAY_MODES.has(profile?.programChangeDisplayMode)
    ? profile.programChangeDisplayMode
    : PC_DISPLAY_MODES.has(profile?.pcDisplayMode)
    ? profile.pcDisplayMode
    : "zeroBased";
  const normalized = {
    id: clampNumber(profile?.id, 1, 65535, index + 1),
    name: String(profile?.name || `Device ${index + 1}`).slice(0, MAX_PRESET_NAME_LENGTH),
    midiChannel: clampNumber(profile?.midiChannel, 1, 16, 1),
    programChangeDisplayMode: pcDisplayMode,
    commands: Array.isArray(sourceCommands)
      ? sourceCommands.slice(0, MAX_COMMANDS).map((command, commandIndex) => normalizeCommand(command, commandIndex))
      : []
  };
  if (!normalized.commands.length) normalized.commands = [normalizeCommand({}, 0)];
  const usedIds = new Set();
  normalized.commands.forEach((command) => {
    while (usedIds.has(command.id)) command.id += 1;
    usedIds.add(command.id);
  });
  return normalized;
}

function normalizeHardware(source = {}) {
  source = source || {};
  const buttonPins = Array.isArray(source.buttonPins) ? source.buttonPins : [];
  const sourceMenuMode = source.menuControls?.mode;
  return {
    buttonCount: clampNumber(source.buttonCount, 1, MAX_BUTTONS, 6),
    buttonPins: Array.from({ length: MAX_BUTTONS }, (_, index) =>
      normalizePin(buttonPins[index], DEFAULT_BUTTON_PINS[index])),
    menuControls: {
      mode: MENU_MODES.has(sourceMenuMode) ? sourceMenuMode : "stemmaQtEncoder",
      previousButtonPin: normalizePin(source.menuControls?.previousButtonPin, 10),
      nextButtonPin: normalizePin(source.menuControls?.nextButtonPin, 26),
      encoderAPin: normalizePin(source.menuControls?.encoderAPin, 27),
      encoderBPin: normalizePin(source.menuControls?.encoderBPin, 28),
      encoderSwitchPin: normalizePin(source.menuControls?.encoderSwitchPin, 29),
      qtEncoderInterruptPin: normalizePin(source.menuControls?.qtEncoderInterruptPin, 10)
    }
  };
}

function normalizeSettings(source = {}) {
  const outputMode = OUTPUT_MODES.has(source.midiOutputMode) ? source.midiOutputMode : "both";
  return {
    holdToEditEnabled: Boolean(source.holdToEditEnabled),
    buttonScreenSeconds: clampNumber(source.buttonScreenSeconds, 1, 30, 5),
    midiOutputMode: outputMode
  };
}

function validCommandRefs(profiles) {
  const refs = new Map();
  profiles.forEach((profile) => {
    refs.set(profile.id, new Set(profile.commands.map((command) => command.id)));
  });
  return refs;
}

function isValidCommandRef(refs, profileId, commandId) {
  return profileId > 0 && commandId > 0 && refs.get(profileId)?.has(commandId);
}

function migrateLegacyConfig(input) {
  const source = clone(input || {});
  if (Array.isArray(source.deviceProfiles)) return source;

  if (!Array.isArray(source.presets) && Array.isArray(source.assignments)) {
    source.presets = [{
      id: 1,
      name: "HX Stomp",
      midiChannel: source.device?.midiChannel || 1,
      assignments: source.assignments,
      buttons: source.buttons
    }];
    source.device = { name: source.device?.name || "MIDI Controller", activePresetId: 1 };
  }

  if (!Array.isArray(source.presets)) return source;

  const profiles = [];
  let nextProfileId = 1;
  const profileKeyToId = new Map();

  function getProfileForCommand(preset, command) {
    const channel = clampNumber(command?.midiChannel, 1, 16, preset.midiChannel || 1);
    const baseName = String(preset.name || "MIDI Device").slice(0, MAX_PRESET_NAME_LENGTH);
    const name = channel === clampNumber(preset.midiChannel, 1, 16, channel)
      ? baseName
      : `${baseName} Ch ${channel}`.slice(0, MAX_PRESET_NAME_LENGTH);
    const key = `${name}|${channel}`;
    if (profileKeyToId.has(key)) return profiles.find((profile) => profile.id === profileKeyToId.get(key));
    if (profiles.length >= MAX_DEVICE_PROFILES) return null;
    const profile = { id: nextProfileId++, name, midiChannel: channel, commands: [] };
    profiles.push(profile);
    profileKeyToId.set(key, profile.id);
    return profile;
  }

  const presets = source.presets.slice(0, MAX_PRESETS).map((preset, presetIndex) => {
    const legacyAssignments = Array.isArray(preset.assignments) ? preset.assignments.slice(0, MAX_COMMANDS) : [];
    legacyAssignments.forEach((assignment, assignmentIndex) => {
      const profile = getProfileForCommand(preset, assignment);
      if (!profile) return;
      if (!profile.commands.some((command) => Number(command.id) === Number(assignment.id))) {
        profile.commands.push(normalizeCommand(assignment, assignmentIndex));
      }
    });
    const buttons = Array.isArray(preset.buttons) ? preset.buttons : [];
    return {
      id: preset.id || presetIndex + 1,
      name: preset.name || `Preset ${presetIndex + 1}`,
      buttons: buttons.map((button, buttonIndex) => {
        const assignment = legacyAssignments.find((item) => Number(item.id) === Number(button.assignmentId));
        const profile = assignment ? getProfileForCommand(preset, assignment) : null;
        return {
          number: buttonIndex + 1,
          pin: button.pin,
          deviceProfileId: profile?.id || 0,
          commandId: assignment ? Number(assignment.id) : 0
        };
      })
    };
  });

  source.deviceProfiles = profiles.length ? profiles : clone(EMBEDDED_DEFAULT_CONFIG.deviceProfiles);
  source.presets = presets;
  return source;
}

function normalizePreset(source, index, hardware, profiles) {
  const refs = validCommandRefs(profiles);
  const preset = {
    id: clampNumber(source?.id, 1, 65535, index + 1),
    name: String(source?.name || `Preset ${index + 1}`).slice(0, MAX_PRESET_NAME_LENGTH),
    buttons: []
  };
  const sourceButtons = Array.isArray(source?.buttons) ? source.buttons : [];
  preset.buttons = Array.from({ length: hardware.buttonCount }, (_, buttonIndex) => {
    const button = sourceButtons[buttonIndex] || {};
    const fallbackProfileId = profiles[0]?.id || 0;
    const profileId = clampNumber(button.deviceProfileId ?? button.profileId, 0, 65535, 0);
    const commandId = clampNumber(button.commandId ?? button.assignmentId, 0, 65535, 0);
    const legacyCommandProfile = profileId === 0 && commandId > 0 && refs.get(fallbackProfileId)?.has(commandId)
      ? fallbackProfileId
      : profileId;
    const valid = isValidCommandRef(refs, legacyCommandProfile, commandId);
    return {
      number: buttonIndex + 1,
      pin: pinLabel(hardware.buttonPins[buttonIndex]),
      deviceProfileId: valid ? legacyCommandProfile : 0,
      commandId: valid ? commandId : 0
    };
  });
  return preset;
}

function normalizePresetToggle(source = {}, presets = [], hardware = normalizeHardware()) {
  source = source || {};
  const presetIds = new Set(presets.map((preset) => preset.id));
  const firstPresetId = presets[0]?.id || 0;
  const secondPresetId = presets.find((preset) => preset.id !== firstPresetId)?.id || 0;
  let primaryPresetId = clampNumber(source.primaryPresetId, 0, 65535, firstPresetId);
  let alternatePresetId = clampNumber(source.alternatePresetId, 0, 65535, secondPresetId);
  if (!presetIds.has(primaryPresetId)) primaryPresetId = firstPresetId;
  if (!presetIds.has(alternatePresetId) || alternatePresetId === primaryPresetId) {
    alternatePresetId = presets.find((preset) => preset.id !== primaryPresetId)?.id || 0;
  }
  const switchNumber = clampNumber(source.switchNumber, 1, hardware.buttonCount, Math.min(6, hardware.buttonCount));
  const enabled = Boolean(source.enabled) &&
    presets.length >= 2 &&
    primaryPresetId > 0 &&
    alternatePresetId > 0 &&
    primaryPresetId !== alternatePresetId;
  return { enabled, primaryPresetId, alternatePresetId, switchNumber };
}

function normalizeConfig(candidate) {
  const source = migrateLegacyConfig(candidate || defaults);
  const hardware = normalizeHardware(source.hardware);
  const profiles = Array.isArray(source.deviceProfiles)
    ? source.deviceProfiles.slice(0, MAX_DEVICE_PROFILES).map((profile, index) => normalizeProfile(profile, index))
    : [];
  if (!profiles.length) profiles.push(normalizeProfile(EMBEDDED_DEFAULT_CONFIG.deviceProfiles[0], 0));
  const usedProfileIds = new Set();
  profiles.forEach((profile) => {
    while (usedProfileIds.has(profile.id)) profile.id += 1;
    usedProfileIds.add(profile.id);
  });

  const result = {
    schemaVersion: 12,
    device: {
      name: String(source.device?.name || "MIDI Controller").slice(0, MAX_DEVICE_NAME_LENGTH),
      activePresetId: clampNumber(source.device?.activePresetId, 1, 65535, 1)
    },
    hardware,
    settings: normalizeSettings(source.settings),
    deviceProfiles: profiles,
    presets: Array.isArray(source.presets)
      ? source.presets.slice(0, MAX_PRESETS).map((preset, index) => normalizePreset(preset, index, hardware, profiles))
      : []
  };
  if (!result.presets.length) result.presets = [normalizePreset(EMBEDDED_DEFAULT_CONFIG.presets[0], 0, hardware, profiles)];
  const usedPresetIds = new Set();
  result.presets.forEach((preset) => {
    while (usedPresetIds.has(preset.id)) preset.id += 1;
    usedPresetIds.add(preset.id);
  });
  if (!usedPresetIds.has(result.device.activePresetId)) result.device.activePresetId = result.presets[0].id;
  result.presetToggle = normalizePresetToggle(source.presetToggle, result.presets, hardware);
  return result;
}

function activePinRoles(hardware = config?.hardware) {
  if (!hardware) return [];
  const roles = [];
  for (let index = 0; index < hardware.buttonCount; index += 1) {
    roles.push({ id: `button-${index}`, label: `Switch ${index + 1}`, pin: Number(hardware.buttonPins[index]) });
  }
  if (hardware.menuControls.mode === "twoButton") {
    roles.push({ id: "previousButtonPin", label: "Previous", pin: Number(hardware.menuControls.previousButtonPin) });
    roles.push({ id: "nextButtonPin", label: "Next", pin: Number(hardware.menuControls.nextButtonPin) });
  } else {
    roles.push({ id: "encoderAPin", label: "GPIO encoder A", pin: Number(hardware.menuControls.encoderAPin) });
    roles.push({ id: "encoderBPin", label: "GPIO encoder B", pin: Number(hardware.menuControls.encoderBPin) });
    roles.push({ id: "encoderSwitchPin", label: "GPIO encoder switch", pin: Number(hardware.menuControls.encoderSwitchPin) });
    if (hardware.menuControls.mode === "stemmaQtEncoder") {
      roles.push({ id: "qtEncoderInterruptPin", label: "QT encoder interrupt", pin: Number(hardware.menuControls.qtEncoderInterruptPin) });
    }
  }
  return roles;
}

function validateConfigDraft(candidate = config) {
  const blocking = [];
  const invalidPinRoleIds = new Set();
  if (!candidate?.hardware) return { blocking: ["Configuration is not loaded"], invalidPinRoleIds };

  const hardware = candidate.hardware;
  hardware.buttonPins.slice(0, MAX_BUTTONS).forEach((pin, index) => {
    if (!allowedPinValues.has(Number(pin))) {
      blocking.push(`Switch ${index + 1} uses unavailable ${pinLabel(pin)}`);
      if (index < hardware.buttonCount) invalidPinRoleIds.add(`button-${index}`);
    }
  });

  [
    ["previousButtonPin", "Previous", hardware.menuControls.previousButtonPin],
    ["nextButtonPin", "Next", hardware.menuControls.nextButtonPin],
    ["encoderAPin", "GPIO encoder A", hardware.menuControls.encoderAPin],
    ["encoderBPin", "GPIO encoder B", hardware.menuControls.encoderBPin],
    ["encoderSwitchPin", "GPIO encoder switch", hardware.menuControls.encoderSwitchPin],
    ["qtEncoderInterruptPin", "QT encoder interrupt", hardware.menuControls.qtEncoderInterruptPin]
  ].forEach(([id, label, pin]) => {
    if (!allowedPinValues.has(Number(pin))) {
      blocking.push(`${label} uses unavailable ${pinLabel(pin)}`);
      invalidPinRoleIds.add(id);
    }
  });

  const usedPins = new Map();
  activePinRoles(hardware).forEach((role) => {
    const existing = usedPins.get(role.pin);
    if (existing) {
      blocking.push(`${pinLabel(role.pin)} is assigned to ${existing.label} and ${role.label}`);
      invalidPinRoleIds.add(existing.id);
      invalidPinRoleIds.add(role.id);
    } else {
      usedPins.set(role.pin, role);
    }
  });

  const seenProfiles = new Map();
  candidate.deviceProfiles.forEach((profile) => {
    if (seenProfiles.has(profile.id)) blocking.push(`Duplicate Device Profile ID ${profile.id}`);
    seenProfiles.set(profile.id, profile);
    const seenCommands = new Map();
    profile.commands.forEach((command) => {
      if (seenCommands.has(command.id)) blocking.push(`${profile.name} has duplicate command ID ${command.id}`);
      seenCommands.set(command.id, command);
    });
  });

  if (candidate.presetToggle?.enabled) {
    const presetIds = new Set(candidate.presets.map((preset) => preset.id));
    if (candidate.presets.length < 2) {
      blocking.push("Preset page toggle needs at least two presets");
    }
    if (!presetIds.has(candidate.presetToggle.primaryPresetId)) {
      blocking.push("Preset page toggle primary preset is missing");
    }
    if (!presetIds.has(candidate.presetToggle.alternatePresetId)) {
      blocking.push("Preset page toggle alternate preset is missing");
    }
    if (candidate.presetToggle.primaryPresetId === candidate.presetToggle.alternatePresetId) {
      blocking.push("Preset page toggle needs two different presets");
    }
    if (candidate.presetToggle.switchNumber < 1 ||
        candidate.presetToggle.switchNumber > candidate.hardware.buttonCount) {
      blocking.push("Preset page toggle switch is outside the active switch count");
    }
  }

  return { blocking, invalidPinRoleIds };
}

function currentPreset() {
  return config.presets.find((preset) => preset.id === editingPresetId) || config.presets[0];
}

function presetById(id) {
  return config.presets.find((preset) => preset.id === Number(id));
}

function ensurePresetToggleValid() {
  config.presetToggle = normalizePresetToggle(config.presetToggle, config.presets, config.hardware);
}

function currentProfile() {
  return config.deviceProfiles.find((profile) => profile.id === editingProfileId) || config.deviceProfiles[0];
}

function findProfile(id) {
  return config.deviceProfiles.find((profile) => profile.id === Number(id));
}

function findCommand(profileId, commandId) {
  return findProfile(profileId)?.commands.find((command) => command.id === Number(commandId));
}

function presetSelectOptions(selectedId) {
  return config.presets.map((preset) =>
    `<option value="${preset.id}"${preset.id === Number(selectedId) ? " selected" : ""}>${escapeHtml(preset.name)}</option>`).join("");
}

function switchNumberOptions(selectedNumber) {
  return Array.from({ length: config.hardware.buttonCount }, (_, index) => {
    const number = index + 1;
    return `<option value="${number}"${number === Number(selectedNumber) ? " selected" : ""}>Switch ${number}</option>`;
  }).join("");
}

function presetToggleTargetForPreset(preset) {
  if (!config.presetToggle.enabled || !preset) return null;
  if (preset.id === config.presetToggle.primaryPresetId) {
    return presetById(config.presetToggle.alternatePresetId);
  }
  if (preset.id === config.presetToggle.alternatePresetId) {
    return presetById(config.presetToggle.primaryPresetId);
  }
  return null;
}

function isPresetToggleButton(preset, buttonNumber) {
  return Number(buttonNumber) === config.presetToggle.switchNumber &&
    Boolean(presetToggleTargetForPreset(preset));
}

function pcDisplayValue(profile, rawProgram) {
  return profile?.programChangeDisplayMode === "oneBased" ? Number(rawProgram) + 1 : Number(rawProgram);
}

function rawPcValueFromDisplay(profile, displayProgram, fallbackRaw) {
  const minimum = profile?.programChangeDisplayMode === "oneBased" ? 1 : 0;
  const maximum = profile?.programChangeDisplayMode === "oneBased" ? 128 : 127;
  const fallback = pcDisplayValue(profile, fallbackRaw);
  const displayValue = clampNumber(displayProgram, minimum, maximum, fallback);
  return profile?.programChangeDisplayMode === "oneBased" ? displayValue - 1 : displayValue;
}

function nextPresetId() {
  return config.presets.reduce((highest, preset) => Math.max(highest, preset.id), 0) + 1;
}

function nextProfileId() {
  return config.deviceProfiles.reduce((highest, profile) => Math.max(highest, profile.id), 0) + 1;
}

function nextCommandId(profile = currentProfile()) {
  return profile.commands.reduce((highest, command) => Math.max(highest, command.id), 0) + 1;
}

function createCommand(profile = currentProfile()) {
  return { id: nextCommandId(profile), messageType: "cc", type: "fixed", label: "New command", number: 0, valueOn: 127, valueOff: 0 };
}

function createProfile() {
  return normalizeProfile({
    id: nextProfileId(),
    name: `Device ${config.deviceProfiles.length + 1}`,
    midiChannel: 1,
    programChangeDisplayMode: "zeroBased",
    commands: [{ id: 1, messageType: "cc", type: "fixed", label: "New command", number: 0, valueOn: 127, valueOff: 0 }]
  }, config.deviceProfiles.length);
}

function createPreset() {
  const id = nextPresetId();
  return normalizePreset({
    id,
    name: `Preset ${config.presets.length + 1}`,
    buttons: Array.from({ length: config.hardware.buttonCount }, (_, index) => ({
      number: index + 1,
      pin: pinLabel(config.hardware.buttonPins[index]),
      deviceProfileId: 0,
      commandId: 0
    }))
  }, config.presets.length, config.hardware, config.deviceProfiles);
}

function persistDraft() {
  localStorage.setItem(DRAFT_STORAGE_KEY, JSON.stringify(config));
}

function renderAll() {
  ensurePresetToggleValid();
  renderPresetContext();
  renderPresets();
  renderPresetToggle();
  renderAssignments();
  renderButtons();
  renderDevice();
  renderValidation();
}

function renderPresetContext() {
  const selector = document.querySelector("#preset-selector");
  selector.innerHTML = config.presets.map((preset) =>
    `<option value="${preset.id}"${preset.id === editingPresetId ? " selected" : ""}>${escapeHtml(preset.name)}</option>`).join("");
  const active = config.presets.find((preset) => preset.id === config.device.activePresetId);
  document.querySelector("#active-preset-label").textContent = `Active: ${active?.name || "Preset"}`;
  document.querySelector("#set-active-preset").disabled = editingPresetId === config.device.activePresetId;
}

function renderPresets() {
  document.querySelector("#preset-count").textContent = `${config.presets.length} of ${MAX_PRESETS} presets`;
  document.querySelector("#add-preset").disabled = config.presets.length >= MAX_PRESETS;
  const list = document.querySelector("#preset-list");
  list.replaceChildren();
  config.presets.forEach((preset) => {
    const row = document.createElement("article");
    row.className = `preset-row${preset.id === config.device.activePresetId ? " is-active" : ""}`;
    row.dataset.presetId = preset.id;
    row.innerHTML = `
      <label class="field"><span>Preset name${preset.id === config.device.activePresetId ? " / Active" : ""}</span><input class="preset-name" maxlength="${MAX_PRESET_NAME_LENGTH}" value="${escapeHtml(preset.name)}"></label>
      <div class="preset-row-actions">
        <button class="row-icon text-row-icon edit-preset" type="button" title="Edit preset" aria-label="Edit ${escapeHtml(preset.name)}">Edit</button>
        <button class="row-icon remove-preset" type="button" title="Remove preset" aria-label="Remove ${escapeHtml(preset.name)}"${config.presets.length === 1 ? " disabled" : ""}>-</button>
      </div>`;
    list.append(row);
  });
}

function renderPresetToggle() {
  ensurePresetToggleValid();
  document.querySelectorAll('input[name="preset-toggle-enabled"]').forEach((input) => {
    input.checked = input.value === (config.presetToggle.enabled ? "on" : "off");
    input.disabled = config.presets.length < 2;
  });
  const disabled = !config.presetToggle.enabled || config.presets.length < 2;
  const primary = document.querySelector("#preset-toggle-primary");
  const alternate = document.querySelector("#preset-toggle-alternate");
  const toggleSwitch = document.querySelector("#preset-toggle-switch");
  primary.innerHTML = presetSelectOptions(config.presetToggle.primaryPresetId);
  alternate.innerHTML = presetSelectOptions(config.presetToggle.alternatePresetId);
  toggleSwitch.innerHTML = switchNumberOptions(config.presetToggle.switchNumber);
  primary.disabled = disabled;
  alternate.disabled = disabled;
  toggleSwitch.disabled = disabled;
}

function renderAssignments() {
  const profile = currentProfile();
  document.querySelector("#profile-count").textContent = `${config.deviceProfiles.length} of ${MAX_DEVICE_PROFILES} profiles`;
  document.querySelector("#add-profile").disabled = config.deviceProfiles.length >= MAX_DEVICE_PROFILES;
  document.querySelector("#command-count").textContent = `${profile.commands.length} of ${MAX_COMMANDS} commands in ${profile.name}`;
  document.querySelector("#add-command").disabled = profile.commands.length >= MAX_COMMANDS;
  document.querySelector("#profile-name").value = profile.name;
  document.querySelector("#profile-channel").innerHTML = channelOptions(profile.midiChannel);
  document.querySelectorAll('input[name="profile-pc-display-mode"]').forEach((input) => {
    input.checked = input.value === profile.programChangeDisplayMode;
  });
  document.querySelector("#remove-profile").disabled = config.deviceProfiles.length === 1;

  const profileList = document.querySelector("#profile-list");
  profileList.replaceChildren();
  config.deviceProfiles.forEach((item) => {
    const button = document.createElement("button");
    button.className = `profile-list-item${item.id === profile.id ? " is-active" : ""}`;
    button.type = "button";
    button.dataset.profileId = item.id;
    button.innerHTML = `<span>${escapeHtml(item.name)}</span><small>Ch ${item.midiChannel} / PC ${pcDisplayModes[item.programChangeDisplayMode]} / ${item.commands.length} commands</small>`;
    profileList.append(button);
  });

  const rows = document.querySelector("#command-rows");
  rows.replaceChildren();
  profile.commands.forEach((command) => {
    const row = document.createElement("tr");
    row.dataset.commandId = command.id;
    const isProgramChange = command.messageType === "pc";
    const pcNumbering = isProgramChange && profile.programChangeDisplayMode === "oneBased";
    const numberMinimum = pcNumbering ? 1 : 0;
    const numberMaximum = pcNumbering ? 128 : 127;
    const numberValue = isProgramChange ? pcDisplayValue(profile, command.number) : command.number;
    const secondValue = command.type === "fixed" ? "" : `<label class="value-field-wrap"><span>${command.type === "toggle" ? "Off" : "Release"}</span><input class="row-field command-value-off" type="number" min="0" max="127" value="${command.valueOff}"></label>`;
    const valuesCell = isProgramChange
      ? `<span class="muted-cell">Not used</span>`
      : `<div class="value-fields${command.type === "fixed" ? " is-single" : ""}"><label class="value-field-wrap"><span>${command.type === "momentary" ? "Press" : command.type === "toggle" ? "On" : "Send"}</span><input class="row-field command-value-on" type="number" min="0" max="127" value="${command.valueOn}"></label>${secondValue}</div>`;
    row.innerHTML = `
      <td data-label="Message"><select class="row-field command-message-type">${Object.entries(messageTypes).map(([value, label]) => `<option value="${value}"${command.messageType === value ? " selected" : ""}>${label}</option>`).join("")}</select></td>
      <td data-label="Type"><select class="row-field command-type"${isProgramChange ? " disabled" : ""}>${Object.entries(commandTypes).map(([value, label]) => `<option value="${value}"${command.type === value ? " selected" : ""}>${label}</option>`).join("")}</select></td>
      <td data-label="Display label"><input class="row-field command-label" maxlength="${MAX_LABEL_LENGTH}" value="${escapeHtml(command.label)}"></td>
      <td data-label="Number"><label class="value-field-wrap"><span>${isProgramChange ? "Program" : "CC"}</span><input class="row-field command-number" type="number" min="${numberMinimum}" max="${numberMaximum}" value="${numberValue}"></label></td>
      <td data-label="Values">${valuesCell}</td>
      <td data-label="Remove"><div class="row-actions"><button class="row-icon remove-command" type="button" title="Remove command"${profile.commands.length === 1 ? " disabled" : ""}>-</button></div></td>`;
    rows.append(row);
  });
}

function renderButtons() {
  const preset = currentPreset();
  const grid = document.querySelector("#button-grid");
  grid.replaceChildren();
  preset.buttons.forEach((button) => {
    const profile = findProfile(button.deviceProfileId);
    const command = findCommand(button.deviceProfileId, button.commandId);
    const toggleTarget = isPresetToggleButton(preset, button.number) ? presetToggleTargetForPreset(preset) : null;
    const element = document.createElement("article");
    element.className = "button-config";
    element.dataset.buttonNumber = button.number;
    element.innerHTML = toggleTarget ? `
      <div class="button-title"><span class="button-number">Switch ${button.number}</span><span class="pin-label">${escapeHtml(button.pin)}</span></div>
      <label class="field compact-field"><span>Device Profile</span><select class="button-profile" disabled><option>Preset page toggle</option></select></label>
      <label class="field compact-field"><span>Command</span><select class="button-command" disabled><option>Switch to ${escapeHtml(toggleTarget.name)}</option></select></label>
      <div class="button-summary">Preset page toggle / no MIDI sent</div>` : `
      <div class="button-title"><span class="button-number">Switch ${button.number}</span><span class="pin-label">${escapeHtml(button.pin)}</span></div>
      <label class="field compact-field"><span>Device Profile</span><select class="button-profile"><option value="0">Unassigned</option>${config.deviceProfiles.map((item) => `<option value="${item.id}"${item.id === button.deviceProfileId ? " selected" : ""}>${escapeHtml(item.name)}</option>`).join("")}</select></label>
      <label class="field compact-field"><span>Command</span><select class="button-command"${profile ? "" : " disabled"}><option value="0">Unassigned</option>${profile ? profile.commands.map((item) => `<option value="${item.id}"${item.id === button.commandId ? " selected" : ""}>${escapeHtml(item.label)}</option>`).join("") : ""}</select></label>
      <div class="button-summary">${profile && command ? commandSummary(profile, command) : "No MIDI message"}</div>`;
    grid.append(element);
  });
}

function renderDevice() {
  document.querySelector("#device-name").value = config.device.name;
  const displayName = config.device.name || "MIDI Controller";
  document.querySelector("#device-title").textContent = displayName;
  document.title = displayName;
  document.querySelector("#button-count").value = config.hardware.buttonCount;
  document.querySelector("#remove-stomp-switch").disabled = config.hardware.buttonCount <= 1;
  document.querySelector("#add-stomp-switch").disabled = config.hardware.buttonCount >= MAX_BUTTONS;
  const hardwareButtons = document.querySelector("#hardware-buttons");
  hardwareButtons.replaceChildren();
  for (let index = 0; index < config.hardware.buttonCount; index += 1) {
    const element = document.createElement("article");
    element.className = "button-config";
    element.dataset.hardwareButton = String(index);
    element.innerHTML = `<div class="button-title"><span class="button-number">Switch ${index + 1}</span><span class="pin-label">${pinLabel(config.hardware.buttonPins[index])}</span></div><label class="field compact-field"><span>GPIO pin</span><select class="hardware-button-pin" data-pin-role="button-${index}">${pinSelectOptions(config.hardware.buttonPins[index])}</select></label>`;
    hardwareButtons.append(element);
  }
  document.querySelectorAll('input[name="menu-mode"]').forEach((input) => {
    input.checked = input.value === config.hardware.menuControls.mode;
  });
  document.querySelectorAll(".menu-two-button-field").forEach((field) => {
    field.classList.toggle("is-hidden", config.hardware.menuControls.mode !== "twoButton");
  });
  document.querySelectorAll(".menu-encoder-field").forEach((field) => {
    field.classList.toggle("is-hidden", !["encoder", "stemmaQtEncoder"].includes(config.hardware.menuControls.mode));
  });
  document.querySelectorAll(".menu-qt-encoder-field").forEach((field) => {
    field.classList.toggle("is-hidden", config.hardware.menuControls.mode !== "stemmaQtEncoder");
  });
  document.querySelector("#previous-button-pin").innerHTML = pinSelectOptions(config.hardware.menuControls.previousButtonPin);
  document.querySelector("#next-button-pin").innerHTML = pinSelectOptions(config.hardware.menuControls.nextButtonPin);
  document.querySelector("#encoder-a-pin").innerHTML = pinSelectOptions(config.hardware.menuControls.encoderAPin);
  document.querySelector("#encoder-b-pin").innerHTML = pinSelectOptions(config.hardware.menuControls.encoderBPin);
  document.querySelector("#encoder-switch-pin").innerHTML = pinSelectOptions(config.hardware.menuControls.encoderSwitchPin);
  document.querySelector("#qt-encoder-interrupt-pin").innerHTML = pinSelectOptions(config.hardware.menuControls.qtEncoderInterruptPin);
  document.querySelectorAll('input[name="hold-edit-mode"]').forEach((input) => {
    input.checked = input.value === (config.settings.holdToEditEnabled ? "on" : "off");
  });
  document.querySelector("#button-screen-seconds").value = config.settings.buttonScreenSeconds;
  document.querySelectorAll('input[name="midi-output-mode"]').forEach((input) => {
    input.checked = input.value === config.settings.midiOutputMode;
  });
}

function renderValidation() {
  const panel = document.querySelector("#validation-panel");
  const saveButton = document.querySelector("#save-button");
  document.querySelectorAll(".pin-conflict").forEach((element) => element.classList.remove("pin-conflict"));
  if (!config) {
    panel.hidden = true;
    saveButton.disabled = true;
    return;
  }

  const report = validateConfigDraft(config);
  report.invalidPinRoleIds.forEach((id) => {
    document.querySelector(`[data-pin-role="${id}"]`)?.classList.add("pin-conflict");
  });

  if (!report.blocking.length) {
    panel.hidden = true;
    panel.replaceChildren();
    saveButton.disabled = saveInProgress;
    return;
  }

  panel.hidden = false;
  panel.innerHTML = `<strong>Configuration needs attention</strong><ul>${report.blocking.map((issue) => `<li>${escapeHtml(issue)}</li>`).join("")}</ul>`;
  saveButton.disabled = true;
}

function resizePresetButtons() {
  const refs = validCommandRefs(config.deviceProfiles);
  config.presets.forEach((preset) => {
    preset.buttons = Array.from({ length: config.hardware.buttonCount }, (_, index) => {
      const existing = preset.buttons[index] || {};
      const profileId = Number(existing.deviceProfileId);
      const commandId = Number(existing.commandId);
      const valid = isValidCommandRef(refs, profileId, commandId);
      return {
        number: index + 1,
        pin: pinLabel(config.hardware.buttonPins[index]),
        deviceProfileId: valid ? profileId : 0,
        commandId: valid ? commandId : 0
      };
    });
  });
}

function clearCommandReferences(profileId, commandId = null) {
  config.presets.forEach((preset) => {
    preset.buttons.forEach((button) => {
      const profileMatches = button.deviceProfileId === profileId;
      const commandMatches = commandId === null || button.commandId === commandId;
      if (profileMatches && commandMatches) {
        button.deviceProfileId = 0;
        button.commandId = 0;
      }
    });
  });
}

function setButtonCount(nextCount) {
  config.hardware.buttonCount = clampNumber(nextCount, 1, MAX_BUTTONS, config.hardware.buttonCount);
  resizePresetButtons();
  ensurePresetToggleValid();
  persistDraft();
  renderAll();
}

function commandSummary(profile, command) {
  if (command.messageType === "pc") return `Ch ${profile.midiChannel} / PC ${pcDisplayValue(profile, command.number)} / Program Change`;
  if (command.type === "toggle") return `Ch ${profile.midiChannel} / CC ${command.number} / ${command.valueOn} - ${command.valueOff} / Toggle`;
  if (command.type === "momentary") return `Ch ${profile.midiChannel} / CC ${command.number} / ${command.valueOn} - ${command.valueOff} / Momentary`;
  return `Ch ${profile.midiChannel} / CC ${command.number} / ${command.valueOn} / Fixed value`;
}

function escapeHtml(value) {
  return String(value).replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&#039;");
}

function updateCommandFromRow(row) {
  const profile = currentProfile();
  const command = profile.commands.find((item) => item.id === Number(row.dataset.commandId));
  if (!command) return;
  command.messageType = row.querySelector(".command-message-type").value;
  command.type = command.messageType === "pc" ? "fixed" : row.querySelector(".command-type").value;
  command.label = row.querySelector(".command-label").value.slice(0, MAX_LABEL_LENGTH);
  command.number = command.messageType === "pc"
    ? rawPcValueFromDisplay(profile, row.querySelector(".command-number").value, command.number)
    : clampNumber(row.querySelector(".command-number").value, 0, 127, command.number);
  const valueOn = row.querySelector(".command-value-on");
  if (valueOn) command.valueOn = clampNumber(valueOn.value, 0, 127, command.valueOn);
  const off = row.querySelector(".command-value-off");
  if (off) command.valueOff = clampNumber(off.value, 0, 127, command.valueOff);
}

function exportJson(data, filename, toastMessage) {
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
  const anchor = document.createElement("a");
  anchor.href = URL.createObjectURL(blob);
  anchor.download = filename;
  anchor.click();
  URL.revokeObjectURL(anchor.href);
  showToast(toastMessage);
}

document.querySelector(".tabs").addEventListener("click", (event) => {
  const tab = event.target.closest("[data-tab]");
  if (!tab) return;
  document.querySelectorAll(".tab").forEach((item) => item.classList.toggle("is-active", item === tab));
  document.querySelectorAll(".panel").forEach((panel) => panel.classList.toggle("is-active", panel.dataset.panel === tab.dataset.tab));
});

document.querySelector("#preset-selector").addEventListener("change", (event) => {
  editingPresetId = Number(event.target.value);
  renderAll();
});

document.querySelectorAll('input[name="theme"]').forEach((input) => {
  input.addEventListener("change", () => {
    if (input.checked) applyTheme(input.value);
  });
});

document.querySelector("#set-active-preset").addEventListener("click", () => {
  config.device.activePresetId = editingPresetId;
  persistDraft();
  renderAll();
});

document.querySelector("#add-preset").addEventListener("click", () => {
  if (config.presets.length >= MAX_PRESETS) return;
  const preset = createPreset();
  config.presets.push(preset);
  editingPresetId = preset.id;
  persistDraft();
  renderAll();
});

document.querySelector("#preset-list").addEventListener("input", (event) => {
  const row = event.target.closest("[data-preset-id]");
  const preset = config.presets.find((item) => item.id === Number(row?.dataset.presetId));
  if (!preset) return;
  if (event.target.classList.contains("preset-name")) preset.name = event.target.value.slice(0, MAX_PRESET_NAME_LENGTH);
  persistDraft();
  renderPresetContext();
});

document.querySelector("#preset-list").addEventListener("click", (event) => {
  const row = event.target.closest("[data-preset-id]");
  const index = config.presets.findIndex((item) => item.id === Number(row?.dataset.presetId));
  if (index < 0) return;
  if (event.target.closest(".edit-preset")) {
    editingPresetId = config.presets[index].id;
    renderAll();
  } else if (event.target.closest(".remove-preset") && config.presets.length > 1) {
    const [removed] = config.presets.splice(index, 1);
    if (config.device.activePresetId === removed.id) config.device.activePresetId = config.presets[0].id;
    if (editingPresetId === removed.id) editingPresetId = config.presets[0].id;
    ensurePresetToggleValid();
    persistDraft();
    renderAll();
  }
});

document.querySelectorAll('input[name="preset-toggle-enabled"]').forEach((input) => {
  input.addEventListener("change", () => {
    if (!input.checked) return;
    config.presetToggle.enabled = input.value === "on" && config.presets.length >= 2;
    ensurePresetToggleValid();
    persistDraft();
    renderAll();
  });
});

document.querySelector("#preset-toggle-primary").addEventListener("change", (event) => {
  config.presetToggle.primaryPresetId = Number(event.target.value);
  ensurePresetToggleValid();
  persistDraft();
  renderAll();
});

document.querySelector("#preset-toggle-alternate").addEventListener("change", (event) => {
  config.presetToggle.alternatePresetId = Number(event.target.value);
  ensurePresetToggleValid();
  persistDraft();
  renderAll();
});

document.querySelector("#preset-toggle-switch").addEventListener("change", (event) => {
  config.presetToggle.switchNumber = clampNumber(event.target.value, 1, config.hardware.buttonCount, config.presetToggle.switchNumber);
  ensurePresetToggleValid();
  persistDraft();
  renderAll();
});

document.querySelector("#profile-list").addEventListener("click", (event) => {
  const item = event.target.closest("[data-profile-id]");
  if (!item) return;
  editingProfileId = Number(item.dataset.profileId);
  renderAll();
});

document.querySelector("#add-profile").addEventListener("click", () => {
  if (config.deviceProfiles.length >= MAX_DEVICE_PROFILES) return;
  const profile = createProfile();
  config.deviceProfiles.push(profile);
  editingProfileId = profile.id;
  persistDraft();
  renderAll();
});

document.querySelector("#duplicate-profile").addEventListener("click", () => {
  if (config.deviceProfiles.length >= MAX_DEVICE_PROFILES) return;
  const duplicate = clone(currentProfile());
  duplicate.id = nextProfileId();
  duplicate.name = `${duplicate.name} copy`.slice(0, MAX_PRESET_NAME_LENGTH);
  config.deviceProfiles.push(duplicate);
  editingProfileId = duplicate.id;
  persistDraft();
  renderAll();
});

document.querySelector("#remove-profile").addEventListener("click", () => {
  if (config.deviceProfiles.length <= 1) return;
  const profile = currentProfile();
  if (!window.confirm(`Remove ${profile.name}? Preset buttons using it will become unassigned.`)) return;
  config.deviceProfiles = config.deviceProfiles.filter((item) => item.id !== profile.id);
  clearCommandReferences(profile.id);
  editingProfileId = config.deviceProfiles[0].id;
  persistDraft();
  renderAll();
});

document.querySelector("#profile-name").addEventListener("input", (event) => {
  currentProfile().name = event.target.value.slice(0, MAX_PRESET_NAME_LENGTH);
  persistDraft();
  renderAssignments();
  renderButtons();
});

document.querySelector("#profile-channel").addEventListener("change", (event) => {
  currentProfile().midiChannel = clampNumber(event.target.value, 1, 16, currentProfile().midiChannel);
  persistDraft();
  renderAssignments();
  renderButtons();
});

document.querySelectorAll('input[name="profile-pc-display-mode"]').forEach((input) => {
  input.addEventListener("change", () => {
    if (!input.checked) return;
    currentProfile().programChangeDisplayMode = PC_DISPLAY_MODES.has(input.value) ? input.value : "zeroBased";
    persistDraft();
    renderAssignments();
    renderButtons();
  });
});

document.querySelector("#command-rows").addEventListener("change", (event) => {
  const row = event.target.closest("tr");
  if (!row) return;
  updateCommandFromRow(row);
  persistDraft();
  if (event.target.classList.contains("command-type") ||
      event.target.classList.contains("command-message-type")) renderAssignments();
  renderButtons();
});

document.querySelector("#command-rows").addEventListener("input", (event) => {
  const row = event.target.closest("tr");
  if (row) { updateCommandFromRow(row); persistDraft(); }
});

document.querySelector("#command-rows").addEventListener("click", (event) => {
  const profile = currentProfile();
  const row = event.target.closest("tr");
  const index = profile.commands.findIndex((item) => item.id === Number(row?.dataset.commandId));
  if (index < 0) return;
  if (event.target.closest(".remove-command") && profile.commands.length > 1) {
    const [removed] = profile.commands.splice(index, 1);
    clearCommandReferences(profile.id, removed.id);
  } else return;
  persistDraft();
  renderAll();
});

document.querySelector("#add-command").addEventListener("click", () => {
  const profile = currentProfile();
  if (profile.commands.length >= MAX_COMMANDS) return;
  profile.commands.push(createCommand(profile));
  persistDraft();
  renderAll();
});

document.querySelector("#button-grid").addEventListener("change", (event) => {
  const card = event.target.closest("[data-button-number]");
  const button = currentPreset().buttons.find((item) => item.number === Number(card?.dataset.buttonNumber));
  if (!button) return;
  if (isPresetToggleButton(currentPreset(), button.number)) return;
  if (event.target.classList.contains("button-profile")) {
    button.deviceProfileId = Number(event.target.value);
    button.commandId = 0;
  }
  if (event.target.classList.contains("button-command")) {
    button.commandId = Number(event.target.value);
  }
  persistDraft();
  renderButtons();
});

document.querySelector("#hardware-buttons").addEventListener("change", (event) => {
  const card = event.target.closest("[data-hardware-button]");
  if (!card || !event.target.classList.contains("hardware-button-pin")) return;
  config.hardware.buttonPins[Number(card.dataset.hardwareButton)] = normalizePin(event.target.value, config.hardware.buttonPins[Number(card.dataset.hardwareButton)]);
  resizePresetButtons();
  persistDraft();
  renderAll();
});

const fieldBindings = [
  ["#button-count", "input", (value) => setButtonCount(value)],
  ["#previous-button-pin", "change", (value) => config.hardware.menuControls.previousButtonPin = normalizePin(value, config.hardware.menuControls.previousButtonPin)],
  ["#next-button-pin", "change", (value) => config.hardware.menuControls.nextButtonPin = normalizePin(value, config.hardware.menuControls.nextButtonPin)],
  ["#encoder-a-pin", "change", (value) => config.hardware.menuControls.encoderAPin = normalizePin(value, config.hardware.menuControls.encoderAPin)],
  ["#encoder-b-pin", "change", (value) => config.hardware.menuControls.encoderBPin = normalizePin(value, config.hardware.menuControls.encoderBPin)],
  ["#encoder-switch-pin", "change", (value) => config.hardware.menuControls.encoderSwitchPin = normalizePin(value, config.hardware.menuControls.encoderSwitchPin)],
  ["#qt-encoder-interrupt-pin", "change", (value) => config.hardware.menuControls.qtEncoderInterruptPin = normalizePin(value, config.hardware.menuControls.qtEncoderInterruptPin)],
  ["#button-screen-seconds", "input", (value) => config.settings.buttonScreenSeconds = clampNumber(value, 1, 30, config.settings.buttonScreenSeconds)],
  ["#device-name", "input", (value) => {
    config.device.name = value.slice(0, MAX_DEVICE_NAME_LENGTH);
    const displayName = config.device.name || "MIDI Controller";
    document.querySelector("#device-title").textContent = displayName;
    document.title = displayName;
  }]
];

fieldBindings.forEach(([selector, eventName, update]) => document.querySelector(selector).addEventListener(eventName, (event) => {
  update(event.target.value, event.target);
  if (selector !== "#button-count") {
    persistDraft();
    if (selector !== "#device-name") renderAll();
  }
}));

document.querySelector("#remove-stomp-switch").addEventListener("click", () => {
  setButtonCount(config.hardware.buttonCount - 1);
});

document.querySelector("#add-stomp-switch").addEventListener("click", () => {
  setButtonCount(config.hardware.buttonCount + 1);
});

document.querySelectorAll('input[name="menu-mode"]').forEach((input) => {
  input.addEventListener("change", () => {
    if (!input.checked) return;
    config.hardware.menuControls.mode = input.value;
    persistDraft();
    renderAll();
  });
});

document.querySelectorAll('input[name="hold-edit-mode"]').forEach((input) => {
  input.addEventListener("change", () => {
    if (!input.checked) return;
    config.settings.holdToEditEnabled = input.value === "on";
    persistDraft();
  });
});

document.querySelectorAll('input[name="midi-output-mode"]').forEach((input) => {
  input.addEventListener("change", () => {
    if (!input.checked) return;
    config.settings.midiOutputMode = input.value;
    persistDraft();
    renderDevice();
  });
});

document.querySelector("#export-button").addEventListener("click", () => {
  const safeName = (config.device.name || "midi-controller").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/(^-|-$)/g, "");
  exportJson(normalizeConfig(config), `${safeName || "midi-controller"}-config.json`, "Configuration exported");
});

document.querySelector("#import-button").addEventListener("click", () => document.querySelector("#import-file").click());
document.querySelector("#import-file").addEventListener("change", async (event) => {
  const [file] = event.target.files;
  if (!file) return;
  try {
    config = normalizeConfig(JSON.parse(await file.text()));
    editingPresetId = config.device.activePresetId;
    editingProfileId = config.deviceProfiles[0].id;
    persistDraft();
    renderAll();
    const report = validateConfigDraft(config);
    showToast(report.blocking.length ? "Imported configuration needs attention" : "Configuration imported");
  } catch { showToast("That configuration file is not valid"); }
  event.target.value = "";
});

document.querySelector("#export-profiles").addEventListener("click", () => {
  exportJson({ schemaVersion: 1, deviceProfiles: config.deviceProfiles }, "midi-device-profiles.json", "Device Profiles exported");
});

document.querySelector("#import-profiles").addEventListener("click", () => document.querySelector("#profile-import-file").click());
document.querySelector("#profile-import-file").addEventListener("change", async (event) => {
  const [file] = event.target.files;
  if (!file) return;
  try {
    const parsed = JSON.parse(await file.text());
    const imported = Array.isArray(parsed.deviceProfiles) ? parsed.deviceProfiles : Array.isArray(parsed) ? parsed : [];
    if (!imported.length) throw new Error();
    let added = 0;
    imported.forEach((profile) => {
      if (config.deviceProfiles.length >= MAX_DEVICE_PROFILES) return;
      const normalized = normalizeProfile(profile, config.deviceProfiles.length);
      normalized.id = nextProfileId();
      config.deviceProfiles.push(normalized);
      editingProfileId = normalized.id;
      added += 1;
    });
    persistDraft();
    renderAll();
    showToast(added ? `Imported ${added} Device Profile${added === 1 ? "" : "s"}` : "No room for more Device Profiles");
  } catch { showToast("That Device Profile file is not valid"); }
  event.target.value = "";
});

async function refreshDefaultsFromDevice() {
  if (!connected) return;
  try {
    const response = await fetchWithTimeout("/default-config.json", { cache: "no-store" });
    if (response.ok) defaults = normalizeConfig(await response.json());
  } catch {
    // The embedded defaults remain available for browser preview mode.
  }
}

async function loadConfigFromDevice() {
  const response = await fetchWithTimeout("/api/config", { cache: "no-store" });
  if (!response.ok) throw new Error("Device configuration was not available");
  const loaded = normalizeConfig(await response.json());
  config = loaded;
  editingPresetId = config.device.activePresetId;
  editingProfileId = config.deviceProfiles[0].id;
  setConnection(true, "Configuration mode");
  await refreshDefaultsFromDevice();
  persistDraft();
  renderAll();
}

document.querySelector("#reload-device").addEventListener("click", async () => {
  setConnection(false, "Loading device");
  try {
    await loadConfigFromDevice();
    showToast("Loaded from device");
  } catch {
    setConnection(false, "Browser preview");
    renderAll();
    showToast("Device configuration not available");
  }
});

document.querySelector("#factory-reset").addEventListener("click", async () => {
  if (!window.confirm("Default settings will be loaded into the editor first. Click Save & restart to write them to the pedal.")) return;
  await refreshDefaultsFromDevice();
  config = normalizeConfig(clone(defaults));
  editingPresetId = config.device.activePresetId;
  editingProfileId = config.deviceProfiles[0].id;
  persistDraft();
  renderAll();
  showToast("Default settings loaded in the editor");
});

document.querySelector("#firmware-update").addEventListener("click", async () => {
  if (!connected) {
    showToast("Connect to the pedal configuration page first");
    return;
  }
  if (!window.confirm("Restart the KB2040 in firmware update mode? Unsaved editor changes will be lost.")) return;
  try {
    const response = await fetch("/api/bootloader", { method: "POST" });
    if (!response.ok) throw new Error();
    showToast("Entering firmware update mode");
    setConnection(false, "Firmware update mode");
  } catch {
    showToast("Could not enter firmware update mode");
  }
});

document.querySelector("#save-button").addEventListener("click", async () => {
  config = normalizeConfig(config);
  const report = validateConfigDraft(config);
  if (report.blocking.length) {
    renderValidation();
    showToast("Resolve configuration issues before saving");
    return;
  }
  persistDraft();
  if (!connected) { showToast("Draft saved in this browser"); return; }
  const button = document.querySelector("#save-button");
  saveInProgress = true;
  button.disabled = true;
  button.textContent = "Saving";
  try {
    const response = await fetch("/api/config", { method: "PUT", headers: { "Content-Type": "application/json" }, body: JSON.stringify(config) });
    if (!response.ok) {
      let message = "Save failed. The pedal configuration was not changed.";
      try {
        const failure = await response.json();
        if (failure?.error) message = failure.error;
      } catch {
        // Keep the default message when the device returns a non-JSON error.
      }
      throw new Error(message);
    }
    await fetch("/api/reboot", { method: "POST" });
    showToast("Configuration saved. Restarting pedal.");
    setConnection(false, "Restarting");
  } catch (error) { showToast(error.message || "Save failed. The pedal configuration was not changed."); }
  saveInProgress = false;
  button.textContent = SAVE_BUTTON_LABEL;
  renderValidation();
});

async function loadConfiguration() {
  applyTheme(localStorage.getItem(THEME_STORAGE_KEY) || document.documentElement.dataset.theme, false);
  defaults = clone(EMBEDDED_DEFAULT_CONFIG);
  const saved = localStorage.getItem(DRAFT_STORAGE_KEY);
  config = normalizeConfig(saved ? JSON.parse(saved) : clone(defaults));
  editingPresetId = config.device.activePresetId;
  editingProfileId = config.deviceProfiles[0].id;
  setConnection(false, "Loading device");
  renderAll();

  for (let attempt = 1; attempt <= DEVICE_CONFIG_LOAD_ATTEMPTS; attempt += 1) {
    try {
      await loadConfigFromDevice();
      return;
    } catch {
      if (attempt < DEVICE_CONFIG_LOAD_ATTEMPTS) {
        setConnection(false, `Waiting for device ${attempt}/${DEVICE_CONFIG_LOAD_ATTEMPTS}`);
        await delay(DEVICE_CONFIG_RETRY_DELAY_MS);
      }
    }
  }

  setConnection(false, "Browser preview");
  editingPresetId = config.device.activePresetId;
  editingProfileId = config.deviceProfiles[0].id;
  renderAll();
}

loadConfiguration().catch(() => {
  setConnection(false, "Unable to load configuration");
  showToast("Unable to load the configurator");
});
