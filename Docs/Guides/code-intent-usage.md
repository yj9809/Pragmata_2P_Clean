# Code Intent 사용 가이드

코드 연결과 개발 의도를 로컬 HTML, graph, intent card로 확인하는 방법.

## Skill 요약

| Skill | 쓸 때 | 결과 |
| --- | --- | --- |
| `code-intent-check` | 계획 상태, 누락 조건, worktree 청결 검증 | 충족/미충족, manual 검증 필요, 다음 행동 |
| `code-intent-restore` | 특정 symbol의 존재 이유와 사용처 복원 | 근거, 추정, 미확인, caller/callee, 유사 API |
| `code-intent-write-card` | public API, Blueprint-facing API, networking boundary 의도 기록 | `Docs/Intent/<Subsystem>/<Symbol>.md` card |
| `code-intent-build-site` | Doxygen, graph, MkDocs site 생성과 공유 HTML 갱신 | `Docs/CodeIntentSite/`, `Saved/CodeIntent/` |
| `code-intent-organize-docs` | `Docs/` inventory, 분류, 이동, link 검증 | 역할별 문서 구조와 link 검증 결과 |
| `code-intent-scan-blueprints` | Blueprint asset 연결 확인 | `Saved/CodeIntent/blueprints/` scan report |

짧은 요청 예시:

```text
code-intent-restore로 ReceiveDamage 의도 복원해줘
code-intent-write-card로 NewFunction 의도 카드 만들어줘
code-intent-build-site로 site 다시 만들어줘
code-intent-check로 현재 상태 검증해줘
```

## 언제 사용

- 함수나 component 사용처를 project-wide search보다 깊게 보고 싶을 때
- `TakeDamage`와 `ReceiveDamage`처럼 비슷한 API 차이를 확인할 때
- 새 public API, Blueprint-facing API, networking boundary를 추가하기 전
- AI가 만든 코드의 의도와 근거를 나중에 다시 확인해야 할 때

## 팀원 사용

도구 설치 없이 아래 파일을 연다.

```text
Docs/CodeIntentSite/index.html
```

포함 항목:

- `Docs/CodeIntentSite/graph/graph.html`
- `Docs/CodeIntentSite/mkdocs-site/index.html`

Doxygen과 Blueprint scan은 Git에 넣지 않고 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존. 이 zip은 수동 Google Drive 업로드용.
clang-uml, compile DB는 maintainer scratch output.

## maintainer 첫 설정

도구 설치와 version 확인:

```powershell
.\Tools\CodeIntent\install-tools.ps1
```

이미 설치된 도구는 skip. 새 설치가 필요하면 Windows UAC `Yes` 창이 뜰 수 있음.

## site 갱신

```powershell
.\Tools\CodeIntent\build-site.ps1
```

주요 출력:

- `Docs/CodeIntentSite/index.html`
- `Docs/CodeIntentSite/graph/graph.html`
- `Docs/CodeIntentSite/mkdocs-site/index.html`
- `Saved/CodeIntent/site/index.html`
- `Saved/CodeIntent/mkdocs-site/index.html`
- `Saved/CodeIntent/doxygen/html/index.html`
- `Saved/CodeIntent/graph/graph.html`
- `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`

먼저 열 파일:

```text
Docs/CodeIntentSite/index.html
```

이 시작 페이지는 dark mode 기본값. 사람이 바로 읽는 입구는 Graph와 Docs.

## 읽는 순서

| 목적 | 먼저 볼 곳 |
| --- | --- |
| 시작 페이지 | `Docs/CodeIntentSite/index.html` |
| 연결 graph 검색 | `Docs/CodeIntentSite/graph/graph.html` |
| 문서 검색 | `Docs/CodeIntentSite/mkdocs-site/index.html` |
| 함수와 class 상세 reference | `Saved/CodeIntent/doxygen/html/index.html` |
| Google Drive 업로드용 heavy artifact | `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` |
| 의도 카드 목록 | `Docs/Intent/README.md` |
| 작성 규칙 | `Docs/code-intent-style.md` |
| 전체 계획과 완료 조건 | `Docs/Plans/PLAN-code-intent-visualization.md` |

MkDocs는 로컬 파일로 열어도 왼쪽 목차가 `.html` 파일로 바로 이동. 폴더 화면이 뜨면 `mkdocs.yml`의 `use_directory_urls: false`가 빠진 상태.

## 검증

생성 결과까지 확인:

```powershell
.\Tools\CodeIntent\validate.ps1 -RequireGenerated
```

통과 의미:

- intent card 20개 이상 존재
- 필수 항목 누락 없음
- 공유용 `Docs/CodeIntentSite/` HTML 존재
- Doxygen과 Blueprint scan이 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`에 보존됨
- graph, Blueprint scan, MkDocs site output 존재
- graph HTML이 `file://`에서 JSON fetch 없이 자체 동작
- MkDocs가 직접 이동 가능한 `.html` output 생성
- generated output이 `Saved/CodeIntent/` 밖으로 새지 않음
- repo 내부에 `.venv`, `node_modules`, installer 없음
- Unreal `Source`, `Content`, `Config`, `.uproject` 변경 없음

## Blueprint scan

Blueprint asset 연결을 다시 확인할 때 사용.

조건:

- Unreal Editor 실행
- UnreelMCP 연결
- Python 실행 가능 상태

짧은 요청:

```text
code-intent-scan-blueprints로 Blueprint 연결 scan 해줘
```

출력:

- `Saved/CodeIntent/blueprints/blueprint-asset-scan.json`
- `Saved/CodeIntent/blueprints/blueprint-asset-scan-summary.md`

## 의도 카드 갱신

새 API나 헷갈리는 symbol이 생기면 card 추가.

짧은 요청 예시:

```text
code-intent-restore로 ReceiveDamage 의도 복원해줘
code-intent-write-card로 NewFunction 의도 카드 만들어줘
code-intent-check로 code intent 상태 검증해줘
```

card 작성 위치:

```text
Docs/Intent/<Subsystem>/<Symbol>.md
```

## 정리

generated output 삭제:

```powershell
.\Tools\CodeIntent\clean.ps1
```

삭제 대상은 `Saved/CodeIntent/`와 임시 파일. source, asset, tracked docs는 삭제하지 않음.
`Docs/CodeIntentSite/`는 팀 공유용 HTML이므로 삭제하지 않음.
`gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`도 삭제하지 않음.

## 주의

- 이 workflow는 Unreal build, playtest, packaging에 자동으로 끼어들지 않음.
- generated output은 git에 넣지 않음.
- 팀에 공유할 핵심 파일은 `Docs/CodeIntentSite/`, `Docs/`, `Tools/CodeIntent/`, `.agents/skills/code-intent-*`, `mkdocs.yml`, `.gitignore`, `AGENTS.md`.
- 팀원 PC 실제 열람 검증은 clean clone에서 `Docs/CodeIntentSite/index.html`을 여는 것으로 확인.
- Doxygen과 Blueprint scan 공유는 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`을 Google Drive에 수동 업로드해 처리.
- maintainer 재생성 검증은 `install-tools.ps1`, `build-site.ps1`, `validate.ps1 -RequireGenerated` 순서로 확인.
