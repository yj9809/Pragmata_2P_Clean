# 구조 모호성 변경 이유

## 목적

- 대상: `b3b43099` P1 GripGun 권한 경계 fix부터 `bc1e58e5`까지 구조 모호성 정리 커밋
- 성격: `Docs/KNOW_HOW.md`의 현재 동작 레시피가 아니라, 각 커밋이 왜 필요했는지 남기는 rationale 문서
- 기준: 소유 주체, 서버 권한, Blueprint 노출 경계, listen server/remote client 표시 차이, UI와 gameplay 책임 분리

## `b3b43099` fix: GripGun 발사 권한 경계 보강

### 변경 이유

- GripGun 발사는 ammo 차감, hit 판정, impact VFX, reload 상태가 함께 움직이는 gameplay mutation이므로 서버에서만 확정되어야 함.
- 기존 구조는 Hugh 입력, `WeaponBase` trigger gate, `GripGun` fire path, reload 완료 API가 서로 다른 수준에서 발사 가능 여부를 판단해 “최종 권한 위치”가 흐렸음.
- P1로 분류한 이유는 client/host 간 발사 결과가 갈라질 수 있는 권한 경계 문제였기 때문.

### 남겨두면 생기는 문제

- client에서 local 발사처럼 보이지만 서버 ammo나 hit 결과가 일치하지 않는 split-brain이 생길 수 있음.
- reload montage callback과 ammo replenish가 여러 public helper를 통해 우회되면, 장전 완료 타이밍이 weapon별 서버 규칙을 벗어날 수 있음.
- impact VFX가 gameplay 확정과 분리되지 않으면 cosmetic multicast와 실제 hit 결과의 원인을 추적하기 어려움.

### 선택한 경계

- 발사 확정은 `GripGun::CommitServerShot`의 서버 경로로 모음.
- reload 완료는 단일 `OnCompleteReload` API로 정리.
- ammo 보충과 timer helper는 weapon 내부의 서버 helper로 축소.

## `4aada14a` docs: GripGun 권한 경계 변경 문서 추가

### 변경 이유

- P1 fix는 여러 파일의 권한 경계를 바꿨기 때문에 commit diff만으로는 “왜 이 경계가 맞는가”를 나중에 복원하기 어려움.
- `KNOW_HOW.md`와 `architecture.md`는 현재 규칙을 남기기에 적합하지만, 변경 전후와 검증 결과까지 담는 changelist 성격에는 별도 문서가 더 적합함.
- 사용자가 P1 fix를 먼저 해결하고 문서화한 뒤 ship하라고 요청했으므로, 코드 변경과 review용 변경 설명을 분리해 남길 필요가 있었음.

### 남겨두면 생기는 문제

- GripGun 권한 변경을 다시 검토할 때 어떤 public API를 왜 닫았는지 commit history를 직접 파야 함.
- P1 fix가 이후 P2 weapon 정리의 기준점이었다는 맥락이 사라짐.

### 선택한 경계

- `Docs/Create/20260703-190759-gripgun-authority-boundary.md`에 변경 전후, 대상 파일, 검증을 기록.
- 장기 규칙은 `KNOW_HOW.md`/`architecture.md`, 변경 설명은 `Docs/Create/` 문서로 분리.

## `78e73502` fix: Monster AI Blackboard 소유 경계 정리

### 변경 이유

- Behavior Tree가 사용하는 Blackboard schema는 BT asset의 계약인데, controller가 별도 hard-load하면 schema 소유자가 둘처럼 보임.
- AIController가 어떤 Blackboard를 쓸지 직접 알고 있으면 BT asset 교체나 테스트 BT 적용 시 controller 코드가 불필요하게 따라 바뀜.
- project 구조상 AI 의사결정 data contract는 BT/Blackboard asset 쪽에 두고 controller는 실행자 역할을 맡는 편이 명확함.

### 남겨두면 생기는 문제

- controller hard-load Blackboard와 BT 선언 Blackboard가 달라질 경우 key lookup이 실패하거나 잘못된 schema를 기준으로 동작할 수 있음.
- asset 경로 변경이 C++ controller 수정 요구로 번짐.

### 선택한 경계

- controller의 Blackboard hard-load 제거.
- BT가 선언한 Blackboard를 실행 시 schema 원천으로 사용.

## `a931e9ce` fix: Monster BT target 타입 계약 정렬

### 변경 이유

- Monster BT key의 target은 attack range decorator와 attack position service가 함께 읽는 shared contract임.
- 한쪽에서 pawn/character처럼 좁게 해석하고 다른 쪽에서 actor처럼 해석하면 blackboard schema 의미가 흐려짐.
- 공격 대상은 플레이어 pawn뿐 아니라 향후 다른 actor target으로 확장될 수 있으므로 `AActor` 계약이 더 안전함.

### 남겨두면 생기는 문제

- key 값은 들어 있는데 특정 node에서 cast 실패로 공격 거리나 위치 갱신이 멈출 수 있음.
- BT asset의 key type과 C++ node의 기대 type이 어긋나 디버깅이 어려워짐.

### 선택한 경계

- BT decorator/service 모두 Blackboard target을 `AActor` 계약으로 조회.
- target 세부 타입 판단은 필요한 behavior node 내부에서만 제한적으로 수행.

## `111fffa1` fix: damage library 서버 권한 gate 보강

### 변경 이유

- `PragmataDamageLibrary`는 damage 적용의 공통 진입점이므로 caller가 항상 서버라는 전제에 의존하면 안 됨.
- damage는 health 변경, hit reaction, enemy state 전환 같은 gameplay mutation을 만들기 때문에 최종 방어선이 필요함.
- 여러 subsystem이 같은 damage helper를 호출하므로 gate를 caller마다 복제하는 것보다 library 입구에서 막는 편이 명확함.

### 남겨두면 생기는 문제

- client-side 호출이 local damage state를 만들거나 서버 결과와 다른 hit feedback을 만들 수 있음.
- 새 caller가 추가될 때마다 authority check 누락 위험이 반복됨.

### 선택한 경계

- 공통 damage 진입점이 target authority를 확인.
- damage 적용은 서버 actor authority가 있을 때만 진행.

## `6b6977d8` fix: damage faction source fallback 정리

### 변경 이유

- faction 판정의 source가 `SourceActor`인지 `DamageCauser`인지 불명확하면 friendly fire, enemy damage, trap/projectile damage의 의미가 흔들림.
- `SourceActor`는 의도를 가진 원천이고 `DamageCauser`는 실제 피해를 전달한 actor일 수 있으므로 우선순위가 필요함.
- source가 비어 있는 일부 damage path에서도 최소한의 fallback 규칙이 있어야 기존 damage flow가 끊기지 않음.

### 남겨두면 생기는 문제

- 같은 damage가 caller마다 다른 faction으로 판정되어 아군/적군 필터가 흔들릴 수 있음.
- SourceActor 누락 상황에서 damage가 무조건 실패하거나, 반대로 causer 기준으로 잘못 통과할 수 있음.

### 선택한 경계

- faction source는 `SourceActor` 우선.
- `SourceActor`가 없을 때만 `DamageCauser` fallback.

## `98ffefe3` fix: WeaponBase 발사 gate 서버 계약 보강

### 변경 이유

- `WeaponBase`의 공통 발사 gate가 authority를 보지 않으면 derived weapon이 서버 전용 발사 계약을 매번 자체 구현해야 함.
- P1 GripGun fix 이후에도 base gate가 client 발사 가능처럼 보이면 weapon 계층의 권한 모델이 다시 흐려짐.
- base class는 “발사 가능한가”의 공통 전제 조건을 표현해야 하므로 authority 확인이 포함되어야 함.

### 남겨두면 생기는 문제

- GripGun 외 다른 weapon이 base gate만 믿고 client에서 fire mutation을 진행할 수 있음.
- server-only helper를 추가해도 public trigger 경로가 권한을 우회하는 모양으로 남음.

### 선택한 경계

- `WeaponBase::PullTrigger` 계열 공통 gate에 authority 확인 포함.
- weapon별 특수 발사 처리는 서버 gate를 통과한 뒤에만 진행.

## `199eed0c` refactor: Weapon ammo 직접 변경 API 범위 축소

### 변경 이유

- public `RefillAmmo` 같은 직접 ammo 변경 API는 weapon별 reload 규칙을 우회할 수 있음.
- GripGun은 reload montage, timer, server helper를 통해 ammo를 보충해야 하므로 외부에서 임의 보충하는 API가 남아 있으면 경계가 깨짐.
- 미사용 public API는 현재 사용되지 않아도 Blueprint나 새 C++ caller가 잘못된 우회로로 채택하기 쉬움.

### 남겨두면 생기는 문제

- reload animation이 끝나기 전에 ammo가 채워지거나, 서버 guard 없이 ammo가 바뀌는 경로가 생길 수 있음.
- weapon별 reload 정책을 이해하지 못한 caller가 base API를 직접 호출할 수 있음.

### 선택한 경계

- 미사용 public `RefillAmmo` 경로 제거.
- ammo replenish는 weapon별 서버 reload helper가 소유.

## `d3d10084` fix: Hugh AnimInstance 표시 속도 원본 정리

### 변경 이유

- `HughAnimInstance`가 원시 `MovementComponent` 속도와 가속도를 직접 읽으면 owner와 remote client가 서로 다른 애니메이션 입력값을 사용할 수 있음.
- 프로젝트는 `HughAnimState`를 통해 애니메이션 표시용 이동 상태를 별도 관리하므로, AnimInstance도 같은 snapshot을 읽어야 함.
- listen server에서는 서버 물리 상태, owner 예측 상태, remote 복제 상태가 한 화면 안에 섞이므로 “어느 값을 animation truth로 볼지”가 명확해야 함.

### 남겨두면 생기는 문제

- Hugh 본인은 자연스럽게 보이지만 다른 플레이어 화면에서는 blendspace 입력이 다르게 보여 locomotion이 어긋날 수 있음.
- 나중에 `HughAnimState`를 보강해도 AnimInstance가 원시 값을 계속 읽으면 수정 효과가 일부 경로에만 적용됨.

### 선택한 경계

- `HughAnimState`가 표시용 이동 상태의 단일 원천.
- 정정: 이 선택은 stop/braking feel을 깨뜨려 폐기.
- 현재 경계: AnimInstance의 locomotion `MovementSpeed`/`Acceleration`은 실제 `CharacterMovement` velocity/acceleration 사용.
- `HughAnimState`는 aim/run/jump/hover 같은 상태 복제 원천으로만 사용.

## `e204a5c1` refactor: Diana AnimInstance operator pawn 조회 정리

### 변경 이유

- Diana AnimInstance가 Diana operator pawn을 찾는 경로가 여러 곳에 흩어져 있으면 GameState, owner pawn, cached pawn 중 무엇이 우선인지 불명확해짐.
- Diana는 Hugh에 mount된 표현체와 별도 `ADianaOperatorPawn`이 함께 존재하므로, AnimInstance가 “보이는 Diana”와 “입력/상태를 가진 Diana”를 혼동하기 쉬운 구조.
- GameState가 role pawn 관계를 알고 있으므로 정상 플레이에서는 GameState-first 조회가 가장 명확함.

### 남겨두면 생기는 문제

- BeginPlay, possession, replication 순서에 따라 stale pawn을 계속 참조할 수 있음.
- Anim layer나 combat state가 서로 다른 Diana pawn을 기준으로 갱신될 수 있음.

### 선택한 경계

- `ResolveDianaOperatorPawn()` / `FindDianaOperatorPawn()`로 조회 정책을 한곳에 모음.
- GameState 조회를 우선하고, 초기 PIE/복제 타이밍용 fallback만 제한적으로 허용.

## `20525e7d` delete: 미사용 HughController 제거

### 변경 이유

- 실제 플레이어 controller는 `APragmataPlayerController`인데, 남아 있던 `HughController`가 “Hugh 전용 controller가 아직 소유권을 가진다”는 잘못된 신호를 줌.
- 이 프로젝트의 역할 구조는 Hugh와 Diana가 controller class를 나누는 방식이 아니라, PlayerState role과 pawn possession으로 분기됨.
- 사용하지 않는 native controller가 남아 있으면 HUD, input, role handoff 코드를 어디에 넣어야 하는지 계속 모호해짐.

### 남겨두면 생기는 문제

- Blueprint나 config가 오래된 `HughController`를 다시 참조하면서 새 controller 경계를 우회할 수 있음.
- 이후 작업자가 Hugh-only 기능을 legacy controller에 추가해 실제 runtime 경로와 문서가 어긋날 수 있음.

### 선택한 경계

- player controller 책임은 `APragmataPlayerController` 계열로 일원화.
- Hugh pawn은 pawn 동작과 입력 action binding만 담당.

## `e6fd9475` refactor: Hugh 입력 mapping 소유 경계 정리

### 변경 이유

- `HughCharacter`에 mapping context 관련 흔적이 남아 있으면 pawn이 Enhanced Input mapping을 소유하는 것처럼 보임.
- 실제 mapping context 추가/전환은 controller와 role input flow가 맡아야 하며, pawn은 자신에게 전달된 input action을 바인딩하는 쪽이 자연스러움.
- commented legacy mapping code는 “필요하면 되살려도 되는 코드”처럼 보이지만 현재 구조와 맞지 않음.

### 남겨두면 생기는 문제

- 같은 IMC가 controller와 pawn 양쪽에서 추가되어 중복 binding, 우선순위 충돌, packaged 입력 차이를 만들 수 있음.
- Hugh 입력 버그를 고칠 때 pawn-local mapping 복구라는 잘못된 방향으로 회귀할 위험.

### 선택한 경계

- mapping context 소유는 controller/role input flow.
- Hugh pawn은 `UInputAction` 참조와 실제 action handler만 유지.

## `7306cb33` fix: Diana node interaction 권한 gate 보강

### 변경 이유

- Diana node interaction은 Diana 전용 행동인데, 공통 gate가 role, action lock, combat 상태를 모두 확인하지 않으면 서버 재검증 전까지 잘못된 intent가 진입할 수 있음.
- node interaction은 map/hack 흐름과 연결되므로 단순 거리/조준 조건만으로 허용하면 gameplay role 경계가 흐려짐.
- forced paired action 중에는 Diana 입력 의도가 잠겨야 하므로 interaction gate도 같은 정책을 따라야 함.

### 남겨두면 생기는 문제

- Hugh 또는 잘못된 role 상태에서 node interaction RPC를 시도할 수 있음.
- enemy combat, roaming, aim 상태와 관계없이 상호작용 가능 여부가 경로마다 다르게 판단됨.

### 선택한 경계

- `CanOwnerUseNodeInteraction()`가 action lock, enemy combat, Diana role, roaming, aim 조건을 함께 검사.
- interaction 시작 전 로컬/서버 공통 조건을 같은 component gate로 묶음.

## `360d1eaf` fix: Diana scan role 검증 추가

### 변경 이유

- scan은 Diana 전용 능력인데 `ResolveScanTrace()`가 거리, cone, action lock 중심으로만 검증하면 role identity가 빠짐.
- component가 올바른 owner에 붙어 있다는 가정은 runtime 보안 경계로 충분하지 않음.
- server trace는 최종 판정 지점이므로 role 확인도 이 지점에 있어야 함.

### 남겨두면 생기는 문제

- component 참조나 possession 흐름이 꼬였을 때 Diana가 아닌 player가 scan 판정을 받을 수 있음.
- scan/hack 계열 코드가 “Diana-only”라는 gameplay 계약을 코드상으로 보장하지 못함.

### 선택한 경계

- controller `PlayerState`의 assigned role이 Diana인지 trace resolution에서 확인.
- scan component는 owner 상태뿐 아니라 gameplay role까지 검증.

## `5ffd97c9` refactor: Diana node interaction gate 일원화

### 변경 이유

- pawn의 `RequestNodeInteraction()`와 component gate가 각각 조건을 들고 있으면 어느 쪽이 진짜 정책인지 불명확함.
- 이전에는 pawn 쪽 `Roaming && Aim`과 component/server 쪽 조건이 따로 움직일 수 있었음.
- node interaction은 component가 실질 동작을 수행하므로, gate도 component가 소유하는 편이 추적 가능함.

### 남겨두면 생기는 문제

- 새 조건을 component에 추가해도 pawn 쪽 중복 조건이 낡은 상태로 남아 false reject 또는 false allow를 만들 수 있음.
- caller가 pawn gate만 보고 “이 조건이면 충분하다”고 오해할 수 있음.

### 선택한 경계

- `ADianaOperatorPawn::RequestNodeInteraction()`는 component에 위임.
- node interaction 가능 여부의 단일 정책 위치는 `UDianaNodeInteractionComponent`.

## `78547e94` fix: Diana action lock 중 local hack intent 차단

### 변경 이유

- server가 action lock 중 scan/hack을 거부하더라도 local intent가 먼저 reticle, timer, RPC 시도, 방향 입력을 움직이면 플레이어 피드백이 거짓이 됨.
- forced paired action 중 Diana는 조작이 잠긴 상태이므로 UI와 로컬 preview도 같은 잠금 정책을 따라야 함.
- server-authoritative reject만으로는 UX와 네트워크 노이즈 문제를 해결하지 못함.

### 남겨두면 생기는 문제

- 플레이어는 hack이 시작된 것처럼 보지만 서버는 거부하는 상태가 생김.
- locked 상태에서 불필요한 RPC가 반복되어 실제 실패 원인을 로그에서 찾기 어려워짐.

### 선택한 경계

- `CanUseDianaActionIntent()`가 로컬 scan/hack/reticle/direction intent의 공통 입구.
- action lock 상태에서는 gameplay 요청뿐 아니라 로컬 표시 의도도 막음.

## `7d06d2a0` fix: WeaponDataAsset 기본 탄약 범위 명확화

### 변경 이유

- `WeaponDataAsset`의 기본 탄약 값은 무기 획득 직후 HUD와 gameplay 상태의 기준값임.
- DataAsset 값이 `MaxAmmo`보다 크면 소비자마다 clamp 여부가 달라질 수 있어 “기본 탄약이 얼마인가”가 모호해짐.
- asset 입력 실수는 가능한 한 데이터 제공자 경계에서 정리하는 편이 downstream 코드를 단순하게 만듦.

### 남겨두면 생기는 문제

- 획득 UI, 실제 ammo component, reload flow가 서로 다른 ammo 값을 표시할 수 있음.
- 소비자마다 방어 clamp를 추가해야 하며 일부 경로 누락 가능성이 커짐.

### 선택한 경계

- `GetDefaultCurrentAmmo()`가 기본 탄약을 `0..MaxAmmo` 범위로 정규화.
- DataAsset accessor가 gameplay-safe 기본값을 제공.

## `0c70abf8` refactor: PairedAnimationSet 매칭 정책 명시

### 변경 이유

- paired animation lookup은 cue, strength, direction 중 어느 필드가 필수인지 읽기만으로 알기 어려웠음.
- direction/strength가 빠진 row에서도 fallback으로 montage를 찾을 수 있는데, 이 정책이 암묵적이면 데이터 오류와 의도적 fallback을 구분하기 힘듦.
- designer가 DataAsset을 수정할 때 매칭 우선순서를 알아야 안전하게 row를 추가할 수 있음.

### 남겨두면 생기는 문제

- 잘못된 montage가 재생되어도 “왜 이 row가 선택됐는지” 추적이 어려움.
- fallback 정책을 모른 채 direction row를 삭제하거나 strength row를 추가해 regression을 만들 수 있음.

### 선택한 경계

- cue+strength+direction, cue+strength, cue-only 순서의 named helper로 정책을 드러냄.
- DataAsset lookup 코드는 fallback 규칙을 숨기지 않고 이름으로 설명.

## `467160f2` fix: Monster hack 상태와 damage 보정 경계 분리

### 변경 이유

- monster가 hack되기 전에는 일반 damage가 0.1배로 줄어드는 규칙과, hack 성공으로 `bHacked`가 true가 되는 상태 전환이 섞여 있었음.
- hack damage가 full로 들어가야 하는 이유가 `bHacked` 선설정의 부작용이면 규칙이 코드 순서에 의존함.
- 상태 전환과 damage 보정은 서로 다른 책임이므로 분리해야 나중에 hack 종류나 damage kind가 늘어도 안전함.

### 남겨두면 생기는 문제

- `bHacked`를 언제 세팅하는지에 따라 같은 hack damage가 0.1배 또는 full damage로 달라질 수 있음.
- hack 성공 실패, damage 처리, 상태 복제가 한 함수 순서에 과도하게 묶임.

### 선택한 경계

- `DamageKind::Hack`은 pre-hack attenuation에서 명시적으로 제외.
- `ApplySuccessfulHack()`은 authority에서 damage 적용 후 상태를 전환.

## `67b98e7d` fix: Enemy damage text 표시 경로 명확화

### 변경 이유

- damage 계산과 적용은 서버가 authoritative해야 하지만, floating damage text는 각 peer의 local visual feedback임.
- 서버에서만 text를 만들면 listen server 화면과 client 화면의 피드백이 달라짐.
- damage text는 gameplay state가 아니라 cosmetic 표시이므로 서버 계산 결과를 multicast해 각 client가 로컬로 생성하는 구조가 맞음.

### 남겨두면 생기는 문제

- host만 damage number를 보고 client는 못 보거나, 반대로 특정 경로에서 중복 표시가 생길 수 있음.
- visual feedback 부재가 실제 damage 미적용 버그처럼 보임.

### 선택한 경계

- 서버가 실제 damage와 표시 위치를 계산.
- `MulticastSpawnDamageText`로 각 peer가 local-only text를 생성하고 dedicated server는 skip.

## `f1a9e356` refactor: session 광고 제어 Blueprint 경계 축소

### 변경 이유

- session advertising toggle은 lobby/session backend 상태 제어이며, UI Blueprint가 임의 호출할 성격의 API가 아님.
- BlueprintCallable로 열려 있으면 화면 전환이나 버튼 graph에서 network session 가시성을 직접 바꿀 수 있음.
- 이 프로젝트는 session flow를 subsystem/GameMode 쪽에서 통제하므로 Blueprint 노출은 최소화해야 함.

### 남겨두면 생기는 문제

- UI 작업 중 실수로 advertising이 꺼져 join 목록에서 방이 사라질 수 있음.
- session lifecycle 원인을 C++과 Blueprint 양쪽에서 추적해야 함.

### 선택한 경계

- `SetSessionAdvertisingEnabled()`는 내부 C++ 제어 API.
- Blueprint는 session backend toggle을 직접 호출하지 않음.

## `63f8cc1d` fix: gameplay role 할당 실패 경계 분리

### 변경 이유

- room code가 있으면 모든 role assignment 실패를 defer하면 안 됨.
- defer가 필요한 것은 join 직후 role query timing 때문에 생기는 `pragmata_invalid_role` 같은 특정 초기화 지연뿐임.
- missing pawn class, invalid room, role unavailable 같은 terminal error는 즉시 실패해야 원인 파악이 가능함.

### 남겨두면 생기는 문제

- 치명적인 설정 오류가 spawn 이후까지 숨겨져 디버깅 지점이 멀어짐.
- join flow가 “나중에 해결될 수도 있는 오류”와 “즉시 중단해야 하는 오류”를 구분하지 못함.

### 선택한 경계

- `ShouldDeferRoleAssignmentFailure()`가 defer 가능한 실패만 명시.
- terminal role assignment failure는 `InitNewPlayer` 단계에서 즉시 반환.

## `eb1d8054` refactor: IPragmataHackable 계약 명확화

### 변경 이유

- interface 구현자는 hack target인데, parameter 이름이 `Hacker`이면 target actor와 요청 source의 관계가 흐려짐.
- hack 요청자는 controller, pawn, component 등 여러 계층일 수 있으므로 “누가 요청했는가”를 정확히 표현해야 함.
- interface 계약은 구현체가 읽는 첫 번째 문서이므로 이름 자체가 ownership을 설명해야 함.

### 남겨두면 생기는 문제

- target 내부에서 `Hacker`를 target actor처럼 해석하거나, authority/instigator 처리 위치를 잘못 잡을 수 있음.
- 새 hackable actor 구현 시 parameter 의미를 다시 추론해야 함.

### 선택한 경계

- parameter 이름을 `HackInstigator`로 정리.
- interface 주석은 target과 request source의 책임 차이를 설명.

## `26216854` refactor: IPragmataScannable result 책임 명확화

### 변경 이유

- scannable target은 scan 결과 metadata를 제공하는 대상이지, scan 성공 판정의 주체가 아님.
- 최종 trace, 거리, cone, role, action lock 검증은 scan component가 이미 담당함.
- `Scanner`라는 이름은 target이 validation requester를 직접 다루는 것처럼 읽혀 책임이 섞임.

### 남겨두면 생기는 문제

- scannable 구현체가 거리 판정이나 visibility 판정을 임의로 다시 수행할 수 있음.
- scan result와 scan validation의 소유자가 분산되어 결과 불일치가 생김.

### 선택한 경계

- parameter 이름을 `ScanRequester`로 정리.
- scannable은 metadata 제공, scan component는 validation/final distance 소유.

## `c15cccef` fix: Lobby preview 표시 조건 분리

### 변경 이유

- selection marker visibility와 preview actor visibility는 서로 다른 개념임.
- `UpdateSelectionMarkers()`에서 actor 전체 hidden state를 바꾸면 “선택 가능 표시”가 “캐릭터 preview 표시”를 지배하게 됨.
- lobby phase나 role selection 상태가 바뀔 때 preview mesh는 계속 보여야 하고, marker만 켜고 꺼져야 함.

### 남겨두면 생기는 문제

- 선택 marker를 숨기는 순간 preview mesh까지 사라져 role lobby가 빈 화면처럼 보일 수 있음.
- phase별 표시 버그를 marker 문제로 추적하기 어려움.

### 선택한 경계

- selection ring/marker visibility는 marker component만 제어.
- preview actor/mesh visibility는 별도 preview state가 담당.

## `04f8709d` refactor: PreviewRole 갱신 경로 정리

### 변경 이유

- BeginPlay, `ConfigureForRole()`, `OnRep_PreviewRole`가 각자 preview mesh, anim, marker를 갱신하면 replication 순서에 따라 결과가 달라짐.
- preview role은 lobby 표시의 핵심 상태이므로 적용 경로가 하나여야 함.
- server 설정과 client 복제 반영이 같은 함수를 통과해야 host/client 표시 차이를 줄일 수 있음.

### 남겨두면 생기는 문제

- 서버에서는 올바른 role mesh가 보이지만 client에서는 marker나 anim class만 이전 상태로 남을 수 있음.
- 새 preview 속성을 추가할 때 세 경로 중 하나를 빠뜨릴 가능성이 큼.

### 선택한 경계

- `RefreshPreviewForRole()`가 preview role 적용의 단일 함수.
- BeginPlay, configure, OnRep 모두 같은 refresh 경로 사용.

## `0cbe1237` fix: puzzle 완료 처리 서버 권한 가드 추가

### 변경 이유

- `Interact_Implementation`에 server guard가 있어도 `CompletePuzzle()` 자체가 열려 있으면 completion side effect의 최종 방어선이 없음.
- puzzle completion은 state 변경, delegate broadcast, power supply unlock 같은 authoritative side effect를 포함함.
- Unreal에서는 함수가 나중에 Blueprint, timer, callback에서 직접 호출될 수 있으므로 state mutation 함수 자체가 authority를 확인해야 함.

### 남겨두면 생기는 문제

- client-side call이 puzzle solved state나 power 상태를 로컬에서만 바꾸는 split-brain을 만들 수 있음.
- completion 호출 경로가 늘어날 때마다 caller guard를 다시 믿어야 함.

### 선택한 경계

- `APuzzleInteractable::CompletePuzzle()`와 `AInteractionPowerSupply::CompletePuzzle()` 내부에 `HasAuthority()` guard.
- puzzle 완료 state mutation은 서버 전용.

## `a6b1199b` fix: puzzle locked door 진행 상태 복제

### 변경 이유

- locked door의 solved count와 lock mirror가 서버에서만 갱신되면 client UI/door 표시가 현재 puzzle 상태를 알 수 없음.
- 문이 실제로 열리는 권한은 서버에 있지만, client도 progress와 open/locked 표시를 받아야 함.
- puzzle progression은 gameplay state이므로 cosmetic-only 처리로 둘 수 없음.

### 남겨두면 생기는 문제

- client는 퍼즐을 풀어도 문이 잠긴 것처럼 보거나 progress UI가 갱신되지 않을 수 있음.
- interaction 가능 여부와 visual state가 서버/client에서 다르게 보임.

### 선택한 경계

- `CurrentSolvedCount`와 lock mirror state를 replicate.
- `OnRep`에서 Blueprint progress와 door state 표시를 갱신하고, mutation은 서버 guard 유지.

## `53882224` fix: Map hack depth 범위 고정

### 변경 이유

- map hack UI/viewmodel은 3-slot depth를 전제로 구성되어 있음.
- metadata나 runtime sequence가 3을 넘으면 gameplay는 더 긴 sequence를 요구하지만 UI가 이를 표시하지 못함.
- DataAsset과 runtime generator 양쪽에서 같은 범위를 보장해야 designer 입력과 procedural 생성이 같은 계약을 따름.

### 남겨두면 생기는 문제

- 보이지 않는 4번째 이상의 hack step 때문에 플레이어가 puzzle을 해결할 수 없는 상태가 생김.
- UI 쪽에서 임시 clamp를 넣어도 gameplay sequence와 표시 sequence가 달라질 수 있음.

### 선택한 경계

- map hack depth는 `1..3`.
- metadata와 `GenerateSequence()` 모두 같은 범위로 clamp.

## `5c40324a` refactor: Hack puzzle 시작 결과 명시

### 변경 이유

- `StartHack()` 성공 여부를 상태 변화 후 추론하면 bind/start 실패의 원인을 caller가 알기 어렵음.
- enemy hack과 map hack 모두 “시작 요청이 받아들여졌는지”를 bool로 돌려주면 caller가 후속 bind/UI 처리 여부를 명확히 결정할 수 있음.
- replicated state는 관찰 결과이지 즉시 함수 호출의 성공 반환값을 대신하기 어려움.

### 남겨두면 생기는 문제

- 실패한 start 이후에도 UI나 delegate bind가 진행될 수 있음.
- StartHack 내부 조건이 바뀌면 caller의 상태 추론 로직이 같이 깨질 수 있음.

### 선택한 경계

- enemy/map `StartHack()`는 성공 여부를 `bool`로 반환.
- caller는 반환값을 기준으로 후속 bind/start flow를 진행.

## `5dac5fc8` refactor: 해킹 ViewModel gameplay 변경 API 정리

### 변경 이유

- ViewModel은 UI 표현과 binding 상태를 담당해야 하며, gameplay start/cancel을 직접 소유하면 presentation과 server gameplay가 섞임.
- hack 시작/취소는 Diana component와 server RPC 경계에 있어야 권한과 실패 처리가 명확함.
- failure animation 완료 callback이 gameplay cancel을 부르면, UI 애니메이션 종료가 gameplay state를 바꾸는 역방향 의존이 생김.

### 남겨두면 생기는 문제

- UI 상태 전환만 의도했는데 실제 hack gameplay가 취소될 수 있음.
- server authoritative hack flow를 ViewModel call path까지 추적해야 해 유지보수가 어려움.

### 선택한 경계

- legacy ViewModel Start/Stop은 deprecated wrapper로 축소.
- 정정: 기존 Blueprint asset이 legacy Start/Stop wrapper를 아직 참조하므로 wrapper의 start/cancel 동작은 호환용으로 유지.
- 새 HUD 경로는 ViewModel `BindToComponent()`/`UnbindFromComponent()`를 직접 쓰고, gameplay 시작/취소는 Diana component/server RPC 경로를 우선.

## `bc1e58e5` refactor: 로비 host join API 이름 정리

### 변경 이유

- 기존 `HostAsHugh`, `JoinAsDiana` 이름은 host가 Hugh, joiner가 Diana라는 오래된 가정을 API에 박아 둠.
- 현재 role은 lobby preselect와 PlayerState role flow가 결정하므로 host/join 동작과 role 선택은 분리되어야 함.
- 이름이 틀리면 caller가 실제 동작보다 과거 정책을 믿고 UI나 network flow를 잘못 연결할 수 있음.

### 남겨두면 생기는 문제

- host가 Diana를 고르거나 joiner가 Hugh를 고르는 흐름을 추가할 때 API 이름과 동작이 정면으로 충돌함.
- role selection bug를 session host/join 코드에서 찾는 잘못된 디버깅 경로를 만들 수 있음.

### 선택한 경계

- handler는 `HostLobbySession`, `JoinLobbySession`을 호출.
- `HostAsHugh`, `JoinAsDiana`는 obsolete role assumption을 담은 deprecated wrapper로만 유지.

## 요약

- 구조 모호성 변경의 공통 목적: “어느 계층이 이 상태를 소유하는가”를 코드 이름, gate 위치, replication 경로, Blueprint 노출 범위로 명확화.
- gameplay 권한: 서버 mutation 함수 내부에서 최종 방어.
- 표시 경로: local cosmetic은 multicast 결과로 각 peer가 생성, replicated gameplay state는 `OnRep`로 표시 갱신.
- UI/ViewModel 경계: presentation은 bind/display, gameplay start/cancel은 component/server RPC.
- role/input/session 경계: role은 PlayerState/lobby role flow, mapping은 controller input flow, pawn은 action handler.
