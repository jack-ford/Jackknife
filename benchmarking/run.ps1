param(
    [ValidateSet("all", "movegen", "evaluation", "search", "features")]
    [string]$Mode = "all",
    [ValidateRange(1, 127)]
    [int]$Depth = 7,
    [ValidateRange(1, 100)]
    [int]$Repetitions = 1,
    [string]$BuildDirectory = "$PSScriptRoot/../build/benchmarking"
)

$ErrorActionPreference = "Stop"
$sourceDirectory = Resolve-Path "$PSScriptRoot/.."

cmake -S $sourceDirectory -B $BuildDirectory -G "Visual Studio 17 2022" `
    -DCHESS_BUILD_GUI=OFF -DBUILD_TESTING=OFF -DCHESS_BUILD_BENCHMARKS=ON
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}
cmake --build $BuildDirectory --config Release --target chess_benchmark
if ($LASTEXITCODE -ne 0) {
    throw "Benchmark build failed with exit code $LASTEXITCODE"
}

$resultDirectory = Join-Path $PSScriptRoot "results"
New-Item -ItemType Directory -Force $resultDirectory | Out-Null
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resultPath = Join-Path $resultDirectory "$Mode-$timestamp.csv"

& "$BuildDirectory/Release/chess_benchmark.exe" $Mode $Depth $Repetitions | Tee-Object -FilePath $resultPath
if ($LASTEXITCODE -ne 0) {
    throw "Benchmark failed with exit code $LASTEXITCODE"
}

Write-Host "Results: $resultPath"
