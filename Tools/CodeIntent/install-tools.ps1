$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ManifestPath = Join-Path $PSScriptRoot "tool-versions.json"

function Add-ToolPathCandidates {
    $candidates = @(
        (Join-Path $env:ProgramFiles "doxygen\bin"),
        (Join-Path $env:ProgramFiles "Graphviz\bin"),
        (Join-Path $env:ProgramFiles "LLVM\bin"),
        (Join-Path $env:ProgramFiles "clang-uml\bin"),
        (Join-Path $env:APPDATA "Python\Python312\Scripts"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\Scripts")
    )

    $existing = $env:Path -split ";"
    foreach ($candidate in $candidates) {
        if ((Test-Path -LiteralPath $candidate) -and ($existing -notcontains $candidate)) {
            $env:Path = "$candidate;$env:Path"
        }
    }
}

function Test-ToolVersion([string]$Name, [string]$Version) {
    Add-ToolPathCandidates

    try {
        switch ($Name) {
            "Doxygen" {
                $output = & doxygen --version 2>$null
                return $output -match [regex]::Escape($Version)
            }
            "Graphviz" {
                $previousErrorAction = $ErrorActionPreference
                $ErrorActionPreference = "Continue"
                $output = (& dot -V 2>&1 | Out-String)
                $ErrorActionPreference = $previousErrorAction
                return $output -match [regex]::Escape($Version)
            }
            "LLVM" {
                $output = & clangd --version 2>$null
                return $output -match [regex]::Escape($Version)
            }
            "clang-uml" {
                $output = & clang-uml --version 2>$null
                return $output -match [regex]::Escape($Version)
            }
            default {
                return $false
            }
        }
    }
    catch {
        return $false
    }
}

function Test-PythonPackageVersion([string]$Name, [string]$Version) {
    $script = "import importlib.metadata as m; print(m.version('$Name'))"
    $output = python -c $script 2>$null
    return $output -match "^$([regex]::Escape($Version))$"
}

if (-not (Test-Path -LiteralPath $ManifestPath)) {
    throw "tool-versions.json 없음"
}

$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget 없음. Windows App Installer 또는 다른 package manager 필요"
}

foreach ($tool in $manifest.windows.winget) {
    Write-Host "설치 확인: $($tool.name) $($tool.version)"
    if (Test-ToolVersion $tool.name $tool.version) {
        Write-Host "이미 설치됨: $($tool.name) $($tool.version)"
        continue
    }

    winget install -e --id $tool.id --version $tool.version --silent --accept-source-agreements --accept-package-agreements
    Add-ToolPathCandidates
}

foreach ($package in $manifest.windows.python) {
    Write-Host "Python package 설치 확인: $($package.name) $($package.version)"
    if (Test-PythonPackageVersion $package.name $package.version) {
        Write-Host "이미 설치됨: $($package.name) $($package.version)"
        continue
    }

    python -m pip install --user "$($package.name)==$($package.version)"
}

& (Join-Path $PSScriptRoot "check-env.ps1")

Write-Host "Code Intent 도구 설치 확인 완료"
