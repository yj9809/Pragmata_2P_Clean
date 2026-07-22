# Diana 액션 보이스 계획

## 목표

- `C:\Users\hojin\Downloads\PRAG_PROJ\Assets\SFX\WAV files\Selected`의 이번 대상 Diana WAV 31개를 Unreal runtime 음성과 자막으로 가져온다.
- Diana 액션이 서버에서 실제로 성공·확정될 때만 해당 카테고리 pool에서 한 줄을 무작위 재생한다.
- 같은 카테고리는 직전 줄 반복을 피한다. 두 줄 이상이면 매 trigger마다 후보 중 하나를 선택한다.
- 모든 voiced action은 독립 cooldown을 가진다. cooldown 중 또는 Diana 음성이 재생 중이면 audio와 자막을 모두 요청하지 않는다.
- Hugh 전용 action에는 연결하지 않는다. 다만 Diana가 실행한 action의 음성은 listen server의 Hugh와 Diana 양쪽 client에서 같은 한 줄을 듣는다.

## 범위와 전제

- speaker는 Diana로 한정한다. `on death`도 Diana가 Hugh 사망에 반응하는 경우만 포함한다.
- source WAV 원본과 `Selected` 폴더는 외부 작업물로 보존한다. 이번 범위는 15개 category, 31 WAV이며 `cleanse skill upgrade`(2 WAV), `deletion skill`(1 WAV)은 import, DataAsset 연결, trigger를 모두 제외한다.
- 서버가 line 하나와 대응 자막을 선택한다. client별 랜덤 선택은 금지한다.
- 선택 결과는 audio multicast와 `APragmataGameState::Multicast_PlayDialogueLine()`에 함께 전달한다. `WBP_DialogueWidget`은 기존 root HUD child를 그대로 사용하며, 대사 선택·queue를 소유하지 않는다.
- 현재 source에서 확인된 확정 경계는 scan, enemy/map hack 시작·실패, cleanse cast, OverDrive ready/fire, ItemBox open, Hugh 실제 피격, Hugh 무기 최초 획득이다. boss defeat와 남은 category도 실제 state transition에만 연결한다.

## 카테고리와 연결 후보

| Selected 카테고리 | 수 | 연결 기준 |
| --- | ---: | --- |
| `scan` | 1 | `UDianaScanComponent::Server_RequestScan` accepted 뒤 |
| `hacking` | 3 | enemy/map hack이 server validation 뒤 실제 시작될 때 |
| `hacking aim` | 2 | server가 enemy hack puzzle 시작을 승인할 때. reticle tick, hold, release, retry마다 재요청 금지 |
| `hacking fail` | 2 | puzzle의 실패/reset 결과. 단순 cancel·거리 이탈에는 재생하지 않음 |
| `cleanse skill` | 4 | `UDianaCleanseComponent::ApplyCleanseFromMode`가 server validation을 통과한 cast |
| `overdrive skill ready` | 3 | OverDrive gauge가 full로 전환될 때. full 상태 유지 중 재요청 금지 |
| `overdrive skill fire` | 1 | `UOverDriveComponent::Server_RequestOverDrive` accepted 뒤 |
| `item box opened` | 1 | `AItemBoxInteractable::Interact_Implementation`의 최초 open |
| `Decoy generator obtain` | 1 | `Decoy.FirstAcquired` tutorial gate가 최초 성공한 weapon pickup |
| `Shockwave gun obtain` | 1 | `Shotgun.FirstAcquired` tutorial gate가 최초 성공한 weapon pickup |
| `stasis net obtain` | 1 | `Stasis.FirstAcquired` tutorial gate가 최초 성공한 weapon pickup |
| `caution` | 5 | `AHughCharacter::ReceiveDamage_Implementation`에서 실제 HP가 감소했고 Hugh가 생존한 경우 |
| `imminent victory` | 1 | boss/encounter의 단 한 번인 low-health threshold 확인 뒤 |
| `victory` | 2 | active boss의 server death가 확정된 경우 |
| `on death` | 3 | Hugh의 실제 death transition. damage 시도·respawn에는 재생하지 않음 |

| 제외 category | 수 | 이번 범위 |
| --- | ---: | --- |
| `cleanse skill upgrade` | 2 | 보류. import, DataAsset, trigger 없음 |
| `deletion skill` | 1 | 보류. import, DataAsset, trigger 없음 |

`caution`과 weapon obtain은 현재 C++ 경계가 확정됐다. `victory`는 boss actor의 actual death를 기준으로 하며, HUD의 `ActiveBossTarget` 해제만으로 재생하지 않는다. `imminent victory`는 구현 시작 시 current event source를 확인해 정확한 low-health threshold를 확정한다. source가 없으면 임의 polling이나 tick을 만들지 않고 명시적인 server event를 추가한다.

## 재생 제한 정책

`DA_DianaVoiceLineSet`의 모든 구현 대상 action pool은 `CooldownSeconds`를 명시한다. 아래 값은 첫 PIE 검증용 기본값이며, DataAsset에서만 조정한다. `Cleanse`와 `OverDriveFire`만은 이미 긴 gameplay cooldown/gauge gate가 있으므로 action cooldown을 추가로 적용하지 않는다.

| Action | 초기 cooldown | 추가 guard |
| --- | ---: | --- |
| `Caution` | 6초 | 실제 non-lethal HP 감소만 |
| `Scan` | 8초 | server scan acceptance만 |
| `Hack` | 6초 | puzzle 실제 시작만 |
| `HackAim` | 10초 | 유효 target 최초 획득 transition만 |
| `HackFail` | 6초 | actual fail/reset만 |
| `Cleanse` | 0초 | 기존 cleanse gameplay cooldown과 검증된 cast만 |
| `OverDriveReady` | 20초 | gauge `not full -> full` 전환만 |
| `OverDriveFire` | 0초 | full gauge 소비와 server acceptance만 |
| `ItemBoxOpened` | 15초 | 첫 open만 |
| 무기 획득 3종 | 60초 | `ShowTutorialOnce() == true`만 |
| `ImminentVictory` | 60초 | boss threshold 1회만 |
| `Victory` | 60초 | active boss death 1회만 |
| `OnDeath` | 30초 | Hugh death 1회만 |

한 줄이 재생 중이면 다른 Diana voice request는 억제한다. 억제된 request는 자막도 표시하지 않고 해당 action의 cooldown도 시작하지 않는다. 이렇게 기존 line의 audio와 `WBP_DialogueWidget` 자막이 서로 다른 action으로 덮이지 않게 한다.

## 구현 TODO

### 1. 기존 이벤트와 Blueprint ownership 확정

- [ ] `BP_DianaOperatorPawn`, active boss/encounter Blueprint, OverDrive gauge transition, enemy hack aim target source를 read-only로 조사한다.
- [x] 표의 15개 구현 대상 category마다 server-authoritative trigger 함수, one-shot 여부, repeat 가능 여부 확인
- [ ] clear intent와 충돌하는 기존 문서가 있으면 구현을 멈추고 override 여부를 확인한다.
- [ ] trigger가 아직 없는 category는 해당 gameplay owner에 한 개의 명시적 delegate/function만 추가한다. HUD, tick, animation notify를 gameplay truth 대용으로 사용하지 않는다.

### 2. 음성 데이터와 runtime component 추가

- [x] `EDianaVoiceAction`, `FDianaVoiceLine`, `FDianaVoiceActionPool` 추가
- [x] `UDianaVoiceLineSet`와 `UDianaVoiceComponent`를 `ADianaOperatorPawn`에 추가
- [x] server random, 직전 line 제외, action별 cooldown, active-voice suppression 적용
- [x] 선택 SoundWave multicast와 `Multicast_PlayDialogueLine()` 기존 UI 연결
- [ ] null asset, 빈 pool, load 실패, non-authority 호출은 gameplay를 막지 않고 `[DianaVoiceComponent::...]` 로그 후 skip한다.
- [ ] loop, persistence, global random stream, separate subsystem은 추가하지 않는다.

### 3. 승인된 Diana action 경계에 연결

- [x] scan, cleanse, OverDrive ready/fire, ItemBox first open, Hugh damage/death, weapon first-acquired, boss death 연결
- [x] enemy hack 시작은 `hacking aim`, enemy hack completion은 `hacking`, map hack 시작은 `hacking`, map hack reset은 `hacking fail`로 연결
- [x] imminent victory는 active boss HP가 `DianaImminentVictoryHealthRatio`(기본 25%)를 처음 통과할 때만 연결

### 4. SoundWave import와 Blueprint data configuration

- [x] `/Game/Sound/DianaVoice/<Category>/`에 31 WAV import
- [x] `cleanse skill upgrade`, `deletion skill` 3 WAV 제외 확인
- [x] `/Game/Sound/DianaVoice/DA_DianaVoiceLineSet`에 15 action pool, 31 SoundWave, subtitle, cooldown 연결
- [x] `BP_DianaOperatorPawn` CDO의 `VoiceComponent`가 `DA_DianaVoiceLineSet`을 상속 참조하는지 readback 확인
- [x] `PAL_CoreRuntime` explicit asset에 31 SoundWave + DataAsset 추가, `AlwaysCook`·`IsRuntimeLabel` readback 확인

### 5. 문서와 intent 갱신

- [x] `Docs/Intent/Audio/DianaVoiceComponent-PlayVoiceAction.md`에 Diana-only boundary, server selection, audio/subtitle multicast, no-immediate-repeat, per-action cooldown, active-voice suppression 정책 기록
- [ ] `Docs/Intent`의 각 modified gameplay action card에 voice trigger가 gameplay success 뒤에만 실행되는 이유를 보강한다.
- [x] `Docs/KNOW_HOW.md`에 source asset, cooldown, cook ownership 현재 truth 갱신
- [ ] Blueprint 연결과 asset property 값은 필요한 경우 `Docs/Operations/blueprint-tasks.md`에 path·panel·property·검증 순서로 기록한다.

## 수용 기준

- [x] 15개 구현 대상 category, 31 WAV가 `DA_DianaVoiceLineSet`에 누락 없이 연결되고, 보류한 2개 category, 3 WAV는 포함되지 않는다.
- [ ] 같은 category가 두 줄 이상이면 연속 trigger에서 직전 line을 다시 선택하지 않는다.
- [ ] line 선택은 server에서 한 번이며 Hugh host/Diana client와 Diana host/Hugh client에서 같은 asset path가 재생된다.
- [ ] 선택된 음성과 같은 subtitle이 `FDialogueLine`으로 `Multicast_PlayDialogueLine()`에 전달되어 양쪽 `WBP_DialogueWidget`에 표시된다.
- [ ] 모든 action pool은 DataAsset에 명시적인 cooldown을 가진다. `Cleanse`와 `OverDriveFire`만 `0초`이고, 나머지는 0보다 큰 독립 cooldown 중에는 audio/subtitle을 재생하지 않는다.
- [ ] Diana voice 재생 중 발생한 다른 action request는 audio/subtitle을 재생하지 않으며, 기존 subtitle을 덮지 않는다.
- [ ] invalid action, cooldown 거부, preview, cancelled hack, non-Diana action은 음성을 재생하지 않는다.
- [ ] Hugh 피격으로 HP가 실제 감소할 때만 `caution`이 한 번 재생되고, lethal damage는 `on death` 경로만 사용한다.
- [ ] `ShowTutorialOnce()`가 `true`인 새 weapon 획득에서만 대응하는 Diana 획득 line이 재생된다. ammo refill, 재획득, 이미 표시된 tutorial에는 재생되지 않는다.
- [ ] `victory`는 active boss의 server death에서만 한 번 재생되고, 일반 monster death와 boss HUD 종료에는 재생되지 않는다.
- [ ] scan, hack start/aim/fail, cleanse, OverDrive ready/fire, ItemBox는 실제 success/failure/transition마다 정확히 한 번의 voice request를 만든다.
- [ ] remaining category도 각 owner의 one-shot/transition guard 아래에서만 fire된다.
- [ ] missing audio asset은 warning/log만 남기며 gameplay RPC, action lock, cooldown, damage, reward를 바꾸지 않는다.
- [ ] package build에서 `PAL_CoreRuntime`에 포함된 voice assets가 누락되지 않는다.

## 검증 TODO

- [ ] Editor에서 각 imported SoundWave와 `DA_DianaVoiceLineSet`을 열어 category/line count를 manifest와 대조한다.
- [ ] 각 multi-line category를 cooldown 만료 뒤 최소 10회 trigger해 한 줄만 고정되지 않고 즉시 반복도 없는지 확인한다.
- [ ] `hacking aim`은 같은 target을 계속 조준하거나 aim/release를 반복해도 10초 cooldown 전에는 audio/subtitle을 다시 재생하지 않는지 확인한다.
- [ ] 다른 Diana action이 음성과 자막을 재생 중일 때 새 action을 발생시켜 기존 음성·자막이 유지되는지 확인한다.
- [ ] Standalone PIE에서 모든 mapped action의 accepted/rejected 경로를 분리해 확인한다.
- [ ] 2-player PIE의 Hugh host/Diana client 및 Diana host/Hugh client에서 action당 voice request 1회와 동일한 selected line을 확인한다.
- [ ] server log로 selection과 multicast path를 확인하고, client log로 duplicate local playback이 없는지 확인한다.
- [ ] `BP_DianaOperatorPawn` 및 관련 changed Blueprint를 compile/save하고 PAL asset 설정을 readback한다.
- [ ] Unreal Editor target build, 관련 automation test, `git diff --check`를 실행한다.
- [ ] plan에 없는 기존 수정 파일은 변경하지 않았는지 `git status --short`와 diff로 확인한다.

## 위험과 대응

- [ ] action success가 아닌 input start에 연결하면 거부된 RPC에도 대사가 나올 수 있음
  - 대응: server validation 이후/authoritative state transition만 trigger
- [ ] weapon tutorial gate를 두 번 호출하면 첫 호출이 SaveGame 이력을 소비해 voice 판단이 달라질 수 있음
  - 대응: `ShowTutorialOnce()` 반환값을 한 번만 캡처해 tutorial과 Diana voice가 같은 first-acquired 결과를 공유
- [ ] client별 random 선택은 두 player가 서로 다른 Diana line을 듣게 함
  - 대응: server가 path를 선택해 multicast
- [ ] scan/hack/cleanse loop가 같은 line을 과다 재생할 수 있음
  - 대응: one-shot 성공 경계, mandatory per-action cooldown, no-immediate-repeat 유지
- [ ] aim/release 반복이 `hacking aim`과 자막을 과다 재생할 수 있음
  - 대응: valid target 최초 획득 transition만 요청하고, `HackAim` 10초 cooldown과 active-voice suppression 적용
- [ ] 서로 다른 action의 대사가 겹치면 `WBP_DialogueWidget`이 이전 줄을 덮어 audio와 subtitle이 어긋날 수 있음
  - 대응: 하나의 Diana voice가 끝날 때까지 후속 audio/subtitle request를 함께 억제
- [ ] soft-referenced audio가 packaged build에 빠질 수 있음
  - 대응: `PAL_CoreRuntime` explicit asset readback과 packaged validation
- [ ] external selected WAV의 category가 현재 gameplay action과 1:1이 아닐 수 있음
  - 대응: category를 합치거나 재분류하지 않고, source owner 확인 전에는 data-only 상태로 유지
