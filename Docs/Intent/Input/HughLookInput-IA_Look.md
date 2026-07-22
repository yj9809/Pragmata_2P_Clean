# Hugh IA_Look Axis2D 의도 카드

## 대상

`/Game/Blueprints/Player/Input/Common/IA_Look`, `IMC_Hugh`, `IMC_Diana`, `AHughCharacter::OnTriggerLook`, `ADianaOperatorPawn::OnTriggerLook`

## 종류

Enhanced Input look axis contract

## 정의 위치

- `Content/Blueprints/Player/Input/Common/IA_Look.uasset`
- `Content/Blueprints/Player/Input/Hugh/IMC_Hugh.uasset`
- `Content/Blueprints/Player/Input/Diana/IMC_Diana.uasset`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.h`
- `Source/Pragmata_2P/Pawns/DianaOperatorPawn.cpp`

## 이 코드가 존재하는 이유

mouse/gamepad 2D look 입력을 `FVector2D`로 읽는 C++ 경로와 InputAction asset value type을 맞추기 위해 존재한다. DualSense가 GameInput backend로 들어올 때 right stick idle 축 값이 작게 남을 수 있으므로, gamepad right-stick mapping에는 명시적 deadzone이 필요하다.

## 사용해야 하는 상황

- Hugh camera yaw/pitch를 mouse/gamepad 2D axis로 처리할 때
- Diana operator camera look도 같은 common look action을 사용할 때
- `FInputActionValue::Get<FVector2D>()`로 값을 읽는 경우
- DualSense 또는 다른 gamepad의 idle drift를 look input mapping에서 막아야 할 때

## 사용하면 안 되는 상황

- Boolean trigger만 필요한 button input
- 1D axis input으로만 설계된 action
- `IA_Look` asset value type을 바꾼 뒤 C++ value read type을 그대로 두는 경우

## 기본 흐름

`IMC_Hugh` 또는 `IMC_Diana` input mapping -> mouse는 `Mouse2D` 그대로, gamepad는 `Gamepad_Right2D`에 Axial DeadZone 0.30 적용 -> `IA_Look` Axis2D value -> `OnTriggerLook` -> `FVector2D` 읽기 -> yaw/pitch 또는 Diana camera rotation intent 반영

## 이 코드를 호출하는 쪽

- `IMC_Hugh`
- `IMC_Diana`
- `AHughCharacter::SetupPlayerInputComponent`
- `ADianaOperatorPawn::SetupPlayerInputComponent`

## 이 코드가 호출하는 쪽

- `FInputActionValue::Get<FVector2D>`
- controller yaw/pitch input
- Diana camera rotation update 경로

## 서버/클라이언트 조건

look input은 local player input이다. 서버 gameplay authority를 직접 바꾸지 않고, 각 character/pawn의 camera/control rotation intent로 사용된다.

## Blueprint 연결 여부

InputAction과 InputMappingContext asset이 직접 연결된다. asset value type은 Editor/UnreelMCP로 확인 필요.

## 헷갈리기 쉬운 코드

- `IA_Aim`: button/hold aim state 전환
- `IA_Scan`: Diana scan trigger
- `IA_TitleStart`: title screen start button
- `input.GlobalAxisConfigMode`: 전역 축 처리라 prompt/input 전체에 영향을 줄 수 있으므로 DualSense look drift 해결에는 쓰지 않는다

## 관련 문서

- `Docs/KNOW_HOW.md`

## 관련 커밋

미확인. asset 변경 이력은 binary라 `git log -- Content/Blueprints/Player/Input/Common/IA_Look.uasset` 확인 필요.

## 확실한 근거

- `.omx/context/fix-hugh-look-input-20260609T125404Z.md`에 `IA_Look` Boolean value type과 `FVector2D` read mismatch 기록
- `HughCharacter.cpp`와 `DianaOperatorPawn.cpp`가 look input을 2D vector로 처리
- `IMC_Hugh`의 `Gamepad_Right2D` look mapping과 `IMC_Diana`의 `Gamepad_Right2D` look mapping만 `InputModifierDeadZone`을 가진다

## 추정한 의도

작은 mouse movement가 Boolean처럼 해석되어 camera가 과도하게 도는 문제를 막으려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

UnreelMCP enum export text에서 `InputModifierDeadZone.Type=Axial`이 빈 값으로 보일 수 있다. `DeadZoneType.AXIAL`은 enum 첫 값이므로 변경 응답과 Editor details panel을 함께 확인하면 더 확실하다.

## 검증 방법

```powershell
rg -n "IA_Look|OnTriggerLook|FVector2D" Source Docs
```

UnreelMCP `input.mapping.list`로 `IMC_Hugh`의 `Gamepad_Right2D` look mapping, `IMC_Diana`의 `Gamepad_Right2D` look mapping에만 `InputModifierDeadZone`이 있는지 확인한다. PIE에서는 DualSense right stick untouched 상태에서 view drift가 없는지, mouse look이 deadzone 없이 즉시 반응하는지 확인한다.

## 다음 수정 시 주의점

InputAction value type과 C++ `FInputActionValue::Get<T>` 타입을 따로 바꾸지 않는다. `Mouse2D` mapping에는 deadzone을 추가하지 않는다.
