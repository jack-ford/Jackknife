$root = Split-Path -Parent $PSScriptRoot
$vcpkg = Join-Path $root ".vcpkg"

if (!(Test-Path $vcpkg)) {
    git clone https://github.com/microsoft/vcpkg.git $vcpkg
}

& (Join-Path $vcpkg "bootstrap-vcpkg.bat") -disableMetrics

$env:VCPKG_ROOT = $vcpkg
cmake --preset windows
cmake --build --preset windows
