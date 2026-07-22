$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$OutputRoot = Join-Path $RepoRoot "Saved\CodeIntent"

if (Test-Path -LiteralPath $OutputRoot) {
    Remove-Item -LiteralPath $OutputRoot -Recurse -Force
    Write-Host "삭제: Saved/CodeIntent"
} else {
    Write-Host "삭제할 Saved/CodeIntent 없음"
}

Write-Host "유지: Docs/CodeIntentSite (Git으로 공유되는 핵심 HTML)"
Write-Host "유지: gd-upload/CodeIntent/code-intent-heavy-artifacts.zip (Google Drive 업로드용 보존 zip)"
