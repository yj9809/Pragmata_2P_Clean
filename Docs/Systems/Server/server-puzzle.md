# 서버 퍼즐 로직

Enemy hacking puzzle, map hacking puzzle, 관련 `GameState` 복제와 서버 권위 흐름의 현재 기준.

관련 문서:

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/KNOW_HOW.md` - `Diana Scan, Hack, and Node Interaction`
- `Docs/Operations/blueprint-tasks.md`

## 핵심 규칙

- Diana 입력: client intent
- 시작/방향/취소/완료/damage/장치 trigger: 서버 처리
- Hugh/Diana HUD: `APragmataGameState` active actor + puzzle component 복제 상태 표시
- Enemy hack: Diana 전용 조작, Hugh/Diana 양쪽 HUD 동일 puzzle 표시
- Map hack: Diana `Roaming + non-combat + Aim` 전용
- Enemy hack: Diana `OperatorMode`/combat state와 무관, aim reticle target 기준
- `IA_Hack`: 미사용
- 해킹 시작: aim/focus 경로
- 해킹 진행: `W/A/S/D`, gamepad face button 방향 입력

## GameState 기준

`APragmataGameState`는 puzzle 표시, combat, power 상태의 서버 기준.

| 상태 | 용도 | 갱신 주체 |
| --- | --- | --- |
| `ActiveEnemyHackTarget` | 현재 enemy hack 대상 actor | `UDianaHackComponent` |
| `ActiveEnemyHackSerial` | 같은 enemy 재시작 HUD rebind | `APragmataGameState::SetActiveEnemyHackTarget()` |
| `ActiveMapPuzzleActor` | 현재 map hacking node actor | `UDianaNodeInteractionComponent` |
| `bEnemyCombatActive` | enemy aggro 기반 shared combat state | monster AI / `APragmataGameState` |
| `bGlobalPowerOn` | power supply 완료 후 전역 전원 상태 | `AInteractionPowerSupply` |

복제 흐름:

```text
server GameState update
-> OnRep_* 또는 server local broadcast
-> APragmataPlayerController delegate
-> UPragmataHUDWidget
-> role HUD / puzzle widget / ViewModel
```

주의:

- Hugh listen server: `OnRep` 미호출 가능, setter local delegate 필요
- Diana remote client: `OnRep` 경로
- 같은 actor pointer 재설정: `ActiveEnemyHackSerial`로 별도 알림

## Enemy Hacking

### 시작 조건

진입:

```text
Diana local aim tick
-> reticle target 탐색
-> UDianaHackComponent::RequestHack(TargetActor)
-> Server_RequestHack(TargetActor)
```

서버 검증:

- owner pawn Diana 확인
- Diana aim 상태 확인
- `AreDianaActionsLocked()` 거부
- `IPragmataHackable` 확인
- `CanBeHacked()` 확인. 이미 hacked 상태여도 enemy hack puzzle 시작 허용
- `UEnemyHackingPuzzleComponent` 존재 확인
- Diana authoritative camera origin/direction 재계산
- Hugh anchor 거리 확인
- Diana camera -> target LOS 확인

거리/방향 기준:

- Enemy hack: Hugh 15m 안이면 방향 무관
- Scan: 기존 Hugh forward cone 검증 유지

### 서버 시작 흐름

```text
Server_RequestHack(TargetActor)
-> ValidateHackTarget()
-> UEnemyHackingPuzzleComponent 획득
-> ClearActiveEnemyHack(true)
-> SetBaseHackDamage(GetHackDamage())
-> StartHack()
-> OnHackCompleted -> HandleEnemyHackCompleted 바인딩
-> ActiveHackTarget / ActivePuzzleComponent 저장
-> GameState.SetActiveEnemyHackTarget(TargetActor)
```

중요 순서:

- `SetBaseHackDamage()` 후 `StartHack()`
- 시작마다 `OnHackCompleted.RemoveAll(this)` 후 재바인딩
- client target/camera 값 직접 신뢰 금지

### Enemy Puzzle Model

`UEnemyHackingPuzzleComponent`: enemy actor 부착 model, 서버 권위 + 상태 복제.

Grid:

| `EnemyType` | Grid |
| --- | --- |
| Watcher | 3x3 |
| Walker | 4x4 |
| Executor | 6x6 |
| Sentinel | 8x8 |

규칙:

- 시작 위치: 좌측 상단
- target cell: 시작 위치와 다른 임의 위치
- normal node 수: grid 크기와 동일
- corrupted node: owner `AMonsterBase::bIsTainted`가 true일 때만 생성
- backtracking: 바로 직전 칸 한 칸 후퇴만 허용
- corrupted node 최초 진입: 같은 방향 2회 입력 필요
- damage: `BaseHackDamage * TotalMultiplier`
- multiplier: normal node마다 가산, 3배 수렴

복제 상태:

- `PuzzleState`
- `CursorRepCounter`
- `CachedGridSize`
- `Grid`
- `CursorPos`
- `TargetPos`
- `TotalNormalNodes`
- `NormalNodesVisited`
- `BaseHackDamage`
- `bCorruptedPending`

`CursorRepCounter`: 같은 위치/상태 갱신도 client UI 이벤트 보장.

### Enemy 방향 입력

| 방향 | Keyboard | Gamepad |
| --- | --- | --- |
| Up | `W` | Face Top |
| Left | `A` | Face Left |
| Down | `S` | Face Bottom |
| Right | `D` | Face Right |

흐름:

```text
ADianaOperatorPawn input
-> RequestHackDirection(EHackDirection)
-> Server_ProcessEnemyHackDirection(Direction)
-> ActivePuzzleComponent.ProcessDirectionInput(Direction)
```

서버 저장 `ActivePuzzleComponent`만 조작. 방향 입력마다 target actor 수신 금지.

### Enemy 취소

취소 경로:

- Aim 해제: 즉시 강제 취소, expected target 없음
- Reticle target lost: `0.15초` 지속 후 expected target cancel

서버 방어:

```text
Server_CancelEnemyHack(ExpectedTarget)
-> ExpectedTarget 있고 ActiveHackTarget과 다르면 stale cancel 무시
-> 아니면 ClearActiveEnemyHack(true)
```

목적:

- target A cancel RPC 지연 도착으로 target B 새 UI가 닫히는 문제 방지

### Enemy 완료

Model 완료:

```text
CursorPos == TargetPos
-> UEnemyHackingPuzzleComponent::CompleteHack()
-> PuzzleState = Completed
-> OnHackCompleted.Broadcast()
```

서버 damage:

```text
UDianaHackComponent::HandleEnemyHackCompleted()
-> ActiveHackTarget / ActivePuzzleComponent 일치 확인
-> ApplySuccessfulHack(TargetActor, DianaOwner, GetProjectedDamage(), GetResolvedHackDurationSeconds())
-> Diana OverDrive AddOverDrive(OverDrivePerHack)
-> already-hacked repeat 완료면 기존 bHacked timer 유지
-> ScheduleCompletedHackHudClear(TargetActor)
-> ClearActiveEnemyHack(false, false)
```

완료 HUD hold:

- `ClearActiveEnemyHack(false, false)`: local gameplay state/delegate 정리, `ActiveEnemyHackTarget` 즉시 null 금지
- `CompletedHackHudHoldSeconds`: success animation 표시 시간
- `ClearCompletedHackHudTarget()`: timer 후 target 일치 확인, 아직 같은 target이면 `SetActiveEnemyHackTarget(nullptr)`
- 새 hack session 보호: stale timer가 새 target UI를 닫지 않도록 target 비교

### Enemy HUD / ViewModel

Bind 흐름:

```text
GameState.ActiveEnemyHackTarget 변경
-> APragmataPlayerController::HandleActiveEnemyHackTargetChanged
-> UPragmataHUDWidget::HandleActiveEnemyHackTargetChanged
-> UEnemyHackingWidget::SetHackTarget(TargetActor)
-> UEnemyHackingPuzzleViewModel::SetHackTarget(TargetActor)
-> TargetActor->FindComponentByClass<UEnemyHackingPuzzleComponent>()
-> BindToComponent()
```

ViewModel delegate:

| Delegate | UI 반응 |
| --- | --- |
| `OnHackStarted` | active true, started trigger |
| `OnGridReady` | grid/cache 초기화 |
| `OnCursorMoved` | cursor/progress/damage/cache 갱신 |
| `OnHackCompleted` | final grid/cursor snapshot, projected damage, `OnHackCompletedWithDamage`, completed trigger |
| `OnHackCancelled` | cancelled trigger |

완료 animation:

```text
UEnemyHackingPuzzleViewModel::OnHackCompletedWithDamage
-> UEnemyHackingWidget::HandleHackCompleted()
-> SetEnemyHackingVisible(true)
-> OnHackCompleted() BP event
```

`OnHackCompleted()`는 WBP 성공 animation 시작점.
완료 trigger 전 final grid/cursor snapshot을 반영해 client 복제 순서 차이로 마지막 trail 누락 방지

### Enemy 재타겟 방어

해결한 증상:

- 재조준 시 UI가 잠깐 표시 후 닫힘
- Hugh HUD가 같은 enemy 재시작을 처음 한 번만 표시
- 이전 cancel/complete animation callback이 새 active puzzle 닫음

현재 방어:

- reticle lost `0.15초` 유예
- expected target cancel 검증
- `ActiveEnemyHackSerial` 별도 복제
- 늦은 bind 시 `SyncFromComponent()`로 active/start trigger 복구
- bound puzzle `Active`면 cancel/complete animation finish callback 무시
- widget destruct/null target 시 ViewModel dynamic delegate 해제

## Map Hacking

### 시작 조건

Map hack gate:

- Diana pawn owner
- `APragmataGameState::IsEnemyCombatActive() == false`
- Diana `OperatorMode == Roaming`
- Diana `GetIsAiming() == true`
- `AreDianaActionsLocked()` false
- Hugh 기준 거리 `1500cm` 이내
- Diana 화면 중앙 half-angle `10도` 이내
- Diana camera -> node LOS 통과
- candidate: `AInteractionPowerSupply` 또는 `APuzzleInteractable`
- candidate 미완료 상태

LOS:

- candidate 직접 hit 허용
- candidate child/attached actor hit 허용
- 앞의 다른 blocker hit 차단

### Node 분기

`UDianaNodeInteractionComponent::Server_RequestNodeInteraction()` 분기:

```text
target = APuzzleInteractable
-> IPropInteractableInterface::Interact()
-> APuzzleInteractable::Interact_Implementation()
-> UMapHackingPuzzleComponent 있음: TryStartMapHackFromInteractable()
-> component 없음: 기존 CompletePuzzle()

target = AInteractionPowerSupply 등 non-puzzle interactable
-> IPropInteractableInterface::Interact()
-> Client_ReportNodeInteractionComplete()
```

의도:

- Diana component는 server target 검증과 `Interact()` 호출만 담당
- `APuzzleInteractable::Interact_Implementation()`이 map node communication entrypoint
- map hack node는 `APuzzleInteractable` + `UMapHackingPuzzleComponent` 조합

### 서버 시작 흐름

```text
RefreshFocusedNodeLog()
-> FindBestNodeInteractionTarget()
-> TryRequestFocusedMapHack()
-> Server_RequestNodeInteraction()
-> IPropInteractableInterface::Interact()
-> APuzzleInteractable::Interact_Implementation()
-> TryStartMapHackFromInteractable(PuzzleActor)
-> ClearActiveMapHack(true)
-> PuzzleComponent.StartHack()
-> OnHackCompleted -> HandleMapHackCompleted
-> OnHackReset -> HandleMapHackReset
-> ActiveMapPuzzleActor / ActiveMapPuzzleComponent 저장
-> GameState.SetActiveMapPuzzleActor(PuzzleActor)
```

`FindBestNodeInteractionTarget()`:

- client target actor 미사용
- 서버 Diana authoritative camera intent 사용
- Hugh 위치 기준 재검색

### Map Puzzle Model

`UMapHackingPuzzleComponent`: map node actor 부착 model, 서버 권위 + 상태 복제.

복제 상태:

- `TotalDepths`
- `PuzzleState`
- `CurrentDepth`
- `CompletedDepthCount`
- `DepthSequence`
- `DepthSolvedCounter`
- `ResetCounter`

동작:

- `StartHack()`: depth sequence 생성, `Active` 전환
- `ProcessDirectionInput()`: 현재 depth 정답 비교
- 정답: `OnDepthSolved`, 다음 depth 또는 complete
- 오답: `ResetHack()`
- `CancelHack()`: `Idle` 강제 정리
- `CompleteHack()`: `Solved`, `OnHackCompleted`

제약:

- `TotalDepths` 최소 1
- 현재 UI는 3-depth 기준
- depth 확장 시 ViewModel/WBP 슬롯 확장 필요

### Map 방향 입력

입력 매핑: enemy hack과 동일.

```text
ADianaOperatorPawn input
-> UDianaNodeInteractionComponent::RequestMapHackDirection(Direction)
-> Server_ProcessMapHackDirection(Direction)
-> CanOwnerUseNodeInteraction() 재검증
-> ActiveMapPuzzleComponent.ProcessDirectionInput(Direction)
```

중단 조건:

- combat 전환
- aim 해제
- Diana mode Roaming 이탈

위 조건 발생 시 active map puzzle 즉시 cancel.

### Map 완료

서버 완료 흐름:

```text
UMapHackingPuzzleComponent::CompleteHack()
-> OnHackCompleted.Broadcast()
-> UDianaNodeInteractionComponent::HandleMapHackCompleted()
-> ActiveMapPuzzleActor / ActiveMapPuzzleComponent owner 일치 확인
-> APuzzleInteractable::CompletePuzzle()
-> Client_ReportNodeInteractionComplete()
-> ClearActiveMapHack(false)
```

`APuzzleInteractable::CompletePuzzle()` 역할:

- 1회성 puzzle solved 상태 기록
- `TargetActors` 순회
- `IPuzzleTargetInterface::OnPuzzleTriggered()` 호출
- 문/장치/전원 기반 actor 서버 상태 반응

원칙:

- `UMapHackingPuzzleComponent`에 문/장치 직접 참조 금지
- 결과 처리는 `APuzzleInteractable.TargetActors` 경로 유지

### Map Reset / Cancel

| 상황 | 흐름 |
| --- | --- |
| 오답 | `ResetHack()` -> `OnHackReset` -> `HandleMapHackReset()` -> active map hack 정리 |
| Aim 해제 | `RequestCancelMapHack()` -> `Server_CancelMapHack()` -> `ClearActiveMapHack(true)` |
| Combat 진입 | `SetEnemyCombatActive(true)` -> Diana `Combat` mode -> `CancelActiveMapHackFromServer()` |
| HUD 정리 | `ActiveMapPuzzleActor=nullptr` 복제 -> HUD unbind |

주의:

- `CancelHack()`는 reset trigger broadcast 없음
- 강제 취소 UI는 active actor null + HUD unbind 기준

### Map HUD

현재 기준:

- `APragmataGameState::ActiveMapPuzzleActor` 양쪽 복제
- `APlayerController`가 root HUD `OnActiveMapPuzzleActorChanged(PuzzleActor)` BP event 호출
- Hugh/Diana HUD의 `WBP_MapHackingWidget`에서 `UMapHackingPuzzleComponent` 탐색
- `UHackingPuzzleViewModel::BindToComponent()` 호출
- actor null 또는 component 없음: `UnbindFromComponent()`

남은 작업:

- Editor/BP 작업은 `Docs/Operations/blueprint-tasks.md`에만 유지
- C++ hard-load나 runtime layout 우회 금지

## Enemy Combat / Map Hack 경계

Enemy aggro는 map hack 차단용 서버 상태.

```text
monster AI target 획득
-> RegisterEnemyAggro(Enemy, Target)
-> bEnemyCombatActive = true
-> Diana Roaming -> Combat
-> active map hack cancel
-> OnEnemyCombatActiveChanged(true)

monster target 상실
-> UnregisterEnemyAggro()
-> 1초 grace
-> bEnemyCombatActive = false
-> Diana Combat -> Roaming
-> OnEnemyCombatActiveChanged(false)
```

규칙:

- Enemy hack: combat state 무관
- Map hack: combat 중 차단

## Power / Map Device

전원 상호작용은 Diana node interaction 후보군.

```text
Diana aim focus on AInteractionPowerSupply
-> Server_RequestNodeInteraction()
-> IPropInteractableInterface::Interact()
-> AInteractionPowerSupply::Interact_Implementation()
-> GameState.SetGlobalPowerState(true)
-> bGlobalPowerOn replicated
-> OnPowerStateChanged broadcast
-> ALightBase / APoweredResponBox 반응
```

기준:

- 전원 상태: `APragmataGameState::bGlobalPowerOn`
- server local: setter delegate broadcast
- client: `OnRep_GlobalPowerOn()` delegate broadcast

## 오류와 방어

| 증상 | 원인 | 현재 방어 |
| --- | --- | --- |
| enemy UI 재조준 직후 닫힘 | reticle trace 흔들림, 늦은 cancel RPC, stale animation callback | 0.15초 lost delay, expected target 검증, ViewModel active guard |
| Hugh HUD 같은 enemy 재시작 미표시 | replicated actor pointer 값 동일 | `ActiveEnemyHackSerial` 별도 복제 |
| enemy 완료 animation 미표시 | 완료 직후 active target null로 HUD collapse | `OnHackCompleted()` BP event, `CompletedHackHudHoldSeconds` 후 target clear |
| map node aim 즉시 완료 | Diana component가 `Interact_Implementation()`을 우회하거나 component 분기 없이 `CompletePuzzle()` 실행 | map component 있으면 `TryStartMapHackFromInteractable()`, component 없으면 기존 `CompletePuzzle()` |
| combat 중 map UI 표시 | map focus gate combat state 누락 | `CanOwnerUseNodeInteraction()` non-combat/Roaming/Aim 검증 |
| LOS 뒤 node 선택 | screen angle만 검사 | camera-to-node trace blocker 확인 |
| client target/camera 조작 | client intent 신뢰 | 서버 authoritative camera intent 재계산, 서버 target 재검색 |

## 수정 규칙

- `StartHack()` / `ProcessDirectionInput()` / `CancelHack()`: 서버 호출 기준
- Client ViewModel `Start*()` 단독 호출로 multiplayer gameplay 시작 금지
- Enemy damage/OverDrive: `UDianaHackComponent::HandleEnemyHackCompleted()` 서버 처리
- Map device trigger: `APuzzleInteractable::CompletePuzzle()` + `TargetActors` 유지
- `ActiveEnemyHackTarget` null timing 변경 시 success hold + stale timer 검증 동시 확인
- `ActiveMapPuzzleActor` null timing 변경 시 forced cancel/reset/success unbind 동시 확인
- Map hack node BP: `UMapHackingPuzzleComponent` 필수
- Enemy type 표준명: `Walker`
- `Worker` 참조 신규 추가 금지
- Blueprint asset 배치/Widget layout: `Docs/Operations/blueprint-tasks.md`에 사용자 작업으로 기록

## 검증 체크리스트

Enemy hack:

- [ ] Hugh 기준 15m 안 enemy, Hugh 방향 무관 hack 가능
- [ ] Diana reticle 이탈 0.15초 미만 UI 유지
- [ ] Diana reticle 이탈 0.15초 이상 Hugh/Diana UI 닫힘
- [ ] Aim release 즉시 cancel
- [ ] 같은 enemy 재타겟 시 Hugh/Diana UI 재표시
- [ ] 완료 시 success animation 재생
- [ ] 완료 후 `CompletedHackHudHoldSeconds` 뒤 UI 닫힘
- [ ] Damage/OverDrive 1회 적용
- [ ] hacked enemy도 재해킹 가능, repeat 완료 시 damage/OverDrive 지급
- [ ] repeat 완료가 기존 `bHacked` timer를 reset/extend하지 않음

Map hack:

- [ ] `Roaming + non-combat + Aim`에서만 map node focus 가능
- [ ] Combat 중 map hack UI 미표시
- [ ] combat 진입 시 active map hack cancel
- [ ] Hugh 1500cm 밖 node 시작 불가
- [ ] Diana 화면 중앙 10도 밖 node 시작 불가
- [ ] LOS 막힌 node 시작 불가
- [ ] `UMapHackingPuzzleComponent` 있는 `APuzzleInteractable`은 `Interact_Implementation()` 경유로 즉시 완료 대신 map puzzle 시작
- [ ] component 없는 `APuzzleInteractable`은 기존 `CompletePuzzle()` 경로 유지
- [ ] 성공 시 `CompletePuzzle()`와 `TargetActors` trigger 실행
- [ ] Hugh/Diana HUD 같은 depth, direction, reset/success state 표시

Shared server state:

- [ ] `ActiveEnemyHackTarget` / `ActiveMapPuzzleActor` Hugh/Diana HUD 반영
- [ ] `bEnemyCombatActive` Hugh/Diana HUD/AnimBP hook 반영
- [ ] `bGlobalPowerOn` power-dependent actor 복제 반영
