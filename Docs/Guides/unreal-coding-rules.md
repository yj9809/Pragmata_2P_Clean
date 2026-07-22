# Unreal 코딩 규칙

## Class 이름

- C++ class, Blueprint class, asset, generated type 생성 시 인위적인 class prefix 이름 금지
- 예: `DVCharacter` 금지
- 올바른 이름: `Character`
- 캐릭터, actor, component, system 이름이 분명하면 해당 이름 직접 사용
- 예: `HughCharacter`, `DianaOperatorPawn`

## GAS

- 본 프로젝트는 GAS 사용 안 함
- Gameplay Ability System 전제 금지
- Lyra, GameFeature, Experience 전제 금지
