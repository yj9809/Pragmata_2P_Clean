param(
    [switch]$RequireGenerated
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $RepoRoot

$errors = New-Object System.Collections.Generic.List[string]

function Add-Error([string]$Message) {
    $errors.Add($Message) | Out-Null
}

function Test-RepoPath([string]$Path) {
    return Test-Path -LiteralPath (Join-Path $RepoRoot $Path)
}

$requiredFiles = @(
    "Tools/CodeIntent/Doxyfile",
    "Tools/CodeIntent/clang-uml.yml",
    "Tools/CodeIntent/check-env.ps1",
    "Tools/CodeIntent/install-tools.ps1",
    "Tools/CodeIntent/tool-versions.json",
    "Tools/CodeIntent/generate-compile-database.ps1",
    "Tools/CodeIntent/build-graph.ps1",
    "Tools/CodeIntent/build-site.ps1",
    "Tools/CodeIntent/scan-blueprints.py",
    "Tools/CodeIntent/clean.ps1",
    "Docs/code-intent-style.md",
    "Docs/CodeIntentSite/index.html",
    "Docs/CodeIntentSite/graph/graph.html",
    "Docs/CodeIntentSite/mkdocs-site/index.html",
    "Docs/CodeIntentSite/mkdocs-site/Guides/code-intent-usage.html",
    "Docs/Intent/README.md",
    "Docs/Plans/PLAN-code-intent-visualization.md",
    ".agents/skills/code-intent-check/SKILL.md",
    ".agents/skills/code-intent-restore/SKILL.md",
    ".agents/skills/code-intent-write-card/SKILL.md",
    ".agents/skills/code-intent-build-site/SKILL.md",
    ".agents/skills/code-intent-organize-docs/SKILL.md",
    ".agents/skills/code-intent-scan-blueprints/SKILL.md",
    "mkdocs.yml"
)

foreach ($file in $requiredFiles) {
    if (-not (Test-RepoPath $file)) {
        Add-Error "필수 파일 없음: $file"
    }
}

$toolVersionPath = "Tools/CodeIntent/tool-versions.json"
if (Test-RepoPath $toolVersionPath) {
    try {
        $toolVersions = Get-Content -Raw -LiteralPath $toolVersionPath | ConvertFrom-Json
        foreach ($toolName in @("Doxygen", "Graphviz", "LLVM", "clang-uml")) {
            $toolEntry = @($toolVersions.windows.winget | Where-Object { $_.name -eq $toolName })[0]
            if (-not $toolEntry -or -not $toolEntry.version) {
                Add-Error "tool-versions.json version 누락: $toolName"
            }
        }
        $mkdocsEntry = @($toolVersions.windows.python | Where-Object { $_.name -eq "mkdocs-material" })[0]
        if (-not $mkdocsEntry -or -not $mkdocsEntry.version) {
            Add-Error "tool-versions.json version 누락: mkdocs-material"
        }
    }
    catch {
        Add-Error "tool-versions.json parse 실패: $($_.Exception.Message)"
    }
}

if (-not (Test-RepoPath "Docs/Intent")) {
    Add-Error "Docs/Intent 없음"
}

if (Test-RepoPath "mkdocs.yml") {
    $mkdocsConfig = Get-Content -Raw -LiteralPath "mkdocs.yml"
    if ($mkdocsConfig -notmatch "(?m)^use_directory_urls:\s*false\s*$") {
        Add-Error "mkdocs.yml use_directory_urls: false 누락"
    }
    if ($mkdocsConfig -notmatch "(?m)^exclude_docs:\s*\|[\s\S]*CodeIntentSite/\*\*") {
        Add-Error "mkdocs.yml Docs/CodeIntentSite exclude_docs 누락"
    }
    if ($mkdocsConfig -notmatch "(?m)^\s*scheme:\s*slate\s*$") {
        Add-Error "mkdocs.yml dark mode scheme 누락"
    }
    foreach ($label in @("홈", "의도 카드", "시스템", "구조", "운영", "가이드", "변경 기록", "다이어그램", "노하우")) {
        if ($mkdocsConfig -notmatch "(?m)^\s*-\s*$([regex]::Escape($label))\s*:") {
            Add-Error "mkdocs.yml 한국어 nav label 누락: $label"
        }
    }
    foreach ($legacyLabel in @("Home", "Plan", "Intent", "Systems", "Architecture", "Operations", "Guides", "Create", "Diagrams", "Know How")) {
        if ($mkdocsConfig -match "(?m)^\s*-\s*$([regex]::Escape($legacyLabel))\s*:") {
            Add-Error "mkdocs.yml 영어 nav label 잔존: $legacyLabel"
        }
    }
}

if (Test-RepoPath "Docs/CodeIntentSite/index.html") {
    $publishedIndex = Get-Content -Raw -LiteralPath "Docs/CodeIntentSite/index.html"
    if ($publishedIndex -notmatch "graph/graph\.html") {
        Add-Error "published site graph link 누락"
    }
    if ($publishedIndex -notmatch "mkdocs-site/index\.html") {
        Add-Error "published site MkDocs link 누락"
    }
    if ($publishedIndex -match "doxygen/html") {
        Add-Error "published site에 비공유 Doxygen link 잔존"
    }
}

if (Test-RepoPath "Docs/CodeIntentSite/graph/graph.html") {
    $publishedGraphHtml = Get-Content -Raw -LiteralPath "Docs/CodeIntentSite/graph/graph.html"
    if ($publishedGraphHtml -notmatch 'id="graph-data"') {
        Add-Error "published graph.html self-contained graph data 누락"
    }
    if ($publishedGraphHtml -match "fetch\s*\(") {
        Add-Error "published graph.html file:// 비호환 fetch 사용 감지"
    }
}

if (Test-RepoPath "Docs/CodeIntentSite/mkdocs-site/index.html") {
    $publishedMkDocsIndex = Get-Content -Raw -LiteralPath "Docs/CodeIntentSite/mkdocs-site/index.html"
    foreach ($label in @("홈", "의도 카드", "시스템", "운영", "가이드")) {
        if ($publishedMkDocsIndex -notmatch [regex]::Escape($label)) {
            Add-Error "published MkDocs 한국어 nav label 누락: $label"
        }
    }
    foreach ($legacyLabel in @("Home", "Intent", "Systems", "Operations", "Guides")) {
        if ($publishedMkDocsIndex -match ">$([regex]::Escape($legacyLabel))<") {
            Add-Error "published MkDocs 영어 nav label 잔존: $legacyLabel"
        }
    }
}

if (Test-RepoPath "Tools/CodeIntent/Doxyfile") {
    $doxyfileConfig = Get-Content -Raw -LiteralPath "Tools/CodeIntent/Doxyfile"
    if ($doxyfileConfig -notmatch "(?m)^HTML_COLORSTYLE\s*=\s*DARK\s*$") {
        Add-Error "Doxyfile dark HTML color style 누락"
    }
}

$requiredFields = @(
    "대상",
    "종류",
    "정의 위치",
    "이 코드가 존재하는 이유",
    "사용해야 하는 상황",
    "사용하면 안 되는 상황",
    "기본 흐름",
    "이 코드를 호출하는 쪽",
    "이 코드가 호출하는 쪽",
    "서버/클라이언트 조건",
    "Blueprint 연결 여부",
    "헷갈리기 쉬운 코드",
    "관련 문서",
    "관련 커밋",
    "확실한 근거",
    "추정한 의도",
    "확신도",
    "아직 모르는 점",
    "검증 방법",
    "다음 수정 시 주의점"
)

$cardFiles = @()
if (Test-RepoPath "Docs/Intent") {
    $cardFiles = @(Get-ChildItem -LiteralPath "Docs/Intent" -Recurse -File -Filter "*.md" |
        Where-Object { $_.Name -ne "README.md" })
}

if ($cardFiles.Count -lt 20) {
    Add-Error "기존 의도 복원 card 20개 조건 미충족: 현재 $($cardFiles.Count)개"
}

foreach ($card in $cardFiles) {
    $relativeCard = Resolve-Path -LiteralPath $card.FullName -Relative
    $text = Get-Content -Raw -LiteralPath $card.FullName

    foreach ($field in $requiredFields) {
        if ($text -notmatch [regex]::Escape($field)) {
            Add-Error "$relativeCard 필수 항목 누락: $field"
        }
    }

    if ($text -notmatch "확실한 근거\s*[\r\n]+") {
        Add-Error "$relativeCard 확실한 근거 항목 형식 확인 필요"
    }

    if ($text -notmatch "확신도\s*[\r\n]+[\s\S]*(확정|강한 추정|약한 추정|미확인)") {
        Add-Error "$relativeCard 확신도 값 누락"
    }

    $pathMatches = [regex]::Matches($text, '`(?<path>(Source|Docs|Config|Content|Tools|\.agents)/[^`]+)`')
    foreach ($match in $pathMatches) {
        $candidatePath = $match.Groups["path"].Value
        if ($candidatePath -match "\.(h|cpp|cs|md|ini|uproject|ps1|py|yml|json)$" -and -not (Test-RepoPath $candidatePath)) {
            Add-Error "$relativeCard 존재하지 않는 경로 참조: $candidatePath"
        }
    }
}

$firstCase = "Docs/Intent/Damage/ReceiveDamage.md"
if (Test-RepoPath $firstCase) {
    $firstCaseText = Get-Content -Raw -LiteralPath $firstCase
    foreach ($token in @("TakeDamage", "ReceiveDamage", "FPragmataDamageSpec", "UPragmataDamageLibrary::DealDamageToActor")) {
        if ($firstCaseText -notmatch [regex]::Escape($token)) {
            Add-Error "첫 성공 사례 누락: $firstCase 에 $token 없음"
        }
    }
}

$trackedGenerated = git ls-files Saved/CodeIntent gd-upload node_modules .venv 2>$null
if ($trackedGenerated) {
    Add-Error "generated 또는 install artifact가 git 추적 대상임: $trackedGenerated"
}

$unexpectedInstallArtifacts = @()
foreach ($path in @("Tools/CodeIntent/node_modules", "Tools/CodeIntent/.venv", "node_modules", ".venv")) {
    if (Test-RepoPath $path) {
        $unexpectedInstallArtifacts += $path
    }
}
if ($unexpectedInstallArtifacts.Count -gt 0) {
    Add-Error "repo 내부 설치 artifact 발견: $($unexpectedInstallArtifacts -join ', ')"
}

$absolutePathHits = rg -n "C:\\Users\\|C:\\Workspace\\" Tools/CodeIntent Docs/Intent Docs/Diagrams .agents/skills mkdocs.yml 2>$null
if ($absolutePathHits) {
    Add-Error "개인 절대 경로 발견"
}

$runtimePathChanges = git status --short --untracked-files=no -- Source Content Config Pragmata_2P.uproject 2>$null
if ($runtimePathChanges) {
    Add-Error "Unreal runtime 경로 변경 감지: $runtimePathChanges"
}

if ($RequireGenerated) {
    $generatedFiles = @(
        "Saved/CodeIntent/graph/code-intent-graph.json",
        "Saved/CodeIntent/graph/graph.html",
        "Saved/CodeIntent/blueprints/blueprint-asset-scan.json",
        "Saved/CodeIntent/blueprints/blueprint-asset-scan-summary.md",
        "Saved/CodeIntent/mkdocs-site/index.html"
    )

    foreach ($file in $generatedFiles) {
        if (-not (Test-RepoPath $file)) {
            Add-Error "generated output 없음: $file"
        }
    }

    if (Test-RepoPath "Saved/CodeIntent/graph/graph.html") {
        $graphHtml = Get-Content -Raw -LiteralPath "Saved/CodeIntent/graph/graph.html"
        if ($graphHtml -notmatch 'id="graph-data"') {
            Add-Error "graph.html self-contained graph data 누락"
        }
        if ($graphHtml -match "fetch\s*\(") {
            Add-Error "graph.html file:// 비호환 fetch 사용 감지"
        }
    }

    if (-not (Test-RepoPath "Saved/CodeIntent/mkdocs-site/Guides/code-intent-usage.html")) {
        Add-Error "MkDocs direct .html guide output 없음"
    }

    $heavyArtifactZip = "gd-upload/CodeIntent/code-intent-heavy-artifacts.zip"
    if (-not (Test-RepoPath $heavyArtifactZip)) {
        Add-Error "Code Intent heavy artifact zip 없음: $heavyArtifactZip"
    } else {
        try {
            Add-Type -AssemblyName System.IO.Compression.FileSystem
            $zip = [System.IO.Compression.ZipFile]::OpenRead((Join-Path $RepoRoot $heavyArtifactZip))
            try {
                $entries = @($zip.Entries | ForEach-Object { $_.FullName.Replace("\", "/") })
                if (-not ($entries | Where-Object { $_ -like "Saved/CodeIntent/doxygen/*" })) {
                    Add-Error "heavy artifact zip Doxygen output 누락"
                }
                if ((Test-RepoPath "Saved/CodeIntent/blueprints/blueprint-asset-scan.json") -and
                    -not ($entries | Where-Object { $_ -like "Saved/CodeIntent/blueprints/*" })) {
                    Add-Error "heavy artifact zip Blueprint scan output 누락"
                }
                if (-not ($entries | Where-Object { $_ -eq "Saved/CodeIntent/heavy-artifacts-manifest.json" })) {
                    Add-Error "heavy artifact zip manifest 누락"
                }
            }
            finally {
                $zip.Dispose()
            }
        }
        catch {
            Add-Error "heavy artifact zip 검사 실패: $($_.Exception.Message)"
        }
    }

    $doxygenIndex = Get-ChildItem -LiteralPath "Saved/CodeIntent" -Recurse -File -Filter "index.html" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "doxygen" } |
        Select-Object -First 1
    if (-not $doxygenIndex) {
        Add-Error "Doxygen generated index.html 없음"
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Code Intent 검증 실패" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "- $_" }
    exit 1
}

Write-Host "Code Intent 검증 통과"
if ($RequireGenerated) {
    Write-Host "Generated output 검증 포함"
}
