#include "UI/PragmataUpgradeMenuWidget.h"

#include "Characters/HughCharacter.h"
#include "Components/PragmataHealthComponent.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/UpgradeItemRowWidget.h"

void UPragmataUpgradeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// F/C 키 입력을 받으려면 위젯이 포커스를 가질 수 있어야 함
	SetIsFocusable(true);

	// 강화 상태가 바뀔 때마다 홀드 버튼(F/C) 표시 규칙을 C++에서 직접 갱신
	OnUpgradeMenuChanged.AddDynamic(this, &UPragmataUpgradeMenuWidget::HandleMenuChangedForButtons);

	RefreshSharedUpgradeStats();
}

void UPragmataUpgradeMenuWidget::BindPlayerStatsComponent(UPragmataPlayerStatsComponent* PlayerStatsComponent)
{
	if (BoundPlayerStatsComponent.IsValid())
	{
		BoundPlayerStatsComponent->OnLunafilamentChanged.RemoveDynamic(this, &ThisClass::HandleLunafilamentChanged);
		BoundPlayerStatsComponent->OnDamageMultiplierChanged.RemoveDynamic(this, &ThisClass::HandleDamageMultiplierChanged);
		BoundPlayerStatsComponent->OnUpgradeLevelChanged.RemoveDynamic(this, &ThisClass::HandleUpgradeLevelChanged);
	}

	BoundPlayerStatsComponent = PlayerStatsComponent;

	if (BoundPlayerStatsComponent.IsValid())
	{
		BoundPlayerStatsComponent->OnLunafilamentChanged.AddDynamic(this, &ThisClass::HandleLunafilamentChanged);
		BoundPlayerStatsComponent->OnDamageMultiplierChanged.AddDynamic(this, &ThisClass::HandleDamageMultiplierChanged);
		BoundPlayerStatsComponent->OnUpgradeLevelChanged.AddDynamic(this, &ThisClass::HandleUpgradeLevelChanged);
	}

	BroadcastMenuChanged();
}

void UPragmataUpgradeMenuWidget::UnbindPlayerStatsComponent()
{
	if (BoundPlayerStatsComponent.IsValid())
	{
		BoundPlayerStatsComponent->OnLunafilamentChanged.RemoveDynamic(this, &ThisClass::HandleLunafilamentChanged);
		BoundPlayerStatsComponent->OnDamageMultiplierChanged.RemoveDynamic(this, &ThisClass::HandleDamageMultiplierChanged);
		BoundPlayerStatsComponent->OnUpgradeLevelChanged.RemoveDynamic(this, &ThisClass::HandleUpgradeLevelChanged);
	}

	BoundPlayerStatsComponent.Reset();
	BroadcastMenuChanged();
}

void UPragmataUpgradeMenuWidget::RefreshSharedUpgradeStats()
{
	// HUD와 독립 생성되므로 GameState의 Hugh shared stat source를 직접 구독
	APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	BindGameState(PragmataGameState);

	AHughCharacter* HughCharacter = ResolveSharedHughCharacter();
	BindPlayerStatsComponent(HughCharacter ? HughCharacter->GetPlayerStatsComponent() : nullptr);
}

void UPragmataUpgradeMenuWidget::SetSelectedUpgradeType(EPragmataUpgradeType UpgradeType)
{
	// Diana mirror는 replicated selection만 표시하고 local button 입력은 무시
	if (bReadOnly)
	{
		return;
	}
	ApplySelectedUpgradeType(UpgradeType);
	if (APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(GetOwningPlayer()))
	{
		PragmataController->RequestCheckpointUpgradeSelection(UpgradeType);
	}
}

void UPragmataUpgradeMenuWidget::ApplySelectedUpgradeType(EPragmataUpgradeType UpgradeType)
{
	if (UpgradeType != EPragmataUpgradeType::Suit && UpgradeType != EPragmataUpgradeType::Weapon)
	{
		return;
	}

	if (SelectedUpgradeType == UpgradeType)
	{
		return;
	}

	SelectedUpgradeType = UpgradeType;
	BroadcastMenuChanged();
}

void UPragmataUpgradeMenuWidget::SelectNextUpgrade()
{
	SetSelectedUpgradeType(GetAdjacentSelectableUpgrade(1));
}

void UPragmataUpgradeMenuWidget::SelectPreviousUpgrade()
{
	SetSelectedUpgradeType(GetAdjacentSelectableUpgrade(-1));
}

void UPragmataUpgradeMenuWidget::RequestSelectedUpgrade()
{
	// WBP hold 완료 입력은 controller RPC로 넘겨 서버가 비용과 레벨을 검증
	if (APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(GetOwningPlayer()))
	{
		PragmataController->RequestUpgrade(SelectedUpgradeType);
	}
}

void UPragmataUpgradeMenuWidget::RequestSelectedRefund()
{
	// 환급도 같은 선택 항목 기준으로 서버에 요청
	if (APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(GetOwningPlayer()))
	{
		PragmataController->RequestRefundUpgrade(SelectedUpgradeType);
	}
}

// 입력 어댑터가 호출하는 홀드 시작. 가드를 통과할 때만 게이지를 켠다
void UPragmataUpgradeMenuWidget::BeginHold(EPragmataUpgradeHoldSide Side)
{
	if (bHolding)
	{
		return;
	}

	const bool bRefund = (Side == EPragmataUpgradeHoldSide::Refund);
	UUpgradeItemRowWidget* ActiveRow = GetActiveRow();

	const EPragmataUpgradeType Type = GetSelectedUpgradeType();
	const bool bAllowed = bRefund ? CanRefundUpgrade(Type) : CanUpgrade(Type);
	if (!bAllowed)
	{
		// 불가한 조작은 게이지 대신 X 표시만
		if (ActiveRow)
		{
			ActiveRow->ShowHoldProhibited(bRefund);
		}
		return;
	}

	bHolding = true;
	bFired = false;
	HoldProgress = 0.f;
	HoldSide = Side;

	if (ActiveRow)
	{
		ActiveRow->BeginHold(bRefund);
	}
}

// 키를 뗐을 때. 진행 중 게이지와 불가 표시를 정리하고 홀드 상태를 리셋
void UPragmataUpgradeMenuWidget::EndHold(EPragmataUpgradeHoldSide Side)
{
	const bool bRefund = (Side == EPragmataUpgradeHoldSide::Refund);

	if (UUpgradeItemRowWidget* ActiveRow = GetActiveRow())
	{
		ActiveRow->CancelHold(bRefund);
	}

	if (bHolding && HoldSide == Side)
	{
		bHolding = false;
		bFired = false;
		HoldProgress = 0.f;
	}
}

// 오픈 트리거 확정 전, 이미 뷰포트에 올라온 메뉴에 포커스/입력모드를 걸어 F/C를 눌러볼 수 있게 함
void UPragmataUpgradeMenuWidget::OpenForTest()
{
	SetIsFocusable(true);

	if (APlayerController* OwningController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		OwningController->SetInputMode(InputMode);
		OwningController->bShowMouseCursor = true;
	}

	SetKeyboardFocus();
}

void UPragmataUpgradeMenuWidget::SetReadOnly(bool bNewReadOnly)
{
	bReadOnly = bNewReadOnly;
	SetIsEnabled(!bReadOnly);
}

void UPragmataUpgradeMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHolding || bFired)
	{
		return;
	}

	const bool bRefund = (HoldSide == EPragmataUpgradeHoldSide::Refund);
	UUpgradeItemRowWidget* ActiveRow = GetActiveRow();

	HoldProgress += InDeltaTime / FMath::Max(HoldDuration, KINDA_SMALL_NUMBER);

	if (ActiveRow)
	{
		ActiveRow->SetHoldGauge(bRefund, HoldProgress);
	}
	if (APragmataPlayerController* Controller = Cast<APragmataPlayerController>(GetOwningPlayer()))
	{
		Controller->RequestCheckpointUpgradeHold(HoldProgress, bRefund);
	}

	if (HoldProgress >= 1.f)
	{
		// 한 번의 홀드에 한 번만 실행
		bFired = true;
		bHolding = false;
		HoldProgress = 0.f;
		if (APragmataPlayerController* Controller = Cast<APragmataPlayerController>(GetOwningPlayer()))
		{
			Controller->RequestCheckpointUpgradeHold(0.f, bRefund);
		}

		if (ActiveRow)
		{
			ActiveRow->CancelHold(bRefund);
		}

		if (bRefund)
		{
			RequestSelectedRefund();
		}
		else
		{
			RequestSelectedUpgrade();
		}
	}
}

FReply UPragmataUpgradeMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 오토리핏은 홀드 시작을 재트리거하지 않도록 무시
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled();
	}
	if (bReadOnly)
	{
		return FReply::Handled();
	}

	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::Escape)
	{
		if (APragmataPlayerController* PlayerController = Cast<APragmataPlayerController>(GetOwningPlayer()))
		{
			PlayerController->RequestCloseCheckpointUpgradeMenu();
		}
		return FReply::Handled();
	}
	if (PressedKey == EKeys::F)
	{
		BeginHold(EPragmataUpgradeHoldSide::Upgrade);
		return FReply::Handled();
	}
	if (PressedKey == EKeys::C)
	{
		BeginHold(EPragmataUpgradeHoldSide::Refund);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UPragmataUpgradeMenuWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey ReleasedKey = InKeyEvent.GetKey();
	if (ReleasedKey == EKeys::F)
	{
		EndHold(EPragmataUpgradeHoldSide::Upgrade);
		return FReply::Handled();
	}
	if (ReleasedKey == EKeys::C)
	{
		EndHold(EPragmataUpgradeHoldSide::Refund);
		return FReply::Handled();
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

UUpgradeItemRowWidget* UPragmataUpgradeMenuWidget::GetActiveRow() const
{
	return GetSelectedUpgradeType() == EPragmataUpgradeType::Weapon ? WeaponRow : SuitRow;
}

void UPragmataUpgradeMenuWidget::HandleMenuChangedForButtons()
{
	ApplyHoldButtonStates(EPragmataUpgradeType::Suit, SuitRow);
	ApplyHoldButtonStates(EPragmataUpgradeType::Weapon, WeaponRow);
}

void UPragmataUpgradeMenuWidget::ApplyHoldButtonStates(EPragmataUpgradeType UpgradeType, UUpgradeItemRowWidget* Row)
{
	if (!Row)
	{
		return;
	}

	Row->UpdateHoldButtons(GetUpgradeLevel(UpgradeType), GetMaxUpgradeLevel(UpgradeType), IsUpgradeCostAffordable(UpgradeType));
}

int32 UPragmataUpgradeMenuWidget::GetLunafilament() const
{
	return BoundPlayerStatsComponent.IsValid() ? BoundPlayerStatsComponent->GetLunafilament() : 0;
}

int32 UPragmataUpgradeMenuWidget::GetUpgradeLevel(EPragmataUpgradeType UpgradeType) const
{
	return BoundPlayerStatsComponent.IsValid() ? BoundPlayerStatsComponent->GetUpgradeLevel(UpgradeType) : 0;
}

int32 UPragmataUpgradeMenuWidget::GetMaxUpgradeLevel(EPragmataUpgradeType UpgradeType) const
{
	return BoundPlayerStatsComponent.IsValid() ? BoundPlayerStatsComponent->GetMaxUpgradeLevel(UpgradeType) : 0;
}

int32 UPragmataUpgradeMenuWidget::GetUpgradeCost(EPragmataUpgradeType UpgradeType) const
{
	return BoundPlayerStatsComponent.IsValid() ? BoundPlayerStatsComponent->GetUpgradeCost(UpgradeType) : 0;
}

bool UPragmataUpgradeMenuWidget::CanUpgrade(EPragmataUpgradeType UpgradeType) const
{
	return BoundPlayerStatsComponent.IsValid() && BoundPlayerStatsComponent->CanUpgrade(UpgradeType);
}

bool UPragmataUpgradeMenuWidget::CanRefundUpgrade(EPragmataUpgradeType UpgradeType) const
{
	return BoundPlayerStatsComponent.IsValid() && BoundPlayerStatsComponent->CanRefundUpgrade(UpgradeType);
}

bool UPragmataUpgradeMenuWidget::IsUpgradeCostAffordable(EPragmataUpgradeType UpgradeType) const
{
	const int32 UpgradeCost = GetUpgradeCost(UpgradeType);
	return UpgradeCost > 0 && GetLunafilament() >= UpgradeCost;
}

float UPragmataUpgradeMenuWidget::GetUpgradeCurrentValue(EPragmataUpgradeType UpgradeType) const
{
	return GetUpgradeValueAtLevel(UpgradeType, GetUpgradeLevel(UpgradeType));
}

float UPragmataUpgradeMenuWidget::GetUpgradeNextValue(EPragmataUpgradeType UpgradeType) const
{
	const int32 CurrentLevel = GetUpgradeLevel(UpgradeType);
	const int32 NextLevel = FMath::Min(CurrentLevel + 1, GetMaxUpgradeLevel(UpgradeType));
	return GetUpgradeValueAtLevel(UpgradeType, NextLevel);
}

int32 UPragmataUpgradeMenuWidget::GetUpgradeValueFractionalDigits(EPragmataUpgradeType UpgradeType) const
{
	return UpgradeType == EPragmataUpgradeType::Weapon ? 1 : 0;
}

void UPragmataUpgradeMenuWidget::NativeDestruct()
{
	OnUpgradeMenuChanged.RemoveDynamic(this, &UPragmataUpgradeMenuWidget::HandleMenuChangedForButtons);

	UnbindPlayerStatsComponent();
	UnbindGameState();

	Super::NativeDestruct();
}

void UPragmataUpgradeMenuWidget::HandleLunafilamentChanged(int32 /*NewLunafilament*/)
{
	BroadcastMenuChanged();
}

void UPragmataUpgradeMenuWidget::HandleDamageMultiplierChanged(float /*NewMultiplier*/)
{
	BroadcastMenuChanged();
}

void UPragmataUpgradeMenuWidget::HandleUpgradeLevelChanged(EPragmataUpgradeType /*UpgradeType*/, int32 /*UpgradeLevel*/)
{
	BroadcastMenuChanged();
}

void UPragmataUpgradeMenuWidget::HandleHughCharacterChanged(AHughCharacter* HughCharacter)
{
	BindPlayerStatsComponent(HughCharacter ? HughCharacter->GetPlayerStatsComponent() : nullptr);
}

void UPragmataUpgradeMenuWidget::BindGameState(APragmataGameState* GameState)
{
	if (BoundGameState.Get() == GameState)
	{
		return;
	}

	UnbindGameState();
	BoundGameState = GameState;

	if (BoundGameState.IsValid())
	{
		BoundGameState->OnHughCharacterChanged.AddUObject(this, &ThisClass::HandleHughCharacterChanged);
		BoundGameState->OnCheckpointUpgradeSelectionChanged.AddUObject(this, &ThisClass::HandleCheckpointUpgradeSelectionChanged);
		BoundGameState->OnCheckpointUpgradeHoldChanged.AddUObject(this, &ThisClass::HandleCheckpointUpgradeHoldChanged);
		ApplySelectedUpgradeType(BoundGameState->GetCheckpointUpgradeSelection());
	}
}

void UPragmataUpgradeMenuWidget::UnbindGameState()
{
	// 재생성된 menu가 이전 GameState의 mirror delegate를 중복 수신하지 않게 모두 해제
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnHughCharacterChanged.RemoveAll(this);
		BoundGameState->OnCheckpointUpgradeSelectionChanged.RemoveAll(this);
		BoundGameState->OnCheckpointUpgradeHoldChanged.RemoveAll(this);
	}

	BoundGameState.Reset();
}

void UPragmataUpgradeMenuWidget::HandleCheckpointUpgradeSelectionChanged(EPragmataUpgradeType UpgradeType)
{
	// GameState 복제값은 Hugh와 Diana의 selected row를 같은 상태로 맞춤
	ApplySelectedUpgradeType(UpgradeType);
}

void UPragmataUpgradeMenuWidget::HandleCheckpointUpgradeHoldChanged(float Progress, bool bRefund)
{
	if (UUpgradeItemRowWidget* ActiveRow = GetActiveRow())
	{
		if (Progress > 0.f)
		{
			ActiveRow->BeginHold(bRefund);
			ActiveRow->SetHoldGauge(bRefund, Progress);
		}
		else
		{
			ActiveRow->CancelHold(bRefund);
		}
	}
}

AHughCharacter* UPragmataUpgradeMenuWidget::ResolveSharedHughCharacter() const
{
	if (BoundGameState.IsValid())
	{
		if (AHughCharacter* HughCharacter = BoundGameState->GetHughCharacter())
		{
			return HughCharacter;
		}
	}

	const APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	if (PragmataGameState)
	{
		if (AHughCharacter* HughCharacter = PragmataGameState->GetHughCharacter())
		{
			return HughCharacter;
		}
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	return OwningPlayer ? Cast<AHughCharacter>(OwningPlayer->GetPawn()) : nullptr;
}

// 강화 메뉴가 RowWidget에 넘길 숫자값을 현재 선형 성장 규칙으로 계산
float UPragmataUpgradeMenuWidget::GetUpgradeValueAtLevel(EPragmataUpgradeType UpgradeType, int32 UpgradeLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(UpgradeLevel, 0, GetMaxUpgradeLevel(UpgradeType));
	const UPragmataPlayerStatsComponent* PlayerStatsComponent = BoundPlayerStatsComponent.Get();
	switch (UpgradeType)
	{
	case EPragmataUpgradeType::Suit:
	{
		const AHughCharacter* HughCharacter = ResolveSharedHughCharacter();
		const UPragmataHealthComponent* HealthComponent = HughCharacter ? HughCharacter->FindComponentByClass<UPragmataHealthComponent>() : nullptr;
		const int32 CurrentLevel = GetUpgradeLevel(UpgradeType);
		const int32 CurrentMaxHP = HealthComponent ? HealthComponent->GetMaxHP() : 0;
		const int32 MaxHPPerLevel = PlayerStatsComponent ? PlayerStatsComponent->GetMaxHPPerSuitUpgrade() : 0;
		const int32 BaseMaxHP = FMath::Max(0, CurrentMaxHP - CurrentLevel * MaxHPPerLevel);
		return static_cast<float>(BaseMaxHP + ClampedLevel * MaxHPPerLevel);
	}
	case EPragmataUpgradeType::Weapon:
	{
		const int32 CurrentLevel = GetUpgradeLevel(UpgradeType);
		const float MultiplierPerLevel = PlayerStatsComponent ? PlayerStatsComponent->GetDamageMultiplierPerWeaponUpgrade() : 0.f;
		const float BaseMultiplier = PlayerStatsComponent
			? PlayerStatsComponent->GetPlayerDamageMultiplier() - CurrentLevel * MultiplierPerLevel
			: 0.f;
		return BaseMultiplier + ClampedLevel * MultiplierPerLevel;
	}
	default:
		return 0.f;
	}
}

EPragmataUpgradeType UPragmataUpgradeMenuWidget::GetAdjacentSelectableUpgrade(int32 Direction) const
{
	if (Direction >= 0)
	{
		return SelectedUpgradeType == EPragmataUpgradeType::Suit
			? EPragmataUpgradeType::Weapon
			: EPragmataUpgradeType::Suit;
	}

	return SelectedUpgradeType == EPragmataUpgradeType::Weapon
		? EPragmataUpgradeType::Suit
		: EPragmataUpgradeType::Weapon;
}

void UPragmataUpgradeMenuWidget::BroadcastMenuChanged()
{
	OnUpgradeMenuChanged.Broadcast();
}
