---
name: code-intent-scan-blueprints
description: Scan Blueprint asset references for the code intent workflow. Use when asked to verify Blueprint connections, fill Blueprint 연결 여부 in intent cards, or generate Blueprint dependency reports through UnreelMCP without modifying assets.
---

# Code Intent Scan Blueprints

## 절차

1. `08-unreelmcp-tools.md`와 `09-unreelmcp-operation-recipes.md` 확인.
2. `unreal_status`로 Editor와 UnreelMCP 연결 확인.
3. Python이 필요하면 `python.status` 또는 `unreal_status`에서 `python.enabled=true` 확인.
4. 아래 script를 UnreelMCP `python.execute_script`로 실행:

```text
Tools/CodeIntent/scan-blueprints.py
```

5. 산출물 확인:

```powershell
Test-Path Saved/CodeIntent/blueprints/blueprint-asset-scan.json
Test-Path Saved/CodeIntent/blueprints/blueprint-asset-scan-summary.md
```

6. intent card의 `Blueprint 연결 여부`를 갱신할 때는 scan 결과와 C++ 사용처를 함께 본다.
7. scan 결과를 Google Drive 보존 zip에 반영해야 하면 site 갱신을 실행:

```powershell
.\Tools\CodeIntent\build-site.ps1
```

8. 보존 zip 확인:

```powershell
Test-Path gd-upload/CodeIntent/code-intent-heavy-artifacts.zip
```

## 규칙

- read-only scan만 수행.
- `.uasset`, `.umap`, `Content/` 수정 금지.
- output은 `Saved/CodeIntent/blueprints/`만 사용.
- Doxygen과 Blueprint scan 공유본은 Git이 아니라 `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip`으로 보존.
- 실패 시 backend 상태, Python enabled 여부, script stderr를 기록.
