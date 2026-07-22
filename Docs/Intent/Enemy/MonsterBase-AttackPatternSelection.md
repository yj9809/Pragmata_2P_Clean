# AMonsterBase attack pattern 선택 조건 의도 카드

## 대상

`AMonsterBase::EnsureSelectedAttackPattern`, `AMonsterBase::IsAttackPatternPositionRequirementMet`, `FMonsterAttackPattern::bRequireTargetBehind`, `FMonsterAttackPattern::TargetBehindAngleDegrees`

## 종류

Monster attack pattern selection boundary

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 존재 이유

공격 pattern 선택은 쿨타임, 거리, 연속 사용 제한, 가중치를 한곳에서 확정한다. 뒤로 점프해 내려찍는 공격처럼 target 위치가 맞을 때만 의미 있는 montage는 별도 BT 분기를 만들지 않고 pattern 후보 필터로 제한한다.

## 기본 흐름

```text
CanAttack
-> BT Select Pattern
-> EnsureSelectedAttackPattern
-> cooldown/weight/range/position 조건 검사
-> 조건을 만족한 후보만 SelectionWeight로 랜덤 선택
-> AttackByAI
```

## Blueprint 연결

`AttackPatterns` 배열의 각 항목에서 `Require Target Behind`를 켜면 해당 pattern은 target이 몬스터 뒤쪽 cone 안에 있을 때만 선택 후보에 들어간다. `Target Behind Angle Degrees`는 뒤쪽 중심 기준 전체 cone 폭이며, 거리는 기존 `MinimumAttackRange`, `AttackRange`, `AttackRadius`를 그대로 사용한다.

뒤쪽 조건 pattern은 공격 시작 직전 target 바라보기를 생략한다. BT `Turn To Target` task도 선택된 pattern이 현재 방향 유지 대상인지 확인하고 회전을 생략한다. 뒤로 점프하는 root motion 또는 animation 이동이 몬스터 현재 전방 기준 후방으로 실행되어야 target 쪽으로 향하기 때문.

## 사용해야 하는 상황

- target이 뒤에 있을 때만 자연스러운 공격 montage
- 뒤로 점프, 뒤돌기, 후방 견제 등 방향 조건이 있는 monster attack pattern
- 같은 BT 공격 분기 안에서 일반 공격과 조건부 공격을 가중치로 함께 고를 때

## 사용하면 안 되는 상황

- 공격 시작 전에 반드시 target을 바라봐야 하는 전방 공격
- 거리만 다르고 방향 조건이 필요 없는 공격
- 별도 AI 상태나 연출 sequence가 필요한 특수 행동

## 헷갈리기 쉬운 코드

- `IsAttackPatternInRange`: target과의 2D 거리 조건
- `IsAttackPatternPositionRequirementMet`: target 방향 조건
- `GetEffectiveSelectionWeight`: phase나 연속 사용 제한 등 가중치 필터
- `CanAttack`: 공통 cooldown과 사용 가능한 pattern 존재 여부 확인
- `bTrackTargetDuringAttack`: 공격 중 BT `Turn To Target` 회전 허용 여부
- `ShouldKeepCurrentFacingForSelectedAttackPattern`: BT task와 공격 실행부가 공유하는 현재 방향 유지 조건

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Enemy/MonsterBase-AttackPatternDamage.md`

## 확실한 근거

- `EnsureSelectedAttackPattern`은 최종 후보를 모아 가중치 랜덤을 수행
- `bRequireTargetBehind`가 꺼진 pattern은 기존과 같은 조건으로 선택
- `bRequireTargetBehind`가 켜진 pattern은 뒤쪽 cone 조건 실패 시 후보에서 제외
- `bRequireTargetBehind`가 켜진 pattern은 `AttackByAI` 즉시 회전과 `BTTask_TurnToTarget` 회전을 생략

## 확신도

확실
