$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ProjectFile = Join-Path $RepoRoot "Pragmata_2P.uproject"
$ProjectJson = Get-Content -Raw -LiteralPath $ProjectFile | ConvertFrom-Json
$EngineAssociation = [string]$ProjectJson.EngineAssociation

if ([string]::IsNullOrWhiteSpace($EngineAssociation)) {
    throw "Pragmata_2P.uproject에 EngineAssociation이 없음"
}

$EngineRootCandidates = @(
    (Join-Path ${env:ProgramFiles} "Epic Games\UE_$EngineAssociation"),
    (Join-Path ${env:ProgramFiles(x86)} "Epic Games\UE_$EngineAssociation")
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

if (-not $EngineRootCandidates) {
    throw "UE_$EngineAssociation 설치 경로를 찾지 못함. Epic Games 기본 설치 경로 기준"
}

$EngineRoot = @($EngineRootCandidates)[0]
$UnrealBuildTool = Join-Path $EngineRoot "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
if (-not (Test-Path -LiteralPath $UnrealBuildTool)) {
    throw "UnrealBuildTool.exe 없음: $UnrealBuildTool"
}

$OutputDir = Join-Path $RepoRoot "Saved\CodeIntent\compile"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

& $UnrealBuildTool `
    -Mode=GenerateClangDatabase `
    "-Project=$ProjectFile" `
    -OutputDir="$OutputDir" `
    Pragmata_2PEditor Win64 Development

if ($LASTEXITCODE -ne 0) {
    throw "GenerateClangDatabase 실패: exit $LASTEXITCODE"
}

$DatabasePath = Join-Path $OutputDir "compile_commands.json"
if (-not (Test-Path -LiteralPath $DatabasePath)) {
    throw "compile_commands.json 생성 실패: $DatabasePath"
}

Write-Host "compile_commands.json 생성: Saved/CodeIntent/compile/compile_commands.json"
