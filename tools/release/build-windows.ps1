param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$VcpkgRoot,
    [string]$BuildDirectory,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../..").Path
if (!$BuildDirectory) { $BuildDirectory = Join-Path $root 'build-release-windows' }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $root 'dist' }
function Run([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$program failed with exit code $LASTEXITCODE" }
}
Run 'cmake' @('-S', $root, '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-C', "$PSScriptRoot/profile.cmake", "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake",
    '-DVCPKG_TARGET_TRIPLET=x64-windows', "-DCMAKE_PREFIX_PATH=$QtRoot", '-DUSE_EPOXY=ON')
Run 'cmake' @('--build', $BuildDirectory, '--config', 'Release', '--target', 'gbax-qt', '--parallel', '2')
Run 'python' @("$PSScriptRoot/release.py", 'check-cache', "$BuildDirectory/CMakeCache.txt")
$version = (& python "$PSScriptRoot/release.py" version).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Could not read version' }
$stage = Join-Path $BuildDirectory "package/GBAx-$version"
if (Test-Path $stage) { throw "Remove only the old staging directory before repackaging: $stage" }
New-Item -ItemType Directory -Force $stage, $OutputDirectory | Out-Null
Copy-Item "$BuildDirectory/Release/GBAx.exe" $stage
# Includes SDL/libzip and their transitive DLLs; never copy debug DLLs.
Copy-Item "$VcpkgRoot/installed/x64-windows/bin/*.dll" $stage
Run 'python' @("$PSScriptRoot/release.py", 'resources', $stage,
    '--notices', "$VcpkgRoot/installed/x64-windows/share")
Run "$QtRoot/bin/windeployqt.exe" @('--release', '--compiler-runtime', '--dir', $stage, "$stage/GBAx.exe")
$oldPluginPath = $env:QT_PLUGIN_PATH
$oldPlatform = $env:QT_QPA_PLATFORM
try {
    Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    $env:QT_QPA_PLATFORM = 'offscreen'
    # Start-Process waits for a GUI-subsystem executable and exposes its exit code.
    $process = Start-Process -FilePath "$stage/GBAx.exe" -ArgumentList '--package-check' -Wait -PassThru `
        -RedirectStandardOutput "$OutputDirectory/windows-package-check.json" `
        -RedirectStandardError "$OutputDirectory/windows-package-check.stderr.txt"
    if ($process.ExitCode -ne 0) { throw 'Packaged Windows application failed its runtime check' }
} finally {
    $env:QT_PLUGIN_PATH = $oldPluginPath
    $env:QT_QPA_PLATFORM = $oldPlatform
}
Compress-Archive -Path $stage -DestinationPath "$OutputDirectory/GBAx-$version-windows-x64.zip" -Force
Run 'python' @("$PSScriptRoot/release.py", 'manifest', $OutputDirectory)
