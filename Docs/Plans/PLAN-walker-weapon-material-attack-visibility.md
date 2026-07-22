# Walker 공격 상태 무기 Material 표시 계획

## 변경된 의도

- 2026-07-14 사용자 확인으로 기존 `AttackHitWindow` 전용 표시 의도를 폐기
- normal/Tainted Walker의 무기는 공격 montage 시작부터 정상 종료·중단까지 표시
- wind-up, damage window, recovery를 모두 공격 표시 구간에 포함
- `UAnimNotifyState_AttackHitWindow`는 material과 무관한 server damage window로 유지

## 구현 결과

- [x] 두 Walker mesh의 `ch5100_00_Weapon_NoStamp` 기본 material을 `/Game/VFX/M_Invisible.M_Invisible`로 유지
- [x] `PAL_EnemyRuntime`의 원본 weapon material과 `M_Invisible` `AlwaysCook` 소유권 유지
- [x] `AWalkerMonster`가 각 peer의 `OnMontageStarted`와 `OnMontageEnded` 구독
- [x] 시작 montage를 `AttackPatterns[].AttackMontage`로 필터링
- [x] montage asset별 활성 instance count를 저장하고 첫 시작에서 원본 weapon material 적용
- [x] 같은 montage의 정상 종료·중단에서 `M_Invisible` 복원
- [x] 관계없는 montage 종료 무시
- [x] `EndPlay()`에서 callback, 활성 montage, material override 정리
- [x] `UAnimNotifyState_AttackHitWindow`의 Walker include와 material callback 제거
- [x] `Docs/Intent/Enemy/WalkerMonster-AttackWeaponVisibility.md`와 `Docs/KNOW_HOW.md` 갱신

## 검증

- [x] `git diff --check`
- [x] 제거된 `BeginAttackWeaponHitWindow`, `EndAttackWeaponHitWindow`, `ActiveAttackWeaponWindows` 참조 없음
- [x] `AttackHitWindow`에 Walker/material 참조 없음
- [ ] UE 5.8 compile/UHT 검증
- [ ] normal/Tainted Walker의 wind-up부터 recovery까지 weapon 표시
- [ ] 정상 종료, half-health hit react, stasis, 사망 interrupt 뒤 weapon 숨김
- [ ] 관계없는 montage 종료가 attack weapon을 조기 숨기지 않음
- [ ] Hugh host / Diana client와 Diana host / Hugh client 양쪽 화면에서 동일한 표시 범위

## 수동 검증 기준

1. 오른팔·왼팔 attack montage를 각각 재생
2. `AttackHitWindow` 시작 전 wind-up에서 weapon 표시 확인
3. damage window 이후 recovery에서도 weapon 표시 확인
4. montage 종료 직후 `M_Invisible` 복원 확인
5. 공격 중 hit react, stasis, death로 montage를 끊고 즉시 숨김 확인

## 외부 에셋 상태

- 두 ignored Walker mesh와 `gd-upload` 복구본은 이전 작업에서 SHA-256 일치 확인 완료
- 이번 변경은 C++ lifecycle과 문서만 수정하며 Content asset은 다시 저장하지 않음
- 독립 read-only 검증에서 UE 5.8 delegate/UHT 서명, `AttackPatterns` 접근, montage count, ANS 분리를 확인했으며 blocking finding 없음
- interruption은 montage blend-out weight가 0이 되는 `OnMontageEnded`에서 숨기므로 `Montage_Stop(0.1f)` 뒤 최대 0.1초 동안 attack pose와 weapon이 함께 유지됨
