# APragmataTitlePlayerController::RequestStartFromTitle 의도 카드

## 대상

`APragmataTitlePlayerController::RequestStartFromTitle`, `TravelToLobby`, `IsTitleMapWorld`

## 종류

Title map start gate와 lobby travel 경계

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataTitlePlayerController.h`
- `Source/Pragmata_2P/Game/PragmataTitlePlayerController.cpp`
- `Source/Pragmata_2P/UI/PragmataTitleScreenWidget.cpp`

## 이 코드가 존재하는 이유

press-any-button 시작 흐름을 MatchLobby에서 분리해, title map만 title UI/input/fade/travel을 소유하게 하기 위해 존재한다.

## 사용해야 하는 상황

- `/Game/Map/Title`에서 첫 start 입력을 받아 MatchLobby로 넘어갈 때
- title fade-out과 travel timer를 중복 없이 시작할 때
- title input mapping context를 gameplay/lobby 진입 전에 제거할 때

## 사용하면 안 되는 상황

- MatchLobby 안에서 다시 press-any-button gate를 열 때
- gameplay map에서 title UI를 강제로 띄울 때
- session lobby host/join role 선택을 처리할 때

## 기본 흐름

Title map begin play -> title camera/widget/input setup -> start 입력 -> `RequestStartFromTitle` 중복 방지 -> fade-out -> timer -> title input 제거 -> `UPragmataGameFlowConfig`의 lobby map으로 `OpenLevel`

## 이 코드를 호출하는 쪽

- `UPragmataTitleScreenWidget::RequestTitleStart`
- `APragmataTitlePlayerController::HandleStartPressed`

## 이 코드가 호출하는 쪽

- `UPragmataGameFlowConfig::Get`
- `UGameplayStatics::OpenLevel`
- Enhanced Input mapping context load/remove

## 서버/클라이언트 조건

local title controller에서만 실행한다. dedicated server와 title map이 아닌 world에서는 title UI/travel gate를 열지 않는다.

## Blueprint 연결 여부

`RequestStartFromTitle`은 `BlueprintCallable`. `WBP_TitleScreen` 또는 title widget 계층에서 호출 가능하다.

## 헷갈리기 쉬운 코드

- `UPragmataMultiplayerSessionSubsystem`: host/join session 시작
- `APragmataLobbyGameMode::RequestStartGame`: lobby에서 gameplay map으로 server travel
- `UPragmataGameFlowConfig`: title/lobby/game map path 원본

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Architecture/architecture.md`
- `Docs/Intent/GameFlow/PragmataGameFlowConfig.md`

## 관련 커밋

- `9168f679 docs: title screen 흐름 문서화`
- `0d4981cf feat: title screen 진입 흐름 추가`
- `9754a074 fix: DirectMap GameFlow 제거`

## 확실한 근거

- `IsTitleMapWorld`가 title map에서만 title UI와 travel을 활성화
- `RequestStartFromTitle`이 `bTransitionStarted`로 중복 start를 차단
- `TravelToLobby`가 `UPragmataGameFlowConfig`의 lobby map path를 사용

## 추정한 의도

Title, MatchLobby, Demo가 각각 자기 화면 책임만 갖게 해서 lobby role selection과 title start gate가 서로 섞이지 않게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

packaged build에서 title start input asset이 PAL에 누락되지 않는지는 packaging 검증이 필요.

## 검증 방법

```powershell
rg -n "RequestStartFromTitle|TravelToLobby|IsTitleMapWorld|TitleInputMappingContext" Source Docs
```

## 다음 수정 시 주의점

MatchLobby에 press-any-button 흐름을 되돌리면 title 전용 entrypoint 의도가 깨진다.
