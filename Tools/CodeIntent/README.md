# Code Intent 도구

## 목적

code intent 도구는 C++ symbol 연결, intent card, diagram, 로컬 문서 사이트를 생성한다.

기본 원칙:

- Unreal `Source/`, `Content/`, `Config/`, `.uproject`, `.Build.cs` 수정 금지
- maintainer용 generated output은 `Saved/CodeIntent/` 아래에 생성
- 팀 공유용 핵심 HTML은 `Docs/CodeIntentSite/`에 publish하고 Git으로 추적
- installer, cache, `.venv`, `node_modules` repo 내부 생성 금지
- 모든 경로는 repo root 기준 상대 경로 사용

## 팀원 사용

도구 설치 없이 아래 파일을 연다.

```text
Docs/CodeIntentSite/index.html
```

포함 항목:

- `Docs/CodeIntentSite/graph/graph.html`: self-contained graph
- `Docs/CodeIntentSite/mkdocs-site/`: 읽기용 문서 site

Doxygen과 Blueprint scan은 Git에 넣지 않고 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존한다. 사용자가 이 zip을 Google Drive에 수동 업로드한다.
compile DB와 clang-uml output은 scratch output으로만 둔다.

## 필요 도구

아래 도구는 핵심 HTML을 갱신하는 maintainer에게만 필요하다. 단순 열람자는 설치하지 않는다.

| 도구 | 용도 | 확인 명령 |
| --- | --- | --- |
| Doxygen | C++ reference 생성 | `doxygen --version` |
| Graphviz | Doxygen graph 생성 | `dot -V` |
| clang-uml | C++ diagram 생성 | `clang-uml --version` |
| clangd | C++ symbol 분석 보조 | `clangd --version` |
| MkDocs Material | 로컬 HTML site 생성 | `mkdocs --version` |
| Python | 검증 script 실행 | `python --version` |
| Node.js | graph UI 확장 후보 | `node --version` |

고정 버전은 `Tools/CodeIntent/tool-versions.json`에 기록한다.

## 설치 예시

Windows 기본 설치 예시:

```powershell
.\Tools\CodeIntent\install-tools.ps1
```

repo 안에 `.venv`, `node_modules`, installer, cache를 만들지 않는다.
`winget --silent`와 agreement flag를 사용하지만 Windows UAC 창의 `Yes`는 일반 사용자 권한에서 완전히 없앨 수 없다. 완전 무인 설치가 필요하면 관리자 PowerShell에서 실행한다.

## 첫 실행

```powershell
.\Tools\CodeIntent\check-env.ps1
.\Tools\CodeIntent\generate-compile-database.ps1
.\Tools\CodeIntent\build-site.ps1
.\Tools\CodeIntent\validate.ps1
```

도구가 설치되지 않은 PC에서도 script는 실패 이유를 출력하고 Unreal 파일을 수정하지 않는다.
`generate-compile-database.ps1`는 `Saved/CodeIntent/compile/compile_commands.json`을 만든다. UnrealBuildTool이 `Intermediate/` 아래 generated 파일을 갱신할 수 있으나 tracked 파일은 수정하지 않는다.
`build-site.ps1`는 `clang-uml`가 Unreal macro 또는 compile database 문제로 실패해도 `build-graph.ps1` 기반 graph를 생성한다.
`build-site.ps1`는 `Saved/CodeIntent/` 산출물 중 landing page, graph, MkDocs static site를 `Docs/CodeIntentSite/`로 publish한다.
`build-site.ps1`는 Doxygen과 Blueprint scan output이 있으면 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 묶는다.

## 정리

```powershell
.\Tools\CodeIntent\clean.ps1
```

`Saved/CodeIntent/`와 임시 파일만 삭제한다. `Docs/CodeIntentSite/`는 팀 공유용 tracked HTML이므로 삭제하지 않는다.
`gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`도 Google Drive 업로드 보존본이므로 삭제하지 않는다.

## 재현 기준

- 팀원은 clean clone에서 `Docs/CodeIntentSite/index.html`을 바로 열 수 있음
- maintainer는 위 명령 순서로 site 갱신 가능
- Doxygen과 Blueprint scan은 Git이 아니라 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존
- 실행 전후 `git status --short`에 generated output, cache, installer가 남지 않음
- Unreal Editor를 켜지 않아도 기본 검증 가능
