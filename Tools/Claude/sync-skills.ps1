[CmdletBinding()]
param(
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $scriptRoot '..\..')).Path
}
$sourceRoot = Join-Path $ProjectRoot '.agents\skills'
$targetRoot = Join-Path $ProjectRoot '.claude\skills'

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "Canonical skill directory not found: $sourceRoot"
}

Get-ChildItem -LiteralPath $sourceRoot -Directory | ForEach-Object {
    $sourceSkill = Join-Path $_.FullName 'SKILL.md'
    if (-not (Test-Path -LiteralPath $sourceSkill)) {
        return
    }

    $content = Get-Content -LiteralPath $sourceSkill -Raw
    $name = ([regex]::Match($content, '(?m)^name:\s*(.+)$')).Groups[1].Value.Trim()
    $description = ([regex]::Match($content, '(?m)^description:\s*(.+)$')).Groups[1].Value.Trim()
    if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($description)) {
        throw "Skill frontmatter must define name and description: $sourceSkill"
    }

    $targetDirectory = Join-Path $targetRoot $_.Name
    New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    $wrapper = @"
---
name: $name
description: $description
---

# Canonical project skill

Read and follow `.agents/skills/$($_.Name)/SKILL.md` in full. The file under `.agents/skills/` is the canonical source; this file only exposes it to Claude Code's project skill discovery path.
"@
    Set-Content -LiteralPath (Join-Path $targetDirectory 'SKILL.md') -Value $wrapper -Encoding UTF8
}

Write-Output "Synchronized Claude skill wrappers from $sourceRoot to $targetRoot"
