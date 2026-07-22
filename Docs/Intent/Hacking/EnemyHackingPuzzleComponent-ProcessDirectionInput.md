# UEnemyHackingPuzzleComponent::ProcessDirectionInput 의도 카드

## 대상

`UEnemyHackingPuzzleComponent::ProcessDirectionInput`

## 종류

Enemy hack grid input processor

## 정의 위치

- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h`
- `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp`

## 이 코드가 존재하는 이유

server-side enemy hack grid에서 Diana 방향 입력을 판정하고 replicated cursor/grid 상태를 갱신하기 위해 존재한다.

## 사용해야 하는 상황

- enemy hack session이 active이고 서버가 방향 입력을 받은 경우

## 사용하면 안 되는 상황

- map sequence puzzle
- UI-only cursor preview

## 기본 흐름

authority와 state 확인 -> 방향 offset 계산 -> grid cell 판정 -> cursor/state 복제 갱신 -> 완료 조건 확인

## 이 코드를 호출하는 쪽

- `UDianaHackComponent::Server_ProcessEnemyHackDirection_Implementation`

## 이 코드가 호출하는 쪽

- `MoveCursor`
- `CompleteHack`
- cursor moved delegate

## 서버/클라이언트 조건

server에서만 puzzle state를 변경한다. client는 replicated state와 delegate로 UI 표시.

## Blueprint 연결 여부

`BlueprintCallable`. 운영 경로는 C++ server RPC 경유.

## 헷갈리기 쉬운 코드

- `UMapHackingPuzzleComponent::ProcessDirectionInput`: sequence depth 판정
- `UDianaHackComponent::RequestHackDirection`: client input relay

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 enemy hack active 중 방향 입력이 서버 `ProcessDirectionInput()`으로만 진행된다고 기록

## 추정한 의도

grid 이동, corrupted node gate, projected damage 표시를 하나의 서버 state machine으로 유지하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

corrupted node 입력 UX의 최종 의도.

## 검증 방법

```powershell
rg -n "UEnemyHackingPuzzleComponent::ProcessDirectionInput|CorruptedPending|CompleteHack" Source Docs
```

## 다음 수정 시 주의점

UI convenience 때문에 cursor 위치를 client에서 먼저 확정하지 않는다.
