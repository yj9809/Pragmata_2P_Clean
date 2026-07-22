# AWalkerMonster 공격 무기 표시 의도 카드

## 대상

`AWalkerMonster::HandleLocalMontageStarted`, `HandleLocalMontageEnded`, `ApplyAttackWeaponMaterial`

## 종류

Walker attack presentation boundary

## 정의 위치

- `Source/Pragmata_2P/Enemy/WalkerMonster.h`
- `Source/Pragmata_2P/Enemy/WalkerMonster.cpp`

## 이 코드가 존재하는 이유

normal/Tainted Walker의 blade mesh는 비공격 상태에서 숨기고 공격 animation 전체에서 보여야 한다. 두 Skeletal Mesh의 기본 weapon material을 `M_Invisible`로 두고, 각 peer가 attack montage 재생에 성공하면 원본 material로 바꾼다.

## 기본 흐름

공격 montage local 재생 -> `HandleLocalMontageStarted` -> 원본 weapon material 적용 -> 해당 montage 정상 종료·중단 -> `HandleLocalMontageEnded` -> `M_Invisible` 복원

## 서버/클라이언트 조건

- sweep와 damage 적용은 기존처럼 server authority에서만 실행
- server와 client는 각자의 `UAnimInstance::OnMontageStarted`에서 local presentation 시작
- 시작한 montage가 `AttackPatterns[].AttackMontage`에 등록된 경우만 weapon 표시
- 표시 상태를 replicate하지 않고 각 peer의 montage lifecycle을 presentation 기준으로 사용

## 중단 안전장치

- montage asset별 활성 instance count를 저장하고 같은 montage의 종료만 처리
- local montage 정상 종료와 interrupt 모두 `OnMontageEnded`에서 숨김
- `EndPlay`에서 callback과 count를 정리하고 `M_Invisible`을 최종 적용
- 관계없는 montage 종료는 활성 attack montage map에 없으므로 무시

## 수정 주의

- normal과 Tainted mesh의 material index가 다르므로 `ch5100_00_Weapon_NoStamp` slot name으로 찾아야 함
- hack material 전환 대상 네 slot과 Tainted `DF`/`DF2`는 weapon 표시 로직에서 변경하지 않음
- 원본 weapon material과 `M_Invisible`은 soft load하므로 `PAL_EnemyRuntime`의 `AlwaysCook` explicit asset에서 제거하지 않음
- `UAnimNotifyState_AttackHitWindow`는 타격 가능한 짧은 damage window만 소유하며 material presentation을 호출하지 않음
- weapon 표시 범위를 damage notify 길이로 다시 줄이지 않음

## 검증

- 두 mesh의 weapon slot 기본 material을 Editor에서 `M_Invisible`로 재조회
- 오른팔·왼팔 attack montage의 wind-up, hit window, recovery 전체와 montage 중단 후 표시 상태 확인
- listen server의 host/client 양쪽에서 같은 구간만 보이는지 확인
