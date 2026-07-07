$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$dist = Join-Path $root 'dist'
$generated = Join-Path $root '..\KB2040MIDIController\WebConfigIndex.h'

$html = Get-Content -Raw -LiteralPath (Join-Path $root 'index.html')
$css = Get-Content -Raw -LiteralPath (Join-Path $root 'styles.css')
$javascript = Get-Content -Raw -LiteralPath (Join-Path $root 'app.js')

$html = $html.Replace('  <link rel="stylesheet" href="styles.css">', "  <style>$css</style>")
$html = $html.Replace('  <script src="app.js"></script>', "  <script>$javascript</script>")

New-Item -ItemType Directory -Force -Path $dist | Out-Null
[System.IO.File]::WriteAllText((Join-Path $dist 'index.html'), $html, [System.Text.UTF8Encoding]::new($false))

$sourceBytes = [System.Text.Encoding]::UTF8.GetBytes($html)
$memory = [System.IO.MemoryStream]::new()
$gzip = [System.IO.Compression.GZipStream]::new(
  $memory,
  [System.IO.Compression.CompressionLevel]::Optimal,
  $true
)
$gzip.Write($sourceBytes, 0, $sourceBytes.Length)
$gzip.Dispose()
$compressed = $memory.ToArray()
$memory.Dispose()

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('#include <Arduino.h>')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('const uint8_t WEB_CONFIG_INDEX_GZ[] PROGMEM = {')
for ($offset = 0; $offset -lt $compressed.Length; $offset += 16) {
  $last = [Math]::Min($offset + 15, $compressed.Length - 1)
  $line = ($compressed[$offset..$last] | ForEach-Object { '0x{0:x2}' -f $_ }) -join ', '
  if ($last -lt $compressed.Length - 1) { $line += ',' }
  [void]$builder.AppendLine("  $line")
}
[void]$builder.AppendLine('};')
[void]$builder.AppendLine('const size_t WEB_CONFIG_INDEX_GZ_SIZE = sizeof(WEB_CONFIG_INDEX_GZ);')

[System.IO.File]::WriteAllText($generated, $builder.ToString(), [System.Text.Encoding]::ASCII)

Write-Host "Embedded page: $($sourceBytes.Length) bytes"
Write-Host "Gzip payload: $($compressed.Length) bytes"
