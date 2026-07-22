# AMonsterBase StasisStun 의도 카드

## 대상
`AMonsterBase::ApplyStasisStun`, `RemoveStasisStun`, `BeginStasisStun`, `EndStasisStun`

## 종류
Monster crowd-control state

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 존재 이유

StasisNet field 안에 들어온 monster를 일정 시간 이동과 공격에서 잠그기 위해 존재한다.
field actor가 movement를 직접 오래 붙잡지 않고, monster 자신의 replicated 상태로 gameplay와 AnimBP 표시를 정리한다.

## 기본 흐름

`AStasisNetField::AddMonster` -> `ApplyStasisStun(field, 5s)` -> 공격 montage 중단, movement disable, `bCanAttack=false` -> timer 만료 또는 field 이탈/소멸 -> `RemoveStasisStun`/`EndStasisStun`

## 서버/클라이언트 조건

- stun source timer, movement, attack gate는 server authority에서만 변경
- `bStasisStunned`는 replicate되어 AnimBP와 선택 montage 표시 기준이 됨
- `StasisStunMontage`는 optional BP 설정값이며, 없어도 gameplay stun은 유지

## 구분할 코드

- `bSpawnAppearancePlaying`: spawn 연출 동안 잠금
- `bHalfHealthHitReactPlaying`: 반피 피격 montage 동안 공격 잠금
- `bStasisStunned`: StasisNet field source가 부여한 일시 정지
- `SetDead`: 사망 정리이며 stun 복구보다 우선

## 수정 주의

- 여러 field가 겹칠 수 있으므로 source별 timer를 분리
- field가 사라지면 5초가 남았어도 해당 source stun을 즉시 해제
- stun 종료 시 사망 또는 spawn appearance 상태면 movement를 복구하지 않음
- stun 중 dot damage가 반피를 넘겨도 half-health hit react montage로 정지 상태를 깨지 않음
- 공격 가능 여부는 `RefreshAttackAvailability()`를 통해 기존 BT blackboard 경로로 복구

## 검증

```powershell
rg -n "StasisStun|bStasisStunned|ApplyStasisStun|RemoveStasisStun" Source Docs
```
