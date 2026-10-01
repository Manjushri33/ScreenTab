param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build\symbol-runtime')
)

$ErrorActionPreference = 'Stop'
$version = '20260319.1511.0'
$downloadDirectory = Join-Path $PSScriptRoot '..\build\symbol-packages'
New-Item -ItemType Directory -Force -Path $downloadDirectory, $OutputDirectory | Out-Null

$packages = @(
    @{
        Id = 'microsoft.debugging.platform.dbgeng'
        Sha256 = '875678516F9CEED4A1C8B9B106D165106FCAA38723AD7C584C47B95B231600DE'
        Files = @('dbghelp.dll', 'msdia140.dll')
    },
    @{
        Id = 'microsoft.debugging.platform.symsrv'
        Sha256 = '8D24440267581038DE0101B36C0D75DAF7F063D86F3D6ADCA61038C236DF7CE7'
        Files = @('symsrv.dll')
    }
)

foreach ($package in $packages) {
    $fileName = "$($package.Id).$version.nupkg"
    $packagePath = Join-Path $downloadDirectory $fileName
    if (-not (Test-Path -LiteralPath $packagePath)) {
        $url = "https://api.nuget.org/v3-flatcontainer/$($package.Id)/$version/$fileName"
        Invoke-WebRequest -Uri $url -OutFile $packagePath -UseBasicParsing
    }

    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $packagePath).Hash
    if ($actualHash -ne $package.Sha256) {
        throw "Unexpected SHA-256 for $fileName. Remove the cached package and try again."
    }

    $expandedPath = Join-Path $downloadDirectory "$($package.Id)-$version"
    $zipPath = Join-Path $downloadDirectory "$($package.Id)-$version.zip"
    Copy-Item -LiteralPath $packagePath -Destination $zipPath -Force
    Expand-Archive -LiteralPath $zipPath -DestinationPath $expandedPath -Force
    foreach ($file in $package.Files) {
        $source = Join-Path $expandedPath "content\amd64\$file"
        if (-not (Test-Path -LiteralPath $source)) {
            throw "The Microsoft package does not contain content\amd64\$file"
        }
        Copy-Item -LiteralPath $source -Destination $OutputDirectory -Force
    }
}

Write-Host "Symbol runtime ready in $OutputDirectory"
