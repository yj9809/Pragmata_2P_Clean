# ReceiveDamage 의도 카드

## 대상

`IPragmataDamageable::ReceiveDamage`, `ReceiveDamage_Implementation`

## 종류

Damage receiver entry point

## 정의 위치

- `Source/Pragmata_2P/Interfaces/PragmataDamageable.h`
- `Source/Pragmata_2P/Characters/PlayableCharacter.cpp`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 이 코드가 존재하는 이유

Unreal 기본 `TakeDamage`보다 많은 gameplay 정보를 `FPragmataDamageSpec`으로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- gameplay damage를 actor에 적용할 때
- hit react, death animation, faction gate, hack damage처럼 추가 context가 필요할 때
- monster BP별 weapon 피해 취약/저항 배율을 최종 health 변경 전에 반영해야 할 때
- 최종 health 변경 결과를 보고 후속 반응을 처리해야 할 때
- monster가 weapon 피해를 실제로 받은 뒤 Territory 내부 공격자를 damage stimulus로 등록해야 할 때

## 사용하면 안 되는 상황

- source가 아직 `FPragmataDamageSpec`을 만들 수 없는 단순 engine damage event
- damage가 아닌 UI 표시나 preview 계산

## 기본 흐름

`FPragmataDamageSpec` 생성 -> `UPragmataDamageLibrary::DealDamageToActor` -> `IPragmataDamageable::ReceiveDamage` -> `UPragmataHealthComponent::ReceiveDamage`

## 이 코드를 호출하는 쪽

- `UPragmataDamageLibrary::DealDamageToActor`
- `AMonsterBase::ApplySuccessfulHack_Implementation`

## 이 코드가 호출하는 쪽

- `CanReceiveDamage`
- `UPragmataHealthComponent::ReceiveDamage`
- actor별 hit react 또는 death 처리
- `AMonsterBase::ReportWeaponDamageSense`

## 서버/클라이언트 조건

gameplay damage는 server authority 기준으로 처리한다.

## Blueprint 연결 여부

미확인. C++ graph 밖 Blueprint 호출 가능성은 별도 Unreal asset scan 필요.

## 헷갈리기 쉬운 코드

- `TakeDamage`: Unreal engine damage event용 기본 API. 현재 gameplay damage 경로로 쓰지 않는다.
- `UPragmataHealthComponent::ReceiveDamage`: 최종 health mutation 지점. actor-level 규칙은 이 card의 `ReceiveDamage_Implementation`에서 먼저 처리한다.

## 관련 문서

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/Create/20260703-161329-receive-damage-only-hitreact-refactor.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

미확인. `git log -SReceiveDamage -- Source/Pragmata_2P`로 추적.

## 확실한 근거

- `Docs/Create/20260703-161329-receive-damage-only-hitreact-refactor.md`에 `TakeDamage` bridge 제거와 `ReceiveDamage` 단일화 기록 존재
- `Docs/KNOW_HOW.md`에 `TakeDamage` bridge 제거와 `FPragmataDamageSpec` 직접 전달 기록 존재
- `Docs/Systems/Damage/damage-flow.md`에 현재 damage flow 존재

## 추정한 의도

damage source와 receiver 사이의 gameplay context 손실을 막고, actor별 반응을 health mutation 이후 정확히 처리하려는 의도.

Monster의 weapon 피격 감지는 health 적용 성공 후 생존 상태에서만 실행하며, 공격자 player pawn이 assigned Territory 안에 있을 때만 `AISense_Damage` stimulus를 자기 `AIPerceptionComponent`에 직접 등록한다.

Monster의 non-Hack 피해는 해킹 전 기본 `0.2` 방어 배율을 먼저 받는다. monster BP에서 `Override Unhacked Damage Taken Multiplier`를 켜면 `UnhackedDamageTakenMultiplier` 값으로 이 배율을 대체한다.

Monster의 weapon damage는 해킹 전 방어 배율과 해킹 약점 배율을 계산한 뒤, monster BP의 weapon damage taken multiplier를 마지막에 곱한다. `PerWeaponDamageTakenMultipliers`에 damage causer weapon class와 일치하는 항목이 있으면 그 값을 사용하고, 없으면 공통 `WeaponDamageTakenMultiplier`를 사용한다. class별 override에서 `Use Hacked State Damage Taken Multipliers`를 켜면 monster의 현재 hack 상태에 따라 `DefaultDamageTakenMultiplier` 또는 `HackedDamageTakenMultiplier`를 사용한다. 이 배율은 `DamageKind == Weapon`에만 적용되며 hack damage나 monster melee damage에는 적용하지 않는다.

## 확신도

강한 추정

## 아직 모르는 점

Blueprint에서 직접 `ReceiveDamage`를 호출하는 asset 존재 여부.

## 검증 방법

```powershell
rg -n "ReceiveDamage|TakeDamage|FPragmataDamageSpec" Source Docs
.\Tools\CodeIntent\validate.ps1
```

## 다음 수정 시 주의점

`TakeDamage`를 다시 추가하기 전에 `FPragmataDamageSpec`으로 전달되는 정보가 손실되지 않는지 확인.
Monster 피격 감지를 확장할 때는 damage kind와 Territory 조건을 `AMonsterBase::ReceiveDamage_Implementation` 바깥으로 흩뜨리지 않는다.
AI target 설정은 damage receiver에서 직접 처리하지 않고 `OnTargetDetected`와 `MonsterAIController::SetTarget` 경로에 둔다.
해킹 전/후 비율만 조정할 때는 monster BP의 `UnhackedDamageTakenMultiplier` override로 해결한다. 총기 전체 상성은 `WeaponDamageTakenMultiplier`로 잡고, 특정 무기만 표와 어긋나면 `PerWeaponDamageTakenMultipliers`에 해당 weapon class와 별도 배율을 추가한다. StasisNet처럼 weapon 내부에 default/hacked damage가 따로 있는 무기는 per-weapon 항목에서 hack state split을 켜고 default/hacked 배율을 각각 맞춘다.
