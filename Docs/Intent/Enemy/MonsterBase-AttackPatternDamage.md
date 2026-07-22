# AMonsterBase::GetCurrentAttackDamage 의도 카드

## 대상

`AMonsterBase::GetCurrentAttackDamage_Implementation`, `FMonsterAttackPattern::bUsePatternAttackDamage`, `FMonsterAttackPattern::PatternAttackDamage`

## 종류

Monster attack damage source boundary

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`
- `Source/Pragmata_2P/Animation/AnimNotifyState_AttackHitWindow.cpp`
- `Source/Pragmata_2P/Animation/AnimNotify_AttackHitOverlap.cpp`

## 이 코드가 존재하는 이유

공격 notify가 damage 값을 직접 소유하지 않고, 현재 공격 주체에게 damage를 질의하게 하려는 경계다. Monster는 선택된 attack pattern이 개별 damage를 사용하도록 설정된 경우 pattern damage를 반환하고, 아니면 기존 `CombatData.AttackDamage`를 반환한다.

## 사용해야 하는 상황

- monster pattern별 공격력을 다르게 설정할 때
- 같은 montage notify 로직을 유지하면서 BP pattern 데이터만 조정할 때
- pattern 선택 흐름과 damage 판정을 분리해야 할 때

## 사용하면 안 되는 상황

- 공격 notify마다 임시 damage 값을 하드코딩하려는 경우
- 선택된 pattern이 없는 상태에서 pattern damage를 추론하려는 경우
- player weapon damage, hack damage, environment damage 처리

## 기본 흐름

`AttackByAI` -> `EnsureSelectedAttackPattern` -> attack montage 재생 -> attack notify -> `IPragmataDamageSource::GetCurrentAttackDamage` -> pattern override 또는 `CombatData.AttackDamage` 반환 -> `FPragmataDamageSpec.DamageAmount`

## Blueprint 연결 여부

`AttackPatterns` 배열의 각 항목에서 `Use Pattern Attack Damage`를 켜면 `Pattern Attack Damage`가 해당 pattern 실행 중 melee damage로 사용된다. 체크하지 않은 기존 pattern은 `CombatData.AttackDamage`를 계속 사용한다.

## 헷갈리기 쉬운 코드

- `CombatData.AttackDamage`: monster 공통 fallback damage
- `FMonsterAttackPattern::PatternAttackDamage`: 선택된 pattern에서 override 체크가 켜진 경우만 사용
- `UAnimNotifyState_AttackHitWindow`, `UAnimNotify_AttackHitOverlap`: damage를 저장하지 않고 source interface만 호출

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Damage/FPragmataDamageSpec.md`

## 확실한 근거

- 두 attack notify 모두 `IPragmataDamageSource::Execute_GetCurrentAttackDamage` 결과를 `FPragmataDamageSpec.DamageAmount`에 넣음
- `AMonsterBase::GetCurrentAttackDamage_Implementation`이 선택된 pattern override를 먼저 검사하고 fallback으로 `CombatData.AttackDamage`를 반환

## 확신도

확실
