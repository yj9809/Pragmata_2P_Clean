# AItemBoxInteractable interaction 의도 카드

## 대상

`AItemBoxInteractable::Interact_Implementation`, `ShouldCreateInteractionPromptWidget`, `IsDianaInteractor`

## 종류

Diana-only item box interaction boundary

## 정의 위치

- `Source/Pragmata_2P/Props/ItemBoxInteractable.h`
- `Source/Pragmata_2P/Props/ItemBoxInteractable.cpp`

## 이 코드가 존재하는 이유

ItemBox는 POI scan에는 표시되지만 실제 open은 Diana 전용 상호작용 입력으로만 실행되어야 한다.
Hugh 화면의 공통 prop prompt나 조준 focus가 ItemBox open으로 이어지지 않게 UI 노출과 서버 interaction을 같은 role 기준으로 막는다.

## 사용해야 하는 상황

- Diana가 `IA_Interact`로 ItemBox를 여는 경우
- opened ItemBox를 scan/interaction 후보에서 제외해야 하는 경우
- ItemBox prop에 붙은 prompt가 Diana에게만 보여야 하는 경우

## 사용하면 안 되는 상황

- Hugh 일반 prop interaction
- scan marker 표시만으로 item 지급 또는 open 처리
- Blueprint별 visibility hack으로 Hugh prompt만 숨기는 처리

## 기본 흐름

`IA_Interact` -> `UDianaNodeInteractionComponent::RequestNodeInteraction` -> server target 재판정 -> `AItemBoxInteractable::Interact_Implementation` -> Diana role 검증 -> `bIsOpened` 확정 -> `AAnimatedPropInteraction` animation/replication

`BP_ItemBox.AnimatedMesh`는 `BlockAll`, `QueryAndPhysics`, `Visibility=Block`을 유지한다. 같은 collision surface가 player 이동 차단과 Diana camera trace interaction을 함께 담당한다.

## 서버/클라이언트 조건

prompt는 local player의 `AssignedRole == Diana`일 때만 허용한다.
실제 open은 서버에서 `Interactor`의 controller/player state가 Diana role인지 다시 확인한다.
`ShouldShowInteractionPromptOnDianaFocus()`가 true인 ItemBox 계열만 local Diana focus 전환에서 prop에 붙은 `WBP_HoldKeyPrompt` `WidgetComponent`를 표시한다. focus가 바뀌거나 aim gate가 풀리면 이전 prompt를 즉시 숨긴다.
`APropInteractionBase`는 viewport widget을 새로 만들지 않고, `UHoldKeyPromptWidget` 계열 `WidgetComponent`의 `KeyBox` texture만 교체한다. `BP_ItemBox`, `BP_DeadFilament_Box`는 `InteractionPromptKeyTexture`에 `/Game/UI/Texture/Input/LeftClick`을 지정한다.

## 헷갈리기 쉬운 코드

- `AAnimatedPropInteraction`: animation과 공통 prompt 생성 기반
- `UDianaScanComponent`: ItemBox POI marker 표시만 담당
- `UDianaNodeInteractionComponent`: Diana POI interaction 입력과 server target 재판정 담당

## 관련 문서

- `Docs/KNOW_HOW.md` - `Item Box Interactable`, `Diana Scan, Hack, and Node Interaction`
- `Docs/Intent/Hacking/DianaNodeInteractionComponent-RequestNodeInteraction.md`

## 수정 시 주의점

ItemBox 전용 animation field를 다시 만들지 않는다.
`AnimatedMesh`를 `NoCollision`로 바꾸거나 `Visibility`를 Ignore하지 않는다. player가 통과하거나 Diana focus가 잡히지 않는다.
Hugh 화면 prompt 문제는 Widget Blueprint 개별 설정이 아니라 `ShouldCreateInteractionPromptWidget()` role gate에서 해결한다.
ItemBox 계열 BP에는 `WBP_HoldKeyPrompt`를 쓰는 별도 `WidgetComponent`를 유지한다. `ScanIconComponent`는 scan marker 전용이므로 key prompt 용도로 바꾸지 않는다.
