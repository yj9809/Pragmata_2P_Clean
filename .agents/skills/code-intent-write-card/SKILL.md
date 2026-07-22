---
name: code-intent-write-card
description: Create or update Korean code intent cards under Docs/Intent. Use when adding or changing public APIs, Blueprint-facing APIs, networking boundaries, Unreal custom flow replacements, or when asked to document why a symbol exists.
---

# Code Intent Write Card

## 절차

1. `Docs/code-intent-style.md` 읽기.
2. 대상 symbol 사용처와 기존 문서 확인.
3. card 위치 결정: `Docs/Intent/<Subsystem>/<Symbol>.md`.
4. 필수 항목 모두 작성:
   - 대상
   - 종류
   - 정의 위치
   - 이 코드가 존재하는 이유
   - 사용해야 하는 상황
   - 사용하면 안 되는 상황
   - 기본 흐름
   - 이 코드를 호출하는 쪽
   - 이 코드가 호출하는 쪽
   - 서버/클라이언트 조건
   - Blueprint 연결 여부
   - 헷갈리기 쉬운 코드
   - 관련 문서
   - 관련 커밋
   - 확실한 근거
   - 추정한 의도
   - 확신도
   - 아직 모르는 점
   - 검증 방법
   - 다음 수정 시 주의점
5. `.\Tools\CodeIntent\validate.ps1` 실행.

## 문체

- 설명은 한국어.
- code identifier, path, Unreal 용어는 원문 유지.
- 추정을 사실처럼 쓰지 않기.
