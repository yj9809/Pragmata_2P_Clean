# PragmataPlayerController input device state

## 대상 symbol

- `APragmataPlayerController::InputKey`
- `APragmataPlayerController::UpdateInputDeviceFromKeyEvent`
- `APragmataPlayerController::IsGamepadInput`
- `APragmataPlayerController::bIsGamepadInput`

## 의도

UI가 마지막 로컬 입력 장치에 맞춰 keyboard/mouse prompt와 gamepad prompt를 고를 수 있게 한다.

## 확실한 근거

- `bIsGamepadInput`은 `BlueprintReadOnly`라 Widget Blueprint에서 변수로 읽을 수 있다.
- `IsGamepadInput()`은 `BlueprintPure`라 Blueprint와 C++ 양쪽 getter로 사용할 수 있다.
- 값은 owning local `APragmataPlayerController::InputKey`에서 갱신한다.
- touch, simulated input, release event, 작은 analog drift는 장치 전환 신호로 쓰지 않는다.
- Windows DualSense 경로는 deprecated `RawInput`이 아니라 `GameInputWindows` 플러그인을 사용한다.

## UI 사용법

- Blueprint: `Get Owning Player` -> `Cast To PragmataPlayerController` -> `bIsGamepadInput` 읽기
- Getter: 같은 controller에서 `IsGamepadInput()` 호출
- true면 gamepad icon set, false면 keyboard/mouse icon set을 표시한다.

## 경계

- 이 값은 local UI prompt용 상태이며 replicated gameplay state가 아니다.
- device-specific glyph 종류, 예를 들어 Xbox 버튼과 PlayStation 버튼 구분은 이 card의 범위가 아니다.
- GameInput에서 Unreal gamepad key event가 올라와야 이 값이 true로 바뀐다.

## 주의

- UE 5.8에서 `RawInput`은 deprecated라 DualSense 기본 경로로 쓰지 않는다.
- `Config/Windows/WindowsInput.ini`는 GameInput을 HID family 중심으로 켜서 XInput 중복 입력 위험을 줄인다.
- 실제 DualSense USB/Bluetooth 연결은 hardware-dependent라 에디터에서 한 번 수동 확인해야 한다.
