# 의도 카드

짧은 symbol 단위 개발 의도 문서 모음.

## 읽는 순서

- Damage 혼동 확인: `Damage/ReceiveDamage.md`, `Damage/DealDamageToActor.md`, `Damage/FPragmataDamageSpec.md`
- enemy hack 흐름 확인: `Hacking/DianaHackComponent-RequestHack.md`에서 시작
- map hack 흐름 확인: `Hacking/DianaNodeInteractionComponent-RequestNodeInteraction.md`에서 시작
- Diana POI scan 확인: `Scan/DianaScanComponent-RequestScan.md`, `Scan/PragmataScannable-POI.md`
- HUD 복제 신호 확인: `HUD/PragmataGameState-ActiveEnemyHackTarget.md`, `HUD/PragmataGameState-ActiveMapPuzzleActor.md`
- objective alert 표시 확인: `HUD/ObjectiveAlertWidget.md`
- enemy hack countdown 표시 확인: `HUD/EnemyHackingWidget-Countdown.md`
- WeaponWheel 루트 fallback 확인: `HUD/PragmataHUDWidget-ResolveWeaponWheelRoot.md`
- UI input prompt 장치 상태 확인: `Input/PragmataPlayerController-InputDeviceState.md`
- WaistArm 구조 확인: `Movement/WaistArmComponent-EnsureWaistArmMeshAttachment.md`
- Hugh movement saved move 의도 확인: `Movement/PragmataHughMovementComponent-SavedMoveIntent.md`
- HeadVisor light 확인: `Lighting/HughCharacter-HeadVisorSpotlight.md`
- GameFlow/cook 확인: `GameFlow/PragmataGameFlowConfig.md`
- MatchLobby camera/preview 확인: `Lobby/LobbyPresentationDirector-FocusRole.md`
- lobby start spinner 확인: `Lobby/PragmataLobbyGameMode-StartingGame.md`
- 로컬 세션에서 복원한 전체 의도 목록 확인: `SessionRestored/README.md`

## 규칙

- 사실, 추정, 미확인 분리
- 유사 API 기록
- 관련 graph와 문서 link 유지
- Blueprint 연결 여부가 확정되지 않으면 `미확인` 또는 scan 필요로 표시

## 현재 카드 목록

### Animation

- `Animation/DianaAnimInstance-UpdateIKTargets.md`
- `Animation/PairedAnimationComponent-ForcedDianaInterrupt.md`

### Damage

- `Damage/ReceiveDamage.md`
- `Damage/DealDamageToActor.md`
- `Damage/FPragmataDamageSpec.md`
- `Damage/WeaponBase-DealDamageToHitActor.md`
- `Damage/PragmataHealthComponent-ReceiveDamage.md`
- `Damage/HughCharacter-ReceiveDamage.md`
- `Damage/PlayableCharacter-ReceiveDamage.md`

### Enemy

- `Enemy/MonsterBase-AttackPatternSelection.md`
- `Enemy/MonsterBase-AttackPatternDamage.md`
- `Enemy/MonsterBase-CommonAttackCooldown.md`
- `Enemy/MonsterBase-AttackMontageFlyingMovement.md`
- `Enemy/MonsterBase-ResolveDeathAnimation.md`
- `Enemy/MonsterBase-SetTerritoryTarget.md`
- `Enemy/MonsterTerritoryActor-Activation.md`

### GameFlow

- `GameFlow/PragmataGameFlowConfig.md`
- `GameFlow/GameClearPresentation.md`
- `GameFlow/PragmataSessionUtils.md`
- `GameFlow/PragmataTitlePlayerController-RequestStartFromTitle.md`

### Hacking

- `Hacking/DianaHackComponent-RequestHack.md`
- `Hacking/DianaHackComponent-RequestHackDirection.md`
- `Hacking/DianaHackComponent-ServerRequestHack.md`
- `Hacking/DianaHackComponent-ServerProcessEnemyHackDirection.md`
- `Hacking/EnemyHackingPuzzleComponent-StartHack.md`
- `Hacking/EnemyHackingPuzzleComponent-ProcessDirectionInput.md`
- `Hacking/EnemyHackingPuzzleComponent-CompleteHack.md`
- `Hacking/EnemyHackingPuzzleComponent-DecodeNode.md`
- `Hacking/DianaNodeInteractionComponent-RequestNodeInteraction.md`
- `Hacking/DianaNodeInteractionComponent-RequestMapHackDirection.md`
- `Hacking/DianaNodeInteractionComponent-StartMapHack.md`
- `Hacking/MapHackingPuzzleComponent-StartHack.md`
- `Hacking/MapHackingPuzzleComponent-ProcessDirectionInput.md`
- `Hacking/MapHackingPuzzleComponent-CompleteHack.md`

### HUD

- `HUD/PragmataGameState-ActiveEnemyHackTarget.md`
- `HUD/PragmataGameState-ActiveMapPuzzleActor.md`
- `HUD/PragmataGameState-ActiveBossTarget.md`
- `HUD/PragmataPlayerController-EnsureGameplayHUD.md`
- `HUD/BossHPViewModel.md`
- `HUD/BossHPWidget.md`
- `HUD/ObjectiveAlertWidget.md`
- `HUD/DialogueWidget.md`
- `HUD/EnemyHackingWidget-Countdown.md`
- `HUD/PragmataHUDWidget-HandleActiveEnemyHackTargetChanged.md`
- `HUD/PragmataHUDWidget-HandleActiveMapPuzzleActorChanged.md`
- `HUD/PragmataHUDWidget-HandleActiveBossTargetChanged.md`
- `HUD/PragmataHUDWidget-ResolveWeaponWheelRoot.md`

### Input

- `Input/HughLookInput-IA_Look.md`
- `Input/PragmataPlayerController-InputDeviceState.md`

### Lobby

- `Lobby/LobbyPresentationDirector-FocusRole.md`
- `Lobby/PragmataLobbyGameMode-StartingGame.md`
- `Lobby/PragmataRoleLobbyWidget-RefreshLobbyView.md`

### Scan

- `Scan/DianaScanComponent-RequestScan.md`
- `Scan/PragmataScannable-POI.md`

### Lighting

- `Lighting/HughCharacter-HeadVisorSpotlight.md`

### Movement

- `Movement/WaistArmComponent-SetArmState.md`
- `Movement/WaistArmComponent-EnsureWaistArmMeshAttachment.md`
- `Movement/PragmataHughMovementComponent-SavedMoveIntent.md`

### SessionRestored

- `SessionRestored/README.md`

### Stats

- `Stats/PragmataPlayerStatsComponent.md`

### Weapon

- `Weapon/GripGun-PullTrigger.md`
- `Weapon/HughCharacter-NetworkWeaponActions.md`
