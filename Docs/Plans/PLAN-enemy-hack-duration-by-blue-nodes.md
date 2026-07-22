# 적 해킹 파란 노드별 유지 시간 계획

## 목표

- [x] 적 해킹 퍼즐 완료 시 통과한 파란 노드 수와 적 종류로 `bHacked` 유지 시간을 결정
- [x] 서버 권위 해킹 적용, damage, OverDrive, success signal 흐름 유지
- [x] 이미 `bHacked`인 적의 repeat hack은 기존 timer를 reset/extend하지 않는 정책 유지
- [x] 밸런스 값은 C++ 재작성 없이 class defaults에서 조정 가능한 구조로 제공

## 기준 자료

출처: [Notion `밸런스 디자인`](https://equinox-comet-f04.notion.site/39643b7906dd80f69f6edb1e19f77435?source=copy_link)

| 적 종류 | 파란 노드 0개 | 1개 | 2개 | 3개 이상 |
| --- | ---: | ---: | ---: | ---: |
| Watcher | 6.00초 | 15.00초 | 15.00초로 clamp | 15.00초로 clamp |
| Walker | 2.06초 | 6.71초 | 15.00초 | 15.00초로 clamp |
| Executor | 1.10초 | 4.55초 | 9.10초 | 15.00초 |
| Sentinel boss | 1.00초 | 2.50초 | 5.50초 | 3개 10.00초, 4개 이상 15.00초 |

- [x] Notion에 없는 노드 구간은 마지막 명시 시간을 유지하도록 index clamp
- [x] Sentinel boss는 4개 이상에서만 15초에 도달하는 hard-to-hack profile 적용
- [x] 보간 없이 완료 시점 `NormalNodesVisited`의 정수 tier를 직접 조회

## 현재 흐름

- `UEnemyHackingPuzzleComponent`가 적 종류, 파란 노드 통과 수, 완료 시점을 보유
  - `EnemyType`: `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h:39`
  - `NormalNodesVisited`: `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h:90`
  - 완료 delegate: `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp:269`
- `UDianaHackComponent::HandleEnemyHackCompleted()`가 puzzle 결과를 서버 gameplay effect로 전달
  - damage 전달: `Source/Pragmata_2P/Components/DianaHackComponent.cpp:139`
- `AMonsterBase::ApplySuccessfulHack_Implementation()`이 첫 진입에만 `bHacked`와 timer를 시작
  - 첫 진입 판정: `Source/Pragmata_2P/Enemy/MonsterBase.cpp:417`
  - 고정 5초 기본값: `Source/Pragmata_2P/Enemy/MonsterBase.h:571`
  - timer 시작: `Source/Pragmata_2P/Enemy/MonsterBase.cpp:505`
- `ApplySuccessfulHack` interface는 현재 damage만 전달
  - `Source/Pragmata_2P/Interfaces/PragmataHackable.h:22`
- `AWatcherMonster` C++ constructor는 `EnemyType=Watcher`를 지정하지 않으므로 Blueprint override 의존 여부 확인 필요
  - `Source/Pragmata_2P/Enemy/WatcherMonster.cpp:9`

## 설계 결정

- [x] duration 계산 책임은 puzzle 결과를 소유한 `UEnemyHackingPuzzleComponent`에 배치
- [x] `FEnemyHackDurationProfile`에 적 종류와 노드별 초 배열을 보관하고 `EditDefaultsOnly`로 노출
- [x] component 기본 profile에 Notion의 Watcher, Walker, Executor 값을 등록
- [x] `GetResolvedHackDurationSeconds()`가 `EnemyType`과 `NormalNodesVisited`를 사용해 최종 시간을 반환
- [x] `UDianaHackComponent`가 완료 시 damage와 resolved duration을 함께 `ApplySuccessfulHack`에 전달
- [x] `AMonsterBase`는 전달된 duration이 유효할 때 사용하고, profile이 없으면 기존 `HackDurationSeconds` 사용
- [x] 별도 `DataAsset`/subsystem은 현재 규모에 비해 과하므로 도입하지 않음

## 구현 TODO

### 1. Duration profile과 resolver

- [x] `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.h`에 `FEnemyHackDurationProfile` 추가
  - `EEnemyHackType EnemyType`
  - `TArray<float> DurationSecondsByVisitedNodeCount`
  - 음수 index 방지와 마지막 tier clamp 규칙
- [x] `UEnemyHackingPuzzleComponent`에 `EditDefaultsOnly` profile 배열과 `GetResolvedHackDurationSeconds()` 추가
- [x] `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponent.cpp` constructor에 Notion 기본값 등록
- [x] duplicate 적 종류 profile, 빈 배열, 0 이하 duration은 warning 후 fallback 처리
- [x] `NormalNodesVisited` backtracking 감소 경로를 유지해 완료 순간 최종 경로만 duration에 반영

### 2. 완료 결과 전달과 timer 적용

- [x] `Source/Pragmata_2P/Interfaces/PragmataHackable.h`의 `ApplySuccessfulHack`에 `HackDurationSeconds` 인자 추가
- [x] `Source/Pragmata_2P/Components/DianaHackComponent.cpp:150`에서 `GetProjectedDamage()`와 `GetResolvedHackDurationSeconds()`를 같은 완료 snapshot으로 전달
- [x] `Source/Pragmata_2P/Enemy/MonsterBase.h` declaration과 timer helper를 duration 인자 기반으로 변경
- [x] `Source/Pragmata_2P/Enemy/MonsterBase.cpp:418`에서 첫 `false -> true` 전환에만 전달 duration으로 timer 시작
- [x] repeat hack 중 damage, OverDrive, success signal은 유지하고 기존 timer remaining time은 변경하지 않음
- [x] server authority guard와 `OnRep_Hacked()` presentation 경로 유지

### 3. 적 종류 일치 보강

- [x] `Source/Pragmata_2P/Enemy/WatcherMonster.cpp`에서 `EnemyHackingPuzzleComponent->EnemyType = EEnemyHackType::Watcher` 명시
- [x] Walker default, Executor, Sentinel constructor 설정이 실제 class와 일치하는지 확인
- [ ] 관련 monster Blueprint의 component `EnemyType` override가 C++ 기본값을 잘못 덮지 않는지 Editor에서 확인

### 4. 자동화 테스트

- [x] `Source/Pragmata_2P/Puzzle/EnemyHackingPuzzleComponentTests.cpp` 추가
- [x] Watcher `0=6.00`, `1=15.00`, `2+=15.00` 검증
- [x] Walker `0=2.06`, `1=6.71`, `2=15.00`, `3+=15.00` 검증
- [x] Executor `0=1.10`, `1=4.55`, `2=9.10`, `3=15.00`, `4+=15.00` 검증
- [x] 음수 count, 빈 profile fallback, Sentinel 0~4+ tier 검증
- [x] float 비교는 `FMath::IsNearlyEqual` 사용

### 5. 의도와 운영 문서 갱신

- [x] `Docs/Systems/Hacking/enemy-hacking-integration.md`의 고정 5초 설명을 노드별 duration 표와 전달 흐름으로 교체
- [x] `Docs/KNOW_HOW.md`의 `Diana Scan, Hack, and Node Interaction` recipe에 duration resolver와 repeat timer 정책 반영
- [x] `Docs/Intent/Hacking/MonsterBase-HackState.md`의 고정 duration/미확인 항목 갱신
- [x] `Docs/Intent/Hacking/EnemyHackingPuzzleComponent-HackDuration.md` 생성 후 resolver의 소유권, fallback, clamp 의도 기록
- [x] `Docs/Intent/Hacking/EnemyHackingPuzzleComponent-CompleteHack.md`에 완료 snapshot의 duration 전달 경계 추가
- [x] `Docs/Systems/Server/server-puzzle.md`, `Docs/Systems/Damage/damage-flow.md`의 `ApplySuccessfulHack` signature 갱신

## 수용 기준

- [x] 각 적과 파란 노드 수 조합이 기준 표의 시간을 반환
- [x] 표의 최대 node tier를 넘으면 마지막 값을 반환
- [x] Sentinel은 `1.0/2.5/5.5/10.0/15.0초` profile 사용
- [ ] 첫 hack 완료 후 server timer가 resolved duration으로 `bHacked`를 해제
- [ ] repeat hack 완료 전후 timer remaining time이 증가하거나 초기화되지 않음
- [x] damage 계산식과 OverDrive 지급량에 변화 없음
- [ ] Hugh host/Diana client와 Diana host/Hugh client에서 같은 `bHacked` 시작·종료 상태 관찰
- [ ] `ApplySuccessfulHack` BlueprintNativeEvent signature 변경 후 관련 Blueprint compile 오류 없음
- [x] duration UI, 남은 시간 복제, progress bar는 이번 범위에 포함하지 않음

## 검증 TODO

- [x] `rg -n "ApplySuccessfulHack|StartHackDurationTimer|HackDurationSeconds" Source Docs`로 signature와 문서 누락 확인
- [x] Unreal automation test `Pragmata.Hacking.EnemyHackDuration.*` 실행
- [x] Editor target build로 UHT/interface signature 검증
- [ ] Walker, Watcher, Executor 각각 Standalone PIE에서 0개와 최대 tier 경로 완료 후 실제 해제 시간 측정
- [ ] 2인 PIE Listen Server에서 양쪽 host 배치의 replicated `bHacked` 전환 확인
- [ ] 이미 hacked인 적을 다시 완료해 damage/보상은 적용되고 timer는 유지되는지 확인
- [ ] 관련 monster Blueprint와 HUD Blueprint compile/save 확인
- [x] `git diff --check` 실행 및 계획 외 파일 미변경 확인

## 위험과 대응

- [ ] `BlueprintNativeEvent` 인자 추가로 Blueprint node가 끊길 수 있음
  - 대응: 구현체/호출부 전체 검색, UHT build, 관련 Blueprint compile
- [ ] Watcher가 기본 Walker `EnemyType`을 사용하면 잘못된 profile 선택 가능
  - 대응: subclass constructor 명시와 Blueprint default readback
- [ ] Sentinel boss profile이 실제 encounter 난이도와 불일치 가능
  - 대응: 4개 이상 15초 clamp 유지, PIE에서 약점 window 체감 확인 후 profile 조정
- [ ] future duration gauge UI가 server timer와 다른 값을 표시할 수 있음
  - 대응: 후속 UI 작업에서 replicated end time 또는 remaining time을 `MonsterBase -> ViewModel -> Widget` MVVM 경로로 별도 설계
