# APragmataGameState::ActiveBossTarget 의도 카드

## 대상
`APragmataGameState::ActiveBossTarget`

## 종류

Replicated boss HP HUD signal

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`

## 코드가 존재하는 이유

현재 보스 HP를 Hugh/Diana 양쪽 로컬 HUD에 같은 기준으로 표시하기 위해 존재한다.

## 사용해야 하는 상황

- 보스전 시작 시 양쪽 HUD에 같은 boss HP bar를 열어야 하는 경우
- 보스전 종료 시 양쪽 HUD에서 boss HP bar를 닫아야 하는 경우
- 같은 boss actor를 다시 표시해야 해서 HUD bind 경로를 다시 실행해야 하는 경우

## 사용하면 안 되는 상황

- 일반 enemy 머리 위 HP widget 표시
- 보스 AI phase, 공격 패턴, combat 판정 source
- client 전용 임시 표시 상태 저장

## 기본 흐름

server `SetActiveBossTarget(BossActor)` -> `ActiveBossTarget`/`ActiveBossTargetSerial` replication -> `OnRep_ActiveBossTarget` 또는 `OnRep_ActiveBossTargetSerial` -> `OnActiveBossTargetChanged` delegate -> PlayerController/HUD bind

## 호출하는 쪽

- 보스전 시작/종료를 확정하는 server gameplay code 또는 Blueprint

## 호출되는 쪽

- `APragmataPlayerController::HandleActiveBossTargetChanged`

## 서버/클라이언트 조건

server가 값을 설정한다. 각 client는 replicated actor pointer를 받아 자기 local HUD만 갱신한다.

## Blueprint 연결 여부

`SetActiveBossTarget`은 BlueprintCallable. level Blueprint나 boss Blueprint에서 호출할 수 있지만 server authority에서만 유효하다.

## 헷갈리기 쉬운 코드

- `ActiveEnemyHackTarget`: enemy hack HUD 신호
- `ActiveMapPuzzleActor`: map hack HUD 신호
- `bEnemyCombatActive`: combat mode 신호

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/HUD/PragmataHUDWidget-HandleActiveBossTargetChanged.md`
- `Docs/Intent/HUD/BossHPWidget.md`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 boss HP HUD가 GameState `ActiveBossTarget`을 source로 사용한다고 기록

## 추정된 의도

보스 HP 표시를 보스 actor나 각 client local state에 흩어두지 않고, server-approved active boss target 하나로 통일하려는 의도.

## 확신도

강한 추정

## 아직 모르는 것

보스전 시작/종료를 최종 확정할 actor 또는 Blueprint.

## 검증 방법

```powershell
rg -n "ActiveBossTarget|OnActiveBossTargetChanged|SetActiveBossTarget" Source Docs
```

## 다음 수정 때 주의점

보스전 종료 연출 때문에 HP bar를 즉시 닫지 않아야 하면, `SetActiveBossTarget(nullptr)` 호출 타이밍과 `UBossHPWidget::bHideWhenDead` 정책을 같이 조정한다.
