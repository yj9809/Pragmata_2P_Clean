---
name: code-intent-restore
description: Restore development intent for existing code symbols. Use when asked why a symbol exists, how it differs from similar code, where it is used, or to recover intent from git history, Docs, and callers/callees.
---

# Code Intent Restore

## 입력

- symbol 이름
- optional file path
- optional subsystem

## 절차

1. `rg -n "<symbol>" Source Docs`로 현재 사용처 확인.
2. `git log -S<symbol> -- Source Docs`로 도입과 제거 이력 확인.
3. 필요 시 `git log -G<pattern> -- Source Docs` 실행.
4. `git blame`으로 현재 코드 근거 확인.
5. `Docs/Create/`, `Docs/KNOW_HOW.md`, `Docs/Architecture/`, `Docs/Systems/`에서 관련 근거 확인.
6. caller/callee를 1-hop, 필요 시 2-hop까지 요약.
7. 결과를 세 갈래로 분리:
   - 확실한 근거
   - 추정한 의도
   - 아직 모르는 점

## 출력

- symbol 위치
- 호출하는 쪽
- 호출되는 쪽
- 유사 API
- 근거
- 확신도
- intent card 갱신 필요 여부
