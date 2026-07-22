# UDianaCleanseComponent Cleanse lifecycle 의도 카드

## 대상

`UDianaCleanseComponent::BeginPrecisionCleanse`, `UDianaCleanseComponent::CommitPrecisionCleanse`, `UDianaCleanseComponent::RequestQuickCleanse`, `UDianaCleanseComponent::GetCleanseCooldownProgress`

## 종류

Diana 정화 스킬

## 정의 위치

- `Source/Pragmata_2P/Components/DianaCleanseComponent.h`
- `Source/Pragmata_2P/Components/DianaCleanseComponent.cpp`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.cpp`

## 이 코드가 존재하는 이유

Diana가 별도 정화 스킬 입력으로 dead filament obstacle을 제거하고 오염된 enemy visual state를 해제하게 하기 위해 존재한다. client는 입력 lifecycle intent만 보내고, 실제 target 판정은 server가 mode별 aim source, role, action lock, active hack 상태를 다시 검증한 뒤 처리한다.

## 사용해야 하는 상황

- `IA_Cleanse` 입력이 들어왔을 때
- dead filament obstacle을 정화 스킬로 제거할 때
- 오염된 enemy의 `bIsTainted` 값을 false로 바꿀 때

## 사용하면 안 되는 상황

- 일반 `IA_Interact` 입력이나 node interaction 입력을 처리하는 용도
- screen pixel radius 기반 focus 후보를 그대로 정화 스킬 대상으로 쓰는 용도
- enemy hack 또는 map hack UI가 active인 동안 같은 입력 흐름에 끼워 넣는 용도

## 기본 흐름

정밀 조준:

`IA_Aim` hold -> `IA_Cleanse` started -> `BeginPrecisionCleanse()` -> server 검증 -> `Multicast_SetCleansePreviewMode(PrecisionDianaCamera)` + `AS_Diana_Attack_605_Clean_Up_Start_Lv2` -> start 종료 뒤 `AS_Diana_Attack_606_Clean_Up_Loop_Lv2` -> `IA_Cleanse` release -> `AS_Diana_Attack_633_Clean_Up_Release_Lv2` 시작과 같은 server frame에 `CommitPrecisionCleanse()` -> cooldown 소비 -> `Multicast_StartCleanseCooldown()` -> 3000cm range, 60도 cone, visibility LOS target 수집 -> dead filament `Interact()` 또는 monster `CleanseTaint()`

빠른 실행:

`IA_Cleanse` started while not aiming -> `RequestQuickCleanse()` -> server 검증 -> `Multicast_SetCleansePreviewMode(QuickHughAim)` -> Hugh camera/조준 방향 preview 0.5초 표시 -> server timer 만료 -> loop 없이 `AS_Diana_Attack_633_Clean_Up_Release_Lv2` 시작과 같은 server frame에 cast -> cooldown 소비 -> `Multicast_StartCleanseCooldown()` -> Hugh 기준 3000cm range, 60도 cone, visibility LOS target 수집

각 client의 Diana pawn replica tick -> active preview mode 확인 -> mode별 source actor 아래 surface trace -> mode별 aim direction을 surface plane에 projection -> surface hit를 fan apex로 두고 `CleanseRange`/`CleanseHalfAngleDegrees` preview decal 갱신

cooldown UI:

정화 실행 확정 -> server가 `bCanUseCleanse=false`, cooldown 시작 server time, duration 복제 -> 양쪽 HUD ViewModel이 `OnCleanseCooldownChanged`와 tick refresh로 `bCanUseCleanse`, `CleanseCooldownProgress` FieldNotify 갱신 -> 5초 timer 만료 -> server가 `bCanUseCleanse=true` 복제

## 이 코드를 호출하는 쪽

- `ADianaOperatorPawn::SetupPlayerInputComponent`
- `ADianaOperatorPawn::OnStartCleanse`
- `ADianaOperatorPawn::OnCompleteCleanse`
- `ADianaOperatorPawn::OnCompleteAim`

## 이 코드가 호출하는 쪽

- `ADeadFilamentObstacle::Interact_Implementation`
- `AMonsterBase::CleanseTaint`
- `PragmataTraceUtils::IsHitActorWithinCandidate`
- `UPragmataHUDViewModel::BindCleanseComponent`

## 서버/클라이언트 조건

정밀 조준과 빠른 실행 요청은 owning client 입력에서 호출될 수 있지만 gameplay 판정은 server RPC와 server timer에서만 실행한다. server는 assigned role이 Diana인지, Diana action lock이 없는지, enemy/map hack UI가 active가 아닌지 다시 확인한다. 정밀 조준은 Diana aiming 상태도 요구하고, 빠른 실행은 Hugh camera/조준 방향을 사용한다.

## Blueprint 연결 여부

`IA_Cleanse` asset은 `/Game/Blueprints/Player/Input/Diana/IA_Cleanse`에 있고 `IMC_Diana`에서 `E`, `Gamepad_LeftShoulder`에 매핑한다. preview material은 `/Game/VFX/Diana/M_CleanseConePreview`이며 `PAL_CoreRuntime`에 explicit runtime asset으로 들어간다. component의 cooldown/range/half-angle/trace channel과 preview trace/projection 값은 Blueprint default에서 tuning 가능하다.

## 헷갈리기 쉬운 코드

- `UDianaNodeInteractionComponent`의 focus 계산을 직접 재사용하지 않는다. 정화 스킬은 같은 authoritative camera 재계산 패턴만 공유한다.
- 각도 판정은 화면 중앙 radius가 아니라 `CameraDirection`과 `ToTarget`의 3D dot angle이다.
- `ADeadFilamentObstacle::Interact_Implementation()`은 Cleanse component가 실행 중일 때만 제거된다.
- preview decal은 양쪽 player 화면 안내일 뿐이며 server target 판정이나 LOS 결과를 대체하지 않는다.
- preview decal은 Unreal extent 기준이라 `Z`는 전체 길이가 아니라 `CleanseRange * 0.5`, `Y`는 far edge half-width인 `CleanseRange * tan(CleanseHalfAngleDegrees)`이다.
- preview material은 decal UV 대신 `AbsoluteWorldPosition`과 dynamic material parameter의 apex/forward/right/range 값을 사용해 fan 시작점이 player surface hit에서 벗어나지 않게 한다.
- cooldown 중 preview 숨김은 multicast presentation 상태다. 실제 정화 가능 여부는 mode별 server RPC와 `CanStartCleanse()`가 결정한다.
- `bCanUseCleanse`는 cooldown readiness만 뜻한다. action lock, hack active, aim mode 같은 거부 조건은 실행 요청 시 server 검증으로 유지한다.
- `GetCleanseCooldownProgress()`는 UI progress bar용 `elapsed / max` 값이며, cooldown 중 매 tick 복제하지 않고 server time 기준으로 client가 계산한다.

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Hacking/MonsterBase-TaintState.md`
- `Docs/Intent/Hacking/DianaNodeInteractionComponent-RequestNodeInteraction.md`
- `Source/Pragmata_2P/UI/PragmataHUDViewModel.h`

## 관련 커밋

미확인. `git log -S"DianaCleanseComponent" -- Source Docs`로 추적.

## 확실한 근거

- `ADianaOperatorPawn`이 `IA_Cleanse`를 soft-load하고 `Started`, `Completed`, `Canceled`에 정화 스킬 lifecycle handler를 bind한다.
- `UDianaCleanseComponent` 기본값은 cooldown 5초, range 3000cm, half-angle 30도이다.
- `UDianaCleanseComponent::bCanUseCleanse`는 replicated cooldown readiness이고, `GetCleanseCooldownProgress()`는 사용 직후 0.0에서 완료 시 1.0으로 증가한다.
- 빠른 실행 preview 기본 시간은 0.5초이고, release 여부와 무관하게 server timer 만료 시 실행한다.
- `Multicast_PlayCleanseAnimation()`은 body slot presentation만 처리한다. target, cooldown, unlock 판정은 여전히 server 함수가 처리한다.
- `UDianaCleanseComponent`가 양쪽 client의 Diana pawn replica에서 `M_CleanseConePreview` decal을 표시하고, cooldown, enemy/map hack active, action lock 중에는 숨긴다.
- `Content/Blueprints/Player/Input/Diana/IMC_Diana.uasset`에 `E`, `Gamepad_LeftShoulder` mapping이 들어간다.

## 추정한 의도

정화 스킬은 Diana hack UI와 별도인 area utility skill로 두고, 정밀 조준은 Diana가 직접 target을 잡고 빠른 실행은 Hugh가 보고 있는 target을 지원하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

정화 스킬 실행 순간의 audio feedback은 아직 정해지지 않았다. HUD cooldown 표시는 `UPragmataHUDViewModel` FieldNotify 값으로 넘긴다.

## 검증 방법

```powershell
rg -n "BeginPrecisionCleanse|CommitPrecisionCleanse|RequestQuickCleanse|GetCleanseCooldownProgress|bCanUseCleanse|BindCleanseComponent" Source Docs
```

## 다음 수정 시 주의점

range나 angle을 조정할 때는 dead filament 제거와 오염된 enemy 정화가 같은 target collection 정책을 공유하는지 함께 확인한다.
preview는 같은 range/half-angle 값을 쓰므로 gameplay 값 변경 시 decal extent와 material mask가 함께 맞는지 확인한다.
HUD에 정화 스킬 상태를 추가할 때는 Widget Blueprint가 component를 직접 읽지 않고 `UPragmataHUDViewModel` FieldNotify 값을 바인딩하게 유지한다.
