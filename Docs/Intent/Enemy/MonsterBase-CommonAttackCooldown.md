# AMonsterBase 공통 공격 쿨다운 의도 카드

## 대상

`FMonsterCombatData::CommonAttackCooldown`, `FMonsterCombatData::bUseRandomCommonAttackCooldown`, `AMonsterBase::StartCommonAttackCooldown`

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 존재 이유

공통 공격 쿨다운은 개별 pattern 쿨다운과 별도로 모든 공격 후 `NoAttack` 상태를 유지한다. 고정값만 사용하면 공격 반복 간격이 일정해지므로, monster BP가 선택적으로 최소·최대 범위 안의 대기 시간을 사용하게 한다.

## 동작 흐름

```text
공격 montage 종료
-> StartCommonAttackCooldown
-> 랜덤 옵션 확인
-> 고정값 또는 Min~Max 난수 선택
-> NextAttackAllowedTime 설정
-> RefreshAttackAvailability가 timer를 걸고 bCanAttack 갱신
```

## Blueprint 연결

monster BP의 `Combat Data`에서 기본적으로 `Common Attack Cooldown`만 설정한다. `Use Random Common Attack Cooldown`을 켜면 `Common Attack Cooldown Min`, `Common Attack Cooldown Max`가 표시된다. Min이 Max보다 크면 code가 Max를 Min으로 보정해 Min 고정값으로 처리한다.

## 혼동하기 쉬운 코드

- `FMonsterAttackPattern::AttackCooldown`: 선택된 한 pattern만 다시 고르는 것을 제한
- `CommonAttackCooldown`: 어떤 pattern이든 다음 공격 자체를 막는 공통 대기 시간
- `RefreshAttackAvailability`: 이미 확정된 다음 공격 가능 시각을 timer와 Blackboard `bCanAttack`에 반영하며 난수를 다시 뽑지 않음

## 확인 근거

- 난수는 server의 `StartCommonAttackCooldown`에서 공격 종료마다 한 번만 생성
- `BT_Sentinel`의 `NoAttack` 분기는 Blackboard `bCanAttack`이 다시 true가 될 때까지 유지
