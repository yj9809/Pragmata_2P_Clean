# UDianaHackComponent::Server_ProcessEnemyHackDirection_Implementation 의도 카드

## 대상

`UDianaHackComponent::Server_ProcessEnemyHackDirection_Implementation`

## 종류

Enemy hack server input gate

## 정의 위치

- `Source/Pragmata_2P/Components/DianaHackComponent.cpp`

## 이 코드가 존재하는 이유

Diana의 방향 입력을 서버가 소유한 active enemy puzzle에만 적용하기 위해 존재한다.

## 사용해야 하는 상황

- enemy hack session이 active이고 방향 입력이 서버로 도착한 경우

## 사용하면 안 되는 상황

- map hack 입력
- active target과 active puzzle component가 없는 상태

## 기본 흐름

active puzzle 확인 -> `ProcessDirectionInput(Direction)`

## 이 코드를 호출하는 쪽

- Unreal Server RPC dispatch

## 이 코드가 호출하는 쪽

- `UEnemyHackingPuzzleComponent::ProcessDirectionInput`

## 서버/클라이언트 조건

server only. client는 request만 보낸다.

## Blueprint 연결 여부

RPC 구현부라 Blueprint 직접 호출 대상은 아님.

## 헷갈리기 쉬운 코드

- `UDianaNodeInteractionComponent::Server_ProcessMapHackDirection`
- `UEnemyHackingPuzzleComponent::ProcessDirectionInput`

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 enemy hack active 중 방향 입력이 서버 `ProcessDirectionInput()`으로만 진행된다고 기록

## 추정한 의도

client prediction 없이 서버 puzzle state를 단일 진실로 유지하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

latency 보정이나 local preview가 의도적으로 없는지 여부.

## 검증 방법

```powershell
rg -n "Server_ProcessEnemyHackDirection_Implementation|RequestHackDirection|ProcessDirectionInput" Source Docs
```

## 다음 수정 시 주의점

input relay에서 UI 표시 상태를 직접 바꾸지 말고 replicated puzzle state를 따라가게 한다.
