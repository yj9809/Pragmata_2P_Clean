$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $RepoRoot

$OutputRoot = Join-Path $RepoRoot "Saved\CodeIntent\graph"
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$symbols = @(
    @{ Id = "FPragmataDamageSpec"; Kind = "struct"; Area = "Damage" },
    @{ Id = "UPragmataDamageLibrary::DealDamageToActor"; Pattern = "DealDamageToActor"; Kind = "function"; Area = "Damage" },
    @{ Id = "IPragmataDamageable::ReceiveDamage"; Pattern = "ReceiveDamage"; Kind = "function"; Area = "Damage" },
    @{ Id = "UPragmataHealthComponent::ReceiveDamage"; Pattern = "PragmataHealthComponent::ReceiveDamage"; Kind = "function"; Area = "Damage" },
    @{ Id = "AWeaponBase::DealDamageToHitActor"; Pattern = "DealDamageToHitActor"; Kind = "function"; Area = "Damage" },
    @{ Id = "UAnimNotifyState_AttackHitWindow"; Kind = "class"; Area = "Damage" },
    @{ Id = "UDianaHackComponent"; Kind = "component"; Area = "Hacking" },
    @{ Id = "UDianaHackComponent::RequestHack"; Pattern = "RequestHack"; Kind = "function"; Area = "Hacking" },
    @{ Id = "UDianaHackComponent::RequestHackDirection"; Pattern = "RequestHackDirection"; Kind = "function"; Area = "Hacking" },
    @{ Id = "UDianaHackComponent::Server_RequestHack"; Pattern = "Server_RequestHack"; Kind = "rpc"; Area = "Hacking" },
    @{ Id = "UDianaHackComponent::Server_ProcessEnemyHackDirection"; Pattern = "Server_ProcessEnemyHackDirection"; Kind = "rpc"; Area = "Hacking" },
    @{ Id = "UEnemyHackingPuzzleComponent"; Kind = "component"; Area = "Hacking" },
    @{ Id = "UEnemyHackingPuzzleComponent::StartHack"; Pattern = "StartHack"; Kind = "function"; Area = "Hacking" },
    @{ Id = "UEnemyHackingPuzzleComponent::ProcessDirectionInput"; Pattern = "ProcessDirectionInput"; Kind = "function"; Area = "Hacking" },
    @{ Id = "UEnemyHackingPuzzleComponent::CompleteHack"; Pattern = "CompleteHack"; Kind = "function"; Area = "Hacking" },
    @{ Id = "UMapHackingPuzzleComponent"; Kind = "component"; Area = "Hacking" },
    @{ Id = "UDianaNodeInteractionComponent"; Kind = "component"; Area = "Hacking" },
    @{ Id = "ADianaOperatorPawn"; Kind = "class"; Area = "Hacking" },
    @{ Id = "UDianaScanComponent"; Kind = "component"; Area = "Hacking" },
    @{ Id = "AHughCharacter"; Kind = "class"; Area = "Movement" },
    @{ Id = "UWaistArmComponent"; Kind = "component"; Area = "Movement" },
    @{ Id = "APragmataGameState"; Kind = "class"; Area = "Server" },
    @{ Id = "APragmataPlayerController"; Kind = "class"; Area = "UI" },
    @{ Id = "UPragmataHUDWidget"; Kind = "widget"; Area = "UI" }
)

$sourceFiles = Get-ChildItem -Path "Source\Pragmata_2P" -Recurse -Include *.h,*.cpp -File
$nodes = [ordered]@{}
$edges = New-Object System.Collections.Generic.List[object]

foreach ($symbol in $symbols) {
    $id = $symbol.Id
    $pattern = if ($symbol.Pattern) { $symbol.Pattern } else { ($id -split "::")[-1] }
    $fileMatches = @()
    foreach ($file in $sourceFiles) {
        $text = Get-Content -Raw -LiteralPath $file.FullName
        if ($text -match [regex]::Escape($pattern)) {
            $fileMatches += $file.FullName.Substring($RepoRoot.Path.Length + 1).Replace("\", "/")
        }
    }

    $nodes[$id] = [ordered]@{
        id = $id
        kind = $symbol.Kind
        area = $symbol.Area
        pattern = $pattern
        files = @($fileMatches | Sort-Object -Unique)
    }
}

foreach ($symbol in $symbols) {
    $id = $symbol.Id
    foreach ($file in $nodes[$id].files) {
        $fileId = "file:$file"
        if (-not $nodes.Contains($fileId)) {
            $nodes[$fileId] = [ordered]@{
                id = $fileId
                kind = "file"
                area = "Source"
                path = $file
                files = @($file)
            }
        }
        $edges.Add([ordered]@{ from = $fileId; to = $id; relation = "mentions" }) | Out-Null
    }
}

foreach ($from in $symbols) {
    $fromId = $from.Id
    foreach ($file in $nodes[$fromId].files) {
        $text = Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $file)
        foreach ($to in $symbols) {
            $toId = $to.Id
            if ($fromId -eq $toId) { continue }
            $toPattern = if ($to.Pattern) { $to.Pattern } else { ($toId -split "::")[-1] }
            if ($text -match [regex]::Escape($toPattern)) {
                $edges.Add([ordered]@{ from = $fromId; to = $toId; relation = "co_occurs_in_definition_file"; via = $file }) | Out-Null
            }
        }
    }
}

$uniqueEdges = [ordered]@{}
foreach ($edge in $edges) {
    $key = "$($edge.from)|$($edge.to)|$($edge.relation)|$($edge.via)"
    if (-not $uniqueEdges.Contains($key)) {
        $uniqueEdges[$key] = $edge
    }
}

$graph = [ordered]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    source = "Tools/CodeIntent/build-graph.ps1"
    nodes = @($nodes.Values)
    edges = @($uniqueEdges.Values)
    default_depth = 1
    supported_depth = 2
}

$jsonPath = Join-Path $OutputRoot "code-intent-graph.json"
$graph | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$mmdPath = Join-Path $OutputRoot "code-intent-graph.mmd"
$mmd = New-Object System.Collections.Generic.List[string]
$mmd.Add("flowchart LR") | Out-Null
foreach ($edge in $graph.edges) {
    $fromSafe = ($edge.from -replace "[^A-Za-z0-9_]", "_")
    $toSafe = ($edge.to -replace "[^A-Za-z0-9_]", "_")
    $mmd.Add("  $fromSafe[`$($edge.from)`] -->|$($edge.relation)| $toSafe[`$($edge.to)`]") | Out-Null
}
$mmd | Set-Content -LiteralPath $mmdPath -Encoding UTF8

$graphHtmlPath = Join-Path $OutputRoot "graph.html"
$graphJson = ($graph | ConvertTo-Json -Depth 8) -replace "</script", "<\/script"
$graphHtml = @'
<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Code Intent Graph</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0b1020;
      --panel: #111827;
      --panel-2: #162033;
      --line: #334155;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --function: #5eead4;
      --component: #a78bfa;
      --class: #fbbf24;
      --file: #94a3b8;
      --rpc: #fb7185;
      --widget: #f472b6;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background: var(--bg);
      color: var(--text);
      font-family: "Segoe UI", "Noto Sans KR", system-ui, sans-serif;
    }
    header {
      display: flex;
      gap: 16px;
      align-items: end;
      justify-content: space-between;
      padding: 20px 24px 14px;
      border-bottom: 1px solid #1f2937;
      background: #0f172a;
    }
    h1 { margin: 0; font-size: 22px; letter-spacing: 0; }
    .subtitle { margin-top: 4px; color: var(--muted); font-size: 13px; }
    .controls {
      display: grid;
      grid-template-columns: minmax(260px, 520px) 112px;
      gap: 10px;
      align-items: center;
    }
    input, select {
      width: 100%;
      border: 1px solid #334155;
      border-radius: 8px;
      background: #020617;
      color: var(--text);
      padding: 10px 12px;
      font: inherit;
    }
    input:focus, select:focus { outline: 2px solid rgba(56, 189, 248, .45); }
    main {
      display: grid;
      grid-template-columns: minmax(0, 1fr) 360px;
      min-height: calc(100vh - 82px);
    }
    .graph {
      position: relative;
      overflow: auto;
      min-height: calc(100vh - 82px);
      background:
        linear-gradient(#162033 1px, transparent 1px),
        linear-gradient(90deg, #162033 1px, transparent 1px);
      background-size: 32px 32px;
    }
    #edges {
      position: absolute;
      inset: 0;
      min-width: 1120px;
      min-height: 720px;
      pointer-events: none;
    }
    #nodes {
      position: relative;
      min-width: 1120px;
      min-height: 720px;
    }
    .node {
      position: absolute;
      width: 212px;
      min-height: 72px;
      border: 1px solid #334155;
      border-left-width: 4px;
      border-radius: 8px;
      background: rgba(15, 23, 42, .94);
      box-shadow: 0 14px 34px rgba(0, 0, 0, .28);
      padding: 10px 12px;
      cursor: pointer;
    }
    .node:hover, .node.selected { border-color: var(--accent); }
    .node.function { border-left-color: var(--function); }
    .node.component { border-left-color: var(--component); }
    .node.class { border-left-color: var(--class); }
    .node.file { border-left-color: var(--file); }
    .node.rpc { border-left-color: var(--rpc); }
    .node.widget { border-left-color: var(--widget); }
    .label {
      font-weight: 700;
      font-size: 13px;
      line-height: 1.25;
      overflow-wrap: anywhere;
    }
    .meta {
      margin-top: 6px;
      color: var(--muted);
      font-size: 12px;
      overflow-wrap: anywhere;
    }
    aside {
      border-left: 1px solid #1f2937;
      background: var(--panel);
      padding: 18px;
      overflow: auto;
    }
    .stats {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
      margin-bottom: 16px;
    }
    .stat {
      border: 1px solid #263348;
      border-radius: 8px;
      background: var(--panel-2);
      padding: 10px;
    }
    .stat b { display: block; font-size: 18px; }
    .stat span { color: var(--muted); font-size: 11px; }
    h2 { margin: 18px 0 8px; font-size: 15px; }
    ul { padding-left: 18px; margin: 8px 0; }
    li { margin: 6px 0; overflow-wrap: anywhere; }
    code {
      color: #bae6fd;
      background: #020617;
      border: 1px solid #1f2937;
      border-radius: 5px;
      padding: 1px 4px;
    }
    .muted { color: var(--muted); }
    .empty {
      position: absolute;
      top: 32px;
      left: 32px;
      max-width: 520px;
      border: 1px solid #334155;
      border-radius: 8px;
      background: rgba(15, 23, 42, .96);
      padding: 18px;
      color: var(--muted);
    }
    @media (max-width: 980px) {
      header { display: block; }
      .controls { grid-template-columns: 1fr; margin-top: 12px; }
      main { grid-template-columns: 1fr; }
      aside { border-left: 0; border-top: 1px solid #1f2937; }
    }
  </style>
</head>
<body>
  <header>
    <div>
      <h1>Code Intent Graph</h1>
      <div class="subtitle">symbol, file, area 검색. 검색 결과는 관련 노드까지 함께 표시.</div>
    </div>
    <div class="controls">
      <input id="q" placeholder="예: ReceiveDamage, DianaHackComponent, HUD, WeaponBase">
      <select id="depth" title="검색 결과 주변 연결 깊이">
        <option value="1">1-hop</option>
        <option value="2" selected>2-hop</option>
      </select>
    </div>
  </header>
  <main>
    <section class="graph" id="graphPane">
      <svg id="edges"></svg>
      <div id="nodes"></div>
      <div id="empty" class="empty" hidden></div>
    </section>
    <aside>
      <div class="stats">
        <div class="stat"><b id="visibleNodeCount">0</b><span>표시 노드</span></div>
        <div class="stat"><b id="visibleEdgeCount">0</b><span>표시 연결</span></div>
        <div class="stat"><b id="matchCount">0</b><span>검색 일치</span></div>
      </div>
      <div id="details"></div>
    </aside>
  </main>
  <script id="graph-data" type="application/json">
__GRAPH_JSON__
  </script>
  <script>
    const graph = JSON.parse(document.getElementById('graph-data').textContent);
    const byId = new Map(graph.nodes.map(node => [node.id, node]));
    const q = document.getElementById('q');
    const depthInput = document.getElementById('depth');
    const nodeLayer = document.getElementById('nodes');
    const edgeSvg = document.getElementById('edges');
    const details = document.getElementById('details');
    const empty = document.getElementById('empty');
    const visibleNodeCount = document.getElementById('visibleNodeCount');
    const visibleEdgeCount = document.getElementById('visibleEdgeCount');
    const matchCount = document.getElementById('matchCount');
    let selectedId = null;

    const kindLabel = {
      function: 'function',
      component: 'component',
      class: 'class',
      file: 'file',
      rpc: 'RPC',
      widget: 'widget',
      struct: 'struct',
      caller: 'caller'
    };

    const links = new Map();
    for (const node of graph.nodes) {
      links.set(node.id, []);
    }
    for (const edge of graph.edges) {
      if (links.has(edge.from)) links.get(edge.from).push({ id: edge.to, edge, direction: 'out' });
      if (links.has(edge.to)) links.get(edge.to).push({ id: edge.from, edge, direction: 'in' });
    }

    function escapeHtml(value) {
      return String(value ?? '')
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;')
        .replaceAll("'", '&#39;');
    }

    function textForSearch(node) {
      return [
        node.id,
        node.kind,
        node.area,
        node.pattern,
        node.path,
        ...(node.files || [])
      ].join(' ').toLowerCase();
    }

    function shortLabel(id) {
      const clean = id.startsWith('file:') ? id.slice(5) : id;
      if (clean.length <= 58) return clean;
      return `${clean.slice(0, 28)}...${clean.slice(-24)}`;
    }

    function relationLabel(edge) {
      if (edge.relation === 'mentions') return '파일에서 언급';
      if (edge.relation === 'co_occurs_in_definition_file') return '같은 구현 파일';
      return edge.relation || '연결';
    }

    function collectVisible(term, maxDepth) {
      const matches = term
        ? graph.nodes.filter(node => textForSearch(node).includes(term))
        : graph.nodes.filter(node => node.kind !== 'file');

      if (term && matches.length === 0) {
        return { nodes: [], edges: [], matches, distances: new Map() };
      }

      const selected = new Map();
      const distances = new Map();
      const queue = [];
      const seedNodes = term ? matches : matches.slice(0, 80);

      for (const node of seedNodes) {
        selected.set(node.id, node);
        distances.set(node.id, 0);
        queue.push({ id: node.id, depth: 0 });
      }

      while (queue.length > 0) {
        const current = queue.shift();
        if (current.depth >= maxDepth) continue;
        for (const link of links.get(current.id) || []) {
          const linkedNode = byId.get(link.id);
          if (!linkedNode) continue;
          if (!selected.has(link.id)) {
            selected.set(link.id, linkedNode);
            distances.set(link.id, current.depth + 1);
            queue.push({ id: link.id, depth: current.depth + 1 });
          }
        }
      }

      const visibleIds = new Set(selected.keys());
      const visibleEdges = graph.edges.filter(edge => visibleIds.has(edge.from) && visibleIds.has(edge.to));
      return { nodes: Array.from(selected.values()), edges: visibleEdges, matches, distances };
    }

    function layout(nodes, distances, term) {
      const positions = new Map();
      const width = Math.max(1120, Math.ceil(Math.sqrt(nodes.length || 1)) * 260);
      const height = Math.max(720, Math.ceil((nodes.length || 1) / 4) * 128);

      if (term) {
        const centerX = width / 2 - 106;
        const centerY = height / 2 - 36;
        const byDistance = new Map();
        for (const node of nodes) {
          const distance = distances.get(node.id) || 0;
          if (!byDistance.has(distance)) byDistance.set(distance, []);
          byDistance.get(distance).push(node);
        }

        for (const [distance, group] of byDistance) {
          const radius = distance === 0 ? 0 : 210 + (distance - 1) * 230;
          group.forEach((node, index) => {
            if (distance === 0 && group.length === 1) {
              positions.set(node.id, { x: centerX, y: centerY });
              return;
            }
            const angle = (Math.PI * 2 * index / group.length) - Math.PI / 2;
            const localRadius = distance === 0 ? Math.max(90, group.length * 34) : radius;
            positions.set(node.id, {
              x: Math.max(24, centerX + Math.cos(angle) * localRadius),
              y: Math.max(24, centerY + Math.sin(angle) * localRadius)
            });
          });
        }
      } else {
        const areas = Array.from(new Set(nodes.map(node => node.area || 'Other'))).sort();
        const columns = Math.max(1, areas.length);
        const columnWidth = Math.max(236, width / columns);
        for (const [areaIndex, area] of areas.entries()) {
          const group = nodes.filter(node => (node.area || 'Other') === area);
          group.forEach((node, index) => {
            positions.set(node.id, {
              x: 24 + areaIndex * columnWidth,
              y: 32 + index * 112
            });
          });
        }
      }

      return { positions, width, height };
    }

    function renderDetails(node, visibleEdges) {
      if (!node) {
        details.innerHTML = `
          <h2>선택 없음</h2>
          <p class="muted">노드를 클릭하면 파일, 호출 관계, 연결 이유를 여기서 확인.</p>
          <h2>읽는 순서</h2>
          <ul>
            <li>검색어 입력</li>
            <li>중앙 노드와 주변 노드 확인</li>
            <li>필요하면 연결된 파일을 열어 실제 사용 방식 확인</li>
          </ul>`;
        return;
      }

      const related = visibleEdges
        .filter(edge => edge.from === node.id || edge.to === node.id)
        .slice(0, 40);
      const files = node.files || (node.path ? [node.path] : []);
      details.innerHTML = `
        <h2>${escapeHtml(node.id)}</h2>
        <p class="muted">${escapeHtml(kindLabel[node.kind] || node.kind)} / ${escapeHtml(node.area || '-')}</p>
        <h2>관련 파일</h2>
        ${files.length ? `<ul>${files.map(file => `<li><code>${escapeHtml(file)}</code></li>`).join('')}</ul>` : '<p class="muted">파일 정보 없음</p>'}
        <h2>표시된 연결</h2>
        ${related.length ? `<ul>${related.map(edge => `<li><code>${escapeHtml(edge.from)}</code> -> <code>${escapeHtml(edge.to)}</code><br><span class="muted">${escapeHtml(relationLabel(edge))}${edge.via ? ` / ${escapeHtml(edge.via)}` : ''}</span></li>`).join('')}</ul>` : '<p class="muted">현재 화면에 연결 없음</p>'}`;
    }

    function render() {
      const term = q.value.trim().toLowerCase();
      const maxDepth = Number(depthInput.value);
      const visible = collectVisible(term, maxDepth);
      const { positions, width, height } = layout(visible.nodes, visible.distances, term);
      const selectedNode = visible.nodes.find(node => node.id === selectedId) || visible.nodes[0] || null;
      selectedId = selectedNode ? selectedNode.id : null;

      nodeLayer.style.minWidth = `${width}px`;
      nodeLayer.style.minHeight = `${height}px`;
      edgeSvg.setAttribute('width', width);
      edgeSvg.setAttribute('height', height);
      edgeSvg.style.minWidth = `${width}px`;
      edgeSvg.style.minHeight = `${height}px`;

      visibleNodeCount.textContent = visible.nodes.length;
      visibleEdgeCount.textContent = visible.edges.length;
      matchCount.textContent = visible.matches.length;

      if (visible.nodes.length === 0) {
        nodeLayer.innerHTML = '';
        edgeSvg.innerHTML = '';
        empty.hidden = false;
        empty.innerHTML = `<strong>검색 결과 없음</strong><br>다른 symbol, file, area 이름으로 검색. 예: <code>ReceiveDamage</code>, <code>Hack</code>, <code>HUD</code>`;
        renderDetails(null, []);
        return;
      }

      empty.hidden = true;
      const edgeMarkup = visible.edges.map(edge => {
        const from = positions.get(edge.from);
        const to = positions.get(edge.to);
        if (!from || !to) return '';
        const x1 = from.x + 106;
        const y1 = from.y + 36;
        const x2 = to.x + 106;
        const y2 = to.y + 36;
        return `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="#475569" stroke-width="1.4" opacity=".78"><title>${escapeHtml(relationLabel(edge))}</title></line>`;
      }).join('');
      edgeSvg.innerHTML = edgeMarkup;

      nodeLayer.innerHTML = visible.nodes.map(node => {
        const pos = positions.get(node.id) || { x: 24, y: 24 };
        const files = node.files?.length ? `${node.files.length} files` : '';
        return `
          <button class="node ${escapeHtml(node.kind || 'other')} ${node.id === selectedId ? 'selected' : ''}"
            style="left:${pos.x}px; top:${pos.y}px"
            data-id="${escapeHtml(node.id)}"
            title="${escapeHtml(node.id)}">
            <div class="label">${escapeHtml(shortLabel(node.id))}</div>
            <div class="meta">${escapeHtml(kindLabel[node.kind] || node.kind || '-')} / ${escapeHtml(node.area || '-')}${files ? ` / ${escapeHtml(files)}` : ''}</div>
          </button>`;
      }).join('');

      for (const element of nodeLayer.querySelectorAll('.node')) {
        element.addEventListener('click', () => {
          selectedId = element.dataset.id;
          render();
        });
      }

      renderDetails(selectedNode, visible.edges);
    }
    q.addEventListener('input', render);
    depthInput.addEventListener('change', render);
    const initialSearch = new URLSearchParams(window.location.search).get('q');
    if (initialSearch) q.value = initialSearch;
    render();
  </script>
</body>
</html>
'@
$graphHtml.Replace("__GRAPH_JSON__", $graphJson) | Set-Content -LiteralPath $graphHtmlPath -Encoding UTF8

Write-Host "Code intent graph 생성: Saved/CodeIntent/graph/code-intent-graph.json"
