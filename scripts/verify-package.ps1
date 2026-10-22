param([Parameter(Mandatory=$true)][string]$Archive)
$ErrorActionPreference = "Stop"
$archivePath = (Resolve-Path $Archive).Path
$validationDir = Join-Path ([IO.Path]::GetTempPath()) ("RobBench-package-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $validationDir | Out-Null
try {
    Expand-Archive -LiteralPath $archivePath -DestinationPath $validationDir
    $executables = @(Get-ChildItem -LiteralPath $validationDir -Recurse -Filter RobBench.exe)
    if ($executables.Count -ne 1) { throw "Expected one packaged RobBench.exe; found $($executables.Count)" }
    $executable = $executables[0]
    # Verify the PE subsystem instead of relying on how CI launches the process.
    $image = [IO.File]::ReadAllBytes($executable.FullName)
    $peOffset = [BitConverter]::ToInt32($image, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 94 -gt $image.Length -or
        [BitConverter]::ToUInt32($image, $peOffset) -ne 0x4550) { throw "Invalid Windows executable" }
    $subsystem = [BitConverter]::ToUInt16($image, $peOffset + 24 + 68)
    if ($subsystem -ne 2) { throw "RobBench must use the Windows GUI subsystem (no console)" }
    $debugQt = @(Get-ChildItem -LiteralPath $validationDir -Recurse -Filter Qt6*d.dll)
    if ($debugQt.Count -ne 0) { throw "Debug Qt DLLs must not ship in the Release package" }
    $process = Start-Process -FilePath $executable.FullName -ArgumentList "--check-runtime" `
        -WorkingDirectory $executable.DirectoryName -PassThru
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "Packaged runtime check timed out"
    }
    if ($process.ExitCode -ne 0) { throw "Packaged runtime check failed: exit $($process.ExitCode)" }
    Write-Host "Extracted package loads its native Qt platform and required runtime DLLs. GPU rendering is a separate test."
} finally {
    Remove-Item -LiteralPath $validationDir -Recurse -Force
}
