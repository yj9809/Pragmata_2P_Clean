$ErrorActionPreference = "Stop"

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

function Test-Tool {
    param(
        [string]$Name,
        [string]$Command,
        [string[]]$Arguments
    )

    $tool = Get-Command $Command -ErrorAction SilentlyContinue
    if (-not $tool) {
        [pscustomobject]@{
            Tool = $Name
            Found = $false
            Version = "미설치"
        }
        return
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $versionOutput = & $Command @Arguments 2>&1
    $ErrorActionPreference = $previousErrorActionPreference
    $version = ($versionOutput | Select-Object -First 1) -join " "

    [pscustomobject]@{
        Tool = $Name
        Found = $true
        Version = $version
    }
}

Add-ToolPathCandidates

$results = @(
    Test-Tool "Doxygen" "doxygen" @("--version")
    Test-Tool "Graphviz dot" "dot" @("-V")
    Test-Tool "clang-uml" "clang-uml" @("--version")
    Test-Tool "clangd" "clangd" @("--version")
    Test-Tool "MkDocs" "mkdocs" @("--version")
    Test-Tool "Python" "python" @("--version")
    Test-Tool "Node.js" "node" @("--version")
)

$results | Format-Table -AutoSize

$missing = $results | Where-Object { -not $_.Found }
if ($missing) {
    Write-Host ""
    Write-Host "누락 도구 있음. Unreal 파일은 수정하지 않음." -ForegroundColor Yellow
    exit 1
}

Write-Host "필수 도구 확인 완료."
