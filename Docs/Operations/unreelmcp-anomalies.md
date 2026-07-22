# UnreelMCP 이상 사례

## PhysicsAsset hair body 생성 operation 부재

### 증상

- `PA_Diana_Hair`에서 특정 hair bone에 Physics Body를 자동 생성하려 했으나 catalog에 대응 operation 없음
- `unreal_tool_search`로 `physics asset body constraint create skeletal body setup generate bodies hair bone` 검색 시 다음 수준만 확인
  - `mesh.skeletal_create_physics_asset`: SkeletalMesh 기준 새 PhysicsAsset 생성/할당
  - `asset.create`, `asset.save`, `reflection.asset_info`, `asset.set_property`
- 기존 PhysicsAsset 안의 특정 bone에 `Add/Replace Body`, shape 추가, body mode, mass, damping, collision, constraint limit을 설정하는 전용 operation 없음

### 확인 근거

- `reflection.asset_info` on `/Game/Character/Diana/PA_Diana_Hair`
  - class: `/Script/Engine.PhysicsAsset`
  - properties: `SkeletalBodySetups`, `ConstraintSetup`, `BoundsBodies`, `BodySetup` 등 표시
- `reflection.class_info` on `/Script/Engine.PhysicsAsset`
  - functions: `GetConstraints`, `GetConstraintByName`, `GetConstraintByBoneNames`
  - body 추가/삭제/shape 생성 함수 없음
- `reflection.class_info` on `/Script/SkeletalMeshEditor.SkeletalMeshEditorSubsystem`
  - functions: `CreatePhysicsAsset`, `AssignPhysicsAsset`
  - 기존 PhysicsAsset 내부 body 편집 함수 없음
- current Editor session에서 Python은 `enabled=false`, `requires_editor_relaunch=true`
- 이전 Python enabled session에서도 `PhysicsAsset` body/constraint 배열은 public Python property 접근이 막히거나 안전한 append/edit API가 확인되지 않음

### 영향

- hair/tail/cloth proxy chain처럼 일부 bone에만 Physics Body를 추가하는 작업 자동화 불가
- `asset.set_property`로 `SkeletalBodySetups` 또는 `ConstraintSetup` 배열 전체를 import text로 덮는 방식은 binary asset 손상 위험 큼
- 사용자가 Physics Asset Editor에서 이미 만든 body가 있는 dirty asset은 자동 relaunch/save 과정에서 손상 가능
- `PA_Diana_Hair` 같은 전용 PhysicsAsset은 body 생성과 constraint tuning을 Editor 수동 작업으로 남겨야 함

### 회피

- 자동화 전 `reflection.asset_info`와 binary 문자열 검색으로 현재 body coverage 확인
- body 생성은 Physics Asset Editor에서 수동 처리
  - `Skeleton Tree` → `Options` → `Show Bones`, `Show Primitives`
  - hair bone 우클릭 → `Add/Replace Body`
  - 또는 hair bones만 선택 후 `Tools` tab의 selected-bone generate 사용
- `Generate All Bodies`는 전체 skeleton body를 만들 수 있으므로 hair chain만 선택한 상태에서 사용
- `SM_Diana` 기본 PhysicsAsset slot을 바꾸지 않고 `ABP_Diana` `RigidBody.Override Physics Asset`만 사용
- 상세 수동 절차는 `Docs/Operations/diana-hair-physics-manual.html`에 기록

### 보류

- UnreelMCP operation 추가 필요
  - `physics_asset.inspect_bodies`
  - `physics_asset.add_body_for_bone`
  - `physics_asset.add_shape`
  - `physics_asset.set_body_property`
  - `physics_asset.set_collision_pair`
  - `physics_asset.add_or_update_constraint`
  - `physics_asset.save`
- operation은 기존 body/constraint를 덮어쓰지 않도록 dry-run과 before/after inspect 제공 필요
- Python fallback을 쓰더라도 dirty asset relaunch 전 저장/백업/사용자 확인 절차 필요

## ABP_Diana state machine resolver 오탐

### 증상

- `ABP_Diana` `AnimGraph` 안의 state machine node title: `Main State Machine`
- 같은 owner node 아래 loaded graph 2개 존재
  - `New State Machine`
  - `Main State Machine`
- `animation.state_machine.inspect`에 `Main State Machine` 또는 node id `4CE03A61-4AC0-F5F7-DF48-A28AC13EE7EB` 전달 시 `New State Machine`으로 resolve
- `New State Machine`에는 hacking 테스트 state 5개만 표시
- 실제 `Main State Machine`에는 `Locomotion`, `Combat`, `Jump_*`, `Hover_*`, `CombatAim` 등 기존 Diana state 존재

### 확인 근거

- `animation.anim_graph.inspect`
  - state machine node count: `1`
  - node title: `Main State Machine`
  - node id: `4CE03A61-4AC0-F5F7-DF48-A28AC13EE7EB`
- Python `ObjectIterator()`
  - `/Game/Blueprints/Player/ABP_Diana.ABP_Diana:AnimGraph.AnimGraphNode_StateMachine_0.New State Machine`
  - `/Game/Blueprints/Player/ABP_Diana.ABP_Diana:AnimGraph.AnimGraphNode_StateMachine_0.Main State Machine`
- Python `BlueprintEditorLibrary.find_graph(asset, "Main State Machine")`
  - `None`
- Python `BlueprintEditorLibrary.find_graph(asset, "New State Machine")`
  - `AnimationStateMachineGraph` 반환

### 영향

- UnreelMCP high-level state-machine operations가 잘못된 graph 수정 가능
- `state_machine.add_alias`, `state_machine.connect`, `state_machine.set_node_property` 사용 시 `New State Machine`에 적용될 위험
- Editor에서 사용자가 보는 `Main State Machine`과 MCP operation target 불일치

### 회피

- `ABP_Diana`의 실제 gameplay state machine 작업은 Editor 수동 편집 우선
- UnreelMCP 사용 시 `animation.state_machine.inspect` 결과만 신뢰 금지
- 먼저 `ObjectIterator()`로 full object path 확인
- `New State Machine` 관련 수정 발생 시 즉시 compile/save 전에 영향 범위 재검증

### 보류

- UnreelMCP resolver 수정 필요
- 후보 수정:
  - 같은 owner node 아래 state machine graph 중 title/name 일치 우선
  - full object path 직접 resolve 지원
  - `AnimGraphNode_StateMachine` node id 입력 시 active/bound graph를 정확히 선택
