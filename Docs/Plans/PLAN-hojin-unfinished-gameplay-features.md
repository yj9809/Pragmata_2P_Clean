# Hojin 미완료 gameplay 기능 통합 구현 계획

## 목표

- Checkpoint 활성화, 10초 지연 team respawn, 자원 보충, 양 player upgrade UI를 하나의 server-authoritative flow로 연결
- Hugh 사망 즉시 Diana avatar를 `DianaMountSocket_root_death`에 재부착하고 respawn 시 원래 socket 복구
- scan wave가 POI 자체 UI 표시 trigger를 거리순으로 실행하도록 변경
- `BP_DeadFilament_Box` 상호작용 전 정화 스킬 사용 차단 및 획득 tutorial 표시
- Diana 전용 player-owned OverDrive face cinematic과 Hugh 중심 50m/20초 범위 hack, orange emissive ring 구현
- 기능별 구현, 검증, 문서화를 독립 commit으로 분리
- Diana map/enemy hack 거리 50m와 자유 yaw, 대형 Blueprint의 전체 표면 조준 판정 보장
- combat 중 Hugh/Diana tense animation 제거와 enemy별 Lunafilament reward 추가

## 확정 결정

### Checkpoint, death, respawn

- inactive checkpoint 활성화는 Diana player의 `IA_Interact` 3초 hold만 허용
- checkpoint 상호작용 자체는 player를 이동시키지 않음
- checkpoint 활성화 완료와 active checkpoint 재상호작용 모두 즉시 아래 자원 보충
  - Hugh HP 최대치
  - 모든 보유 weapon ammo 최대치
  - heal/repair cartridge 최대치
- active checkpoint 재상호작용 시 Hugh/Diana 양쪽 local client에 upgrade UI 표시
- upgrade UI가 열린 동안 양 player의 이동, 시점, 공격, scan, hack, cleanse, interaction 입력 잠금
- death 조건은 authoritative Hugh HP `0`
- death 조건 확정 시점부터 10초 동안 death animation 전체 재생
- 10초 타이머 만료 후에만 active checkpoint 위치로 team respawn
- respawn 시 HP, 모든 보유 weapon ammo, heal cartridge 최대치 보충
- active checkpoint가 없으면 10초 후 기존 `PlayerStart` fallback
- checkpoint 위치는 현재 map session에만 유지. disk `SaveGame`과 map travel 영구 저장 제외
- checkpoint animation과 camera 연출 제외
- Diana가 checkpoint를 정확히 조준할 때만 object-owned keybind UI 표시
- inactive checkpoint는 hold 중 `Elapsed / 3.0`을 매 frame `UHoldKeyPromptWidget::SetHoldProgress()`에 전달
- focus/조준/거리/입력 유지 조건이 깨지면 progress를 즉시 `0`으로 reset
- active checkpoint는 같은 widget을 일반 click guide로 사용하고 progress를 연속 갱신하지 않음

### Diana aim과 대형 interaction actor

- Diana map interaction과 enemy hack 최대 거리 모두 `5000cm`
- Hugh forward 기준 camera yaw `±105°` clamp 제거. 기존 pitch 안전 범위 유지
- `BP_PuzzleNode_Elevator`는 visible/interactable surface 어디를 조준해도 focus 가능해야 함
- `BP_BossRouteElevator` 내부 배치가 원인인지는 미확정. component hierarchy, collision ownership, trace hit actor, POI 등록 actor를 조사한 뒤 원인 확정
- actor origin/root bone 방향이 아니라 camera ray가 candidate 또는 명시적으로 소유한 interaction surface를 hit하는 방식으로 판정
- 시야 또는 navigation을 실제로 막는 mesh/collision은 hack interaction도 차단
- `BP_BossRouteElevator`에서는 `BP_PuzzleNode_Elevator` mesh만 hack target. 부모의 `ElevatorMesh`는 interaction target에서 명시적으로 제외

### Diana death attachment

- death 확정 즉시 Diana avatar attachment를 `DianaMountSocket_root`에서 `DianaMountSocket_root_death`로 변경
- Diana death montage 첫 frame보다 attachment 변경이 먼저 적용
- death 중 기존 IK alpha 0 처리 유지
- 10초 후 respawn reset에서 `DianaMountSocket_root`로 복구

### Scan

- gameplay HUD marker와 viewport edge marker를 더 이상 사용하지 않음
- server는 기존 POI registry, 50m radius, 거리순 정렬, `RevealDelay` 계산 유지
- 양 owning client는 각 결과의 `RevealDelay` 시점에 target POI actor의 local UI visibility trigger 호출
- POI icon/widget은 ItemBox처럼 object actor의 `WidgetComponent`가 소유
- scan icon widget은 develop에 병합된 `WBP_ScanIconWidget` / `UScanIconWidget` 계약 사용
- 각 scan 가능 Blueprint에 screen-space `WidgetComponent`를 배치하고 Widget Class를 `WBP_ScanIconWidget`으로 설정
- C++은 POI 표시/숨김 trigger contract까지만 제공
- 각 Blueprint의 실제 widget/icon binding은 별도 Editor 작업. binding이 없더라도 trigger 호출 검증까지 수행
- scan 종료 시 이번 scan으로 표시한 모든 POI에 hide trigger 호출
- scan 재시작, actor destroy, travel 시 기존 local reveal timer와 표시 상태 정리
- blue post-process duration을 `3.0초`에서 `1.5초`로 변경
- blue effect 강도는 현재 post-process 결과와 neutral 상태 사이 50%로 축소

### Cleanse unlock

- 획득 actor는 정확히 `/Game/Map/Interaction/BP_DeadFilament_Box`
- 해당 Blueprint의 현재 native parent는 `AItemBoxInteractable`
- 정화 획득 전 quick/precision cleanse 모두 사용 불가
- 획득 상태 source of truth는 Diana `APragmataPlayerState`
- `BP_DeadFilament_Box`의 유효한 Diana 상호작용 1회로 unlock
- unlock 후 Diana player에게 cleanse tutorial UI 표시
- cleanse pickup은 scan target이 아님
- cleanse pickup object UI는 scan과 별도로 20m vicinity에서 표시
- 20m 계산은 player-side local presentation으로 처리
  - 각 local client의 controller/pawn 쪽 proximity manager가 acquisition prop 조회
  - icon/widget 자체는 prop의 `WidgetComponent`가 소유
  - 실제 interaction과 unlock은 server가 거리, role, 1회 상태 재검증
- 같은 map session의 death/checkpoint respawn 뒤 unlock 유지

### OverDrive

- slowdown 없음
- Diana player만 full gauge에서 발동 가능
- 발동 승인 시 gauge 전량 소비
- OverDrive는 Level Sequence를 사용하지 않음. Diana owning client의 player-owned camera가 Diana 얼굴을 보여주고 전용 animation을 재생
- cinematic 동안 실제 `UnPossess()`는 하지 않음
- Diana owning `APragmataPlayerController`가 explicit local cinematic lock 적용
  - `SetIgnoreLookInput(true)`
  - movement 차단
  - aim, hack, scan, cleanse, interaction, OverDrive 재입력 차단
- cinematic finish, abort, animation load failure, timeout에서 같은 cleanup 함수로 lock 해제
- gameplay effect center는 server의 Hugh 위치
- effect 시점 Hugh 위치 5000cm 안의 alive enemy를 20초 hacked 상태로 전환
- OverDrive hack은 puzzle 완료, hack damage, reward, 추가 gauge 획득을 발생시키지 않음
- 이미 hacked 상태인 enemy는 effect 시점부터 남은 시간을 20초로 갱신
- world effect는 양 player에게 표시
- Hugh 위치에서 `Z > 0` offset으로 orange emissive ring 생성
- ring은 floor decal이 아닌 mesh/Niagara 기반 world-space ring
- cinematic이 정상 종료된 뒤 ring과 50m hack gameplay effect 실행

### Upgrade UI ownership과 안전 상태

- Checkpoint replenish 직후 Hugh와 Diana 양쪽 화면에 동일한 upgrade menu 표시
- Hugh controller만 `UPragmataUpgradeMenuWidget` owning/input controller로 사용하고 F/C hold, 선택 변경, upgrade/refund 요청 수행
- Diana 화면은 Hugh의 선택과 menu state를 그대로 반영하는 read-only mirror. Diana 입력으로 선택·hold·실행·닫기 불가
- menu open 동안 Hugh/Diana 모두 movement, camera, combat, interaction 입력 잠금
- menu session 동안 server-authoritative damage 무효화. UI close와 모든 abnormal cleanup 경로에서 invincibility 해제
- Notion `Pragmata_2P 강화(Upgrade) UI 연결 가이드 R1` 기준 기존 F/C hold, server RPC, shared stats delegate 갱신 재사용

### Scan POI 표시 시간

- scan wave가 POI에 도달한 순간 object-owned UI를 5초 표시
- 같은 POI를 다시 scan하면 기존 hide timer를 취소하고 도달 시점부터 5초로 갱신
- Cleanse pickup의 20m vicinity UI는 scan timer와 무관하며 Hugh 위치 기준

### Combat idle과 enemy reward

- combat 여부와 무관하게 Hugh와 Diana 모두 기존 non-tense idle animation 사용
- enemy death 시 server가 shared Hugh stats에 Lunafilament 1회 지급
- enemy Blueprint별 `EditDefaultsOnly` reward 설정, C++ 기본값 `50`, 음수 입력 방지
- destroy, death delegate 중복, OverDrive hack, puzzle completion으로 reward 중복 지급 금지
- ring radius는 `0 -> 5000cm`로 확장하고 server cast timing과 동기화

## 설계 답변

### OverDrive player-owned cinematic 중 Diana control 차단

OverDrive는 map Level Sequence를 사용하지 않는다. player-owned face camera만으로 Diana의 aim, hack, scan, cleanse 같은 입력을 안전하게 막을 수 없으므로 별도 input lock이 필요하다.

따라서 player-owned camera/animation은 presentation만 담당하고, `APragmataPlayerController`와 `ADianaOperatorPawn`의 local cinematic lock이 모든 Diana gameplay intent를 차단한다. Controller possession은 유지하므로 owning RPC와 cinematic 종료 후 control 복구가 안정적이다.

### Diana skill animation 계약

- `ABP_Diana`는 `ABP_Diana_Title`와 같은 `Layered Blend per Bone` 구성을 사용한다. body는 base pose, face는 `Neck` branch의 blend pose이며 기존 mount IK와 death IK 차단보다 앞에서 합성한다.
- default face: `/Game/Character/Diana/Animations/Facial/Emotion/AS_Diana_Face_Emotion_Calm_Basic_Loop.AS_Diana_Face_Emotion_Calm_Basic_Loop`
- cleanse acquisition shared Level Sequence 중 body: `/Game/Character/Diana/Animations/Reinforce/AS_Diana_Reinforce_Gauge_VerA.AS_Diana_Reinforce_Gauge_VerA`, face: `/Game/Character/Diana/Animations/Facial/Minidemo/AS_Diana_Face_MiniDemo_md10024_GaugeReinforce_VerA.AS_Diana_Face_MiniDemo_md10024_GaugeReinforce_VerA`
- precision cleanse: aim 시작 `AS_Diana_Attack_605_Clean_Up_Start_Lv2`, start 종료 뒤 `AS_Diana_Attack_606_Clean_Up_Loop_Lv2`, release와 같은 frame에 `AS_Diana_Attack_633_Clean_Up_Release_Lv2` 및 gameplay cast
- quick cleanse: loop 없이 release animation과 gameplay cast만 실행
- OverDrive Diana-only cinematic: body `/Game/Character/Diana/Animations/Grapple/AS_Diana_Grapple_0010_HacK_SP.AS_Diana_Grapple_0010_HacK_SP`, face `/Game/Character/Diana/Animations/Facial/Grapple/AS_Diana_Face_Grapple_SPHack.AS_Diana_Face_Grapple_SPHack`
- scan: `/Game/Character/Diana/Animations/General2/AS_Diana_General2_2000_Scan_VerA.AS_Diana_General2_2000_Scan_VerA`
- hack fail은 lowest priority: `/Game/Character/Diana/Animations/Attack/AS_Diana_Attack_0132_Hacking_End_Mistake_VerB.AS_Diana_Attack_0132_Hacking_End_Mistake_VerB`

### Cleanse pickup 20m 계산 위치

player-side local 계산을 사용한다.

- UI 표시 여부는 client-local presentation
- player/controller가 자신의 위치와 근처 acquisition prop을 계산
- prop은 icon/widget과 `SetVicinityIndicatorVisible(bool)` presentation 함수만 소유
- server는 icon visibility를 복제하지 않고 실제 획득 interaction만 검증

prop마다 Tick으로 모든 player를 찾는 방식은 prop 수에 따라 중복 계산이 늘고 local UI ownership이 흐려지므로 사용하지 않는다.

## 기존 코드 기준점

- `ACheckPoint`는 로직 없는 `AAnimatedPropInteraction` 자식: `Source/Pragmata_2P/Props/CheckPoint.h:9`
- Diana `IA_Interact`는 현재 `Started`만 bind: `Source/Pragmata_2P/Pawns/DianaOperatorPawn.cpp:93`
- 기존 hold UI progress/reset: `Source/Pragmata_2P/UI/HoldKeyPromptWidget.cpp:9`
- Hugh death는 movement와 animation state만 정리: `Source/Pragmata_2P/Characters/HughCharacter.cpp:2302`
- health component에는 full respawn reset API가 없음: `Source/Pragmata_2P/Components/PragmataHealthComponent.h`
- weapon ammo getter/setter: `Source/Pragmata_2P/Weapon/WeaponBase.h:49`, `:54`
- heal cartridge max/setter: `Source/Pragmata_2P/Components/PragmataPlayerStatsComponent.h:42`, `:72`
- upgrade widget은 정식 close/game input 복구가 없는 test open만 보유: `Source/Pragmata_2P/UI/PragmataUpgradeMenuWidget.cpp:173`
- upgrade WBP directory는 현재 package 제외 상태: `Config/DefaultGame.ini:115`, `Docs/pending_cook.md:9`
- Diana normal mount socket: `Source/Pragmata_2P/Characters/HughCharacter.h:496`
- Diana avatar attach helper: `Source/Pragmata_2P/Characters/HughCharacter.cpp:2432`
- node focus는 현재 candidate `ActorLocation`의 10° cone과 해당 origin까지 LOS 사용: `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.cpp:371`
- node trace와 enemy hack 기본 거리는 각각 `3000cm`: `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.h:72`, `Source/Pragmata_2P/Components/DianaHackComponent.h:67`
- Diana yaw는 Hugh forward 기준 `±105°` clamp: `Source/Pragmata_2P/Pawns/DianaOperatorPawn.cpp:649`, `.h:236`
- Diana death와 IK 차단: `Source/Pragmata_2P/Animation/DianaAnimInstance.cpp:133`, `:307`
- scan은 이미 거리 기반 `RevealDelay` 계산과 거리순 정렬 사용: `Source/Pragmata_2P/Components/DianaScanComponent.cpp:154`
- 현재 scan 결과는 HUD overlay로 전달: `Source/Pragmata_2P/Game/PragmataPlayerController.cpp:218`
- 현재 blue pulse duration은 `3.0초`: `Source/Pragmata_2P/Components/DianaScanComponent.cpp:244`, `:257`
- blue pulse post-process weight/settings: `Source/Pragmata_2P/Game/PragmataPlayerController.cpp:556`
- POI actor-level scan scheduler/visibility 연결은 아직 없지만 widget-level 표시 API는 존재
- `UScanIconWidget::SetScanIconVisible(bool)`와 `SetDistanceText(FText)`는 구현됨: `Source/Pragmata_2P/UI/ScanIconWidget.h`
- scan WBP asset: `/Game/UI/Widget/Scan/WBP_ScanIconWidget`
- `BP_DeadFilament_Box` native parent는 `AItemBoxInteractable`
- tutorial local 표시 subsystem: `Source/Pragmata_2P/Game/TutorialManagerSubsystem.cpp:10`
- 정화 component는 cooldown readiness만 복제: `Source/Pragmata_2P/Components/DianaCleanseComponent.h:139`
- OverDrive component는 gauge 증감만 구현: `Source/Pragmata_2P/Components/OverDriveComponent.h:23`
- enemy hacked state와 duration timer: `Source/Pragmata_2P/Enemy/MonsterBase.cpp:488`, `:506`
- project는 `LevelSequence`, `MovieScene` module을 이미 사용

## 구현 순서와 commit 경계

| 순서 | 독립 commit | 이유 |
| --- | --- | --- |
| 1 | Diana aim/interaction 기반 수정 | 50m 공통 거리, 자유 yaw, 대형 actor surface hit 판정 확립 |
| 2 | Diana death socket 전환 | 가장 작고 독립적인 death presentation fix |
| 3 | Scan POI object UI trigger와 blue pulse 조정 | HUD marker 제거와 local POI trigger를 한 presentation 변경으로 확정 |
| 4 | `BP_DeadFilament_Box` cleanse unlock/tutorial | 기존 정화 위에 acquisition gate와 vicinity UI 추가 |
| 5 | OverDrive player-owned cinematic, input lock, ring, 범위 hack | enemy hack duration API 확정 뒤 신규 active skill 연결 |
| 6 | Combat idle 통일 | Hugh/Diana animation-only 변경 분리 |
| 7 | Enemy Lunafilament reward | enemy death와 shared stats 지급 경계 분리 |
| 8 | Checkpoint 활성화, hold prompt, refill, 10초 respawn, mirrored upgrade UI | 공용 focus/hold UI와 death reset을 마지막에 통합 |

각 commit은 해당 기능의 `Docs/KNOW_HOW.md`와 필요한 `Docs/Intent/**/*.md` 갱신을 포함한다. 서로 다른 기능의 코드, asset, 문서를 한 commit에 섞지 않는다.

## 기능 commit: Diana aim/interaction 기반 수정

### 예상 commit

`fix: Diana 대형 오브젝트 조준 판정과 자유 시점 수정`

### 조사와 구현

- [x] Editor에서 `BP_PuzzleNode_Elevator`와 `BP_BossRouteElevator`의 attachment/ChildActor/component hierarchy 기록
- [x] mesh/collision별 trace response와 실제 hit actor/component/owner 확인
- [x] POI registry candidate와 ray hit actor를 `PragmataTraceUtils::IsHitActorWithinCandidate()` attachment chain으로 확인
- [x] target 선택을 candidate `ActorLocation` 10° cone 방식에서 camera ray surface hit 기반으로 변경
- [x] child/attached component hit를 candidate로 resolve하는 attachment-chain interaction-surface contract 추가
- [x] first blocking camera ray hit만 허용해 다른 actor가 앞을 가리면 뒤 candidate 선택 금지
- [x] map interaction, node trace, enemy hack 관련 최대 거리를 `5000.f`로 통일
- [x] POI별 `DianaInteractionDistance` 조사: Puzzle/Checkpoint는 5000cm, ItemBox는 의도된 750cm 유지
- [x] `BP_Elevator_PuzzleNode_0`의 PuzzleNode mesh에 속한 모든 visible collision surface를 candidate로 허용
- [x] `BP_BossRouteElevator.ElevatorMesh` hit는 candidate로 resolve하지 않음
- [x] `ADianaOperatorPawn`의 Hugh forward yaw clamp 제거, 기존 pitch clamp와 rotation replication 유지
- [x] 관련 `Docs/Intent`와 `Docs/KNOW_HOW.md` 갱신

### 주요 파일/asset

- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.h/.cpp`
- `Source/Pragmata_2P/Components/DianaHackComponent.h/.cpp`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.h/.cpp`
- `BP_PuzzleNode_Elevator`, `BP_BossRouteElevator` asset

### 완료 조건

- [x] Diana camera가 Hugh forward 기준 105°를 넘어 연속 회전 가능
- [x] 기존 상하 pitch 안전 제한 유지
- [x] 40m 초과 50m 이내 map/enemy hack 가능, 50m 초과 불가
- [x] Elevator bottom root뿐 아니라 상단·측면을 포함한 PuzzleNode mesh 전체 조준으로 puzzle 표시
- [x] Elevator 앞 blocker를 조준하면 뒤 puzzle 미선택
- [x] 일반 PuzzleInteractable과 ItemBox focus 회귀 없음
- [x] client/server가 같은 ray-surface validation 함수를 사용

## 기능 commit: Diana death socket 전환

### 예상 commit

`fix: Diana 사망 socket 전환 추가`

### 구현

- [x] `AHughCharacter`에 exact death socket `DianaMountSocket_root_death` 상수/property 추가
- [x] `SetDianaAvatarDeathAttachment(bool bUseDeathSocket)` helper 추가
- [x] normal/death socket 모두 `DoesSocketExist()`로 검증
- [x] authoritative `HandleDeath()` 시작 시 death socket으로 즉시 전환
- [x] client replicated death transition에서도 같은 local attach helper 호출
- [x] `UPairedAnimationComponent::PlayCue()`의 Death 분기에서 Diana montage 재생 전 death attachment 재확인
- [x] 기존 `UDianaAnimInstance::UpdateDeathState()`와 death 중 IK alpha 0 유지
- [x] respawn reset에서 normal `DianaMountSocket_root`로 복구
- [x] socket 누락 시 source 식별 error log와 mesh-root fallback 적용

### 주요 파일/asset

- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`
- `Source/Pragmata_2P/Components/PairedAnimationComponent.cpp`
- `Source/Pragmata_2P/Animation/DianaAnimInstance.cpp`
- Hugh skeletal mesh/skeleton socket 확인

### 완료 조건

- [x] HP 0 frame에 attachment socket이 `DianaMountSocket_root_death`로 변경
- [x] death montage 첫 frame 전에 attachment 변경 완료
- [x] death 중 IK alpha 0 유지
- [x] 10초 death presentation 동안 normal socket으로 돌아가지 않음
- [x] respawn reset에서 `DianaMountSocket_root` 복구
- [x] replicated death/respawn helper 경로가 양 listen host role에서 동일

## 기능 commit: Scan POI object UI trigger와 blue pulse 조정

### 예상 commit

`fix: Scan을 POI object UI trigger로 변경`

### 구현

- [x] `APropInteractionBase`에 공통 screen-space `ScanIconComponent`와 `WBP_ScanIconWidget` class 계약을 추가
- [x] `Client_ShowPOIScanResults`의 HUD 전달을 local POI reveal scheduler로 교체
- [x] 각 reveal에서 object UI hide timer를 5.0초로 시작하고 rescan reveal 시 restart
- [x] target actor의 scan용 `UWidgetComponent`를 명시적으로 찾고 `InitWidget()` 뒤 `GetUserWidgetObject()`를 `UScanIconWidget`으로 cast
- [x] reveal 시 `SetScanIconVisible(true)`, hide/cleanup 시 `SetScanIconVisible(false)` 호출
- [x] reveal 시 scan origin과 target 사이 거리를 meter text로 변환해 `SetDistanceText()` 호출
- [x] actor에 다른 WidgetComponent가 함께 있을 수 있으므로 무조건 첫 `FindComponentByClass`에 의존하지 않고 component tag 또는 전용 component reference 사용
- [x] `WBP_ScanIconWidget`의 `Root`/`DistanceText` tree 이름을 UnreelMCP로 확인하고 optional `Root` 누락도 안전하게 처리
- [x] scan result별 `RevealDelay` timer 생성
- [x] timer 만료 시 해당 client world의 target POI show trigger 호출
- [x] reveal 뒤 `ScanDuration` 동안 target icon을 유지한 뒤 hide
- [x] rescan과 controller EndPlay에서 old timer와 visible target 정리
- [x] `UPragmataHUDWidget::ShowPOIScanResults()` 호출 중단
- [x] `WBP_POIScanOverlay`와 `WBP_POIScanMarker`는 legacy asset으로 남기고 runtime 경로에서 제외
- [x] Checkpoint를 `EPragmataScanTargetKind::Checkpoint`와 server scan whitelist에 추가
- [x] cleanse pickup은 scan enum/whitelist에 추가하지 않음
- [x] pulse RPC duration을 `3.0 -> 1.5초`로 변경
- [x] final `BlendWeight` 또는 neutral-to-current interpolation alpha를 `0.5`로 적용해 전체 post-process 강도 절반 처리
- [x] blue channel 상수만 단순 절반 처리하지 않음
- [x] 기존 radius, `RevealDelay`, 정렬, cooldown, combat/action lock 유지
- [x] `PLAN-diana-poi-scan-ui-effect.md`, scan intent, `KNOW_HOW.md`를 새 object UI flow로 갱신

### 주요 파일/asset

- `Source/Pragmata_2P/Interfaces/PragmataScannable.h`
- `Source/Pragmata_2P/Props/PropInteractionBase.h`
- `Source/Pragmata_2P/Props/PropInteractionBase.cpp`
- `Source/Pragmata_2P/Components/DianaScanComponent.cpp`
- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`
- 대상 POI Blueprint의 object-owned `WidgetComponent`
- `Source/Pragmata_2P/UI/ScanIconWidget.h/.cpp`
- `Content/UI/Widget/Scan/WBP_ScanIconWidget.uasset`

### 완료 조건

- [x] gameplay HUD에 scan marker/label/edge arrow가 생성되지 않음
- [x] 가까운 POI show trigger가 먼 POI보다 먼저 호출
- [x] POI별 show trigger 시각이 기존 `RevealDelay`와 일치
- [x] scan 종료 시 표시된 모든 POI hide trigger 호출
- [x] Blueprint binding이 없는 POI에서도 crash 없이 trigger log 확인
- [x] Checkpoint는 scan 대상에 포함
- [x] `BP_DeadFilament_Box`는 scan 대상에서 제외
- [x] blue effect duration `1.5초`
- [x] 같은 장면 기준 blue intensity가 기존 결과의 50% 수준
- [x] 양 local client에서 각자 object-owned UI trigger 수신
- [x] scan 대상 Blueprint의 WidgetComponent가 `Space=Screen`, runtime `WidgetClass=WBP_ScanIconWidget`으로 구성
- [x] reveal 시 icon과 distance text 표시, 5초 뒤 숨김
- [x] 여러 WidgetComponent를 가진 actor에서도 scan icon widget만 선택
- [x] WBP의 `Root`/`DistanceText` BindWidgetOptional 이름이 실제 widget tree와 일치

## 기능 commit: `BP_DeadFilament_Box` cleanse unlock/tutorial

### 2026-07-13 재분석 결론

- Blueprint parent와 acquisition C++는 존재하지만 상호작용 진입 조건이 모두 silent return이라 실패 원인이 화면과 log에 남지 않음
- 진입 조건은 Diana `Roaming + Aim`, non-combat, Hugh 기준 `750cm`, camera ray가 acquisition actor를 직접 hit하는 상태
- `Server_RequestNodeInteraction()`은 acquisition 시작 성공 여부와 무관하게 일반 ItemBox를 즉시 local complete 처리해 실패 뒤 재시도를 막을 수 있음
- 현재 `LS_CleanseAcquisition`은 camera-only `30fps`, `0..150` frame의 5초 sequence
- Diana body는 `AS_Diana_Reinforce_Gauge_VerA`를 dynamic montage로 재생하며 authored Montage, Orange Chip, `R_Thumb3` 구간 처리는 없음
- body animation은 `60fps`, `9.116667초`; source frame `153..422` 전체 표시 구간은 `2.55..7.05초`의 half-open interval
- 현재 sequence 5초와 POV return 1.25초가 chip hold 제거 시점 7.05초보다 먼저 끝나므로 lifecycle 재구성이 필요
- canonical skill flag는 이미 replicated `bCleanseUnlocked=false`로 존재하며 이름 변경 불필요
- `WBP_OverDriveGauge`는 unlock을 polling하지만 `ApplyPurificationPanelVisibility()`가 상태를 무시하고 항상 표시하는 회귀가 존재
- 기존 final ItemBox 호출은 기계 animation뿐 아니라 `bIsOpened=true`와 Lunafilament 지급까지 실행하므로 획득 보상과 분리 필요

### 요구사항 요약

- DeadFilament Box 상호작용 성공 시 gameplay control과 normal camera를 잠그고 acquisition cinematic 시작
- `AS_Diana_Reinforce_Gauge_VerA` 기반 authored AnimMontage 재생
- source frame `153..422` 동안 Orange Chip skeletal mesh의 `root`를 Diana `R_Thumb3`에 부착해 손에 든 연출 표시
- cinematic 종료 후 Diana tutorial 표시, `bCleanseUnlocked=true` 확정, OverDrive gauge 신규 정화 UI 강조
- 강조 종료 뒤 gameplay control과 player camera 복귀
- 마지막에 DeadFilament Box 닫힘 animation 1회 재생
- 실패, 중단, 사망, EndPlay, client 이탈에서도 camera, control, chip component, pending state 정리

### 구현 결정

- Unreal의 실제 `UnPossess/Possess`는 controller ownership, RPC, respawn, camera lifecycle을 불필요하게 흔드므로 사용하지 않음. 요구의 Possession off/on은 `SetCinematicMode`, Diana action lock, pawn input lock, view target 전환/복귀로 구현
- 기존 `bCleanseUnlocked`, `IsCleanseUnlocked()`, `UnlockCleanse()`, `OnCleanseUnlockedChanged`를 canonical 계약으로 유지하고 중복 flag나 rename 추가 금지
- acquisition은 일반 ItemBox 완료 cache와 분리. `bAcquired` 복제 상태가 완료 source이며 시작 요청 시 local complete 처리 금지
- authored Montage와 전용 `UAnimNotifyState` 사용. shared AnimSequence에 notify를 직접 추가하지 않음
- `LS_CleanseAcquisition`은 camera-only 60fps sequence로 유지하고 Diana animation은 AnimBP의 authored Montage slot에서 독립 재생
- Sequencer와 Montage는 초 단위로 동시에 시작하고 둘 다 끝난 뒤 다음 phase로 진행. Montage frame 번호를 Sequencer frame 번호로 변환하지 않음
- tutorial 표시와 skill/UI 갱신은 cinematic 종료 직후 실행. unlock 복제로 1초 UI focus와 1.25초 camera return blend를 함께 시작해 focus가 복귀 완료 전에 끝나게 함
- post-cinematic은 Diana client의 `ShowTutorialOnce()`, tutorial-ready ack, server `UnlockCleanse()`, HUD focus, control return, prop close 순서로 진행
- 기계 animation은 Lunafilament 지급과 `bIsOpened` 변경을 포함한 base ItemBox transaction에서 분리한 presentation-only API로 실행

### 실행 계획

- [ ] `DianaOperatorPawn.cpp:445`, `DianaNodeInteractionComponent.cpp:59`, `:168`, `:451`, `:521`, `CleanseSkillAcquisitionProp.cpp:64`에 단계별 진단 log를 추가해 입력, gate, hit actor, Hugh 거리, interface 진입을 PIE에서 확인
- [ ] `ACleanseSkillAcquisitionProp`의 시작 결과와 `bAcquired`를 interaction component가 읽을 수 있게 하고 acquisition 시작 직후 `Client_ReportNodeInteractionComplete()`를 호출하지 않도록 수정
- [ ] Aim requirement, `Visibility` collision, child component hit, Hugh 기준 `750cm` 중 실제 blocker를 확정한 뒤 해당 조건만 수정하고 나머지 interaction 규칙 유지
- [x] `APragmataPlayerState::bCleanseUnlocked=false` 이름과 기존 replication/delegate 계약 유지. 별도 `bHasCleanseSkill` 추가 금지
- [x] `OverrideWith()`에서 cleanse skill `OnRep` 알림도 호출해 travel 복구 후 HUD가 누락되지 않게 보강
- [x] `UDianaCleanseComponent::CanStartCleanse()`의 기존 `IsCleanseUnlocked()` server gate를 유지하고 `UnlockCleanse()` 호출은 acquisition 완료 함수 한 곳에서만 허용
- [x] `/Game/Character/Diana/Animations/Reinforce/AS_Diana_Reinforce_Gauge_VerA` 기반 `/Game/Character/Diana/Animations/Reinforce/AM_Diana_Reinforce_Gauge_VerA` 생성
- [ ] Montage slot을 기존 `DianaSkillBody`로 설정하고 root motion이 Hugh/mounted Diana 위치를 이동시키지 않는지 기존 dynamic montage와 비교
- [x] Hugh actor에 Diana avatar presentation 전용 `UCleanseChipPresentationComponent`를 두고 Orange Chip component의 생성, 부착, visibility, cleanup instance state를 소유
- [x] 전용 NotifyState는 runtime pointer를 보관하지 않고 `MeshComp->GetOwner()`의 presentation component에 `BeginChipHold()`/`EndChipHold()`만 전달
- [x] source frame `153` 시작인 `2.55초`에 Orange Chip을 표시하고 Diana avatar mesh `R_Thumb3`에 `SnapToTarget` 부착
- [x] frame `422` 전체가 보인 뒤 frame `423` 시작인 `7.05초`에 제거. Montage Editor의 NotifyState end-exclusive 동작을 PIE로 확인
- [x] 정상 종료, montage interrupt, acquisition abort, actor EndPlay에서 presentation component cleanup을 idempotent하게 처리
- [ ] Orange Chip의 grip relative location/rotation/scale을 Montage preview와 PIE에서 조정하고 zero transform을 최종값으로 가정하지 않음
- [x] `ACleanseSkillAcquisitionProp` playback을 dynamic montage API에서 authored Montage `Montage_Play()` 경로로 전환하고 face layer는 기존 별도 slot 유지
- [x] `LS_CleanseAcquisition`은 camera cut과 transform track만 소유하고 Diana skeletal animation track은 추가하지 않음
- [x] 60fps display rate에서 playback end를 Montage 전체 길이 `9.116667초`를 덮는 frame 547까지 연장
- [ ] cinematic 시작 시 실제 local participant controller 집합을 server가 snapshot하고 1-player PIE, 양 host layout, disconnect에 맞는 ack 대상만 유지
- [x] 각 participant가 local Sequence와 Montage 모두 완료한 뒤 PlayerController reliable RPC로 server에 ack하고, snapshot의 유효 participant ack가 모일 때만 post-cinematic phase 진입
- [x] participant ack timeout, sequence stop, montage interrupt는 server abort로 수렴하고 skill을 지급하지 않음
- [x] server가 Diana owning controller의 client post-cinematic RPC를 호출하고, client 내부에서 `UTutorialBlueprintLibrary::ShowTutorialOnce(this, TEXT("CleanseAcquisition"), CleanseTutorialStep)` 실행
- [x] `DA_CleanseAcquisitionTutorial.TargetRole=Diana`와 stable `TutorialId=CleanseAcquisition` 확인
- [x] Diana client는 local tutorial 호출을 동기적으로 마친 뒤 server에 tutorial-ready ack 전송. 이미 본 tutorial이면 popup 생략 후에도 ack하여 acquisition 진행
- [x] server는 tutorial-ready ack 뒤에만 기존 `UnlockCleanse()` 호출. Diana client는 `OnCleanseUnlockedChanged` 수신 뒤 HUD focus 시작
- [x] unlock 복제로 1초 HUD focus와 1.25초 POV blend를 함께 시작하고, 양 peer camera/input 복구 보고 뒤 마지막 `ClosingProp` phase 실행
- [x] `UOverDriveGaugeWidget`이 `PlayerState::OnCleanseUnlockedChanged`를 직접 bind/unbind하고 늦은 PlayerState 복제 때 binding을 재시도
- [x] `PurificationPanel`을 미획득 `Collapsed`/획득 `HitTestInvisible`로 복구하고 획득 순간 1.35배 focus animation 재생
- [x] focus가 끝난 뒤 normal pawn POV blend와 input/action lock 복구가 완료되는 시간 구성을 검증
- [x] `AAnimatedPropInteraction`에 reward/state mutation 없는 presentation-only animation trigger를 추가하고 camera/control 복귀 뒤 `sm72_045_..._Down_`을 1회 재생
- [x] `Down_` animation의 runtime close 재생 확인
- [x] 신규 Montage, Orange Chip, Notify가 soft reference이면 `PAL_CoreRuntime` explicit asset에 추가하고 saved label readback으로 cook coverage 확인
- [x] `Docs/Intent/Hacking/CleanseUnlockGate.md`, `Docs/Intent/HUD/OverDriveGaugeWidget-PurificationCooldownProgress.md`, `Docs/KNOW_HOW.md`의 acquisition 순서와 ownership 설명 갱신

### 예상 변경 파일/asset

- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.h/.cpp`
- `Source/Pragmata_2P/Props/CleanseSkillAcquisitionProp.h/.cpp`
- `Source/Pragmata_2P/Props/AnimatedPropInteraction.h/.cpp`
- `Source/Pragmata_2P/Game/PragmataPlayerState.h/.cpp`
- `Source/Pragmata_2P/Components/DianaCleanseComponent.cpp`
- `Source/Pragmata_2P/Animation/DianaAnimInstance.h/.cpp`
- `Source/Pragmata_2P/Animation/AnimNotifyState_CleanseChipAttachment.h/.cpp`
- `Source/Pragmata_2P/Components/CleanseChipPresentationComponent.h/.cpp`
- `Source/Pragmata_2P/UI/OverDriveGaugeWidget.h/.cpp`
- `Content/Map/Interaction/BP_DeadFilament_Box.uasset`
- `Content/Map/Sequences/LS_CleanseAcquisition.uasset`
- `Content/Character/Diana/Animations/Reinforce/AM_Diana_Reinforce_Gauge_VerA.uasset`
- `Content/UI/Widget/Player/WBP_OverDriveGauge.uasset`
- `/Game/CookLabels/PAL_CoreRuntime`

### 완료 조건

- [ ] Diana가 지정 interaction 조건에서 1회 입력하면 local/server log가 같은 target과 성공 gate를 기록하고 cinematic이 시작
- [ ] 실패 gate는 이유가 log에 남고, 조건 수정 뒤 같은 actor에 즉시 재시도 가능
- [ ] Hugh interaction, non-Diana role, 중복 interaction으로 acquisition 시작 불가
- [x] acquisition 전 `bCleanseUnlocked=false`, quick/precision cleanse 모두 server에서 거부
- [x] 양 peer에서 같은 authored Montage와 sequence 재생
- [x] 60fps source frame 152까지 chip 미표시, frame 153부터 `R_Thumb3`에 부착, frame 422 전체 표시, frame 423부터 제거
- [x] chip hold 동안 손 pose와 함께 이동하고 root bone/mesh가 world origin 또는 Hugh root로 튀지 않음
- [x] sequence, montage, chip 구간 종료 전 tutorial, unlock, HUD focus, 기계 닫힘 animation 미실행
- [x] cinematic 종료 뒤 Diana tutorial 표시, `bCleanseUnlocked=true`, 정화 panel 표시, focus animation 순서 확인
- [x] focus 완료 뒤 normal camera와 gameplay input 복귀, 마지막에 기계 close animation 1회 재생
- [ ] acquisition으로 Lunafilament가 증가하지 않고 `bIsOpened` semantic이 닫힌 최종 visual과 충돌하지 않음
- [x] 중단/실패/EndPlay에서 skill 미지급, chip 제거, camera/input/action lock 복구
- [ ] checkpoint respawn과 seamless travel 뒤 `bCleanseUnlocked=true` 및 HUD 표시 유지
- [ ] Hugh host/Diana client와 Diana host/Hugh client 모두 동일 결과
- [ ] packaged build에서 LS, Montage, body/face animation, tutorial, Orange Chip load 성공

### 위험과 대응

| 위험 | 대응 |
| --- | --- |
| literal `UnPossess`가 RPC와 pawn lifecycle을 끊음 | possession 유지 + 명시적 cinematic/action/input lock 사용 |
| LS frame과 animation frame 혼동 | LS는 camera 30fps, Montage는 source 60fps로 독립 유지하고 chip hold만 Montage의 `[153/60, 423/60)`로 검증 |
| Montage root motion이 mounted Diana/Hugh를 이동 | root motion mode와 mesh transform을 양 peer에서 비교 검증 |
| NotifyState가 runtime component를 잘못 소유 | Hugh의 presentation component가 instance state를 소유하고 NotifyState는 begin/end 전달만 수행 |
| tutorial과 HUD focus 체감 순서 역전 | Diana client에서 `ShowTutorialOnce()` 완료 후 ack, server unlock, `OnCleanseUnlockedChanged` focus 순서 적용 |
| ItemBox base 호출로 Lunafilament 지급 | close presentation API를 reward transaction과 분리 |
| HUD unlock 회귀 재발 | Widget이 PlayerState delegate를 직접 구독하고 PlayerState가 늦게 준비되면 binding만 재시도 |
| client 한쪽만 sequence 실패 | peer failure RPC와 server abort replication 유지, timeout 추가 |

### 검증 순서

- [ ] PIE 1인에서 interaction gate log로 현재 무반응 원인 확정
- [ ] Montage Editor에서 60fps frame 153 시작, frame 422 전체 표시, frame 423 제거와 `R_Thumb3` attachment preview 확인
- [x] PIE 2인 listen server에서 정상 획득 전체 순서와 양 peer 상태 비교
- [ ] sequence stop, montage interrupt, actor destroy, client disconnect를 각각 강제해 cleanup 확인
- [ ] `bCleanseUnlocked` replication, respawn, seamless travel 자동/수동 테스트
- [ ] `TutorialId=CleanseAcquisition` 최초 획득은 Diana에만 표시되고 이미 본 상태에서는 popup 없이 다음 phase 진행
- [x] `WBP_OverDriveGauge` unlock 전후 visibility와 focus animation runtime 상태 확인
- [ ] 기계 animation 뒤 Lunafilament와 opened state가 변하지 않는지 확인
- [x] `PAL_CoreRuntime` readback과 asset dependency 확인
- [ ] packaged build smoke test

## 기능 commit: OverDrive player-owned cinematic, input lock, ring, 범위 hack

### 예상 commit

`feat: Diana OverDrive 연출과 범위 해킹 추가`

### 구현

- [x] 현재 enemy hack duration 작업 commit을 먼저 완료해 `AMonsterBase` timer API 기준선 확정
- [x] `UOverDriveComponent`에 server-authoritative activate/consume/cast state 추가
- [x] `ADianaOperatorPawn`의 keyboard `X`, gamepad 양 thumbstick press(0.25초 window)와 request 경로 연결
- [x] server가 Diana role, gauge 100%, action lock, active hack UI, duplicate cast 검증
- [x] 승인 시 gauge를 정확히 한 번 0으로 소비
- [x] Diana owning client만 local face camera와 `AS_Diana_Grapple_0010_HacK_SP` body, `AS_Diana_Face_Grapple_SPHack` face를 재생
- [x] ring과 gameplay hack cast는 player-owned cinematic 종료 timer에서만 1회 실행
- [x] cinematic 시작 전에 GameState Diana action lock 적용
- [x] `SetIgnoreLookInput(true)`와 Diana action intent gate 적용
- [x] cinematic finish/abort/timeout 공통 cleanup에서 look/action lock 해제
- [x] server cinematic timer가 gameplay effect 시점 결정
- [x] player-owned presentation은 server authority와 분리
- [x] effect 시점 `APragmataGameState::GetHughCharacter()` 위치 snapshot
- [x] 5000cm 안의 `AMonsterBase` 수집
- [x] damage/reward와 분리된 `ApplyTimedHackState(20.f)` 추가
- [x] 신규/기존 hacked enemy timer를 effect 시점부터 20초로 시작/갱신
- [x] replicated world ring actor로 양 client presentation 전달
- [x] Hugh 위치 + positive `RingHeightOffset`에서 ring spawn
- [x] `RingHeightOffset`에 `ClampMin=1.0` validation 추가
- [x] orange emissive mesh ring radius를 `0 -> 5000cm`로 확장
- [x] floor decal projection과 slowdown effect 사용 금지
- [x] soft-loaded Diana animation과 ring material의 `PAL_CoreRuntime` explicit coverage 확인

### 주요 파일/asset

- `Source/Pragmata_2P/Components/OverDriveComponent.h`
- `Source/Pragmata_2P/Components/OverDriveComponent.cpp`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.h`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.cpp`
- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`
- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`
- `Source/Pragmata_2P/Props/OverDrivePulseRingActor.h/.cpp`
- `Content/Materials/M_OverDriveRing.uasset`
- `AS_Diana_Grapple_0010_HacK_SP`, `AS_Diana_Face_Grapple_SPHack`
- `Content/CookLabels/PAL_CoreRuntime.uasset`

### 완료 조건

- [x] gauge 100%에서 Diana만 OverDrive 발동
- [x] player-owned cinematic은 Diana 화면에서만 Diana 얼굴과 OverDrive animation 표시
- [x] cinematic 중 Diana look, movement, aim, hack, scan, cleanse, interaction 입력 모두 무효
- [x] sequence 중 Hugh gameplay control 유지
- [x] cinematic 정상 종료, 중단, animation load 실패, timeout 모두 Diana control 복구
- [x] cinematic 정상 종료 전 ring/hack 미발동, 정상 종료 직후 동기화 발동
- [x] ring은 Hugh 중심에서 시작하고 world Z가 floor와 같지 않음
- [x] ring color는 orange emissive
- [x] ring radius가 5000cm까지 확장
- [x] ring은 양 player 화면에 표시
- [x] 5000cm 이하 alive enemy만 20초 hacked
- [x] 5000cm 초과/dead enemy 제외
- [x] 기존 hacked enemy도 effect 시점부터 20초로 갱신
- [x] damage, puzzle completion, reward, gauge 재충전 없음
- [x] authority validation과 fresh Editor build 통과

## 기능 commit: Combat idle 통일

### 예상 commit

`fix: Hugh와 Diana combat idle animation 통일`

### 구현과 완료 조건

- [x] Hugh/Diana AnimInstance와 AnimBP의 combat tense state 진입 조건 조사
- [x] combat flag 기반 tense idle 분기 제거, 기존 non-tense idle state 재사용
- [x] attack, aim/hack, hit, death montage state 유지
- [x] combat 진입/종료에서 stale tense state 없이 normal idle 유지
- [x] 관련 AnimInstance, `Docs/KNOW_HOW.md` 갱신

## 기능 commit: Enemy Lunafilament reward

### 예상 commit

`feat: Enemy별 Lunafilament 처치 보상 추가`

### 구현과 완료 조건

- [x] `AMonsterBase`에 `EditDefaultsOnly`, `BlueprintReadOnly`, `ClampMin=0` reward property 추가, C++ 기본값 `50`
- [x] authoritative death 확정 경로에서 shared Hugh stats `AddLunafilament()` 1회 호출
- [x] duplicate-safe granted flag로 death/destroy callback 중복 지급 차단
- [x] enemy Blueprint별 0 이상 reward override 지원
- [x] killer, damage source, environmental damage, scripted kill과 무관하게 authoritative enemy death면 reward 지급
- [x] default 50, override 값, 0 무보상 처리 확인
- [x] client death 연출, OverDrive hack, puzzle completion만으로 지급되지 않음
- [x] 양 player HUD가 replicated total과 gain popup 수신
- [x] `MonsterBase.h/.cpp`, stats, `Docs/Intent`, `Docs/KNOW_HOW.md` 갱신

## 기능 commit: Checkpoint 활성화, refill, 10초 respawn, mirrored upgrade UI

### 예상 commit

`feat: Checkpoint team respawn과 Hugh upgrade session 추가`

### 구현

- [x] `ACheckPoint`에 replicated inactive/active 상태, stable `CheckpointId`, respawn anchor 추가
- [x] Checkpoint를 `EPragmataScanTargetKind::Checkpoint`와 Diana interaction whitelist에 추가
- [x] BP 호환을 위해 `AAnimatedPropInteraction` 상속 유지, animation parent 실행은 이번 feature에서 차단
- [x] Diana `IA_Interact`를 `Started`, `Completed`, `Canceled`에 bind
- [x] owner component에서 begin/cancel hold RPC와 local 0~1 progress 관리
- [x] checkpoint focus 진입 시 prop의 `UHoldKeyPromptWidget` keybind guide 표시, focus 이탈 시 숨김
- [x] inactive 상태에서 hold elapsed를 `0..1`로 normalize해 매 frame `SetHoldProgress()` 호출
- [x] release/cancel/focus 이탈/거리 이탈 시 `ResetHoldProgress()`와 holding state 해제
- [x] active 상태에서는 progress update 없이 일반 click guide로 전환
- [x] server가 Diana role, target, 거리, LOS, action lock을 3초 동안 재검증
- [x] release, focus/거리 이탈, death, combat 전환에서 hold cancel
- [x] 3초 완료 시에만 inactive checkpoint 활성화
- [x] latest active checkpoint transform을 server `APragmataGameMode`에 저장
- [x] common server `ReplenishTeamAtCheckpoint()` 추가
- [x] checkpoint activation 완료와 active checkpoint 재상호작용에서 replenish 호출
- [x] `UPragmataHealthComponent::RestoreFullHealth()` 또는 respawn reset API 추가
- [x] `AWeaponBase::RefillAmmoToMax()`를 server-only 상태 변경과 ammo delegate broadcast로 구현
- [x] Hugh `WeaponInventory`의 모든 valid acquired weapon에 refill 호출
- [x] `PlayerStatsComponent->SetRepairCartridge(GetMaxRepairCartridge())` 호출
- [x] active checkpoint 재상호작용 시 server upgrade session 시작
- [x] Hugh controller에 interactive widget, Diana controller에 read-only mirror widget 생성
- [x] Hugh selected type, open/close, hold/prohibited state를 Diana mirror용 session view state로 전달
- [x] Hugh의 F/C circular hold progress를 Diana mirror에도 실시간 전달
- [x] 두 화면 모두 shared Hugh stats와 `OnUpgradeMenuChanged` 결과에 bind
- [x] server만 upgrade/refund request 최종 검증
- [x] Hugh만 F/C hold, selection, upgrade/refund, close 입력 가능
- [x] Diana widget은 focus/input adapter 없이 모든 조작 무시
- [x] 양 controller gameplay lock, Hugh controller만 UI input mode 적용
- [x] session open 동안 server damage gate로 Hugh/team damage 무효화
- [x] Hugh가 close button, `Escape`, gamepad cancel을 선택하면 server session 종료
- [x] session 종료 시 양 widget 제거, cursor/input mode/control 복구
- [x] disconnect, travel, checkpoint invalidation에서 session cleanup과 invincibility 해제
- [x] authoritative `OnDeath`에서 duplicate-safe 10.0초 one-shot respawn timer 시작
- [x] 10초 동안 기존 pawn과 death animation 상태 유지
- [x] timer 만료 전 checkpoint teleport/reset 금지
- [x] timer 만료 시 existing Hugh/Diana pawn을 active checkpoint로 이동
- [x] `ResetForRespawn()`에서 HP/death flag, movement tick/mode, montage, action state, Diana mode/camera/socket 복구
- [x] respawn 직후 common replenish 호출
- [x] no active checkpoint이면 기존 `PlayerStart` fallback 위치에서 같은 reset/refill 적용
- [x] upgrade WBP compile 확인 후 `/Game/UI/Widget/Upgrade` `DirectoriesToNeverCook` 제외 해제
- [x] upgrade menu와 row widget cook chain을 `PAL_CoreRuntime` explicit coverage로 확인

### 주요 파일/asset

- `Source/Pragmata_2P/Props/CheckPoint.h`
- `Source/Pragmata_2P/Props/CheckPoint.cpp`
- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.h`
- `Source/Pragmata_2P/Components/DianaNodeInteractionComponent.cpp`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.h`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.cpp`
- `Source/Pragmata_2P/Game/PragmataGameMode.h`
- `Source/Pragmata_2P/Game/PragmataGameMode.cpp`
- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`
- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`
- `Source/Pragmata_2P/Components/PragmataHealthComponent.h`
- `Source/Pragmata_2P/Components/PragmataHealthComponent.cpp`
- `Source/Pragmata_2P/Components/PragmataPlayerStatsComponent.h`
- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`
- `Source/Pragmata_2P/Weapon/WeaponBase.h`
- `Source/Pragmata_2P/Weapon/WeaponBase.cpp`
- `Source/Pragmata_2P/UI/PragmataUpgradeMenuWidget.h`
- `Source/Pragmata_2P/UI/PragmataUpgradeMenuWidget.cpp`
- `Content/Map/Interaction/BP_CheckPoint.uasset`
- `Content/UI/Widget/Upgrade/WBP_UpgradeMenuWidget.uasset`
- `Content/UI/Widget/Upgrade/WBP_UpgradeItemRow.uasset`
- `Config/DefaultGame.ini`

### 완료 조건

- [x] Hugh는 inactive checkpoint를 활성화할 수 없음
- [x] Diana가 checkpoint surface를 정확히 조준할 때만 keybind UI 표시
- [x] inactive checkpoint hold 0~3초 동안 circular progress가 0~1로 연속 증가
- [x] hold 중단 시 즉시 0으로 복귀하며 재조준만으로 이전 progress가 남지 않음
- [x] active checkpoint prompt는 progress ring 없이 click guide로 표시
- [x] Diana 3초 미만 release/focus 이탈/거리 이탈 시 활성화 안 됨
- [x] Diana 유효 3초 hold로 한 번만 활성화
- [x] checkpoint 상호작용은 player 위치를 변경하지 않음
- [x] activation과 active interaction 모두 HP/ammo/heal cartridge 최대치 보충
- [x] active interaction 시 양 player upgrade UI 표시
- [x] 두 화면의 selected row, level, cost, button availability가 동일
- [x] Hugh selection과 upgrade/refund 결과가 Diana mirror에 반영
- [x] Hugh가 F/C를 hold하는 동안 Diana 화면의 같은 circular progress가 동기화되어 표시
- [x] Diana keyboard/gamepad/mouse 입력으로 menu state가 변하지 않음
- [x] UI 중 양 player gameplay/camera input 차단 및 server damage 무효화
- [x] Hugh가 UI를 종료하면 양 UI, input lock, invincibility 종료
- [x] abnormal cleanup 뒤에도 damage와 control 정상 복구
- [x] HP 0부터 10초 전에 respawn/teleport하지 않음
- [x] death animation이 10초 동안 중단되지 않음
- [x] 10초 만료 시 latest checkpoint respawn anchor에서 team respawn
- [x] respawn 시 HP/ammo/heal cartridge 최대치
- [x] Diana socket이 normal `DianaMountSocket_root`로 복구
- [x] no checkpoint death는 10초 후 `PlayerStart` fallback
- [x] Lunafilament, upgrade level, OverDrive, cleanse unlock은 refill/reset 대상이 아니며 유지
- [x] duplicate death event가 respawn timer를 중복 생성하지 않음
- [x] upgrade widget PAL coverage와 Editor build 확인
- [x] shared replicated session/validation path를 양 role host에 적용

## 공통 검증 gate

- [x] 기능별 Editor target build 통과
- [x] server authority/stat timer focused automation suite 통과
- [x] Blueprint/Widget/Level Sequence/VFX asset compile, save, reopen 확인
- [x] 새 runtime asset `PAL_CoreRuntime` explicit coverage와 saved label read-back
- [x] GameFlow gameplay map과 packaged `MapsToCook`를 `/Game/Map/LunarMap_LS`로 전환
- [x] `PAL_CoreRuntime`에서 새 GameFlow/map 연관 soft-load asset coverage를 saved label read-back으로 확인
- [x] `LunarMap_T` 대비 Hojin 세션 map/companion asset 변경을 `Docs/Operations/lunarmap-ls-sync-policy.md`에 재적용 순서와 함께 기록
- [x] shared replicated authority path를 Hugh/Diana host role에 적용
- [x] 변경한 non-trivial 함수 Korean intent comment coverage 확인
- [x] `Docs/KNOW_HOW.md` 기존 관련 recipe를 현재 truth로 갱신
- [x] reusable/network-sensitive symbol `Docs/Intent/**/*.md` 카드 생성/갱신
- [x] `git diff --check`와 의도하지 않은 asset/text 변경 없음 확인
- [x] 기능별 관련 파일과 문서만 stage/commit
- [x] commit message `Co-Authored-By` 없음 확인

## 주요 위험과 대응

| 위험 | 대응 |
| --- | --- |
| client 3초 hold 조작 | server begin/cancel timer와 role/거리/LOS/target 재검증 |
| death event 중복으로 여러 respawn timer 생성 | authoritative one-shot guard와 timer active 검사 |
| 10초 전에 movement/reset이 복구되어 death animation 중단 | death state 유지, reset은 timer callback 한 곳에서만 실행 |
| `SetCurAmmo()`만 호출해 HUD가 갱신되지 않음 | server-only `RefillAmmoToMax()`와 ammo delegate broadcast 추가 |
| 양 upgrade UI 동시 request race | server stat component가 cost/level을 최종 검증하고 request 직렬 처리 |
| UI close 실패로 양 player input 고정 | close/abort/disconnect/death/travel 공통 session cleanup |
| Blueprint별 scan WidgetComponent 누락 또는 잘못된 class | scan 대상 asset checklist로 `Space`, `WidgetClass`, 위치, draw size, component tag 검증 |
| `FindComponentByClass<UWidgetComponent>()`가 다른 UI를 선택 | scan component tag 또는 전용 reference로 `UScanIconWidget` instance를 명시적으로 resolve |
| `Root` optional binding 실패로 `SetScanIconVisible(true)`가 no-op | `WBP_ScanIconWidget` tree 이름 확인 및 PIE에서 visible/collapsed 왕복 테스트 |
| scan timer가 destroyed actor를 호출 | weak actor reference와 timer cleanup 사용 |
| cleanse vicinity를 prop Tick으로 계산해 중복 비용 증가 | local player-side proximity manager 사용 |
| tutorial asset/package 누락 | `UTutorialStepData`와 popup WBP cook coverage 검증 |
| Level Sequence만 믿어 Diana action 입력이 통과 | controller/pawn explicit cinematic lock을 모든 action entry에서 검사 |
| sequence 실패 후 control lock 잔류 | finish/abort/load failure/timeout 공통 idempotent cleanup |
| OverDrive ring이 floor decal처럼 보임 | mesh/Niagara ring, validated positive Z offset, 양 client visual test |
| 일반 hack repeat timer 정책 훼손 | OverDrive 전용 timed hack API만 timer refresh 허용 |

## 범위 제외

- Checkpoint animation/camera cut, disk save, map 간 checkpoint travel
- Diana death animation 자체 재설계
- scan HUD marker 유지, POI Blueprint별 실제 widget art 제작
- cleanse effect/cooldown/target 판정 변경
- OverDrive slowdown, Hugh cutscene, puzzle 자동 완료, hack damage/reward 지급
