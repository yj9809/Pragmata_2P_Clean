# 맵 해킹 퍼즐 연결 가이드

팀원이 맵에 배치된 퍼즐 노드와 문/장치를 연결할 때 쓰는 문서.

전제:

- 퍼즐 시퀀스, 진행도, 성공/실패 상태 복제는 `UMapHackingPuzzleComponent`가 담당
- HUD 표시 상태는 `UHackingPuzzleViewModel`이 담당
- 실제 맵 장치 완료 처리는 기존 `APuzzleInteractable::CompletePuzzle()` -> `TargetActors` -> `IPuzzleTargetInterface::OnPuzzleTriggered()` 경로 사용
- 다이애나 입력은 클라이언트 intent일 뿐이며, 퍼즐 시작/방향 입력/완료 처리는 서버 권위로 실행

## 0. 현재 구현 상태

| 구분 | 상태 | 위치 |
| --- | --- | --- |
| 방향 시퀀스 생성, 입력 판정, 성공/실패 처리 | 완료 | `UMapHackingPuzzleComponent` |
| 퍼즐 상태/진행도/시퀀스 복제 | 완료 | `UMapHackingPuzzleComponent` |
| HUD 표시용 ViewModel | 완료 | `UHackingPuzzleViewModel` |
| 기본 표시 위젯 클래스 | 일부 완료 | `UMapHackingWidget`, `/Game/UI/Widget/MapHacking/WBP_MapHackingWidget` |
| 역할별 HUD asset | 존재 | `/Game/UI/Widget/WBP_HughHUD`, `/Game/UI/Widget/WBP_DianaHUD` |
| Diana Roaming + Aim 노드 상호작용 | 완료 | `UDianaNodeInteractionComponent` |
| 노드 완료 후 타겟 장치 트리거 | 완료 | `APuzzleInteractable::CompletePuzzle()` |
| 맵 퍼즐 시작/방향/취소용 Diana Server RPC | 완료 | `ADianaOperatorPawn` + `UDianaNodeInteractionComponent` |
| 퍼즐 완료 델리게이트 -> `CompletePuzzle()` 연결 | 완료 | `UDianaNodeInteractionComponent::HandleMapHackCompleted` |
| 양쪽 HUD가 현재 퍼즐을 바인딩하는 복제 신호 | 완료 | `APragmataGameState.ActiveMapPuzzleActor` |
| 역할별 HUD 생성 경로 | 완료 | `APragmataPlayerController::EnsureGameplayHUD` → possessed pawn `GameplayHUDClass` |

추측:

- 팀원이 연결할 맵 퍼즐 노드는 기존 `APuzzleInteractable` 계열 actor를 기준으로 배치한다고 가정
- 문, 화물 박스, 레이저 같은 결과물은 `IPuzzleTargetInterface`를 구현한 actor라고 가정

HUD 생성 전제:

- root HUD는 `APragmataPlayerController::EnsureGameplayHUD()`가 local controller에서 1회 생성
- Hugh는 possessed `BP_HughCharacter` defaults의 `GameplayHUDClass`에서 `WBP_HughHUD`를 읽음
- Diana는 possessed `BP_DianaOperatorPawn` defaults의 `GameplayHUDClass`에서 `WBP_DianaHUD`를 읽음
- Blueprint EventGraph에서 별도 `Create Widget` 경로를 남기면 중복 HUD와 중복 ViewModel 바인딩이 생길 수 있음
- 이 문서는 역할별 HUD asset이 viewport에 뜬다는 전제에서 WBP 내부 연결을 설명

## 1. 전체 구조

```text
Diana input
-> ADianaOperatorPawn 또는 UDianaNodeInteractionComponent Server RPC
-> 서버가 현재 조준 가능한 APuzzleInteractable 재검증
-> UMapHackingPuzzleComponent::StartHack()
-> APragmataGameState.ActiveMapPuzzleActor 복제
-> Hugh/Diana 각 HUD ViewModel이 BindToComponent()
-> Diana 방향 입력 Server RPC
-> UMapHackingPuzzleComponent::ProcessDirectionInput()
-> 성공 시 OnHackCompleted
-> APuzzleInteractable::CompletePuzzle()
-> TargetActors의 IPuzzleTargetInterface::OnPuzzleTriggered()
```

핵심 원칙:

- `StartHack()`, `ProcessDirectionInput()`, `CancelHack()`는 서버에서만 의미 있음
- Diana 클라이언트에서 ViewModel 함수만 호출하면 퍼즐이 시작되지 않음
- 맵 장치 트리거는 퍼즐 컴포넌트가 직접 하지 않고, 퍼즐 노드 actor가 완료 델리게이트를 받아 `CompletePuzzle()` 호출
- 양쪽 HUD 표시는 서버가 복제한 현재 퍼즐 actor를 보고 각 클라이언트가 로컬 ViewModel에 바인딩

## 2. 퍼즐 노드 actor 구성

추천 방식:

- `APuzzleInteractable` 기반 Blueprint 또는 C++ 자식 사용
- 같은 actor에 `UMapHackingPuzzleComponent` 추가
- `TargetActors`에 퍼즐 완료 후 움직일 actor 연결
- 기본 1회성 퍼즐은 `bIsRepeatable=false`
- 재해킹 필요 퍼즐만 `bIsRepeatable=true` 후 `Max Hack Count` 설정
- 반복 퍼즐은 성공 뒤에도 즉시 재시작하지 않고, progression actor가 `ResetPuzzleForNextAttempt()`를 호출할 때 다음 시도로 재무장

현재 `APuzzleInteractable::Interact_Implementation()` 기준:

- `UMapHackingPuzzleComponent`가 있으면 Diana component의 map hack helper 호출
- `UMapHackingPuzzleComponent`가 없으면 기존 `CompletePuzzle()` 호출
- Diana 쪽 Server RPC는 target 검증 후 `Interact_Implementation()`을 호출하는 entrypoint 역할
- 완료 시점은 반드시 `UMapHackingPuzzleComponent::OnHackCompleted` 이후

맵 배치 체크:

- 퍼즐 노드 actor가 복제되는지 확인
- `UMapHackingPuzzleComponent`가 같은 actor에 붙어 있는지 확인
- `TotalDepths=3` 유지 권장
- `TargetActors`에 `APuzzleLockedDoor` 또는 `IPuzzleTargetInterface` 구현 actor 지정
- 문 여러 개를 한 퍼즐로 열면 `TargetActors`에 모두 추가
- 문 하나를 여러 퍼즐로 열면 `APuzzleLockedDoor.RequiredPuzzleCount`를 필요한 개수로 설정

## 3. 서버 RPC 연결

`UMapHackingPuzzleComponent`는 서버 권위 component라 Diana 입력을 직접 받을 수 없음.

추천 위치:

- 시작/방향/취소 RPC: `ADianaOperatorPawn` 또는 `UDianaNodeInteractionComponent`
- 현재 active puzzle 저장: 서버의 Diana pawn, node interaction component, 또는 `APragmataGameState`
- 양쪽 HUD 표시용 복제 신호: `APragmataGameState`

현재 RPC:

```cpp
UFUNCTION(Server, Reliable)
void Server_RequestNodeInteraction();

UFUNCTION(Server, Reliable)
void Server_ProcessMapHackDirection(EHackDirection Direction);

UFUNCTION(Server, Reliable)
void Server_CancelMapHack();
```

시작 RPC 흐름:

```text
Server_RequestNodeInteraction
-> CanOwnerUseNodeInteraction() 재검증
-> FindBestNodeInteractionTarget() 재검색
-> IPropInteractableInterface::Interact(target, DianaPawn)
-> APuzzleInteractable::Interact_Implementation()
-> TryStartMapHackFromInteractable(target)
-> target->FindComponentByClass<UMapHackingPuzzleComponent>() 확인
-> PuzzleComp->StartHack()
-> PuzzleComp->OnHackCompleted에 서버 완료 핸들러 바인딩
-> GameState.ActiveMapPuzzleActor = target 복제
```

방향 RPC 흐름:

```text
Server_ProcessMapHackDirection(Direction)
-> 현재 active puzzle actor/component 확인
-> PuzzleState == Active 확인
-> PuzzleComp->ProcessDirectionInput(Direction)
```

취소 RPC 흐름:

```text
Server_CancelMapHack
-> 현재 active puzzle actor/component 확인
-> PuzzleComp->CancelHack()
-> GameState.ActiveMapPuzzleActor = nullptr
-> 서버 저장 active puzzle 정리
```

주의:

- client가 보낸 target actor를 그대로 신뢰하지 않음
- 시작 target은 서버가 `FindBestNodeInteractionTarget()` 조건으로 다시 고름
- 방향 입력은 target actor를 매번 받기보다 서버가 저장한 active puzzle을 사용
- `UDianaNodeInteractionComponent::RequestNodeInteraction()`은 검증 후 `Interact_Implementation()`을 호출하는 entrypoint

## 4. 완료 처리

완료는 서버의 `UMapHackingPuzzleComponent::OnHackCompleted`에서 받음.

추천 완료 핸들러:

```cpp
void UYourMapPuzzleBridge::HandleMapPuzzleCompleted()
{
    APuzzleInteractable* PuzzleNode = ActivePuzzleNode.Get();
    UMapHackingPuzzleComponent* PuzzleComp = ActivePuzzleComp.Get();
    if (!PuzzleNode || !PuzzleComp || PuzzleComp->GetOwner() != PuzzleNode)
    {
        return;
    }

    PuzzleNode->CompletePuzzle();
    PuzzleComp->OnHackCompleted.RemoveAll(this);
    ClearActiveMapPuzzleForHUD();
}
```

왜 `CompletePuzzle()`을 호출하는가:

- 1회성 퍼즐이면 `bIsSolved=true` 복제
- Blueprint 자식의 `OnPuzzleSolvedChanged(true)`가 표시 상태를 바꿈
- `TargetActors`의 `OnPuzzleTriggered()`가 호출되어 문/장치가 열림
- 기존 문/장치 연결 구조를 그대로 사용

실패 처리:

- 잘못된 방향 입력 시 `ResetHack()` 경로로 `OnHackReset`이 발생
- ViewModel은 `bResetTrigger`를 토글
- WBP 실패 애니메이션 종료 시 `OnFailureAnimationFinished()` 호출 필요
- `OnFailureAnimationFinished()`는 `StopMapHacking()` -> `CancelHack()` -> `UnbindFromComponent()` 순서로 정리

성공 처리:

- `OnHackCompleted` 수신 시 ViewModel은 `bSolvedTrigger` 토글
- WBP 성공 애니메이션 종료 시 `OnSuccessAnimationFinished()` 호출 필요
- 서버 쪽은 `CompletePuzzle()` 후 HUD active puzzle을 null로 정리

현재 구현 주의:

- `CancelHack()`는 `OnHackReset`을 broadcast하지 않음
- 강제 취소 UI를 확실히 닫으려면 HUD 바인딩 해제 또는 별도 cancel trigger가 필요
- `TotalDepths`는 최소 1로 clamp되지만 ViewModel/WBP 표시는 현재 3단계 기준

## 5. HUD 바인딩

각 클라이언트는 복제된 active puzzle actor를 보고 자기 HUD ViewModel에 로컬 바인딩.

추천 복제 신호:

```cpp
// APragmataGameState 예시
UPROPERTY(ReplicatedUsing=OnRep_ActiveMapPuzzleActor)
TObjectPtr<AActor> ActiveMapPuzzleActor = nullptr;
```

OnRep 또는 delegate 흐름:

```text
ActiveMapPuzzleActor 변경
-> local HUD 찾기
-> UHackingPuzzleViewModel 찾기
-> Actor->FindComponentByClass<UMapHackingPuzzleComponent>()
-> ViewModel->BindToComponent(PuzzleComp)
-> null이면 ViewModel->UnbindFromComponent()
```

Hugh/Diana 표시 차이:

- Diana HUD: 전체 퍼즐 조작 화면 표시
- Hugh HUD: `UMapHackingWidget::SetCompactMode(true)`로 요약 표시 가능
- 같은 `UMapHackingPuzzleComponent` 복제 상태를 보므로 정답 방향, 완료 ring, 성공/실패 trigger는 양쪽이 동일

역할별 HUD host:

- `WBP_DianaHUD` 안에 `WBP_MapHackingWidget` 배치
- `WBP_HughHUD` 안에도 `WBP_MapHackingWidget` 배치하되, 필요 시 `SetCompactMode(true)` 호출
- 두 HUD 모두 같은 active puzzle actor에서 `UMapHackingPuzzleComponent`를 찾아 `UHackingPuzzleViewModel::BindToComponent()` 호출
- `BP_HughCharacter`/`BP_DianaOperatorPawn` class defaults의 `GameplayHUDClass`가 역할별 root HUD를 가리키는지 확인
- root HUD 생성은 C++ 경로로 일원화. BP EventGraph에서 따로 `Create Widget`을 남기면 role별 HUD가 2개 생길 수 있음

WBP 연결 체크:

- `UHackingPuzzleViewModel` 인스턴스 생성 또는 MVVM ViewModel 지정
- `IsActiveVisibility`를 root visibility에 바인딩
- `Direction0Index`, `Direction1Index`, `Direction2Index`를 방향 이미지 선택에 사용
- `Depth1SuccessRingVisibility` ~ `Depth3SuccessRingVisibility`를 성공 ring에 바인딩
- `Depth1DirectionOpacity` ~ `Depth3DirectionOpacity`를 완료된 방향 아이콘 opacity에 바인딩
- `ResetTrigger` 변경 시 실패 애니메이션 재생
- `SolvedTrigger` 변경 시 성공 애니메이션 재생
- 성공 애니메이션 끝에서 `OnSuccessAnimationFinished()` 호출
- 실패 애니메이션 끝에서 `OnFailureAnimationFinished()` 호출

## 6. 입력 연결

현재 Diana aim 기반 해킹 입력:

- `Roaming + non-combat + Aim`이면 map puzzle reticle 경로
- enemy hack은 combat state와 무관하게 aim 중 monster reticle 경로

맵 해킹 추천 입력:

| 상황 | 입력 | 서버 호출 |
| --- | --- | --- |
| Roaming + non-combat + Aim + 퍼즐 노드 조준 | focus 자동 요청 | `Server_RequestNodeInteraction()` |
| 퍼즐 Active 중 위 | `W` 또는 gamepad face button Top | `Server_ProcessMapHackDirection(Up)` |
| 퍼즐 Active 중 아래 | `S` 또는 gamepad face button Bottom | `Server_ProcessMapHackDirection(Down)` |
| 퍼즐 Active 중 왼쪽 | `A` 또는 gamepad face button Left | `Server_ProcessMapHackDirection(Left)` |
| 퍼즐 Active 중 오른쪽 | `D` 또는 gamepad face button Right | `Server_ProcessMapHackDirection(Right)` |
| 퍼즐 Active 중 취소 | aim 해제 | `Server_CancelMapHack()` |

입력 분기 원칙:

- active map puzzle이 있으면 방향 입력을 캐릭터 이동보다 퍼즐 입력으로 우선 처리
- active map puzzle이 없으면 기존 이동/조준 입력 유지
- 별도 hack 시작 버튼 없이 aim focus 경로로 맵 퍼즐 시작
- `APuzzleInteractable` 중 맵 해킹 component가 있으면 퍼즐 시작, 없으면 기존 즉시 node interaction 경로 사용 가능

## 7. API 요약

### UMapHackingPuzzleComponent

| 멤버 | 용도 | 권한 |
| --- | --- | --- |
| `StartHack()` | 시퀀스 생성, depth 초기화, Active 전환 | 서버 |
| `ProcessDirectionInput(EHackDirection)` | 방향 정답 판정, depth 진행, 실패 reset | 서버 |
| `CancelHack()` | Idle로 강제 정리 | 서버 |
| `GetPuzzleState()` | `Idle` / `Active` / `Solved` 조회 | 양쪽 |
| `GetCurrentDepth()` | 현재 depth index 조회 | 양쪽 |
| `GetCurrentDirection()` | 현재 정답 방향 조회 | 양쪽 |
| `GetCompletedDepthCount()` | 완료한 depth 수 조회 | 양쪽 |
| `GetDepthSequence()` | 전체 방향 시퀀스 조회 | 양쪽 |
| `OnHackStarted` | 시작 UI 동기화 | C++ |
| `OnSequenceReady` | 전체 방향 시퀀스 UI 전달 | C++ |
| `OnDepthChanged` | 현재 depth/방향 UI 전달 | C++ |
| `OnDepthSolved` | 성공 ring UI 전달 | C++ |
| `OnHackReset` | 실패 trigger | C++ |
| `OnHackCompleted` | 완료 trigger와 맵 장치 완료 연결 | C++ |

### UHackingPuzzleViewModel

| 멤버 | 용도 |
| --- | --- |
| `BindToComponent(PuzzleComp)` | HUD가 복제 component 상태 구독 |
| `UnbindFromComponent()` | HUD 상태 초기화 |
| `StartMapHacking(PuzzleComp)` | Standalone 테스트용. 클라에서는 `StartHack()` no-op 가능 |
| `StopMapHacking()` | UI 정리와 권위가 있을 때 cancel |
| `OnSuccessAnimationFinished()` | 성공 애니 후 UI 숨김 |
| `OnFailureAnimationFinished()` | 실패 애니 후 stop/unbind |
| `GetIsActiveVisibility()` | root visibility |
| `GetDirection0Index()` ~ `GetDirection2Index()` | 방향 이미지 선택 |
| `GetDepth1SuccessRingVisibility()` ~ `GetDepth3SuccessRingVisibility()` | 성공 ring 표시 |
| `GetResetTrigger()` | 실패 애니 trigger |
| `GetSolvedTrigger()` | 성공 애니 trigger |

### APuzzleInteractable

| 멤버 | 용도 |
| --- | --- |
| `TargetActors` | 완료 후 신호를 보낼 문/장치 목록 |
| `bIsRepeatable` | 체크한 퍼즐만 제한 횟수 재해킹 허용 |
| `MaxHackCount` | `bIsRepeatable=true`일 때의 최대 성공 횟수 |
| `CanInteractWithPuzzle()` | 재무장 상태와 잔여 해킹 횟수를 함께 확인 |
| `CompletePuzzle()` | 완료 상태 복제와 `TargetActors` trigger |
| `OnPuzzleSolvedChanged(bool)` | BP 표시 상태 변경 event |

## 8. Editor 연결 절차

1. 퍼즐 노드 actor 준비
   - `APuzzleInteractable` 기반 BP 또는 전용 자식 BP 생성
   - `UMapHackingPuzzleComponent` 추가
   - `TotalDepths=3`

2. 맵 배치
   - 퍼즐 노드를 문/장치 근처에 배치
   - Diana가 Roaming + Aim 상태에서 화면 중앙으로 조준할 수 있는 위치 확인
   - Hugh 기준 1500cm 안에 들어오는지 확인
   - visibility trace가 막히지 않게 collision 확인

3. 타겟 연결
   - 퍼즐 노드의 `TargetActors`에 열릴 문/장치 지정
   - 문이면 `APuzzleLockedDoor.RequiredPuzzleCount` 확인
   - 반복 장치면 퍼즐 노드 `Allow Repeated Hacking` 체크 후 `Max Hack Count` 설정

4. HUD 연결
   - Hugh/Diana HUD에 map hacking widget 배치
   - `UHackingPuzzleViewModel`을 widget에 연결
   - GameState active puzzle actor 변경 시 `BindToComponent()` 호출 경로 연결
   - Hugh HUD는 필요 시 `SetCompactMode(true)` 호출
   - active puzzle actor가 null이면 `UnbindFromComponent()` 호출
   - `BP_HughCharacter`/`BP_DianaOperatorPawn` defaults의 `GameplayHUDClass`가 역할별 root HUD를 가리키는지 확인

5. 입력 연결
   - Roaming + Aim + puzzle node 조준 시 시작 RPC 호출
   - active puzzle 중 방향 입력을 `EHackDirection`으로 변환
   - 방향 입력은 서버 RPC를 통해 `ProcessDirectionInput()` 호출
   - 성공/실패 애니 종료 callback 연결

## 9. 검증 체크리스트

Standalone:

- [ ] 퍼즐 노드에서 `StartHack()` 호출 시 HUD 표시
- [ ] 방향 3개가 UI에 표시
- [ ] 정답 방향 입력 때 success ring 순서대로 표시
- [ ] 오답 입력 때 실패 애니 trigger
- [ ] 성공 애니 종료 후 UI 숨김
- [ ] 완료 시 `CompletePuzzle()` 호출
- [ ] `TargetActors`의 문/장치가 반응

2-player listen server:

- [ ] Diana client가 Roaming + Aim 상태에서 퍼즐 노드 조준 가능
- [ ] 서버가 Hugh 거리, 화면 중앙 각도, LOS를 재검증
- [ ] Diana 입력이 서버 RPC로만 퍼즐 진행
- [ ] Hugh HUD와 Diana HUD가 같은 방향/진행도 표시
- [ ] 오답 reset이 양쪽 HUD에 보임
- [ ] 성공 후 양쪽 HUD가 정리
- [ ] 문/장치 완료 상태가 서버와 client에서 일치
- [ ] 이미 푼 1회성 퍼즐은 다시 시작되지 않음

실패 원인 빠른 확인:

- HUD가 안 뜸: active puzzle actor 복제 또는 ViewModel `BindToComponent()` 호출 확인
- HUD asset 자체가 안 뜸: possessed pawn class default의 `GameplayHUDClass`가 `WBP_HughHUD`/`WBP_DianaHUD`를 가리키는지 확인
- Diana에서 시작 안 됨: client에서 `StartHack()` 직접 호출 중인지 확인
- 방향 입력이 안 먹음: `ProcessDirectionInput()`이 서버에서 호출되는지 확인
- 문이 안 열림: `OnHackCompleted` -> `CompletePuzzle()` 바인딩 확인
- Hugh만 표시 안 됨: GameState active puzzle actor OnRep가 Hugh local HUD에도 연결됐는지 확인
- 실패 후 UI가 남음: WBP가 `OnFailureAnimationFinished()`를 호출하는지 확인

## 10. 수정 시 주의

- `UMapHackingPuzzleComponent`에 문/장치 직접 참조를 넣지 않음
- 맵 결과 처리는 `APuzzleInteractable::TargetActors` 경로 유지
- client target actor, camera origin, direction을 서버 판정에 그대로 쓰지 않음
- `TotalDepths`를 3보다 크게 늘리려면 ViewModel/WBP 슬롯도 같이 확장
- `CancelHack()` UI 동작이 필요하면 reset/cancel trigger 설계를 먼저 정리
- Blueprint asset 연결은 Editor에서 처리하고 C++ constructor hard load로 우회하지 않음
