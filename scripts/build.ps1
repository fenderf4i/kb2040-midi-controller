param(
  [string]$Fqbn = "rp2040:rp2040:adafruit_kb2040:usbstack=tinyusb",
  [string]$Sketch = "KB2040MIDIController"
)

$ErrorActionPreference = "Stop"

$arduinoCli = Get-Command arduino-cli -ErrorAction SilentlyContinue

if ($arduinoCli) {
  $arduinoCliPath = $arduinoCli.Source
} else {
  $defaultPath = "C:\Program Files\Arduino CLI\arduino-cli.exe"

  if (-not (Test-Path -LiteralPath $defaultPath)) {
    throw "arduino-cli was not found on PATH or at $defaultPath"
  }

  $arduinoCliPath = $defaultPath
}

& $arduinoCliPath compile --fqbn $Fqbn $Sketch
