# DianaVoiceComponent 의도 카드

## 대상

`EDianaVoiceAction`

`FDianaVoiceLine`

`FDianaVoiceActionPool`

`UDianaVoiceLineSet`

`UDianaVoiceComponent::PlayVoiceAction`

## 종류

Diana 전용 행동 음성·자막 선택 및 co-op presentation 경로

## 정의 위치

- `Source/Pragmata_2P/Data/PragmataTypes.h`
- `Source/Pragmata_2P/Data/DianaVoiceLineSet.h`
- `Source/Pragmata_2P/Components/DianaVoiceComponent.h`
- `Source/Pragmata_2P/Components/DianaVoiceComponent.cpp`
- `/Game/Sound/DianaVoice/DA_DianaVoiceLineSet`

## 이 코드가 존재하는 이유

Diana 행동의 실제 서버 결과에 맞춰, Hugh와 Diana가 같은 무작위 음성 한 줄과 같은 자막을 보게 한다. UI는 표시만 담당하고, 선택·반복 방지·cooldown은 gameplay component가 소유한다.

## 사용해야 하는 상황

- Diana action의 server validation 또는 authoritative state transition이 완료된 뒤
- Hugh 실제 피격/사망, weapon 최초 획득처럼 Diana가 반응하는 co-op event가 서버에서 확정된 뒤
- active boss의 low-health threshold와 death처럼 한 번만 발생해야 하는 encounter event
- Cleanse의 server 검증 hold 시작 또는 IA 입력. 실제 release/timer 발사는 같은 voice를 다시 요청하지 않음

## 사용하면 안 되는 상황

- Cleanse를 제외한 input press, aim tick, preview, hold, animation notify만으로 gameplay 성공이 확정되지 않은 경우
- `cleanse skill upgrade`, `deletion skill` 음성. 현재 asset import와 trigger 범위 밖
- Widget Blueprint에서 random, queue, cooldown을 결정하는 경우
- client가 독자적으로 line을 골라 재생하는 경우

## 서버/클라이언트 조건

`PlayVoiceAction`은 owner authority에서만 실행한다.

```text
authoritative action result
-> UDianaVoiceComponent::PlayVoiceAction
-> action pool cooldown + active voice 확인
-> server random line 선택 (직전 line 제외)
-> Multicast_PlayVoiceLine
-> APragmataGameState::Multicast_PlayDialogueLine
-> Hugh/Diana local HUD와 Diana avatar mesh presentation
```

현재 재생 중인 Diana voice가 있으면 새 request는 audio와 subtitle 모두 억제한다. 이때 억제된 action의 cooldown은 소비하지 않는다. `Cleanse`, `OverDriveFire`는 gameplay gate가 충분하므로 action cooldown을 `0초`로 둔다. `HackAim` 기본 cooldown은 `10초`다.

Cleanse만은 예외로, server가 cooldown·role·상태를 검증한 뒤 precision hold를 시작하거나 quick IA 입력을 받은 시점에 요청한다. 실제 release 또는 quick timer 발사는 gameplay 적용과 cooldown 소비만 처리하며 Cleanse voice를 다시 요청하지 않는다.

## DataAsset 규칙

- 대상 15 category, 30 WAV만 `DA_DianaVoiceLineSet`에 넣는다.
- Caution pool에는 `Dead_filament`을 넣지 않는다. 해당 SoundWave는 runtime asset으로 보관하지 않는다.
- action pool마다 `CooldownSeconds`를 명시한다.
- line이 2개 이상이면 직전 선택 line을 제외한다.
- `DisplaySeconds=0`이면 SoundBase duration을 subtitle 표시 시간으로 사용한다.
- SoundWave와 DataAsset은 `/Game/CookLabels/PAL_CoreRuntime`에서 runtime cook ownership을 가져야 한다.

## Blueprint 연결 여부

`ADianaOperatorPawn` native `VoiceComponent`의 기본 soft reference가 `/Game/Sound/DianaVoice/DA_DianaVoiceLineSet`을 가리킨다. `BP_DianaOperatorPawn`은 component를 새로 추가하거나 UI를 만들지 않는다. 필요하면 Class Defaults에서 같은 asset reference만 확인한다.

## 헷갈리기 쉬운 코드

- `APragmataGameState::Multicast_PlayDialogueLine`: text 전달 경로. 음성 pool 선택과 cooldown을 소유하지 않음
- `UDialogueWidget`: 표시·자연 종료 delegate만 담당. Diana action queue가 아님
- `UTutorialBlueprintLibrary::ShowTutorialOnce`: weapon 최초 획득 gate. 반환값이 `true`일 때만 weapon voice 요청
- `UOverDriveComponent::SetOverDrivePercent`: full gauge 상태 유지가 아니라 `not full -> full` 전환에서만 ready voice 요청

## 검증 방법

```powershell
rg -n "PlayVoiceAction|EDianaVoiceAction|DA_DianaVoiceLineSet" Source Docs
```

- 2-player PIE에서 server가 고른 동일 SoundWave와 subtitle이 양쪽에 한 번씩 전달되는지 확인
- `HackAim` aim/release 반복이 10초 안에 재생되지 않는지 확인
- 기존 voice 재생 중 다른 action request가 subtitle을 덮지 않는지 확인
- `cleanse skill upgrade`, `deletion skill`이 imported asset과 pool에 없는지 확인
