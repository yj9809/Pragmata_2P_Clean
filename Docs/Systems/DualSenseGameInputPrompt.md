# DualSense GameInput prompt state

## 결론

DualSense 입력 감지는 `RawInput` VID/PID 등록이 아니라 `GameInputWindows` 입력 backend와 `APragmataPlayerController`의 마지막 입력 장치 상태 분리로 해결.

UI는 controller 종류를 직접 감지하지 않고 `bIsGamepadInput` 또는 `IsGamepadInput()`만 읽음.

## 가능해진 이유

### Unreal 입력 backend 교체

- UE 5.8의 Windows 기본 게임패드 경로는 XInput 중심
- DualSense는 일반 XInput 장치가 아니므로 기본 경로만으로는 안정적이지 않음
- UE 5.8에 포함된 `GameInputWindows`는 Microsoft GameInput API를 통해 Windows가 노출하는 gamepad/HID family 입력을 Unreal input event로 전달
- GameInput event는 Unreal의 `FInputKeyEventArgs`와 `FInputDeviceScope` 경로를 타므로 `Params.IsGamepad()` 판정과 잘 맞음

### RawInput을 쓰지 않은 이유

- UE 5.8에서 `RawInput`은 deprecated
- RawInput은 VID/PID, HID usage, button/axis 배열을 프로젝트가 직접 맞춰야 함
- DualSense가 Windows에서 어떤 HID collection으로 잡히는지 환경에 따라 흔들릴 수 있음
- UI prompt 전환만 필요한 작업에는 너무 낮은 계층

### UI 상태와 입력 backend 분리

- 입력 backend는 DualSense, Xbox controller, 기타 HID gamepad를 Unreal key event로 올리는 역할
- UI prompt state는 마지막 입력이 gamepad인지 keyboard/mouse인지 저장하는 역할
- 두 책임을 분리해 UI Blueprint가 backend 세부사항에 의존하지 않음

## 구현 구조

### GameInput 활성화

파일:

- `Pragmata_2P.uproject`
- `Config/Windows/WindowsInput.ini`

역할:

- `GameInputWindows` 플러그인 활성화
- Windows target에서 GameInput gamepad 처리 활성화
- keyboard/mouse는 기존 WindowsApplication 경로 유지
- Raw input report 처리는 끔
- HID family 중심으로 제한해 XInput 중복 입력 위험 감소

핵심 설정:

```ini
[GameInputPlatformSettings_Windows GameInputPlatformSettings]
bProcessGamepad=True
bProcessController=False
bProcessRawInput=False
bProcessKeyboard=False
bProcessMouse=False

[/Script/GameInputBase.GameInputDeveloperSettings]
SupportedDeviceFamilies=16
```

`SupportedDeviceFamilies=16`은 `EGameInputDeviceFamily::Hid`.

## 런타임 흐름

```text
DualSense input
-> Windows GameInput API
-> GameInputWindows plugin
-> Unreal gamepad key event
-> APragmataPlayerController::InputKey
-> FInputKeyEventArgs::IsGamepad()
-> bIsGamepadInput 갱신
-> UI Blueprint prompt set 전환
```

## PlayerController 상태

파일:

- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`

노출 API:

- `UPROPERTY(BlueprintReadOnly) bool bIsGamepadInput`
- `UFUNCTION(BlueprintPure) bool IsGamepadInput() const`

동작:

- `InputKey(const FInputKeyEventArgs& Params)` override
- `Super::InputKey(Params)` 호출 후 local prompt state 갱신
- local controller가 아닌 입력 무시
- touch 입력 무시
- simulated input 무시
- release event 무시
- 작은 analog drift 무시
- 남은 입력에서 `Params.IsGamepad()` 결과 저장

## drift 필터

gamepad stick은 중립이어도 작은 analog 값이 들어올 수 있음.

필터 기준:

- gamepad analog: `0.2f` 미만 무시
- pointer analog: `0.01f` 미만 무시
- digital key/button: 즉시 의미 있는 입력으로 처리

목적:

- 손을 대지 않은 gamepad stick drift가 keyboard/mouse prompt를 덮지 않게 방지
- mouse의 작은 실제 이동은 keyboard/mouse 전환으로 인정

## UI 사용법

Widget Blueprint:

1. `Get Owning Player`
2. `Cast To PragmataPlayerController`
3. `bIsGamepadInput` 변수 읽기
4. true: gamepad icon set
5. false: keyboard/mouse icon set

C++ 또는 Blueprint getter:

```cpp
const bool bUseGamepadPrompts = PragmataPlayerController->IsGamepadInput();
```

## 범위 밖

이 구현이 하지 않는 일:

- PlayStation glyph와 Xbox glyph 구분
- DualSense adaptive trigger, haptic, speaker, touchpad feature 처리
- Enhanced Input mapping asset 자동 변경
- UI icon asset 선택 로직 구현
- replicated gameplay state 처리

`bIsGamepadInput`은 local UI prompt 상태이며 network gameplay 판정에 쓰지 않음.

## 검증

확인된 결과:

- `Pragmata_2PEditor Win64 Development` 빌드 성공
- DualSense 실제 입력으로 gamepad prompt 전환 확인
- RawInput 없이 GameInputWindows 경로로 동작 확인

수동 확인 절차:

1. DualSense 연결
2. PIE 실행
3. keyboard/mouse 입력
4. UI가 keyboard/mouse prompt로 전환되는지 확인
5. DualSense button 또는 stick 입력
6. UI가 gamepad prompt로 전환되는지 확인
7. gamepad를 놓은 상태에서 stick drift만으로 prompt가 흔들리지 않는지 확인

## 문제 해결

DualSense가 반응하지 않을 때:

- `GameInputWindows` 플러그인 활성화 확인
- `Config/Windows/WindowsInput.ini` 적용 확인
- Steam Input, DS4Windows가 DualSense를 XInput으로 변환 중인지 확인
- PIE 재시작 또는 Editor 재시작
- `InputKey` breakpoint에서 `Params.Key`, `Params.IsGamepad()`, `Params.InputDevice` 확인

gamepad 입력이 두 번 들어올 때:

- XInput과 GameInput 중복 가능성 확인
- 현재 설정이 HID family 중심인지 확인
- Xbox controller까지 GameInput으로 통합할지, XInput은 기존 경로로 둘지 결정 필요

PlayStation glyph가 필요할 때:

- `bIsGamepadInput`만으로는 부족
- 별도 `EPragmataInputGlyphFamily` 같은 local UI 상태 필요
- GameInput hardware identifier 또는 platform input device metadata 기반으로 확장
