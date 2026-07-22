# LobbyPresentationDirector FocusRole 의도 카드

## 대상

`ALobbyPresentationDirector::FocusRole`

`ALobbyPresentationDirector::SetPreviewSkin`

## 종류

MatchLobby character focus camera와 preview animation 지휘

## 정의 위치

- `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.h`
- `Source/Pragmata_2P/Lobby/LobbyPresentationDirector.cpp`
- `Source/Pragmata_2P/Data/LobbyPreviewAnimationSet.h`
- `Source/Pragmata_2P/Data/LobbyPreviewAnimationSet.cpp`
- `Source/Pragmata_2P/Data/LobbyCharacterSkinSet.h`
- `Source/Pragmata_2P/Data/LobbyCharacterSkinSet.cpp`

## 이 코드가 존재하는 이유

로비 선택 상태를 ring, 1P/2P label, fixed billboard로 표현하지 않고 camera movement와 character animation으로 표현하기 위해 존재한다.

## 사용해야 하는 상황

- local player가 Hugh/Diana 선택 focus를 좌우로 바꿀 때
- MatchLobby 진입 후 현재 선택 role에 맞춰 camera pose를 초기화할 때
- level에 배치한 skeletal mesh actor가 focus animation을 재생해야 할 때
- replicated/predicted lobby skin id에 맞춰 preview mesh를 바꿔야 할 때

## 사용하면 안 되는 상황

- gameplay pawn camera를 제어할 때
- player role 확정이나 possession 기준으로 사용할 때
- lobby replicated state의 source of truth로 사용할 때

## 기본 흐름

local displayed role/skin 변경 -> player controller가 director 검색 -> `SetPreviewSkin`으로 role별 mesh 적용 -> `FocusRole` 호출 -> orbit camera 보간 -> Hugh/Diana start animation 재생 -> start 길이 이후 loop animation 전환

실제 skin mesh 교체는 Single Node animation state를 초기화할 수 있다. `SetPreviewSkin`은 실제 mesh 교체를 animation dirty 상태로 기록하고, 모든 role별 skin 갱신 뒤 호출되는 `FocusRole`이 현재 target sequence를 두 preview에 한 번 적용한다. 같은 mesh 또는 같은 skin id 갱신은 animation을 재시작하지 않는다.

## 이 코드를 호출하는 쪽

- `APragmataLobbyPlayerController::SetupLobbyCameraView`
- `APragmataLobbyPlayerController::AdvanceLocalDisplayedRole`
- `APragmataLobbyPlayerController::AdvanceLocalDisplayedSkin`
- `APragmataLobbyPlayerController::HandleLobbyStateChanged`

## 이 코드가 호출하는 쪽

- `USkeletalMeshComponent` single-node animation API
- `ULobbyPreviewAnimationSet`
- `ULobbyCharacterSkinSet`
- fallback soft animation path

## 서버/클라이언트 조건

local presentation 전용이다. 선택 권한과 ready/start 검증은 서버 `APragmataLobbyGameMode`와 replicated `APragmataPlayerState`가 계속 담당한다.

## Blueprint 연결 여부

director actor는 MatchLobby에 배치하는 것이 최종 workflow다. 아직 배치되지 않았으면 local player controller가 runtime fallback director를 생성한다. character mesh는 `LobbyPreview.Hugh`, `LobbyPreview.Diana` tag를 가진 `SkeletalMeshActor`만 사용한다.

skin mesh는 `DA_LobbyCharacterSkinSet`이 있으면 DataAsset을, 없으면 C++ fallback 정의를 사용한다. 교체할 mesh가 로드되지 않으면 기존 mesh를 유지한다.

camera framing은 오른쪽 UI 공간을 만들기 위해 `ScreenRightOffset`을 크게 올리는 방식이 아니다. Hugh/Diana preview가 왼쪽 화면 밖으로 밀리지 않도록 radius/FOV를 넓히고 offset은 낮게 유지한다. 예전 map instance에 큰 offset이 저장되어 있으면 `BeginPlay`에서 새 framing 값으로 보정한다.

## 헷갈리기 쉬운 코드

- `APragmataLobbyGameMode`: lobby PlayerState와 start travel 담당, preview actor spawn은 하지 않음
- `APragmataLobbyPlayerController::SetupLobbyCameraView`: director가 없으면 runtime fallback director를 생성
- `UPragmataRoleLobbyWidget`: focus 설명 UI를 담당하지만 camera를 움직이지 않음

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Architecture/architecture.md`
- `Docs/Intent/Lobby/PragmataLobbyGameMode-StartingGame.md`

## 확실한 근거

- `FocusRole`은 Hugh/Diana가 아닌 role을 무시한다.
- `ResolveTargetAngle`은 Hugh -> Diana를 clockwise, Diana -> Hugh를 counter-clockwise로 보정한다.
- animation set이 없으면 사용자가 지정한 Reinforce focus animation soft path를 fallback으로 사용한다.
- `SetPreviewSkin`은 skin mesh load 성공 후에만 `SetSkeletalMesh`를 호출한다.
- `SetPreviewSkin`은 실제 mesh 교체 뒤에만 animation dirty 상태를 기록한다.
- `FocusRole`은 같은 stable role 또는 같은 transition target으로 조기 반환하기 전에 dirty 상태인 두 preview의 start/loop sequence를 함께 복구한다.

## 추정한 의도

Title.umap처럼 단순 배치 skeletal mesh를 로비 presentation의 중심으로 쓰고, 기존 preview actor는 개발 fallback으로만 남기려는 의도.

## 확신도

강함

## 아직 모르는 점

최종 camera radius, height, FOV, `ScreenRightOffset` 값은 MatchLobby 실제 composition을 보며 조정해야 한다. 단, `ScreenRightOffset`을 크게 올리면 character가 왼쪽 밖으로 밀리므로 먼저 radius/FOV와 WBP right panel 배치를 조정한다.

## 검증 방법

```powershell
rg -n "ALobbyPresentationDirector|FocusRole|SetPreviewSkin|LobbyPreview\\.Hugh|LobbyPreview\\.Diana" Source Docs
```

## 다음 수정 시 주의점

camera focus가 selection affordance이므로 ring과 1P/2P label을 다시 켜지 말 것.
