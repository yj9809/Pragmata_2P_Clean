# PLAN: 코드 연결 시각화와 개발 의도 보존

## 목적

이 문서는 한 번의 AI agent session에 의존하지 않고, 팀이 계속 이어서 진행할 실행 계획과 점검표다.

해결할 문제:

- component가 어디서 쓰이고 어디와 연결되는지 빠르게 확인
- function이 어디서 호출되고 무엇을 호출하는지 빠르게 확인
- 기존 코드의 개발 의도를 git history, 기존 문서, 코드 사용처로 복원
- 앞으로 작성하는 코드의 개발 의도를 짧은 comment 또는 구조화된 문서로 보존
- `TakeDamage`와 `ReceiveDamage`처럼 의미가 겹치는 코드가 생겼을 때 중복과 차이를 즉시 식별
- 프로젝트 전체 검색만으로는 알 수 없는 호출 깊이, 사용 맥락, 의존 방향을 graph로 추적
- 긴 Markdown 원문을 직접 읽지 않고, 검색 가능한 로컬 HTML과 graph 화면으로 읽기
- 반복 작업은 project-local skill로 만들어 긴 prompt를 다시 쓰지 않기
- 기존 `Docs/` 문서를 역할별로 재구성해 찾기 쉬운 지식 구조로 만들기

## 현재 검증 상태

2026-07-05 Ralph 검증 기준.

완료:

- [x] `Tools/CodeIntent/` scaffold 생성
- [x] `check-env.ps1`, `install-tools.ps1`, `generate-compile-database.ps1`, `build-graph.ps1`, `build-site.ps1`, `validate.ps1`, `clean.ps1` 생성
- [x] `Tools/CodeIntent/tool-versions.json`에 고정 tool version 기록
- [x] `Docs/Intent/` 핵심 intent card 20개 이상 생성
- [x] `Docs/Diagrams/` damage flow Mermaid source 생성
- [x] `.agents/skills/code-intent-*` 필수 skill 6개 생성
- [x] `Docs/` 역할별 재구성 적용
- [x] `mkdocs.yml` navigation 생성
- [x] `Docs/Operations/docs-inventory.md` 생성
- [x] Doxygen, Graphviz, LLVM/clangd, clang-uml, MkDocs 설치와 version 확인
- [x] `compile_commands.json` 생성 script 검증
- [x] Doxygen/MkDocs local site 생성 검증
- [x] 맞춤 symbol graph JSON/HTML 생성 검증
- [x] `file://`로 열어도 graph 검색, MkDocs 목차 direct link, dark mode 기본값 동작 검증
- [x] 팀 공유용 핵심 HTML을 `Docs/CodeIntentSite/`에 publish
- [x] Doxygen과 Blueprint scan heavy output을 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존
- [x] UnreelMCP 기반 Blueprint asset read-only scan 검증
- [x] generated output이 `Saved/CodeIntent/` 아래에만 생성되는지 확인
- [x] `clean.ps1`로 generated output 제거 확인
- [x] Unreal `Source/`, `Content/`, `Config/`, `.uproject` 변경 없음 확인
- [x] `Tools/CodeIntent/validate.ps1` 통과
- [x] `Tools/CodeIntent/validate.ps1 -RequireGenerated` 통과
- [x] `git diff --check` 통과

운영 확인 필요:

- [ ] 팀원 PC 실제 clean clone 실행
  - 담당: 팀
  - 막힌 이유: agent가 다른 팀원 PC에 접근할 수 없음
  - 다음 행동: 팀원이 clean clone에서 `Docs/CodeIntentSite/index.html` 직접 열람
  - agent 완료 판정 근거: tracked static HTML, repo-relative link, generated output ignore, 개인 절대 경로 검사 통과

- [ ] `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` Google Drive 수동 업로드
  - 담당: 팀
  - 막힌 이유: agent가 사용자의 Google Drive에 직접 업로드하지 않음
  - 다음 행동: 사용자가 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` 업로드

제약 기록:

- `winget --silent`와 `pip --user`로 설치 prompt를 줄였지만, non-elevated Windows session의 UAC `Yes` prompt를 agent가 완전히 제거할 수는 없다.
- clang-uml은 설치와 실행 시도를 완료했으나 Unreal/PCH/LLVM 조합에서 실패할 수 있다. 완료 판정은 Doxygen과 맞춤 `build-graph.ps1` graph를 필수 경로로 두고, clang-uml은 보조 diagram 생성기로 둔다.

## 누락 방지 대조표

아래 항목은 빠지면 안 된다. 각 항목은 이 문서의 진행 항목에 반드시 연결되어야 한다.

| 요구 | 반영 위치 | 완료 기준 |
| --- | --- | --- |
| 무료 유지 | `원칙`, `도구 선택`, `0. 준비` | 유료 도구나 cloud 전용 서비스 없이 실행 가능 |
| component 연결 시각화 | `1. 자동 코드 참조`, `2. subsystem diagram`, `5. 로컬 지식 사이트` | class/component의 참조 방향과 관련 문서 확인 가능 |
| function 연결 시각화 | `1. 자동 코드 참조`, `5. 로컬 지식 사이트`, `7. 자동 검증` | caller, callee, 1-hop, 2-hop 탐색 가능 |
| 기존 개발 의도 복원 | `4. 기존 코드 의도 복원` | 근거, 추정, 미확인 항목 분리 |
| future intent 보존 | `6. 향후 의도 보존 규칙` | 새 API 검토 때 의도 기록 여부 확인 |
| Markdown 가독성 문제 해결 | `원칙`, `5. 로컬 지식 사이트` | Markdown 원문 없이 검색과 graph로 탐색 가능 |
| 한국어 문서 원칙 | `원칙`, `8. 문서 품질 차단선` | code와 proper noun 외 설명은 한국어 |
| 애매한 용어 제거 | `핵심 용어`, `의도 카드 형식` | `canonical path` 같은 표현 금지 |
| 중복/유사 API 방지 | `3. 의도 카드 형식`, `7. 자동 검증` | 비슷한 이름과 비슷한 책임의 symbol 점검 |
| Blueprint 참조 누락 방지 | `원칙`, `2. subsystem diagram`, `위험과 대응` | C++ graph 밖 연결은 별도 표시 |
| AI 추정 오염 방지 | `원칙`, `4. 기존 코드 의도 복원`, `8. 문서 품질 차단선` | 추정은 확신도와 근거로 분리 |
| 진행 추적 | `진행 추적표`, `주간 운영 방식` | checkbox와 담당자가 남음 |
| Unreal 개발 비간섭 | `Unreal 개발 비간섭 규칙`, `반려 조건`, `최종 완료 조건` | build, Editor, packaging, gameplay workflow를 바꾸지 않음 |
| 팀원 PC 재현성 | `팀원 PC 재현 규칙`, `0. 준비`, `9. 재현성 검증` | clean clone에서 별도 설치 없이 tracked HTML 열람 |
| worktree 청결 | `worktree 청결 규칙`, `7. 자동 검증`, `최종 완료 조건` | 설치 파일, 임시 파일, generated output이 git status를 오염시키지 않음 |
| 반복 작업 skill화 | `project-local skill 규칙`, `10. skill 생성` | 긴 prompt 없이 짧은 명령으로 반복 업무 실행 |
| `Docs/` 재구성 | `Docs 재구성 규칙`, `11. Docs 재구성` | 기존 문서를 잃지 않고 역할별 구조로 이동 |
| tool version 기록 | `팀원 PC 재현 규칙`, `목표 산출물`, `7. 자동 검증` | `Tools/CodeIntent/tool-versions.json`에 설치한 version과 명령 기록 |
| 설치 자동화 | `0. 준비`, `팀원 PC 재현 규칙` | `Tools/CodeIntent/install-tools.ps1`로 Windows 설치 경로 제공 |

## 원칙

- 무료 도구만 사용한다. 상용 SaaS, 유료 IDE extension, cloud 전용 서비스는 기본 선택지에서 제외한다.
- Markdown은 저장 형식으로만 쓴다. 실제 읽기 화면은 로컬 HTML 사이트와 graph 화면으로 제공한다.
- code, symbol, proper noun, tool name을 제외한 설명 문장은 한국어로 작성한다.
- AI가 추정한 의도를 사실처럼 쓰지 않는다. 반드시 근거, 추정, 확신도를 분리한다.
- 자동 graph는 출발점이지 최종 진실이 아니다. Unreal macro, Blueprint asset, runtime binding은 수동 검증을 병행한다.
- 모든 function을 문서화하지 않는다. public API, Blueprint-facing API, networking boundary, Unreal 기본 흐름을 대체하는 custom flow를 우선한다.
- 비슷한 이름 또는 비슷한 책임의 symbol이 있으면 `헷갈리기 쉬운 코드` 항목을 반드시 작성한다.
- 새 문서가 길어지면 내용을 늘리는 대신 card, graph, subsystem page로 분리한다.
- Unreal 개발 흐름을 바꾸지 않는다. 이 계획의 도구는 기본적으로 읽기 전용 분석과 문서 생성만 수행한다.
- 설치 파일, cache, 임시 파일, generated output은 git worktree를 오염시키지 않는다.
- 두 번 이상 반복될 작업은 project-local skill 후보로 본다.
- skill은 `.agents/skills/` 아래에 두고, 긴 prompt 대신 짧은 trigger로 실행 가능해야 한다.
- 기존 `Docs/` 문서는 버리지 않는다. 먼저 목록화, 분류, link 점검을 끝낸 뒤 이동한다.

## 반려 조건

아래 상태면 해당 단계는 완료로 표시하지 않는다.

- 유료 도구가 없으면 재현할 수 없음
- Markdown 원문을 직접 열어야만 핵심 정보를 이해할 수 있음
- `확실한 근거` 없이 AI 추정을 사실처럼 기록
- caller만 있고 callee가 없거나, callee만 있고 caller가 없음
- 1-hop 사용처만 있고 2-hop 의존 깊이를 볼 방법이 없음
- `TakeDamage`와 `ReceiveDamage`처럼 비슷한 책임의 symbol 차이를 설명하지 않음
- Blueprint 연결 여부를 모름으로 남기면서 `미확인` 표시가 없음
- 설명 문장에 불필요한 영어 일반 표현이 남음
- 진행 checkbox는 있지만 담당자, 막힌 이유, 다음 행동이 없음
- Unreal `Source/`, `Content/`, `Config/`, `.uproject`, `.Build.cs`를 분석 도구 설정 때문에 바꿈
- Unreal Editor 실행, build, packaging, playtest 속도를 기본 개발 흐름에서 느리게 만듦
- 한 명의 PC 절대 경로, 사용자 이름, 개인 설치 위치에 의존함
- `node_modules/`, `.venv/`, installer, cache, 임시 파일이 repo 안에 생김
- generated output 생성 후 `git status --short`에 불필요한 파일이 남음
- 반복 작업인데 skill 없이 긴 prompt를 계속 요구함
- skill이 너무 길어져 일반 prompt보다 읽기 어렵거나, 사용 조건이 불명확함
- `Docs/` 파일을 이동했는데 기존 link, 참조, 검색 경로가 깨짐
- 문서 재구성이 대량 rename만 남기고 실제 탐색성을 개선하지 않음

## Unreal 개발 비간섭 규칙

- 기본 동작은 읽기 전용이다. `Source/`, `Content/`, `Config/`, `.uproject`, `.Build.cs`는 분석 결과 생성을 위해 수정하지 않는다.
- Unreal Editor 자동 실행을 기본값으로 두지 않는다. Blueprint 참조 보강이 필요할 때도 별도 수동 단계로 분리한다.
- build, packaging, playtest command에 code intent 생성 과정을 자동으로 끼워 넣지 않는다.
- CI나 pre-commit에 붙이기 전에는 수동 명령으로만 실행한다. 자동화는 팀이 속도 영향을 확인한 뒤 별도 승인한다.
- 재생성용 분석 output은 `Saved/CodeIntent/` 아래에 둔다.
- 팀원이 열람하는 핵심 HTML은 `Docs/CodeIntentSite/`에 publish하고 Git으로 추적한다.
- Doxygen과 Blueprint scan처럼 재생성 비용이 큰 output은 Git에 넣지 않고 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존한다.
- `Intermediate/`, `Binaries/`, `Saved/`, `DerivedDataCache/`, generated header, `.uasset`, `.umap`은 분석 입력에서 제외하거나 읽기 전용으로만 다룬다.
- 장시간 분석은 Unreal build나 Editor 작업과 동시에 돌리지 않는 운영 규칙을 둔다.

## 팀원 PC 재현 규칙

- 모든 설정은 repo-relative path만 사용한다. `C:\Users\...`, 개인 drive, 개인 Unreal 설치 경로를 문서나 설정에 고정하지 않는다.
- `Tools/CodeIntent/README.md`에 Windows 기준 설치 명령, 버전 확인 명령, 첫 실행 명령을 기록한다.
- `Tools/CodeIntent/check-env.ps1`로 필수 도구 존재와 version을 확인한다.
- `Tools/CodeIntent/build-site.ps1` 하나로 Doxygen, diagram, index, site 생성을 실행할 수 있게 한다.
- 새 팀원은 clean clone에서 도구 설치 없이 `Docs/CodeIntentSite/index.html`을 열 수 있어야 한다.
- 핵심 HTML 갱신 담당자만 설치 문서와 script로 같은 output을 만들 수 있으면 된다.
- tool version은 최소 version 또는 검증된 version range로 기록한다.
- 최종 검증에 사용한 tool version은 `Tools/CodeIntent/tool-versions.json`에 고정해 기록한다.
- 설치 자동화는 `Tools/CodeIntent/install-tools.ps1`가 담당한다.
- Windows UAC 승인은 non-elevated shell에서 완전 자동화하지 않는다. 필요 시 관리자 PowerShell에서 실행해 prompt 횟수를 줄인다.
- Python package, Node package, binary tool 설치는 repo 밖 user/global 위치를 기본으로 한다.
- repo 안에 설치해야 하는 예외가 생기면 이유, 경로, ignore 규칙, 삭제 명령을 먼저 문서화한다.

## worktree 청결 규칙

- repo에 남겨도 되는 파일은 source, 설정, 문서, script뿐이다.
- 재생성용 generated output은 `Saved/CodeIntent/`에 둔다.
- 공유용 핵심 HTML은 `Docs/CodeIntentSite/`에 둔다.
- Doxygen과 Blueprint scan 보존 zip은 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`에 둔다.
- 임시 파일은 `%TEMP%` 또는 `Saved/CodeIntent/tmp/`에 만들고 작업 종료 시 삭제한다.
- `node_modules/`, `.venv/`, installer, downloaded archive, package cache를 repo 안에 만들지 않는다.
- `Tools/CodeIntent/clean.ps1`로 `Saved/CodeIntent/`와 임시 파일을 삭제할 수 있게 한다.
- 실행 전후 `git status --short`를 비교해 새로 생긴 불필요 파일이 없는지 확인한다.
- `.gitignore`에는 `Saved/CodeIntent/`와 예외적으로 필요한 local-only 경로만 추가한다.

## project-local skill 규칙

- 반복될 가능성이 높은 작업은 구현과 동시에 `.agents/skills/`에 project-local skill로 만든다.
- skill은 긴 배경 설명을 반복하지 않도록 핵심 절차, 입력, 출력, 검증만 담는다.
- skill의 `description`에는 언제 자동으로 써야 하는지 명확히 적는다.
- skill 본문은 500줄을 넘기지 않는다. 긴 참고 내용은 `references/`로 분리한다.
- skill 안에 `README.md`, 설치 가이드, changelog 같은 보조 문서를 만들지 않는다.
- script가 필요한 skill은 `scripts/`에 넣고 대표 실행을 검증한다.
- skill 생성과 수정은 `skill-creator` 지침을 따른다.
- skill은 이 계획의 도구를 대신 설치하지 않는다. 설치와 산출물 생성은 `Tools/CodeIntent/` script가 담당한다.

## Docs 재구성 규칙

- 기존 문서를 삭제하지 않는다. 먼저 inventory, 분류, 이동 계획, link 점검을 만든다.
- 문서 이동은 code intent 도구가 안정된 뒤 별도 PR 또는 별도 commit으로 처리한다.
- 이동 전후 `rg`로 기존 경로 참조를 찾아 갱신한다.
- `Docs/Create/`는 changelist 기록으로 유지한다. subsystem 문서와 섞지 않는다.
- `Docs/Plans/`는 진행 계획과 장기 작업 추적만 둔다.
- 문서 재구성은 읽기 경로를 줄이기 위한 작업이어야 한다. 단순 폴더 꾸미기로 끝내지 않는다.
- 이동된 문서는 새 위치에서 첫 화면에 목적, 현재 상태, 관련 graph 또는 intent card link를 둔다.

## 핵심 용어

- `심볼`: class, struct, enum, function, property, interface처럼 코드에서 이름으로 참조되는 대상.
- `연결 지도`: 심볼이 호출하거나 참조하는 대상과, 해당 심볼을 호출하거나 참조하는 대상을 보여주는 graph.
- `의도 카드`: 특정 심볼이나 기능 흐름에 대해 존재 이유, 사용 조건, 금지 조건, 근거를 정리한 짧은 문서 단위.
- `기본 흐름`: 해당 기능이 정상적으로 동작할 때의 호출 순서와 책임 분담.
- `확신도`: `확정`, `강한 추정`, `약한 추정`, `미확인` 중 하나.
- `1-hop`: 선택한 symbol과 직접 연결된 caller 또는 callee.
- `2-hop`: 1-hop symbol과 다시 직접 연결된 caller 또는 callee.
- `유사 API`: 이름, parameter, 반환값, 호출 위치, 책임이 비슷해 혼동될 수 있는 function 또는 class.

## 도구 선택

| 목적 | 도구 | 사용 이유 |
| --- | --- | --- |
| C++ reference와 caller/callee graph | Doxygen + Graphviz | 무료이고 C++ code reference와 graph 생성이 안정적 |
| component/class/sequence diagram | clang-uml | C++ AST 기반 diagram 생성, Mermaid, PlantUML, JSON 출력 지원 |
| 빠른 symbol 이동과 사용처 확인 | clangd | `compile_commands.json` 기반 C++ symbol 분석 가능 |
| 읽기용 로컬 사이트 | MkDocs Material | Markdown을 검색 가능한 HTML로 렌더링 가능 |
| 맞춤 graph 화면 | Cytoscape.js 또는 D3.js | function/component graph 접기, 펼치기, 깊이 제한 구현 가능 |
| 의도 복원 근거 수집 | git log, git blame, rg | 비용 없이 기존 history와 문서 추적 가능 |

## 제외한 선택지

| 선택지 | 제외 이유 |
| --- | --- |
| Swimm | 의도 보존 제품에 가깝지만 무료 유지 원칙과 장기 비용 조건에 맞지 않음 |
| CodeScene | 분석 기능은 강하지만 상용 서비스 중심 |
| Understand, CppDepend | 강력하지만 유료 제품 |
| Sourcetrail | 목적은 맞지만 archived 상태라 장기 유지 위험 |
| 순수 Markdown 문서 | 분량이 늘면 읽기 품질이 급격히 떨어지는 문제가 이미 확인됨 |

## 목표 산출물

| 경로 | 추적 여부 | 내용 |
| --- | --- | --- |
| `Docs/Intent/` | 추적 | 사람이 쓰는 한국어 의도 카드 원문 |
| `Docs/Diagrams/` | 추적 | 수동 보정 가능한 Mermaid 또는 PlantUML source |
| `Docs/Architecture/` | 추적 | architecture, 구조 판단, cross-cutting reasoning |
| `Docs/Systems/` | 추적 | Damage, Hacking, Movement, Server, UI 같은 subsystem 문서 |
| `Docs/Operations/` | 추적 | packaging, build warning, history rewrite, unstaged asset 같은 운영 문서 |
| `Docs/Guides/` | 추적 | 사람이 따라 하는 규칙, coding rule, branch rule, commit rule |
| `Tools/CodeIntent/` | 추적 | graph, index, site 생성 script와 설정 |
| `Tools/CodeIntent/README.md` | 추적 | 팀원 PC 재현용 설치와 실행 절차 |
| `Tools/CodeIntent/check-env.ps1` | 추적 | 필수 도구와 version 확인 |
| `Tools/CodeIntent/build-site.ps1` | 추적 | 전체 산출물 생성 명령 |
| `Tools/CodeIntent/tool-versions.json` | 추적 | 팀원 PC 재현용 고정 도구 version과 설치 명령 |
| `Tools/CodeIntent/install-tools.ps1` | 추적 | 고정 version 기준 Windows 설치 자동화 |
| `Tools/CodeIntent/scan-blueprints.py` | 추적 | UnreelMCP Python 기반 Blueprint asset 참조 scan |
| `Tools/CodeIntent/clean.ps1` | 추적 | generated output과 임시 파일 정리 |
| `Docs/CodeIntentSite/` | 추적 | 팀원이 설치 없이 여는 핵심 HTML site |
| `Saved/CodeIntent/` | 비추적 | maintainer 재생성용 Doxygen, generated graph JSON, local site output |
| `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` | 비추적 | Google Drive 업로드용 Doxygen과 Blueprint scan 보존 zip |
| `Docs/code-intent-style.md` | 추적 | 의도 카드 작성 규칙과 comment 작성 규칙 |
| `Docs/Plans/PLAN-code-intent-visualization.md` | 추적 | 이 실행 계획과 진행 현황 |
| `.agents/skills/code-intent-check/` | 추적 | 계획 상태, worktree 청결, 누락 조건 점검 skill |
| `.agents/skills/code-intent-restore/` | 추적 | git/docs/code 사용처 기반 의도 복원 skill |
| `.agents/skills/code-intent-write-card/` | 추적 | 의도 카드 생성과 갱신 skill |
| `.agents/skills/code-intent-build-site/` | 추적 | local site와 graph 생성 workflow 실행 skill |
| `.agents/skills/code-intent-organize-docs/` | 추적 | `Docs/` inventory, 분류, 이동, link 검증 skill |
| `.agents/skills/code-intent-scan-blueprints/` | 추적 | Blueprint asset 연결 scan workflow 실행 skill |

`Saved/CodeIntent/`는 generated output으로 취급한다. 핵심 HTML만 `Docs/CodeIntentSite/`에 publish해 Git으로 공유한다.
Doxygen과 Blueprint scan은 Git에 넣지 않고 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 묶어 Google Drive에 수동 업로드한다.

## 구현 구조

1. 분석 계층
   - Doxygen이 class, file, include, caller, callee 정보를 생성한다.
   - clang-uml이 subsystem 단위 component diagram과 sequence diagram을 생성한다.
   - custom script가 `Docs/Intent/`, `Docs/Create/`, `Docs/KNOW_HOW.md`, git history를 읽어 symbol별 근거 index를 만든다.

2. 자료 계층
   - symbol별 JSON index를 만든다.
   - 각 항목에는 `symbol`, `kind`, `definition`, `callers`, `callees`, `related_docs`, `related_commits`, `similar_symbols`, `intent_card`, `confidence`를 둔다.

3. 읽기 계층
   - MkDocs Material 사이트에서 한국어 문서를 검색하고 탐색한다.
   - Doxygen HTML로 symbol reference를 연다.
   - 맞춤 graph 화면에서 component/function 연결을 접고 펼친다.
   - graph는 기본 1-hop, 선택 2-hop, 필요 시 전체 펼치기를 제공한다.

4. 작업 계층
   - 새 public API, Blueprint-facing API, networking boundary, Unreal 기본 흐름을 대체하는 custom flow는 intent card 또는 intent comment를 요구한다.
   - AI agent와 개발자는 구현 전 graph와 intent card를 확인하고, 구현 후 변경된 의도를 갱신한다.

## 진행 추적표

상태 표기는 `[ ]` 미시작, `[~]` 진행 중, `[x]` 완료로 통일한다.

각 항목은 필요 시 아래 줄을 추가한다.

```text
담당:
막힌 이유:
다음 행동:
```

### 0. 준비

- [x] `Doxygen`, `Graphviz`, `clangd`, `Python`, `Node.js` 설치 상태 확인
- [x] Unreal project에서 `compile_commands.json` 생성 방법 확인
- [x] `Saved/CodeIntent/`가 git ignore 대상인지 확인
- [x] 첫 분석 대상 subsystem을 `Damage Flow`로 확정
- [ ] 계획 담당자와 주간 검토 주기 확정
- [ ] 기존 `Docs/` 문서 부채 목록 작성
- [ ] 기존 `Docs/`에서 중복, 오래된 내용, 근거 없는 추정 후보 표시
- [ ] 기존 `Docs/` 파일을 `Architecture`, `Systems`, `Operations`, `Guides`, `Plans`, `Create` 후보로 분류
- [x] `Tools/CodeIntent/README.md` 재현 절차 초안 작성
- [x] `Tools/CodeIntent/check-env.ps1` 요구사항 확정
- [x] `Tools/CodeIntent/tool-versions.json`에 검증된 tool version 기록
- [x] `Tools/CodeIntent/install-tools.ps1`로 설치 자동화
- [x] `Tools/CodeIntent/clean.ps1` 요구사항 확정
- [x] repo-relative path만 쓰는지 확인
- [x] Unreal 개발 흐름에 자동으로 끼어드는 hook이 없는지 확인
- [x] 반복 작업 skill 후보 6개 확정

완료 기준:

- [x] agent 검증 기준에서 필요한 무료 도구를 설치하고 같은 결과를 만들 수 있다.
- [x] generated output이 tracked file을 오염시키지 않는다.
- [x] `Damage Flow`를 첫 샘플로 분석하기로 팀이 합의했다.
- [ ] 기존 문서 부채를 추적할 목록이 있다.
- [x] clean clone 기준 실행 절차가 있다.
- [x] Unreal build, Editor, packaging 흐름에 영향이 없다.
- [x] `Docs/` 재구성 후보 표가 있다.
- [x] skill 후보마다 trigger, 입력, 출력, 검증 기준이 있다.

### 1. 자동 코드 참조와 caller/callee graph

- [ ] `Tools/CodeIntent/Doxyfile` 추가
- [ ] `Source/` 입력 설정
- [ ] `Intermediate/`, `Binaries/`, `Saved/`, generated folder 제외 설정
- [ ] `Content/` asset은 기본 분석 입력에서 제외하고 필요 시 별도 읽기 전용 단계로 분리
- [ ] Unreal generated header와 engine source 제외 설정
- [ ] `HAVE_DOT` 활성화
- [ ] `CALL_GRAPH`, `CALLER_GRAPH` 활성화
- [ ] `INCLUDE_GRAPH` 활성화
- [ ] `REFERENCES_RELATION`, `REFERENCED_BY_RELATION` 활성화
- [ ] `Saved/CodeIntent/doxygen/html/`에 첫 HTML output 생성
- [ ] output이 `Saved/CodeIntent/` 밖에 생성되지 않는지 확인
- [ ] `ReceiveDamage` reference page 확인
- [ ] `DealDamageToActor` reference page 확인
- [ ] `FPragmataDamageSpec` reference page 확인
- [ ] caller와 callee가 양방향으로 보이는지 확인
- [ ] 1-hop graph와 2-hop graph를 구분해 볼 수 있는지 확인

완료 기준:

- [ ] `ReceiveDamage` page에서 정의 위치, 호출하는 쪽, 호출되는 쪽을 볼 수 있다.
- [ ] `TakeDamage`와 `ReceiveDamage`의 차이를 추적할 link path가 있다.
- [ ] 자동 분석 한계를 문서에 표시할 수 있다.
- [ ] project-wide search보다 깊은 의존 추적이 가능하다.

### 2. subsystem diagram

- [ ] `clang-uml` 설치와 실행 확인
- [ ] `Tools/CodeIntent/clang-uml.yml` 추가
- [ ] `Damage Flow` diagram 생성
- [ ] `Diana Hack / Node Interaction` diagram 생성
- [ ] `Hugh Movement Intent` diagram 생성
- [ ] 자동 diagram이 읽기 어려운 부분을 `Docs/Diagrams/` source로 보정
- [ ] diagram node에서 Doxygen page 또는 intent card로 이동하는 link 정책 확정
- [ ] Blueprint asset 연결이 graph 밖에 있을 때 별도 marker 표시
- [ ] diagram이 code와 어긋났을 때 불일치 표시 방식 확정

완료 기준:

- [ ] 개발자가 `Damage Flow` diagram만 보고 damage 정보 생성자와 수신자를 설명할 수 있다.
- [ ] diagram node에서 관련 문서로 이동할 수 있다.
- [ ] code와 어긋난 diagram을 불일치 상태로 표시하거나 재생성할 수 있다.
- [ ] C++ graph 밖의 Blueprint 연결을 놓치지 않는다.
- [ ] diagram 생성이 Unreal project file이나 asset을 수정하지 않는다.

### 3. 의도 카드 형식

- [ ] `Docs/code-intent-style.md` 작성
- [ ] `Docs/Intent/` 생성
- [ ] 의도 카드 template 작성
- [ ] 확신도 표기 규칙 작성
- [ ] AI 추정 내용 표기 규칙 작성
- [ ] 유사 API 탐지 규칙 작성
- [ ] `ReceiveDamage` 의도 카드 작성
- [ ] `DealDamageToActor` 의도 카드 작성
- [ ] `FPragmataDamageSpec` 의도 카드 작성
- [ ] `TakeDamage`를 쓰지 않는 이유와 관련 근거 연결
- [ ] `TakeDamage`와 `ReceiveDamage`의 차이를 `헷갈리기 쉬운 코드`에 명시

의도 카드 필수 항목:

- [ ] `대상`
- [ ] `종류`
- [ ] `정의 위치`
- [ ] `이 코드가 존재하는 이유`
- [ ] `사용해야 하는 상황`
- [ ] `사용하면 안 되는 상황`
- [ ] `기본 흐름`
- [ ] `이 코드를 호출하는 쪽`
- [ ] `이 코드가 호출하는 쪽`
- [ ] `서버/클라이언트 조건`
- [ ] `Blueprint 연결 여부`
- [ ] `헷갈리기 쉬운 코드`
- [ ] `관련 문서`
- [ ] `관련 커밋`
- [ ] `확실한 근거`
- [ ] `추정한 의도`
- [ ] `확신도`
- [ ] `아직 모르는 점`
- [ ] `검증 방법`
- [ ] `다음 수정 시 주의점`

완료 기준:

- [ ] 첫 세 개 intent card가 작성되어 있다.
- [ ] 추정한 내용은 `추정한 의도`와 `확신도`에 분리되어 있다.
- [ ] `TakeDamage` 혼동을 줄이는 설명이 존재한다.
- [ ] 비슷한 symbol을 새로 만들 때 기존 symbol과 비교할 기준이 있다.

### 4. 기존 코드 의도 복원

우선순위:

- [ ] `Damage Flow`
- [ ] `Hugh movement intent/state`
- [ ] `Diana scan/hack/node interaction`
- [ ] `GameFlow`, `session`, `travel`
- [ ] `UI`, `HUD`, `ViewModel` ownership

복원 절차:

- [ ] `git log -S<symbol>`로 symbol 도입 commit 확인
- [ ] `git log -G<pattern>`으로 관련 흐름 변경 확인
- [ ] `git blame`으로 현재 코드의 마지막 변경자와 변경 시점 확인
- [ ] `Docs/Create/`에서 before/after와 검증 기록 확인
- [ ] `Docs/KNOW_HOW.md`에서 관련 recipe 확인
- [ ] `Docs/Architecture/architecture.md`에서 관련 architecture note 확인
- [ ] 실제 코드 사용처를 열어 호출 의미 확인
- [ ] 호출 chain을 2-hop 이상 따라가며 의미가 바뀌는 지점 확인
- [ ] 비슷한 이름이나 책임의 symbol 후보 목록 작성
- [ ] 근거 없는 추정은 `약한 추정` 또는 `미확인`으로 표시

완료 기준:

- [ ] 상위 20개 핵심 symbol에 intent card가 있다.
- [ ] 각 card에는 최소 하나 이상의 근거가 있다.
- [ ] 미확인 의도 목록이 따로 남아 있다.
- [ ] 중복 또는 유사 API 후보 목록이 있다.

### 5. 로컬 지식 사이트

- [ ] `mkdocs.yml` 추가
- [ ] MkDocs Material 설치 방법 문서화
- [ ] subsystem 중심 navigation 구성
- [ ] code symbol 검색 가능 여부 확인
- [ ] intent card page에서 Doxygen page link 제공
- [ ] intent card page에서 diagram link 제공
- [ ] intent card page에서 관련 commit link 제공
- [ ] intent card page에서 관련 docs link 제공
- [ ] 맞춤 graph page prototype 추가
- [ ] graph에서 1-hop, 2-hop, 전체 펼치기 제공
- [ ] graph에서 caller와 callee 방향을 색 또는 화살표로 구분
- [ ] graph에서 유사 API 후보를 경고 표시
- [ ] `ReceiveDamage` 검색 흐름 검증

완료 기준:

- [ ] Markdown 원문을 직접 열지 않아도 주요 정보를 읽을 수 있다.
- [ ] 로컬 사이트에서 `ReceiveDamage` 검색 시 의도, graph, 관련 문서, 관련 commit이 이어진다.
- [ ] 긴 문서는 subsystem page로 분리되고, symbol page는 짧은 card 중심으로 유지된다.
- [ ] project-wide search보다 적은 클릭으로 사용 맥락을 파악할 수 있다.

### 6. 향후 의도 보존 규칙

- [ ] review checklist에 intent 갱신 항목 추가
- [ ] AI agent 작업 지시에 intent card 확인 규칙 추가
- [ ] 새 public function 작성 시 intent comment 필요 여부 판단 규칙 추가
- [ ] 새 Blueprint-facing function 작성 시 intent card 필요 여부 판단 규칙 추가
- [ ] networking boundary 변경 시 architecture note 필요 여부 판단 규칙 추가
- [ ] 기존 symbol과 이름이 비슷한 새 symbol 작성 시 `헷갈리기 쉬운 코드` 항목 작성 규칙 추가
- [ ] Unreal 기본 함수나 engine flow를 대체하는 custom function 작성 시 의도 카드 필수화
- [ ] AI가 작성한 코드에는 작성 당시의 선택 이유를 남기는 규칙 추가

완료 기준:

- [ ] 새 API가 추가될 때 graph와 intent card 갱신 여부를 review에서 확인한다.
- [ ] Unreal 기본 흐름을 대체하는 custom flow는 의도 기록 없이 merge하지 않는다.
- [ ] code comment는 길게 쓰지 않고, 문서 link 또는 짧은 의도 설명으로 제한한다.
- [ ] 작성자가 기억에 의존하지 않아도 의도 근거를 확인할 수 있다.

### 7. 자동 검증

- [ ] `Tools/CodeIntent/validate.ps1` 또는 동등한 script 추가
- [ ] intent card의 `정의 위치`가 실제 파일에 존재하는지 확인
- [ ] `관련 문서` link가 깨졌는지 확인
- [ ] Doxygen generation 성공 여부 확인
- [ ] clang-uml generation 성공 여부 확인
- [ ] 고위험 subsystem symbol에 intent card가 없는지 report
- [ ] diagram 불일치 후보 report
- [ ] 유사 API 후보 report
- [ ] 새 public API 중 intent 기록이 없는 symbol report
- [ ] `Blueprint 연결 여부`가 비어 있는 card report
- [ ] `확신도`가 없는 추정 문장 report
- [ ] 실행 전후 `git status --short` 비교
- [ ] `Saved/CodeIntent/` 밖 generated output report
- [ ] repo 안 installation artifact report
- [ ] absolute path 포함 여부 report
- [ ] Unreal 개발 경로 변경 여부 report

완료 기준:

- [ ] 한 명령으로 code intent report를 만들 수 있다.
- [ ] report에 broken link, missing symbol, diagram mismatch, missing high-risk intent card가 표시된다.
- [ ] report에 유사 API 후보와 의도 기록 누락 후보가 표시된다.
- [ ] report에 worktree 오염 후보가 표시된다.
- [ ] 실패 원인이 사람이 고칠 수 있는 단위로 표시된다.

### 8. 문서 품질 차단선

- [ ] code, symbol, proper noun, tool name 외 설명 문장의 영어 표현 점검
- [ ] `canonical path` 같은 애매한 용어 금지 목록 작성
- [ ] 긴 문서가 card나 subsystem page로 분리되었는지 점검
- [ ] 각 page가 첫 화면에서 목적과 다음 행동을 보여주는지 점검
- [ ] `확실한 근거`, `추정한 의도`, `아직 모르는 점`이 섞이지 않았는지 점검
- [ ] 사람이 읽는 화면에서 표, 목록, graph 중심으로 읽히는지 점검

완료 기준:

- [ ] 문서 설명은 한국어 중심이다.
- [ ] 모호한 용어는 정의 없이 사용하지 않는다.
- [ ] 긴 Markdown 원문을 직접 읽지 않아도 된다.
- [ ] 의도 복원 결과가 사실과 추정으로 분리되어 있다.

### 9. 재현성 검증

- [ ] 팀원 1명 이상이 clean clone에서 `Docs/CodeIntentSite/index.html`을 바로 열람
- [ ] `check-env.ps1` 실패 메시지가 설치해야 할 도구와 해결 방법을 알려주는지 확인
- [ ] maintainer PC에서 `build-site.ps1`가 개인 PC 절대 경로 없이 실행되는지 확인
- [ ] `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`이 Doxygen과 Blueprint scan을 포함하는지 확인
- [ ] 생성된 site에서 `ReceiveDamage` 검색이 동일하게 동작하는지 확인
- [ ] `clean.ps1` 실행 후 generated output이 제거되는지 확인
- [ ] 실행 전후 `git status --short`가 의도한 tracked 문서와 설정 외 파일을 보여주지 않는지 확인
- [ ] Unreal Editor를 켜지 않은 상태에서도 기본 site 생성이 되는지 확인
- [ ] Unreal build와 packaging 설정 파일이 변경되지 않았는지 확인

완료 기준:

- [x] agent 검증: 개인 경로 의존 없이 tracked HTML 열람과 maintainer 갱신 절차가 설명된다.
- [ ] 팀 운영 검증: 내 PC가 아닌 팀원 PC에서 tracked HTML을 바로 열 수 있다.
- [x] 개인 경로 의존이 없다.
- [x] worktree에 설치 파일, cache, 임시 파일이 남지 않는다.
- [x] Unreal 개발 흐름에 영향이 없다.

### 10. skill 생성

반복 작업을 긴 prompt로 다시 설명하지 않기 위한 필수 단계다.

필수 skill:

- [x] `.agents/skills/code-intent-check/SKILL.md`
  - trigger: code intent 계획 점검, 누락 검증, worktree 청결 검증 요청
  - 입력: 계획 문서, `git status --short`, generated output 경로
  - 출력: 누락 항목, 반려 조건 위반, 다음 행동
  - 검증: 이 문서의 `누락 방지 대조표`와 `최종 완료 조건`을 모두 대조
- [x] `.agents/skills/code-intent-restore/SKILL.md`
  - trigger: 특정 symbol의 개발 의도 복원 요청
  - 입력: symbol 이름, 관련 file path, optional subsystem
  - 출력: 근거, 추정, 미확인, 관련 commit, caller/callee 요약
  - 검증: `git log -S`, `git log -G`, `git blame`, 기존 `Docs/` 확인 포함
- [x] `.agents/skills/code-intent-write-card/SKILL.md`
  - trigger: intent card 생성 또는 갱신 요청
  - 입력: symbol 또는 subsystem
  - 출력: `Docs/Intent/` card 초안 또는 갱신안
  - 검증: 필수 항목 누락 없음, 추정과 사실 분리
- [x] `.agents/skills/code-intent-build-site/SKILL.md`
  - trigger: local site, graph, code intent report 생성 요청
  - 입력: optional subsystem 또는 전체
  - 출력: `Saved/CodeIntent/` generated output
  - 검증: `git status --short` 청결, output 경로 제한
- [x] `.agents/skills/code-intent-organize-docs/SKILL.md`
  - trigger: `Docs/` 정리, 문서 이동, link 검증 요청
  - 입력: `Docs/` 파일 목록, 목표 분류
  - 출력: 이동 계획, link 갱신 목록, 검증 결과
  - 검증: 기존 문서 손실 없음, 깨진 link 없음
- [x] `.agents/skills/code-intent-scan-blueprints/SKILL.md`
  - trigger: Blueprint asset 연결 scan, `Blueprint 연결 여부` 보강 요청
  - 입력: Unreal Editor + UnreelMCP, `Tools/CodeIntent/scan-blueprints.py`
  - 출력: `Saved/CodeIntent/blueprints/blueprint-asset-scan.json`, summary markdown
  - 검증: read-only scan, `.uasset`/`.umap` 변경 없음

분리 후보:

- [ ] `.agents/skills/code-intent-find-duplicates/SKILL.md`
  - 생성 조건: 유사 API 탐지가 반복 작업으로 분리될 만큼 자주 발생할 때
  - 기본 방침: 초기에는 `code-intent-check`에 포함

skill 작성 기준:

- [ ] 각 skill은 `SKILL.md` 하나로 시작
- [ ] 필요할 때만 `references/` 또는 `scripts/` 추가
- [ ] 보조 `README.md`, changelog, 설치 문서 금지
- [ ] `description`에 사용 조건 명시
- [ ] project-local skill 목록이 `AGENTS.md` 또는 해당 manifest에서 발견 가능
- [ ] 대표 요청 1개로 skill 동작 검증

완료 기준:

- [ ] 반복 작업을 긴 prompt 없이 skill 이름이나 짧은 요청으로 실행할 수 있다.
- [ ] 각 skill이 worktree 청결, 한국어 문서, Unreal 비간섭 규칙을 따른다.
- [ ] skill이 실제 도구 실행을 대신 상상하지 않고, 필요한 script와 검증을 호출한다.

### 11. Docs 재구성

기존 `Docs/`는 code intent workflow의 입력 자료다. 정리하지 않으면 site와 intent card가 오래된 문서를 계속 노출한다.

현재 문서 분류 초안:

| 현재 문서 | 후보 위치 | 판단 |
| --- | --- | --- |
| `Docs/Architecture/architecture.md` | `Docs/Architecture/architecture.md` | broad architecture |
| `Docs/Architecture/structural-ambiguity-rationale.md` | `Docs/Architecture/structural-ambiguity-rationale.md` | 구조 판단 근거 |
| `Docs/Architecture/replication-role-animation-fix.md` | `Docs/Architecture/replication-role-animation-fix.md` | networking/animation reasoning |
| `Docs/Systems/Damage/damage-flow.md` | `Docs/Systems/Damage/damage-flow.md` | subsystem flow |
| `Docs/Systems/Hacking/enemy-hacking-integration.md` | `Docs/Systems/Hacking/enemy-hacking-integration.md` | subsystem integration |
| `Docs/Systems/Hacking/map-hacking-integration.md` | `Docs/Systems/Hacking/map-hacking-integration.md` | subsystem integration |
| `Docs/Systems/Movement/hugh-movement-component-guide.md` | `Docs/Systems/Movement/hugh-movement-component-guide.md` | subsystem guide |
| `Docs/Systems/Server/server.md` | `Docs/Systems/Server/server.md` | server subsystem |
| `Docs/Systems/Server/server-puzzle.md` | `Docs/Systems/Server/server-puzzle.md` | server puzzle flow |
| `Docs/Operations/blueprint-tasks.md` | `Docs/Operations/blueprint-tasks.md` 또는 유지 | manual task 목록 |
| `Docs/Operations/packaging-guide.md` | `Docs/Operations/packaging-guide.md` | packaging 운영 |
| `Docs/Operations/unreal-build-warnings.md` | `Docs/Operations/unreal-build-warnings.md` | build 운영 |
| `Docs/Operations/history-rewrite-team-runbook.md` | `Docs/Operations/history-rewrite-team-runbook.md` | git 운영 |
| `Docs/Operations/unstaged-assets.md` | `Docs/Operations/unstaged-assets.md` | asset 운영 |
| `Docs/Operations/unreelmcp-anomalies.md` | `Docs/Operations/unreelmcp-anomalies.md` | tooling anomaly |
| `Docs/Guides/unreal-coding-rules.md` | `Docs/Guides/unreal-coding-rules.md` | coding guide |
| `Docs/Guides/commit-rules.md` | `Docs/Guides/commit-rules.md` | git guide |
| `Docs/Guides/branching-rules.md` | `Docs/Guides/branching-rules.md` | git guide |
| `Docs/KNOW_HOW.md` | 유지 또는 `Docs/Knowledge/KNOW_HOW.md` | 기존 directive와 참조가 많아 별도 판단 |
| `Docs/Create/` | 유지 | changelist 기록 |
| `Docs/Plans/` | 유지 | 계획 문서 |

작업 순서:

- [ ] `Docs/` 전체 inventory 생성
- [ ] 각 문서의 최신성, 중복, 참조 빈도 표시
- [ ] 이동 전 link map 작성
- [ ] `rg`로 기존 path 참조 검색
- [ ] 이동 계획을 별도 review 후 적용
- [ ] 이동 후 모든 내부 link 갱신
- [ ] local site navigation에 새 구조 반영
- [ ] `Docs/KNOW_HOW.md`는 directive 영향이 있으므로 마지막에 별도 판단
- [ ] 이동 후 `git diff --check` 실행
- [ ] 이동 후 `git status --short`에서 예상 문서 이동만 보이는지 확인

완료 기준:

- [ ] 문서가 역할별 폴더로 분리되어 탐색 비용이 줄었다.
- [ ] 기존 문서 내용 손실이 없다.
- [ ] 내부 link가 깨지지 않았다.
- [ ] code intent site에서 새 구조를 그대로 탐색할 수 있다.
- [ ] `AGENTS.md`와 directive의 기존 Docs 경로 참조가 깨지지 않는다.

## 첫 성공 사례: `TakeDamage`와 `ReceiveDamage` 혼동 제거

이 계획의 첫 성공 사례는 `TakeDamage`와 `ReceiveDamage` 혼동을 제거하는 것이다.

완료 조건:

- [x] `ReceiveDamage`가 왜 존재하는지 한국어로 설명되어 있다.
- [x] Unreal 기본 `TakeDamage`와 무엇이 다른지 설명되어 있다.
- [x] `ReceiveDamage`를 호출하는 쪽과 호출되는 쪽을 graph로 볼 수 있다.
- [x] `ReceiveDamage`의 1-hop caller/callee를 볼 수 있다.
- [x] `ReceiveDamage`의 2-hop caller/callee를 볼 수 있다.
- [x] `FPragmataDamageSpec`가 어떤 정보를 추가로 전달하는지 설명되어 있다.
- [x] 관련 commit, 기존 문서, code location이 연결되어 있다.
- [x] `TakeDamage`를 다시 추가하려는 개발자가 먼저 읽어야 할 warning이 있다.
- [x] 유사 API 후보 탐지에서 두 symbol의 차이가 표시된다.
- [x] `Blueprint 연결 여부`가 확인 또는 `미확인`으로 표시되어 있다.

이 사례가 통과하면 같은 구조를 다른 subsystem으로 확장한다.

## 주간 운영 방식

- 매주 한 번 이 문서의 checkbox를 갱신한다.
- 완료된 항목은 `[x]`로 바꾸고, 진행 중인 항목은 `[~]`로 표시한다.
- 막힌 항목은 해당 checkbox 아래에 `막힌 이유:`를 한 줄로 남긴다.
- 다음 행동이 불명확한 항목은 완료로 표시하지 않는다.
- 새로 발견한 위험은 `위험과 대응`에 추가한다.
- 구현 중 새로 만든 script나 문서가 있으면 `목표 산출물` 표에 실제 경로를 반영한다.
- 요구가 추가되면 `누락 방지 대조표`에 먼저 추가한 뒤 세부 todo에 연결한다.
- 반복 작업이 보이면 먼저 skill 후보인지 판단한다.
- 문서가 늘어나면 `Docs/` 재구성 기준에 맞는 위치를 먼저 확인한다.

## 최종 완료 조건

- [x] 무료 도구만으로 전체 흐름 실행 가능
- [x] component 연결 graph 확인 가능
- [x] function caller/callee graph 확인 가능
- [x] 1-hop과 2-hop 의존 탐색 가능
- [x] 기존 의도 복원 card 20개 이상 작성
- [x] 유사 API 후보 report 생성
- [x] Blueprint 연결 여부 누락 report 생성
- [x] 로컬 HTML 사이트 검색 가능
- [x] Markdown 원문을 직접 읽지 않아도 핵심 정보 확인 가능
- [x] 한국어 문서 품질 차단선 통과
- [x] `TakeDamage`와 `ReceiveDamage` 첫 성공 사례 통과
- [x] future intent 보존 규칙이 review checklist에 반영
- [x] agent 검증 기준에서 clean clone 재현 절차, version manifest, repo-relative path 검사 통과
- [ ] 팀 운영 기준에서 내 PC가 아닌 팀원 PC clean clone HTML 열람 통과
- [ ] Google Drive에 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` 업로드 완료
- [x] `git status --short`가 불필요한 generated, cache, installer, temp file을 보이지 않음
- [x] Unreal `Source/`, `Content/`, `Config/`, `.uproject`, `.Build.cs`가 도구 실행만으로 변경되지 않음
- [x] Unreal build, Editor, packaging 흐름에 자동 hook이 없음
- [x] 필수 project-local skill 6개 생성과 대표 요청 검증 완료
- [x] `Docs/` 재구성 inventory, 이동 계획, link 검증 완료
- [x] 기존 `Docs/` 문서가 새 구조에서도 검색과 site navigation으로 접근 가능

## 위험과 대응

| 위험 | 대응 |
| --- | --- |
| Unreal macro 때문에 C++ 자동 분석이 불완전함 | Doxygen/clang-uml 결과를 단독 진실로 보지 않고 clangd, rg, 수동 확인 병행 |
| Blueprint asset 참조가 graph에 빠짐 | 의도 카드에 `Blueprint 연결 여부`를 따로 두고, 이후 Unreal Editor 또는 asset scan으로 보강 |
| 문서가 다시 길어짐 | Markdown을 읽기 화면으로 쓰지 않고 card, graph, search 중심 HTML로 렌더링 |
| AI가 의도를 지어냄 | `확실한 근거`, `추정한 의도`, `확신도`, `아직 모르는 점` 분리 |
| graph가 너무 복잡해짐 | subsystem, class, function level을 나누고 기본 화면은 1-hop, 선택 화면은 2-hop |
| 문서 유지가 귀찮아짐 | public API, Blueprint-facing API, networking boundary, Unreal 기본 흐름 대체 코드에만 의무 부여 |
| 유사 API가 계속 쌓임 | 이름, parameter, caller/callee, 의도 카드 기준으로 후보 report 생성 |
| 진행 문서가 방치됨 | 주간 갱신 규칙과 담당자, 막힌 이유, 다음 행동 필드 유지 |
| 도구가 Unreal 개발을 방해함 | 기본 수동 실행, 읽기 전용 분석, output 경로 제한, 자동 hook 금지 |
| 팀원 PC에서 재현 실패 | repo-relative path, 설치 문서, 환경 확인 script, clean clone 검증 |
| worktree에 설치 파일이 남음 | repo 밖 설치 기본값, `clean.ps1`, 실행 전후 `git status --short` 비교 |
| 긴 prompt를 계속 반복함 | 반복 작업을 `.agents/skills/` project-local skill로 승격 |
| skill이 또 다른 문서 부채가 됨 | `SKILL.md`는 짧게 유지하고 긴 내용은 references로 분리 |
| `Docs/` 이동으로 link가 깨짐 | 이동 전 link map, 이동 후 `rg`와 site build 검증 |
