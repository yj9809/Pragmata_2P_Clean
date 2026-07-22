# OverDrive cast

## 목적

- Diana full gauge cast를 server-authoritative 1회 action으로 처리
- OverDrive는 map `ULevelSequence`와 camera actor 전환을 사용하지 않고 Diana owning client의 현재 camera, `AS_Diana_Grapple_0010_HacK_SP` body, `AS_Diana_Face_Grapple_SPHack` face로만 연출

## 흐름

```text
X 또는 양 thumbstick press
-> UOverDriveComponent::Server_RequestOverDrive
-> Diana action lock + local animation
-> 2초 server timer
-> Hugh 위치 snapshot
-> 5000cm MonsterBase ApplyTimedHackState(20초) + replicated ring
```

## 경계

- server는 Diana role, full gauge, action lock, duplicate cast를 검증
- Diana owning client는 현재 view target을 유지하고, look ignore와 possessed Diana pawn `DisableInput()`을 함께 적용해 movement·aim·hack·scan·cleanse·interaction을 막고 종료/abort에서 복구
- `DianaSkillBody` slot 재생 중에는 Diana 오른손 IK를 끄며 종료 후 기존 IK 정책으로 복구
- ring 중심은 Diana가 아니라 effect 시점 Hugh 위치
- `ApplyTimedHackState()`는 damage, reward, puzzle completion, gauge 증가를 호출하지 않음
- `AOverDrivePulseRingActor`는 `M_OverDriveRing`의 orange emissive material과 positive Z mesh segment만 사용하며 floor decal이나 slowdown을 추가하지 않음
- Diana body/face animation은 soft reference이므로 packaged build 전 `PAL_CoreRuntime` explicit asset 포함을 확인
