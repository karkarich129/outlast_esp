$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
}
if (-not $msbuild) {
    $msbuild = "MSBuild.exe"
}
Write-Host "Found MSBuild: $msbuild"
& $msbuild "$PSScriptRoot\OutlastInternal.sln" /p:Configuration=Release /p:Platform=x64
