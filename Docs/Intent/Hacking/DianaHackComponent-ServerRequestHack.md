# UDianaHackComponent::Server_RequestHack_Implementation 의도 카드

## 대상

`UDianaHackComponent::Server_RequestHack_Implementation`

## 종류

Enemy hack server authority gate

## 정의 위치

- `Source/Pragmata_2P/Components/DianaHackComponent.cpp`

## 이 코드가 존재하는 이유

enemy hack 시작 조건을 서버에서 검증하고, puzzle component와 GameState HUD 신호를 같은 지점에서 묶기 위해 존재한다.

## 사용해야 하는 상황

- `RequestHack`에서 서버로 도착한 hack 시작 요청 처리
- target, line of sight, distance, puzzle component, active state를 검증해야 하는 경우

## 사용하면 안 되는 상황

- client에서 직접 호출하려는 흐름
- map puzzle actor 처리

## 기본 흐름

target 검증 -> puzzle component 찾기 -> base hack damage 주입 -> `StartHack()` -> active target 복제

## 이 코드를 호출하는 쪽

- Unreal Server RPC dispatch

## 이 코드가 호출하는 쪽

- `ValidateHackTarget`
- `UEnemyHackingPuzzleComponent::SetBaseHackDamage`
- `UEnemyHackingPuzzleComponent::StartHack`
- `APragmataGameState::SetActiveEnemyHackTarget`

## 서버/클라이언트 조건

server only.

## Blueprint 연결 여부

RPC 구현부라 Blueprint 직접 연결 대상은 아님. 결과는 `ActiveEnemyHackTarget` 복제로 HUD Blueprint에 전달.

## 헷갈리기 쉬운 코드

- `UEnemyHackingPuzzleComponent::StartHack`: puzzle 내부 세션 시작
- `APragmataGameState::SetActiveEnemyHackTarget`: HUD bind용 복제 신호

## 관련 문서

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/Systems/Server/server.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `6dbb1c83 fix: 해킹 퍼즐 서버 흐름 보강`
- `5fe4d726 feat: 해킹 UI 서버 권위 통합`

## 확실한 근거

- `Docs/Systems/Hacking/enemy-hacking-integration.md`가 현재 구현 RPC 이름을 명시
- `Docs/KNOW_HOW.md`에 `StartHack()` 성공 결과로 bind 여부를 결정한다고 기록

## 추정한 의도

hack 시작과 HUD 표시를 같은 서버 검증 결과에 묶어 client별 표시 불일치를 줄이려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

target validation 실패 사유가 UI에 얼마나 노출되는지.

## 검증 방법

```powershell
rg -n "Server_RequestHack_Implementation|SetActiveEnemyHackTarget|StartHack" Source Docs
```

## 다음 수정 시 주의점

`StartHack()` 실패 후 GameState active target을 설정하지 않는다.
enemy hack 최대 거리는 5000cm이며, camera origin과 target 사이 `ECC_Visibility` blocker는 계속 거부한다.
