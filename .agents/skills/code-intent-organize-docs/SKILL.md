---
name: code-intent-organize-docs
description: Reorganize Docs for code intent workflow. Use when asked to inventory Docs, classify documents, move Docs files, update links, or verify documentation navigation after reorganization.
---

# Code Intent Organize Docs

## 절차

1. `rg --files Docs`로 inventory 작성.
2. 문서 분류:
   - `Docs/Architecture/`
   - `Docs/Systems/`
   - `Docs/Operations/`
   - `Docs/Guides/`
   - `Docs/Plans/`
   - `Docs/Create/`
3. 이동 전 path 참조 확인:

```powershell
rg -n "Docs/<old-path>|<old-file-name>" .
```

4. 이동은 손실 없이 수행.
5. 이동 후 내부 link와 `AGENTS.md` 참조 갱신.
6. 검증:

```powershell
git diff --check
rg -n "Docs/<old-path>|<old-file-name>" .
git status --short
```

## 금지

- 내용 삭제 금지.
- `Docs/Create/` changelist 기록 섞기 금지.
- `Docs/KNOW_HOW.md` 이동은 directive 영향 검토 전 금지.
