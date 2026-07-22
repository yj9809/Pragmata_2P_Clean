param(
    [string]$BaseRef = "origin/develop",
    [string]$PurgeList = ".agents/work/map-props-selected.txt",
    [string]$UnstagedAssetsDoc = "Docs/Operations/unstaged-assets.md"
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Fail {
    param([string]$Message)
    Write-Error $Message
    exit 1
}

git rev-parse --is-inside-work-tree *> $null
if ($LASTEXITCODE -ne 0) {
    Fail "This script must be run inside the repository."
}

$branch = (git branch --show-current).Trim()
if ([string]::IsNullOrWhiteSpace($branch)) {
    Fail "Detached HEAD state. Switch to the branch you plan to push."
}

git rev-parse --verify $BaseRef *> $null
if ($LASTEXITCODE -ne 0) {
    Fail "Missing $BaseRef. Run: git fetch origin --prune"
}

$mergeBase = (git merge-base HEAD $BaseRef).Trim()
$baseCommit = (git rev-parse $BaseRef).Trim()
if ($mergeBase -ne $baseCommit) {
    Fail "Current branch is not rebased on $BaseRef. Run: git fetch origin --prune; git rebase $BaseRef"
}

function Get-PurgePathsFromDocs {
    param([string]$DocPath)

    if (-not (Test-Path -LiteralPath $DocPath)) {
        return @()
    }

    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($line in Get-Content -LiteralPath $DocPath) {
        if ($line -notmatch "^\|\s*``?([^``]+)``?\s*\|\s*map prop mesh/texture/animation asset\s*\|") {
            continue
        }

        $path = $Matches[1]
        if ($path -like "Content/Map/Props/*" -or $path -like "Content/Map/PropsWithAnim/*") {
            $paths.Add($path)
        }
    }

    return $paths
}

$paths = @()
if (Test-Path -LiteralPath $PurgeList) {
    $paths = @(Get-Content -LiteralPath $PurgeList | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
} else {
    $paths = @(Get-PurgePathsFromDocs -DocPath $UnstagedAssetsDoc)
}

if ($paths.Count -eq 0) {
    Write-Warning "Purge list not found: $PurgeList. Falling back to broad map prop tree check."
    $treePaths = @(git ls-tree -r --name-only HEAD -- Content/Map/Props Content/Map/PropsWithAnim | Where-Object {
        $fileName = [IO.Path]::GetFileName($_)
        $_ -like "*.uasset" -and
        $_ -notmatch "/Materials/" -and
        $fileName -notlike "BP_*.uasset" -and (
            $_ -match "/Textures/" -or
            $_ -match "MotionList|Animation|Animations|Anim" -or
            $_ -match "/SM/" -or
            $fileName -match "^(SM_|SK_|sm\d)" -or
            $fileName -match "_unreal\.uasset$"
        )
    })
    if ($treePaths.Count -gt 0) {
        $treePaths | Select-Object -First 20 | ForEach-Object { Write-Error "Purged asset still present in branch tree: $_" }
        Fail "Found $($treePaths.Count) map prop mesh/texture/animation assets in branch tree."
    }
} else {
    $treeSet = New-Object 'System.Collections.Generic.HashSet[string]'
    git ls-tree -r --name-only HEAD -- Content/Map/Props Content/Map/PropsWithAnim | ForEach-Object {
        [void]$treeSet.Add($_)
    }

    $present = @($paths | Where-Object { $treeSet.Contains($_) } | Select-Object -First 20)
    if ($present.Count -gt 0) {
        $present | ForEach-Object { Write-Error "Purged asset still present in branch tree: $_" }
        Fail "Branch tree still contains purge target assets."
    }
}

$historyHits = @()
$range = "$BaseRef..HEAD"
if ($paths.Count -gt 0) {
    $purgeSet = New-Object 'System.Collections.Generic.HashSet[string]'
    $paths | ForEach-Object { [void]$purgeSet.Add($_) }
    $historyHits = @(git log --name-only --pretty=format: $range -- Content/Map/Props Content/Map/PropsWithAnim |
        Where-Object { $purgeSet.Contains($_) } |
        Sort-Object -Unique)
}
if ($historyHits.Count -gt 0) {
    $historyHits | Select-Object -First 20 | ForEach-Object { Write-Error "Branch history touches purge target: $_" }
    Fail "Branch commits after $BaseRef touch purge target assets. Remove those commits or redo the rebase without these assets."
}

[PSCustomObject]@{
    Branch = $branch
    BaseRef = $BaseRef
    BaseCommit = $baseCommit.Substring(0, 8)
    Status = "OK"
    Next = "git push --force-with-lease"
}
