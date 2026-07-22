# AHughCharacter::EnsureHeadVisorSpotlightAttachment 의도 카드

## 대상

`AHughCharacter::EnsureHeadVisorSpotlightAttachment`, `OnToggleHeadVisorSpotlight`, `SetHeadVisorSpotlightEnabled`, `OnRep_HeadVisorSpotlightEnabled`

## 종류

Hugh head visor replicated light/attachment boundary

## 정의 위치

- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`

## 이 코드가 존재하는 이유

Hugh head visor light를 mesh socket에 붙여 모든 peer에서 같은 위치와 on/off 상태로 보이게 하고, socket 저장 누락을 root fallback과 warning으로 드러내기 위해 존재한다.

## 사용해야 하는 상황

- Hugh spawn 이후 `HeadVisor_LightSocket` 기준으로 spotlight component를 재부착할 때
- owner input으로 visor light를 켜고 끌 때
- replicated bool 변경을 remote peer의 light visibility에 반영할 때

## 사용하면 안 되는 상황

- map light나 puzzle power light를 제어하려는 경우
- socket 누락을 silent fallback으로 숨기려는 경우
- client가 server 확정 없이 light 상태를 최종 상태로 결정하려는 경우

## 기본 흐름

`IA_HeadVisorLightToggle` -> `OnToggleHeadVisorSpotlight` -> local 적용 -> `Server_SetHeadVisorSpotlightEnabled` -> replicated `bHeadVisorSpotlightEnabled` -> `OnRep_HeadVisorSpotlightEnabled` -> component visibility/activation 반영

## 이 코드를 호출하는 쪽

- `AHughCharacter::BeginPlay`
- `AHughCharacter::SetupPlayerInputComponent`
- owner Hugh input binding
- replication `OnRep_HeadVisorSpotlightEnabled`

## 이 코드가 호출하는 쪽

- `USpotLightComponent::AttachToComponent`
- `USpotLightComponent::SetVisibility`
- `USpotLightComponent::SetHiddenInGame`
- `USpotLightComponent::Activate`
- `USpotLightComponent::Deactivate`

## 서버/클라이언트 조건

owner는 입력 즉시 local 표시를 갱신하지만 server RPC로 상태를 확정한다. 최종 공유 상태는 `bHeadVisorSpotlightEnabled` replication을 통해 remote peer에 반영된다.

## Blueprint 연결 여부

`IA_HeadVisorLightToggle`과 `IMC_Hugh` mapping은 Blueprint asset이 소유한다. C++은 action asset을 load/bind하고 light component attachment/replication을 담당한다.

## 헷갈리기 쉬운 코드

- `APragmataGameState::OnPowerStateChanged`: map puzzle power light용 replicated event
- `ALightBase::HandlePowerStateChanged`: map light listener 역할
- `Content/Map/Props/Lights/BP_Light_*`: 현재 확인되는 light Blueprint asset 계열
- `IMC_Hugh`: key/gamepad mapping owner
- `IA_Look`: Axis2D shared look input, HeadVisor toggle과 value type이 다름

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Input/HughLookInput-IA_Look.md`

## 관련 커밋

- HeadVisor spotlight 관련 session 기록: `codex-260629-hugh-headvisor-spotlight`
- runtime yaw fix 관련 session 기록: `codex-260629-hugh-spotlight-runtime-yaw-fix`

## 확실한 근거

- `HughCharacter.cpp` constructor가 `HeadVisorSpotlight`를 만들고 `HeadVisor_LightSocket`에 attach
- `EnsureHeadVisorSpotlightAttachment`가 socket 없을 때 warning을 남기고 mesh root fallback으로 component를 유지
- socket attach 후 relative yaw `-90`을 다시 적용해 cone 방향을 맞춤
- `bHeadVisorSpotlightEnabled`가 `OnRep_HeadVisorSpotlightEnabled`로 replicated visibility를 적용
- `Docs/KNOW_HOW.md`가 HeadVisor light toggle input, socket, replication 검증 조건을 기록

## 추정한 의도

Blueprint socket 저장 상태가 흔들려도 기능 테스트가 완전히 막히지 않게 하되, 잘못된 socket 상태는 warning으로 즉시 드러내려는 의도.

## 확신도

확정

## 아직 모르는 점

`HeadVisor_LightSocket`의 최신 asset 위치와 packaged build에서 cone 방향이 의도와 맞는지는 Editor/PIE 또는 Blueprint scan으로 재확인 필요.
과거 session에 나온 `BP_Light_Demo` asset명은 현재 `Content/` 파일 목록에서 확인되지 않는다.

## 검증 방법

```powershell
rg -n "HeadVisorSpotlight|HeadVisor_LightSocket|IA_HeadVisorLightToggle" Source Docs
```

## 다음 수정 시 주의점

socket fallback을 성공 상태처럼 숨기지 않는다. `IMC_Hugh` mapping을 바꿀 때는 기존 Hugh input과 충돌하지 않는지 같이 확인한다.
