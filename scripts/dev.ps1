param(
    [Parameter(Position=0)][ValidateSet("configure","build","test","package")][string]$Action = "build",
    [Parameter(Position=1)][ValidateSet("windows-debug","windows-release")][string]$Preset = "windows-debug"
)
$ErrorActionPreference = "Stop"
Push-Location (Join-Path $PSScriptRoot "..")
try {
    switch ($Action) {
        "configure" { cmake --preset $Preset }
        "build" { cmake --build --preset $Preset --parallel }
        "test" { ctest --preset $Preset }
        "package" {
            if ($Preset -ne "windows-release") { throw "Package the Release configuration." }
            ctest --preset $Preset
            if ($LASTEXITCODE -ne 0) { throw "Tests failed; packaging stopped." }
            cpack --config "build/$Preset/CPackConfig.cmake" -C Release
        }
    }
    if ($LASTEXITCODE -ne 0) { throw "$Action failed with exit code $LASTEXITCODE" }
} finally { Pop-Location }
