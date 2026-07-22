# Docs Inventory

## 목적

`Docs/` 재구성 결과와 기존 문서 위치를 추적한다.

## 분류

| 문서 | 위치 | 분류 |
| --- | --- | --- |
| `architecture.md` | `Docs/Architecture/architecture.md` | 구조 |
| `structural-ambiguity-rationale.md` | `Docs/Architecture/structural-ambiguity-rationale.md` | 구조 판단 |
| `replication-role-animation-fix.md` | `Docs/Architecture/replication-role-animation-fix.md` | replication/animation |
| `damage-flow.md` | `Docs/Systems/Damage/damage-flow.md` | Damage |
| `EnemyHacking_Integration.md` | `Docs/Systems/Hacking/enemy-hacking-integration.md` | Hacking |
| `MapHacking_Integration.md` | `Docs/Systems/Hacking/map-hacking-integration.md` | Hacking |
| `hugh-movement-component-guide.md` | `Docs/Systems/Movement/hugh-movement-component-guide.md` | Movement |
| `server.md` | `Docs/Systems/Server/server.md` | Server |
| `server-puzzle.md` | `Docs/Systems/Server/server-puzzle.md` | Server |
| `BLUEPRINT_TASKS.md` | `Docs/Operations/blueprint-tasks.md` | manual Editor 작업 |
| `packaging-guide.md` | `Docs/Operations/packaging-guide.md` | packaging |
| `unreal-build-warnings.md` | `Docs/Operations/unreal-build-warnings.md` | build |
| `history-rewrite-team-runbook.md` | `Docs/Operations/history-rewrite-team-runbook.md` | git 운영 |
| `unstaged-assets.md` | `Docs/Operations/unstaged-assets.md` | 외부 asset 보관 |
| `UNREELMCP_ANOMALIES.md` | `Docs/Operations/unreelmcp-anomalies.md` | tool anomaly |
| `unreal-coding-rules.md` | `Docs/Guides/unreal-coding-rules.md` | coding guide |
| `commit-rules.md` | `Docs/Guides/commit-rules.md` | git guide |
| `branching-rules.md` | `Docs/Guides/branching-rules.md` | git guide |
| `KNOW_HOW.md` | `Docs/KNOW_HOW.md` | 유지. directive 참조 많음 |
| `Create/` | `Docs/Create/` | changelist 기록 |
| `Plans/` | `Docs/Plans/` | 장기 계획 |

## 검증

- old `Docs/<file>.md` 참조 치환 완료
- `Docs/Create/`, `Docs/Plans/`, `Docs/KNOW_HOW.md` 유지
- `mkdocs.yml` navigation 반영
