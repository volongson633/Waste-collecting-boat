param([Parameter(ValueFromRemainingArguments = $true)][string[]]$PioArgs)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$localPython = Join-Path $projectRoot '.tools\python\python.exe'
Push-Location $projectRoot
try {
    if (Test-Path -LiteralPath $localPython) {
        $env:PLATFORMIO_CORE_DIR = Join-Path $projectRoot '.pio-core'
        $env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = 'No'
        if (-not $env:PLATFORMIO_BUILD_DIR) { $env:PLATFORMIO_BUILD_DIR = Join-Path $projectRoot '.pio\verified-build' }
        if (-not $env:PLATFORMIO_LIBDEPS_DIR) { $env:PLATFORMIO_LIBDEPS_DIR = Join-Path $projectRoot '.pio\verified-libdeps' }
        & $localPython -m platformio @PioArgs
    } else {
        & pio @PioArgs
    }
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
