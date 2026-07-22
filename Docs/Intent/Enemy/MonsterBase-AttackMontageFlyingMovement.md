# AMonsterBase attack montage Flying movement 의도 카드

## 대상

`FMonsterAttackPattern::bUseFlyingMovementDuringAttackMontage`, `FMonsterAttackPattern::FlyingLoopMoveSpeed`, `AMonsterBase::BeginAttackFlyingMovement`, `AMonsterBase::RestoreAttackMovementMode`

## 정의 위치

- `Source/Pragmata_2P/Enemy/MonsterBase.h`
- `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

## 존재 이유

장거리 점프·비행 공격은 ground movement 상태에서 지면 처리와 충돌할 수 있다. pattern별 opt-in으로 montage 재생 동안만 `MOVE_Flying`을 사용한다. proximity loop가 자기 반복 section에 들어가면 해당 section의 montage root motion을 무시하고 CharacterMovement 전진 속도로 이동해, 애니메이션 이동량과 무관하게 속도·최대 시간을 조절할 수 있게 한다.

## 동작 흐름

```text
AttackByAI
-> AttackMontage 재생 성공
-> Use Flying Movement During Attack Montage 확인
-> 기존 MovementMode 저장 후 MOVE_Flying
-> self-loop section 진입 시 root motion 무시, Flying Loop Move Speed로 전진
-> End 거리 도달 또는 MaxLoopDuration 경과 시 root motion 복구 후 End section 전환
-> montage 종료 또는 공격 취소
-> 저장한 MovementMode, CustomMovementMode, MaxFlySpeed 복구
```

## Blueprint 연결

monster BP의 `Attack Patterns` 배열 항목에서 `Use Flying Movement During Attack Montage`와 `Use Proximity Controlled Montage Loop`를 함께 켠다. `Flying Loop Move Speed`는 cm/s 단위이며, 최대 전진 거리는 이 값과 `Max Loop Duration`의 곱이다. `End Transition Distance`에 먼저 도달하면 더 짧게 끝난다. `Loop` section의 Next Section은 자기 자신이어야 한다.

## 사용해야 하는 상황

- animation root motion 이동량 대신 일정한 CharacterMovement 속도로 전진해야 하는 장거리 비행 공격
- `Start -> Loop -> End` 구조에서 loop를 여러 번 반복하는 점프·비행 공격

## 사용하면 안 되는 상황

- `Use Proximity Controlled Montage Loop` 없이 단발 section만 재생하는 montage
- CharacterMovement 또는 BT가 별도 경로 이동을 담당하는 일반 공격

## 혼동하기 쉬운 코드

- `bUseProximityControlledMontageLoop`: self-loop 구간과 End 전환 조건을 정의하며, Flying 전진 옵션이 켜졌을 때만 실제 전진을 시작함
- `bUseFallingAppearance`: Territory 출현 연출의 낙하 처리이며 공격 montage 옵션이 아님
- `FlyingLoopMoveSpeed`: `MaxLoopDuration`과 함께 최대 이동 거리를 결정하며, 벽 충돌·target 근접은 실제 이동 거리를 줄일 수 있음
- `RestoreAttackMovementMode`: 공격이 정상 종료되거나 피격·사망으로 취소될 때 이동 모드·속도·root motion mode를 복구

## 확인 근거

- `UpdateProximityControlledAttackLoop`가 self-loop section에서만 전진 입력을 추가하고 End 전환 전에 root motion mode를 복구
- `FinishAttackByAI`와 `AbortAttackForDeath`가 같은 복구 함수를 호출
