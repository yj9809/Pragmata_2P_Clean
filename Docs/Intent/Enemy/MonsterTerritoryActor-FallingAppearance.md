# Monster Territory Falling Appearance 의도 카드

## 대상

`FMonsterTerritorySpawnInfo.bUseFallingAppearance`, `FMonsterTerritorySpawnInfo.FallingSpawnZOffset`, `AMonsterBase::TryPlaySpawnAppearance`, `AMonsterBase::UpdateFallingSpawnAppearance`

## 목적

Runtime spawn monster의 등장 연출 중 하늘에서 떨어지는 montage를 실제 월드 낙하와 분리해 처리.

`SpawnPoint`는 착지 지점으로 유지하고, 하강 appearance를 켠 spawn 항목만 `FallingSpawnZOffset`만큼 위에서 생성한다. 실제 Z 이동은 montage root motion이 아니라 `CharacterMovement`의 `MOVE_Falling`과 중력이 담당한다.

## 흐름

```text
RuntimeSpawns[i].SpawnPoint
-> bUseFallingAppearance true
-> SpawnPoint + FallingSpawnZOffset 에서 spawn
-> TryPlaySpawnAppearance(..., bUseFallingAppearance=true)
-> AnimInstance RootMotionMode = IgnoreRootMotion
-> CharacterMovement MOVE_Falling
-> ground sweep 또는 Landed
-> FallingAppearanceEndSectionName 으로 montage section jump
-> appearance 종료
-> Territory target 지정 + RunAI
```

## 구분

- 일반 appearance: 이동을 잠그고 montage 종료 뒤 AI 시작
- 하강 appearance: 공격과 AI는 잠그지만 movement mode는 `MOVE_Falling` 유지
- montage root motion: pose 재생에는 사용하지만 actor 이동에는 사용하지 않음
- `Loop -> Loop` 연결: asset fallback으로 유지하되 code가 착지 조건에서 `End`로 전환

## 주의

- 하강 montage는 `Start -> Loop -> End` section 구조 필요
- `FallingAppearanceEndSectionName`이 잘못되면 warning 후 montage stop fallback으로 무한 loop 방지
- `FallingEndTransitionDistance`가 너무 작으면 `Landed()`에서 End 전환되어 착지 모션이 늦게 보일 수 있음
- 같은 monster BP라도 Territory spawn 항목마다 하강 appearance 여부와 높이를 다르게 지정 가능

## 검증

- `bUseFallingAppearance` true 항목이 `SpawnPoint` 위에서 생성되는지 확인
- root motion enabled montage라도 actor가 중력으로 내려오는지 확인
- 바닥 접근 시 client/server 모두 `End` section으로 전환되는지 확인
- appearance 종료 후 Target 지정과 Chase 전환 확인
