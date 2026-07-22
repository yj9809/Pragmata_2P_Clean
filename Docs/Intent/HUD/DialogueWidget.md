# DialogueWidget 의도 카드

## 대상

`FDialogueLine`

`UDialogueWidget`

`APragmataGameState::Multicast_PlayDialogueLine`

`APragmataPlayerController::Client_PlayDialogueLine`

`UPragmataHUDWidget::PlayDialogueLine`

## 종류

Co-op shared dialogue HUD 표시 경로

## 정의 위치

- `Source/Pragmata_2P/Data/PragmataTypes.h`
- `Source/Pragmata_2P/UI/DialogueWidget.h`
- `Source/Pragmata_2P/UI/DialogueWidget.cpp`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.h`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.cpp`
- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`
- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`

## 이 코드가 존재하는 이유

Hugh와 Diana의 역할별 root HUD에 같은 대사 UI를 붙이고, 게임 로직이 한 줄 단위로 양쪽 로컬 화면에 표시할 수 있게 하기 위해 존재한다.

## 사용해야 하는 상황

- 양쪽 player에게 같은 대사 한 줄을 보여야 할 때
- 특정 owning client에게만 대사를 보여야 할 때
- Level BP나 gameplay code가 대사 표시 타이밍을 직접 소유해야 할 때

## 사용하면 안 되는 상황

- UI가 자체적으로 다음 대사 줄을 선택해야 하는 경우
- 저장해야 하는 quest/objective state를 대사 위젯에 보관하는 경우
- root HUD 밖에 독립 dialogue viewport widget을 중복 생성하는 경우

## 기본 흐름

공통 표시:

```text
server gameplay logic
-> APragmataGameState::Multicast_PlayDialogueLine
-> 각 local APragmataPlayerController::PlayDialogueLine
-> UPragmataHUDWidget::PlayDialogueLine
-> WBP_DialogueWidget / UDialogueWidget::PlayLine
```

특정 client 표시:

```text
server gameplay logic
-> target APragmataPlayerController::Client_PlayDialogueLine
-> local HUD 경로
```

## 서버/클라이언트 조건

서버가 표시 요청 대상을 결정한다. `UDialogueWidget`은 로컬 화면 표시와 자연 종료 delegate만 담당하고, 대사 순서와 gameplay 진행 판정은 소유하지 않는다.

## Blueprint 연결 여부

`WBP_HughHUD`와 `WBP_DianaHUD` 안에 child 이름 `WBP_DialogueWidget`을 둔다. child class는 `UDialogueWidget` 기반이어야 하며, `SpeakerText`, `LineText`, `Root`는 optional binding이다.

## 헷갈리기 쉬운 코드

- `UObjectiveAlertWidget`: 목표 알림용 단발 HUD, 대사 순서 제어 아님
- `APragmataGameState::Multicast_ShowObjectiveAlert`: objective alert 공통 표시 경로
- `UPragmataHUDWidget::ResolveObjectiveAlertWidget`: child widget 이름 fallback 구조가 비슷한 HUD resolver

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/HUD/ObjectiveAlertWidget.md`
- `Docs/Intent/HUD/PragmataPlayerController-EnsureGameplayHUD.md`

## 검증 방법

```powershell
rg -n "Dialogue|PlayDialogueLine|WBP_DialogueWidget|FDialogueLine" Source Docs
```

## 다음 수정 시 주의점

- `WBP_DialogueWidget` child 이름을 바꾸면 `UPragmataHUDWidget::ResolveDialogueWidget()`도 같이 갱신한다.
- 대사 sequencing을 위젯 내부에 넣지 않는다. 위젯은 현재 줄 표시와 자연 종료 신호만 담당한다.
