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

function Clear-DirectoryContents([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        Get-ChildItem -LiteralPath $Path -Force | Remove-Item -Recurse -Force
    } else {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
}

function Write-CodeIntentLandingPage(
    [string]$IndexPath,
    [string]$GraphHref,
    [string]$DocsHref,
    [string]$DoxygenHref,
    [string]$ArtifactNote
) {
    $DoxygenCard = ""
    if ($DoxygenHref) {
        $DoxygenCard = @"
      <article class="card">
        <h2>Doxygen Reference</h2>
        <p>자동 생성된 class/function 상세 reference. 정보량이 많으므로 구조 확인 후 마지막 근거 확인용으로 사용.</p>
        <a class="button" href="$DoxygenHref">Doxygen 열기</a>
      </article>
"@
    }

    $GridClass = if ($DoxygenHref) { "grid three" } else { "grid two" }

    @"
<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Pragmata_2P Code Intent</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0b1020;
      --panel: #111827;
      --panel-2: #162033;
      --line: #263348;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --accent-2: #5eead4;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: var(--bg);
      color: var(--text);
      font-family: "Segoe UI", "Noto Sans KR", system-ui, sans-serif;
    }
    main {
      width: min(1120px, calc(100% - 48px));
      margin: 0 auto;
      padding: 46px 0;
    }
    h1 {
      margin: 0;
      font-size: clamp(28px, 5vw, 48px);
      letter-spacing: 0;
    }
    .lead {
      max-width: 780px;
      margin: 14px 0 28px;
      color: var(--muted);
      font-size: 17px;
      line-height: 1.6;
    }
    .grid {
      display: grid;
      gap: 14px;
    }
    .grid.two { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .grid.three { grid-template-columns: repeat(3, minmax(0, 1fr)); }
    .card {
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel);
      padding: 18px;
      min-height: 196px;
    }
    .card h2 {
      margin: 0 0 8px;
      font-size: 18px;
      letter-spacing: 0;
    }
    .card p {
      margin: 0 0 16px;
      color: var(--muted);
      line-height: 1.55;
    }
    a.button {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 38px;
      border: 1px solid #2563eb;
      border-radius: 8px;
      background: #0f3b6f;
      color: #e0f2fe;
      padding: 8px 12px;
      text-decoration: none;
      font-weight: 700;
    }
    a.button:hover {
      border-color: var(--accent);
      background: #075985;
    }
    .note {
      margin-top: 18px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: var(--panel-2);
      padding: 14px 16px;
      color: var(--muted);
      line-height: 1.55;
    }
    code {
      color: #bae6fd;
      background: #020617;
      border: 1px solid #1f2937;
      border-radius: 5px;
      padding: 1px 4px;
    }
    @media (max-width: 860px) {
      main { width: min(100% - 28px, 1120px); padding-top: 28px; }
      .grid.two, .grid.three { grid-template-columns: 1fr; }
      .card { min-height: 0; }
    }
  </style>
</head>
<body>
  <main>
    <h1>Pragmata_2P Code Intent</h1>
    <p class="lead">
      코드 연결을 빠르게 찾고, 의도 문서를 읽는 로컬 HTML 시작 페이지.
    </p>
    <section class="$GridClass">
      <article class="card">
        <h2>Code Intent Graph</h2>
        <p>function, component, file 연결을 검색. <code>ReceiveDamage</code>, <code>Hack</code>, <code>HUD</code>처럼 입력하면 주변 연결까지 표시.</p>
        <a class="button" href="$GraphHref">Graph 열기</a>
      </article>
      <article class="card">
        <h2>Readable Docs</h2>
        <p>의도 카드, 시스템 문서, 사용 가이드를 MkDocs 화면에서 읽기. 왼쪽 목차 링크는 로컬 파일에서도 바로 이동.</p>
        <a class="button" href="$DocsHref">Docs 열기</a>
      </article>
$DoxygenCard
    </section>
    <div class="note">
      $ArtifactNote
    </div>
  </main>
</body>
</html>
"@ | Set-Content -LiteralPath $IndexPath -Encoding UTF8
}

function Remove-TrailingWhitespace([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $Extensions = @(".html", ".css", ".js", ".json", ".xml", ".txt")
    Get-ChildItem -LiteralPath $Path -Recurse -File |
        Where-Object { $Extensions -contains $_.Extension.ToLowerInvariant() } |
        ForEach-Object {
            $Content = Get-Content -LiteralPath $_.FullName -Raw
            if ($null -ne $Content) {
                $Normalized = $Content -replace "(?m)[ \t]+$", ""
                $Normalized = $Normalized -replace "(\r?\n){2,}\z", "`n"
                if ($Normalized -ne $Content) {
                    Set-Content -LiteralPath $_.FullName -Value $Normalized -NoNewline -Encoding UTF8
                }
            }
        }
}

function New-CodeIntentHeavyArtifactZip([string]$RepoRoot, [string]$OutputRoot) {
    $UploadRoot = Join-Path $RepoRoot "gd-upload\CodeIntent"
    $ZipPath = Join-Path $UploadRoot "code-intent-heavy-artifacts.zip"
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("codeintent-heavy-" + [guid]::NewGuid().ToString("N"))
    $TempCodeIntentRoot = Join-Path $TempRoot "Saved\CodeIntent"
    $Included = New-Object System.Collections.Generic.List[string]

    try {
        New-Item -ItemType Directory -Force -Path $UploadRoot | Out-Null
        New-Item -ItemType Directory -Force -Path $TempCodeIntentRoot | Out-Null

        $DoxygenRoot = Join-Path $OutputRoot "doxygen"
        if (Test-Path -LiteralPath (Join-Path $DoxygenRoot "html\index.html")) {
            Copy-Item -LiteralPath $DoxygenRoot -Destination (Join-Path $TempCodeIntentRoot "doxygen") -Recurse -Force
            $Included.Add("Saved/CodeIntent/doxygen") | Out-Null
        } else {
            Write-Host "Doxygen output 없음: heavy artifact zip에서 제외" -ForegroundColor Yellow
        }

        $BlueprintRoot = Join-Path $OutputRoot "blueprints"
        if (Test-Path -LiteralPath (Join-Path $BlueprintRoot "blueprint-asset-scan.json")) {
            Copy-Item -LiteralPath $BlueprintRoot -Destination (Join-Path $TempCodeIntentRoot "blueprints") -Recurse -Force
            $Included.Add("Saved/CodeIntent/blueprints") | Out-Null
        } else {
            Write-Host "Blueprint scan output 없음: heavy artifact zip에서 제외" -ForegroundColor Yellow
        }

        if ($Included.Count -eq 0) {
            Write-Host "heavy artifact zip 생략: 포함할 Doxygen/Blueprint scan output 없음" -ForegroundColor Yellow
            return
        }

        $Manifest = [ordered]@{
            generated_at_utc = [DateTime]::UtcNow.ToString("o")
            source = "Tools/CodeIntent/build-site.ps1"
            zip = "gd-upload/CodeIntent/code-intent-heavy-artifacts.zip"
            restore_note = "프로젝트 root에서 압축을 풀면 Saved/CodeIntent 아래로 복원된다."
            included = @($Included)
        }
        $Manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $TempCodeIntentRoot "heavy-artifacts-manifest.json") -Encoding UTF8

        if (Test-Path -LiteralPath $ZipPath) {
            Remove-Item -LiteralPath $ZipPath -Force
        }

        Compress-Archive -Path (Join-Path $TempRoot "*") -DestinationPath $ZipPath -Force
        Write-Host "Code Intent heavy artifact zip 갱신: gd-upload/CodeIntent/code-intent-heavy-artifacts.zip"
    }
    finally {
        if (Test-Path -LiteralPath $TempRoot) {
            Remove-Item -LiteralPath $TempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $RepoRoot

Add-ToolPathCandidates

$OutputRoot = Join-Path $RepoRoot "Saved\CodeIntent"
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$CompileDatabase = Join-Path $OutputRoot "compile\compile_commands.json"
if (-not (Test-Path -LiteralPath $CompileDatabase)) {
    try {
        & (Join-Path $PSScriptRoot "generate-compile-database.ps1")
    } catch {
        Write-Host "compile_commands.json 생성 실패: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

if (Get-Command doxygen -ErrorAction SilentlyContinue) {
    doxygen Tools/CodeIntent/Doxyfile
} else {
    Write-Host "Doxygen 미설치: Doxygen HTML 생략"
}

if ((Get-Command clang-uml -ErrorAction SilentlyContinue) -and (Test-Path -LiteralPath $CompileDatabase)) {
    $ClangUmlTimeoutSeconds = 60
    $ClangUmlLog = Join-Path $OutputRoot "clang-uml.log"
    $ClangUmlErr = Join-Path $OutputRoot "clang-uml.err.log"
    $ClangUmlProcess = Start-Process -FilePath "clang-uml" `
        -ArgumentList @("-c", "Tools/CodeIntent/clang-uml.yml") `
        -WorkingDirectory $RepoRoot `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput $ClangUmlLog `
        -RedirectStandardError $ClangUmlErr

    if (-not $ClangUmlProcess.WaitForExit($ClangUmlTimeoutSeconds * 1000)) {
        Stop-Process -Id $ClangUmlProcess.Id -Force
        Write-Host "clang-uml diagram 생성 timeout: 자체 graph 생성으로 보완" -ForegroundColor Yellow
    } elseif ($ClangUmlProcess.ExitCode -ne 0) {
        Write-Host "clang-uml diagram 생성 실패: 자체 graph 생성으로 보완" -ForegroundColor Yellow
    }
} else {
    Write-Host "clang-uml 또는 compile_commands.json 없음: 자동 diagram 생략"
}

& (Join-Path $PSScriptRoot "build-graph.ps1")

$SiteRoot = Join-Path $OutputRoot "site"
New-Item -ItemType Directory -Force -Path $SiteRoot | Out-Null

$GraphRoot = Join-Path $OutputRoot "graph"
New-Item -ItemType Directory -Force -Path $GraphRoot | Out-Null

$GraphJson = Join-Path $GraphRoot "damage-flow.json"
@"
{
  "nodes": [
    {"id": "FPragmataDamageSpec", "kind": "struct"},
    {"id": "UPragmataDamageLibrary::DealDamageToActor", "kind": "function"},
    {"id": "IPragmataDamageable::ReceiveDamage", "kind": "function"},
    {"id": "UPragmataHealthComponent::ReceiveDamage", "kind": "function"},
    {"id": "AWeaponBase::DealDamageToHitActor", "kind": "caller"},
    {"id": "UAnimNotifyState_AttackHitWindow", "kind": "caller"},
    {"id": "AMonsterBase::ApplySuccessfulHack_Implementation", "kind": "caller"}
  ],
  "edges": [
    {"from": "AWeaponBase::DealDamageToHitActor", "to": "FPragmataDamageSpec"},
    {"from": "UAnimNotifyState_AttackHitWindow", "to": "FPragmataDamageSpec"},
    {"from": "FPragmataDamageSpec", "to": "UPragmataDamageLibrary::DealDamageToActor"},
    {"from": "UPragmataDamageLibrary::DealDamageToActor", "to": "IPragmataDamageable::ReceiveDamage"},
    {"from": "AMonsterBase::ApplySuccessfulHack_Implementation", "to": "IPragmataDamageable::ReceiveDamage"},
    {"from": "IPragmataDamageable::ReceiveDamage", "to": "UPragmataHealthComponent::ReceiveDamage"}
  ],
  "defaultDepth": 1,
  "supportedDepth": 2
}
"@ | Set-Content -LiteralPath $GraphJson -Encoding UTF8

$IndexPath = Join-Path $SiteRoot "index.html"
Write-CodeIntentLandingPage `
    -IndexPath $IndexPath `
    -GraphHref "../graph/graph.html" `
    -DocsHref "../mkdocs-site/index.html" `
    -DoxygenHref "../doxygen/html/index.html" `
    -ArtifactNote "원자료 JSON과 Mermaid source는 사람이 직접 읽는 입구에서 제외. 필요하면 <code>Saved/CodeIntent/graph/</code>와 <code>Docs/Diagrams/</code>에서 확인."

if (Get-Command mkdocs -ErrorAction SilentlyContinue) {
    mkdocs build --clean --site-dir "Saved/CodeIntent/mkdocs-site"
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    python -m mkdocs build --clean --site-dir "Saved/CodeIntent/mkdocs-site"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "MkDocs Python module 실행 실패: MkDocs HTML 생략" -ForegroundColor Yellow
    }
} else {
    Write-Host "MkDocs 미설치: MkDocs HTML 생략"
}

$PublishedRoot = Join-Path $RepoRoot "Docs\CodeIntentSite"
$PublishedGraphRoot = Join-Path $PublishedRoot "graph"
$PublishedMkDocsRoot = Join-Path $PublishedRoot "mkdocs-site"

Clear-DirectoryContents $PublishedRoot
New-Item -ItemType Directory -Force -Path $PublishedGraphRoot | Out-Null
New-Item -ItemType Directory -Force -Path $PublishedMkDocsRoot | Out-Null

Copy-Item -LiteralPath (Join-Path $GraphRoot "graph.html") -Destination (Join-Path $PublishedGraphRoot "graph.html") -Force
if (Test-Path -LiteralPath (Join-Path $OutputRoot "mkdocs-site\index.html")) {
    Get-ChildItem -LiteralPath (Join-Path $OutputRoot "mkdocs-site") -Force |
        Copy-Item -Destination $PublishedMkDocsRoot -Recurse -Force
} else {
    Write-Host "MkDocs site 없음: published Docs 복사 생략" -ForegroundColor Yellow
}

Remove-TrailingWhitespace -Path $PublishedRoot

Write-CodeIntentLandingPage `
    -IndexPath (Join-Path $PublishedRoot "index.html") `
    -GraphHref "graph/graph.html" `
    -DocsHref "mkdocs-site/index.html" `
    -DoxygenHref "" `
    -ArtifactNote "이 폴더는 Git으로 공유되는 핵심 HTML이다. 팀원은 별도 도구 설치 없이 <code>Docs/CodeIntentSite/index.html</code>을 열어 graph와 문서를 확인한다. 무거운 Doxygen과 Blueprint scan output은 <code>gd-upload/CodeIntent/code-intent-heavy-artifacts.zip</code>에 보존한다."

New-CodeIntentHeavyArtifactZip -RepoRoot $RepoRoot -OutputRoot $OutputRoot

Write-Host "Code Intent site 생성: $IndexPath"
Write-Host "Code Intent published site 갱신: Docs/CodeIntentSite/index.html"
