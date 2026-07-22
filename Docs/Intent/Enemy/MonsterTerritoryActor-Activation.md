# AMonsterTerritoryActor 활성화 게이트 의도 카드

## 대상

`AMonsterTerritoryActor::ActivateTerritoryTrigger`, `OnPlayerEnteredTerritory`, `bRequireMapGimmickActivation`, `BeginMonsterEncounter`

## 종류

Monster Territory map gimmick activation boundary

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterTerritoryActor.h`
- `Source/Pragmata_2P/Enemy/MonsterTerritoryActor.cpp`
- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 이 코드가 존재하는 이유

Territory 진입과 전투 시작을 분리하기 위해 존재.

player가 영역에 들어오면 문, 감금 장치, 엘리베이터 같은 map gimmick은 즉시 알아야 하지만, monster sequence, spawn, 배치 monster BT 시작은 map gimmick이 준비를 끝낸 뒤 시작해야 하는 경우가 있다.

## 사용해야 하는 상황

- player 진입으로 map gimmick을 먼저 작동시키고, 이후 전투를 시작해야 할 때
- 배치 monster를 처음부터 Patrol시키지 않고 외부 기믹 완료 뒤 appearance와 함께 시작해야 할 때
- `FirstEntrySequence` 이후 runtime spawn과 monster appearance를 묶어서 실행해야 할 때

## 사용하면 안 되는 상황

- 영역에 들어가자마자 배치 monster가 기존 Patrol/perception 흐름으로 움직이면 되는 일반 Territory
- player가 영역 밖에 있어도 전투 시작을 예약해야 하는 이벤트
- 반복 wave처럼 같은 trigger를 여러 번 소비해야 하는 구조

## 기본 흐름

```text
Player enters Territory
-> OnPlayerEnteredTerritory broadcast
-> bRequireMapGimmickActivation이 false면 즉시 첫 전투 flow
-> true면 ActivateTerritoryTrigger 전까지 대기
-> ActivateTerritoryTrigger
-> player가 아직 Territory 안에 있으면 TriggerFirstPlayerEntry
-> FirstEntrySequence 있으면 FirstEntrySequenceDelay 대기 후 재생
-> FirstEntrySequence 완료 후 StartFirstEntryGameplay
-> RuntimeSpawns 생성
-> FirstEntrySequenceMonsters에 들어간 배치 monster, 활성화형 배치 monster, appearance 보유 배치 monster, runtime spawned monster가 BeginMonsterEncounter 통과
-> RuntimeSpawns 또는 PlacedMonsterOverrides의 AppearanceMontageOverride, 없으면 SpawnAppearanceMontage 확인
-> appearance가 있으면 TryPlaySpawnAppearance 성공 뒤 종료 대기
-> appearance 종료 후 RunAI + Target 지정
-> montage 없으면 즉시 RunAI + Target 지정
```

## Blueprint 연결 여부

- `OnPlayerEnteredTerritory`: 여러 map gimmick이 구독하는 진입 알림
- `ActivateTerritoryTrigger`: map gimmick이 준비 완료 후 Territory reference로 직접 호출
- `bRequireMapGimmickActivation`: Territory Actor 인스턴스에서 체크
- `APoweredResponBox.TerritoriesToActivateOnOpen`: 전원 켜짐 + player overlap으로 문 개방 조건을 통과할 때 Territory trigger 호출

## 헷갈리기 쉬운 코드

- `OnPlayerEnteredTerritory`: 들어왔다는 사실만 알림
- `OnFirstPlayerEntered`: 첫 전투 시작이 확정됐다는 알림
- `ActivateTerritoryTrigger`: 외부 map gimmick 완료 후 Territory 첫 전투 flow를 여는 단일 진입점
- `BeginMonsterEncounter`: appearance 여부에 따라 monster별 AI 시작 시점을 결정
- `AMonsterBase::TryPlaySpawnAppearance`: Territory가 appearance 시작 성공 여부를 확인할 때 사용
- `FMonsterTerritorySpawnInfo.AppearanceMontageOverride`: 같은 monster BP라도 Territory spawn 항목별 등장 몽타주를 다르게 지정
- `FMonsterTerritoryPlacedMonsterInfo.AppearanceMontageOverride`: 배치 monster 인스턴스를 직접 열지 않고 Territory에서 등장 몽타주 override
- `FirstEntrySequenceMonsters`: appearance가 없어도 sequence 종료 뒤 Target 지정과 함께 Chase로 시작해야 하는 배치 monster 목록
- `FirstEntrySequenceDelay`: 첫 진입 확정 뒤 sequence 재생 전 대기 시간. 대기 중에는 spawn, 배치 monster 시작, sequence 재생이 모두 보류
- `PlacedMonsters`: 기존 맵 배치 참조 보존용 기본 배열
- `PlacedMonsterOverrides`: override가 필요한 배치 monster만 추가하는 선택 배열
- `AMonsterBase::SetAutoRunAIOnPossess`: Territory가 spawn 또는 배치 appearance 전 자동 BT 시작을 막을 때 사용
- `CreateTerritorySequencePlayer`: runtime 생성 sequence player의 종료 상태를 `ForceKeepState`로 고정해 마지막 Transform key가 재생 종료 후에도 유지되게 함

## 서버/클라이언트 조건

전투 시작, spawn, target 지정, AI 시작은 server authority에서만 처리.

Sequence와 appearance montage 표시만 multicast 또는 replicated state로 peer에 맞춘다.

## 확실한 근거

- `ActivateTerritoryTrigger`는 server에서 Territory를 활성화하고, 첫 전투가 아직이면 `TriggerBox` 현재 overlap player를 다시 찾아 시작 조건으로 사용
- `APoweredResponBox::UpdateDoorState`와 `OpenDoor`는 개방 조건을 통과한 server 시점에 연결된 Territory를 한 번만 활성화
- `TriggerFirstPlayerEntry`는 `bTerritoryTriggerActivated`가 false면 실행하지 않음
- `BeginPlay`에서 `bTerritoryTriggerActivated = !bRequireMapGimmickActivation`
- FirstEntrySequenceMonsters에 들어간 배치 monster, 활성화형 배치 monster, appearance 보유 배치 monster는 `SuspendMonsterUntilTerritoryTrigger`로 BT를 멈춤
- `StartPlacedMonstersForEncounter`는 `PlacedMonsters`와 `PlacedMonsterOverrides`를 함께 순회하고 중복 시작과 비지연 배치 monster 시작을 막음
- `BeginMonsterEncounter`는 appearance가 있으면 `OnSpawnAppearanceFinished` 이후 `StartMonsterAIWithCurrentTarget` 호출
- 비하강 appearance는 종료 callback이 누락돼도 montage 길이 기반 fallback으로 `StartMonsterAIWithCurrentTarget`까지 진행
- `StartMonsterAIWithCurrentTarget`는 Target 지정 성공 후 BT 루트를 재평가해 sequence 직후 Chase 조건을 즉시 반영
- `FirstEntrySequence` runtime player는 `ForceKeepState`로 종료되어 sequence가 지정한 마지막 배치 위치를 복구하지 않음
- `FirstEntrySequenceDelay`가 0보다 크면 server timer가 끝난 뒤 `MulticastPlayFirstEntrySequence`를 호출하고, 그 전에는 `StartFirstEntryGameplay`를 호출하지 않음

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Enemy/MonsterBase-SetTerritoryTarget.md`

## 검증 방법

```powershell
rg -n "ActivateTerritoryTrigger|OnPlayerEnteredTerritory|BeginMonsterEncounter|SetAutoRunAIOnPossess" Source Docs
```

Editor PIE:

- `bRequireMapGimmickActivation = false`: FirstEntrySequenceMonsters와 appearance가 모두 없는 배치 monster는 기존 Patrol 유지, FirstEntrySequenceMonsters 또는 appearance가 있는 배치 monster는 첫 진입 전 BT 정지
- `bRequireMapGimmickActivation = true`: player 진입 후 `ActivateTerritoryTrigger` 전까지 sequence/spawn/BT 시작 없음
- `bRequireMapGimmickActivation = false`: `ActivateTerritoryTrigger`가 나중에 호출돼도 첫 전투가 아직이면 현재 overlap player로 시작 가능
- `ActivateTerritoryTrigger` 호출 뒤 player가 안에 있으면 sequence 이후 runtime spawn 생성
- appearance montage가 있는 monster는 montage 종료 뒤 Target을 갖고 Chase 진입
- 비하강 appearance override montage가 section link로 끝나지 않아도 montage 길이 이후 Target을 갖고 Chase 진입
- `PlacedMonsterOverrides`에 지정한 배치 monster는 Monster BP 기본 montage 대신 override montage 재생
- 활성화형 배치 monster 중 appearance montage가 없는 monster는 즉시 Target을 갖고 Chase 진입
- `FirstEntrySequenceMonsters`에 들어간 배치 monster는 appearance montage가 없어도 sequence 종료 뒤 즉시 Target을 갖고 Chase 진입
