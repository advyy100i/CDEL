Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ProjectPath = Join-Path $PSScriptRoot 'dummy_project'
$BuildPath = Join-Path $ProjectPath 'build-extractor'
$OutputPath = Join-Path $PSScriptRoot 'artifacts\build_config_map.json'

$PythonExe = 'python'
$VenvPython = Join-Path $RepoRoot '.venv\\Scripts\\python.exe'
if (Test-Path $VenvPython) {
  $PythonExe = $VenvPython
}

$CmakeArgs = @()
$VenvCmake = Join-Path $RepoRoot '.venv\\Scripts\\cmake.exe'
if (Test-Path $VenvCmake) {
  $CmakeArgs = @('--cmake-bin', $VenvCmake)
}

Write-Host "[Phase1] Running extractor..."
if (Test-Path $BuildPath) {
  Remove-Item -Recurse -Force $BuildPath
}

& $PythonExe (Join-Path $RepoRoot 'extractor\\extractor.py') `
  --project $ProjectPath `
  --build-dir $BuildPath `
  --output $OutputPath `
  @CmakeArgs `
  --verbose

Write-Host "[Phase1] Extractor output: $OutputPath"
