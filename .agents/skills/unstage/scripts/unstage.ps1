param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [string]$Reason = "대용량 Unreal asset",
    [string]$Location = "팀 공유 Google Drive",

    [ValidateSet("NewOnly", "All")]
    [string]$CopyMode = "NewOnly"
)

$ErrorActionPreference = "Stop"
$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Convert-ToRepoPath {
    param([string]$InputPath)

    $normalized = $InputPath.Replace('\', '/')
    while ($normalized.StartsWith("./")) {
        $normalized = $normalized.Substring(2)
    }
    return $normalized
}

function Write-LinesNoBom {
    param(
        [string]$Path,
        [string[]]$Lines
    )

    $text = ($Lines -join [Environment]::NewLine) + [Environment]::NewLine
    [System.IO.File]::WriteAllText([System.IO.Path]::GetFullPath($Path), $text, $Utf8NoBom)
}

function Ensure-GitignoreEntry {
    param(
        [string]$GitignorePath,
        [string]$RepoPath
    )

    if (-not (Test-Path -LiteralPath $GitignorePath)) {
        throw "Missing .gitignore"
    }

    $sectionHeader = "# Unstaged assets kept in team Google Drive."
    $sourceLine = "# Source of truth: Docs/Operations/unstaged-assets.md."
    $visibilityLine = "# Keep each asset as an individual path so new Map assets appear in git status."
    $uploadHeader = "# Google Drive upload staging folder for ignored large assets."
    $lines = [System.Collections.Generic.List[string]]::new()
    [System.IO.File]::ReadAllLines((Resolve-Path -LiteralPath $GitignorePath)) | ForEach-Object { $lines.Add($_) }

    $sectionStart = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq $sectionHeader) {
            $sectionStart = $i
            break
        }
    }

    if ($sectionStart -lt 0) {
        $insertAt = $lines.Count
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -eq $uploadHeader) {
                $insertAt = $i
                while ($insertAt -gt 0 -and $lines[$insertAt - 1] -eq "") {
                    $insertAt--
                }
                break
            }
        }

        $newLines = [System.Collections.Generic.List[string]]::new()
        for ($i = 0; $i -lt $insertAt; $i++) { $newLines.Add($lines[$i]) }
        while ($newLines.Count -gt 0 -and $newLines[$newLines.Count - 1] -eq "") {
            $newLines.RemoveAt($newLines.Count - 1)
        }
        $newLines.Add("")
        $newLines.Add($sectionHeader)
        $newLines.Add($sourceLine)
        $newLines.Add($visibilityLine)
        $newLines.Add($RepoPath)
        $newLines.Add("")
        for ($i = $insertAt; $i -lt $lines.Count; $i++) { $newLines.Add($lines[$i]) }
        Write-LinesNoBom -Path $GitignorePath -Lines $newLines
        return
    }

    $sectionEnd = $lines.Count
    for ($i = $sectionStart + 1; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq $uploadHeader) {
            $sectionEnd = $i
            while ($sectionEnd -gt $sectionStart -and $lines[$sectionEnd - 1] -eq "") {
                $sectionEnd--
            }
            break
        }
    }

    $paths = [System.Collections.Generic.SortedSet[string]]::new([System.StringComparer]::Ordinal)
    for ($i = $sectionStart + 1; $i -lt $sectionEnd; $i++) {
        $line = $lines[$i].Trim()
        if ($line -like "Content/*") {
            if ($line -match '[*?!\[\]]') {
                throw "Unstaged asset section contains broad ignore pattern: $line"
            }
            [void]$paths.Add($line)
        }
    }
    [void]$paths.Add($RepoPath)

    $newLines = [System.Collections.Generic.List[string]]::new()
    for ($i = 0; $i -lt $sectionStart; $i++) { $newLines.Add($lines[$i]) }
    while ($newLines.Count -gt 0 -and $newLines[$newLines.Count - 1] -eq "") {
        $newLines.RemoveAt($newLines.Count - 1)
    }
    $newLines.Add("")
    $newLines.Add($sectionHeader)
    $newLines.Add($sourceLine)
    $newLines.Add($visibilityLine)
    foreach ($path in $paths) { $newLines.Add($path) }
    $newLines.Add("")
    for ($i = $sectionEnd; $i -lt $lines.Count; $i++) { $newLines.Add($lines[$i]) }
    Write-LinesNoBom -Path $GitignorePath -Lines $newLines
}

function Ensure-DocsEntry {
    param(
        [string]$DocsPath,
        [string]$RepoPath,
        [string]$Reason,
        [string]$Location
    )

    if (-not (Test-Path -LiteralPath $DocsPath)) {
        throw "Missing $DocsPath"
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    [System.IO.File]::ReadAllLines((Resolve-Path -LiteralPath $DocsPath)) | ForEach-Object { $lines.Add($_) }

    $headerIndex = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq "| 파일 | 이유 | 위치 |") {
            $headerIndex = $i
            break
        }
    }
    if ($headerIndex -lt 0 -or $headerIndex + 1 -ge $lines.Count -or $lines[$headerIndex + 1] -ne "| --- | --- | --- |") {
        throw "Could not find unstaged asset table in $DocsPath"
    }

    $rowsByPath = [System.Collections.Generic.SortedDictionary[string,string]]::new([System.StringComparer]::Ordinal)
    $tableEnd = $lines.Count
    for ($i = $headerIndex + 2; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ([string]::IsNullOrWhiteSpace($line)) {
            $tableEnd = $i
            break
        }
        if ($line -match '^\| `([^`]+)` \| (.+) \| (.+) \|$') {
            $path = $Matches[1]
            if ($path -like "Content/*") {
                $rowsByPath[$path] = $line
                continue
            }
        }
        throw "Unexpected unstaged asset table row: $line"
    }

    if (-not $rowsByPath.ContainsKey($RepoPath)) {
        $docToken = '`' + $RepoPath + '`'
        $rowsByPath[$RepoPath] = "| $docToken | $Reason | $Location |"
    }

    $newLines = [System.Collections.Generic.List[string]]::new()
    for ($i = 0; $i -le $headerIndex + 1; $i++) { $newLines.Add($lines[$i]) }
    foreach ($row in $rowsByPath.Values) { $newLines.Add($row) }
    for ($i = $tableEnd; $i -lt $lines.Count; $i++) { $newLines.Add($lines[$i]) }
    Write-LinesNoBom -Path $DocsPath -Lines $newLines
}

function Copy-OneAssetToUpload {
    param(
        [string]$RepoPath,
        [string]$UploadRoot
    )

    $dest = Join-Path $UploadRoot $RepoPath
    $destDir = Split-Path $dest -Parent
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    Copy-Item -LiteralPath $RepoPath -Destination $dest -Force

    $sourceItem = Get-Item -LiteralPath $RepoPath
    $uploadItem = Get-Item -LiteralPath $dest
    if ($sourceItem.Length -ne $uploadItem.Length) {
        throw "Upload copy size mismatch: $dest"
    }

    return $dest.Replace('\', '/')
}

$repoPath = Convert-ToRepoPath $Path

if ([string]::IsNullOrWhiteSpace($repoPath)) {
    throw "Path argument is required. Usage: unstage.ps1 -Path Content/..."
}

if (-not (Test-Path -LiteralPath $repoPath)) {
    throw "Asset does not exist on disk: $repoPath"
}

$gitStatus = git status --short -- $repoPath

Ensure-GitignoreEntry -GitignorePath ".gitignore" -RepoPath $repoPath
Ensure-DocsEntry -DocsPath "Docs/Operations/unstaged-assets.md" -RepoPath $repoPath -Reason $Reason -Location $Location

$isTracked = $false
$trackedMatches = @(git ls-files -- $repoPath)
if ($trackedMatches -contains $repoPath) {
    $isTracked = $true
}

$isIndexed = $false
git ls-files --cached -- $repoPath | Where-Object { $_ } | ForEach-Object { $script:isIndexed = $true }

$removedFromIndex = $false
if ($isTracked -or $isIndexed -or ($gitStatus -match '^[ A-Z][AMDCRU]')) {
    git rm --cached -- $repoPath
    $removedFromIndex = $true
}

$uploadRoot = "gd-upload"
$uploadZipPath = "gd-upload/unstaged-assets-content.zip"
$fullBundleRefreshed = $false
$bundleZipSizeMiB = $null

if ($CopyMode -eq "All") {
    powershell -ExecutionPolicy Bypass -File .agents/skills/gen-gd/scripts/gen-gd.ps1
    $fullBundleRefreshed = $true
    if (-not (Test-Path -LiteralPath $uploadZipPath)) {
        throw "Upload zip was not generated: $uploadZipPath"
    }
    $bundleZipSizeMiB = [Math]::Round((Get-Item -LiteralPath $uploadZipPath).Length / 1MB, 2)
} else {
    New-Item -ItemType Directory -Force -Path $uploadRoot | Out-Null
}

$uploadCopyPath = Copy-OneAssetToUpload -RepoPath $repoPath -UploadRoot $uploadRoot

$sourceItem = Get-Item -LiteralPath $repoPath

[PSCustomObject]@{
    SourcePath = $repoPath
    UploadCopyPath = $uploadCopyPath
    UploadZipPath = if ($fullBundleRefreshed) { $uploadZipPath } else { $null }
    CopyMode = $CopyMode
    FullBundleRefreshed = $fullBundleRefreshed
    SizeMiB = [Math]::Round($sourceItem.Length / 1MB, 2)
    RemovedFromIndex = $removedFromIndex
    WasTrackedOrIndexed = ($isTracked -or $isIndexed)
    GitStatusBefore = ($gitStatus -join "`n")
    BundleZipSizeMiB = $bundleZipSizeMiB
}
