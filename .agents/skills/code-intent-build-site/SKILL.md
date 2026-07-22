---
name: code-intent-build-site
description: Build, publish, or validate the Code Intent HTML site. Use when asked to refresh Docs/CodeIntentSite, generate Doxygen/diagram/site output, verify graph artifacts, or clean generated CodeIntent output.
---

# Code Intent Build Site

## 절차

1. 실행 전 상태 확인:

```powershell
git status --short
.\Tools\CodeIntent\check-env.ps1
```

2. maintainer용 generated output, 팀 공유용 HTML, Google Drive 업로드용 heavy zip 갱신:

```powershell
.\Tools\CodeIntent\build-site.ps1
```

3. 검증:

```powershell
.\Tools\CodeIntent\validate.ps1
git status --short
```

4. 필요 시 정리:

```powershell
.\Tools\CodeIntent\clean.ps1
```

## 규칙

- 재생성용 output은 `Saved/CodeIntent/`에 둔다.
- 팀원이 열람하는 핵심 HTML은 `Docs/CodeIntentSite/`에서 Git으로 공유.
- `Saved/CodeIntent/`는 재생성용 scratch output으로만 사용.
- Doxygen과 Blueprint scan은 Git에 넣지 않고 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존.
- compile DB와 clang-uml output은 scratch output으로만 둠.
- `gd-upload/`의 다른 파일은 삭제하거나 덮어쓰지 않음.
- Unreal Editor 자동 실행 금지.
- build, packaging, playtest hook 추가 금지.
