# BGMRegionActor

## Intent

`ABGMRegionActor`는 맵에 배치하는 지역 BGM 액터다. 각 listen server/client world에서 Hugh 또는 Diana 로컬 플레이어 Pawn이 `RegionTrigger` 안에 있을 때만 `BackgroundMusic`을 재생하고, Trigger 밖에서는 재생하지 않는다.

## Runtime Flow

1. `RegionTrigger`는 Pawn 채널만 Overlap한다.
2. Overlap Actor가 `AHughCharacter` 또는 `ADianaOperatorPawn`이고 로컬 컨트롤 Pawn이면 `PlaybackDelay` 뒤 `PlayRegionMusic()`을 호출한다.
3. BeginPlay 시점에 이미 Trigger 안에 있으면 기존 overlap 검사로 재생한다.
4. Pawn possession이 늦어 기존 overlap 검사를 놓친 client는 짧은 retry로 다시 검사한다.
5. EndOverlap에서 `ExitRecheckDelay`만큼 짧게 기다린 뒤 더 이상 로컬 Pawn이 Trigger 안에 없고 `bStopOnExit`가 켜져 있으면 `StopRegionMusic()`으로 페이드 아웃한다.

## Runtime Safeguards

- `BeginPlay`에서 `RegionTrigger` collision을 QueryOnly, Pawn Overlap으로 다시 맞춘다.
- 기존 BP 인스턴스에 `MusicAudioComponent` `Auto Activate`가 켜져 있거나 이미 재생 중이어도 `ResetMusicComponentForTriggerPlayback()`이 먼저 끈다.
- `PlayRegionMusic()`은 재생 직전에 `VolumeMultiplier`를 1로 되돌린다. 컴포넌트 기본 볼륨이 0이면 `FadeIn`을 호출해도 최종 출력이 음소거될 수 있다.
- `IsLocalPawnInsideRegion()`은 Hugh/Diana 로컬 플레이어 Pawn 검사만 담당하고 재생을 직접 시작하지 않는다. 재생 시작은 `PlayMusicForExistingLocalOverlap()`과 BeginOverlap 경로에서만 수행한다.
- `ScheduleStopIfOutsideRegion()`은 EndOverlap 직후 바로 정지하지 않고 `ExitRecheckDelay` 후 재검사한다. 캐릭터 collision 구성이나 순간적인 overlap 흔들림 때문에 BGM이 처음부터 반복 재생되는 것을 막기 위해서다.
- `PlaybackDelay`가 0보다 크면 BeginOverlap과 BeginPlay 기존 overlap 경로는 timer를 하나만 예약한다. delay가 끝날 때도 로컬 Pawn이 Region 안에 있을 때만 시작하며, 떠나면 예약을 취소한다.
- Blueprint가 직접 호출하는 `PlayRegionMusic()`은 연출용 즉시 재생 API이므로 `PlaybackDelay`를 적용하지 않는다.

## Blueprint Boundary

- Blueprint에서는 `RegionTrigger` 크기, `BackgroundMusic`, Fade 시간, 볼륨, `PlaybackDelay`를 조정한다.
- BGM은 기본적으로 지역 안에서만 켜지는 2D 음악으로 다룬다. 거리 감쇠가 필요한 경우 SoundWave 또는 SoundCue의 attenuation/spatial 설정을 별도로 맞춘다.
- 여러 BGM 영역이 겹치는 우선순위 처리는 이 클래스가 담당하지 않는다.

## Notes

- 현재 게임 구조는 두 플레이어가 항상 같은 지역에 있다는 전제라서 전역 BGM 매니저를 두지 않는다.
- listen server와 client는 각자 자기 Hugh/Diana 로컬 Pawn overlap만 처리하도록 `IsLocalControlledPragmataPlayer()`를 사용한다. AI 몬스터 Pawn은 BGM 시작 조건에서 제외한다.
