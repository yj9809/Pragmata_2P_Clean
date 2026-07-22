#include "Game/TutorialManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Data/TutorialStepData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/PragmataPlayerController.h"
#include "Game/TutorialProgressSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TutorialPopupWidget.h"

namespace
{
	const FString TutorialProgressSlotName = TEXT("PragmataTutorialProgress");
	constexpr int32 TutorialProgressUserIndex = 0;
}

// GameInstance보다 짧은 world 수명에 맞춰 popup presentation 상태를 정리하도록 cleanup delegate를 구독
void UTutorialManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &UTutorialManagerSubsystem::HandleWorldCleanup);
	LoadTutorialProgress();
}

// GameInstance 종료 시 world delegate와 viewport widget 참조를 함께 해제
void UTutorialManagerSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	HideTutorial();
	ActiveTutorialWidget = nullptr;
	Super::Deinitialize();
}

// legacy 직접 표시도 local serial과 timer를 받아 다른 tutorial 종료와 독립적으로 닫힘
void UTutorialManagerSubsystem::ShowTutorialStep(const UTutorialStepData* StepData)
{
	if (TryShowTutorialStep(StepData))
	{
		ActivePresentationSerial = AllocateLocalPresentationSerial();
		ScheduleTutorialDismiss(StepData, ActivePresentationSerial);
	}
}

// 표시 대상과 MVVM 초기화를 모두 통과한 로컬 viewport만 성공으로 처리
bool UTutorialManagerSubsystem::TryShowTutorialStep(const UTutorialStepData* StepData)
{
	if (!StepData)
	{
		return false;
	}

	if (TutorialWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] TutorialWidgetClass가 설정되지 않음"));
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* PlayerController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] PlayerController를 찾지 못함"));
		return false;
	}

	// 이 클라이언트의 role이 스텝 대상과 다르면 표시하지 않음 (None이면 전체 대상)
	const APragmataPlayerController* PragmataController = Cast<APragmataPlayerController>(PlayerController);
	const EPragmataPlayerRole LocalRole = PragmataController ? PragmataController->GetAssignedRole() : EPragmataPlayerRole::None;
	const EPragmataPlayerRole TargetRole = StepData->GetTargetRole();
	if (TargetRole != EPragmataPlayerRole::None && LocalRole != TargetRole)
	{
		return false;
	}

	const bool bWasInViewport = ActiveTutorialWidget && ActiveTutorialWidget->IsInViewport();
	if (!ActiveTutorialWidget)
	{
		// 에디터 부팅 시 조기 하드 로드를 피하려고 소프트 참조를 첫 표시 시점에 로드
		UClass* LoadedWidgetClass = TutorialWidgetClass.LoadSynchronous();
		if (!LoadedWidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] TutorialWidgetClass 로드 실패: %s"), *TutorialWidgetClass.ToSoftObjectPath().ToString());
			return false;
		}

		ActiveTutorialWidget = CreateWidget<UTutorialPopupWidget>(PlayerController, LoadedWidgetClass);
	}

	if (!ActiveTutorialWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] 튜토리얼 위젯 생성 실패"));
		return false;
	}
	// AddToViewport가 먼저 실행돼야 NativeConstruct에서 MVVM Create Instance ViewModel이 만들어짐 -> InitializeStep보다 앞에 와야 함
	if (!ActiveTutorialWidget->IsInViewport())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] 튜토리얼 위젯을 뷰포트에 추가"));
		// 게임플레이 HUD보다 항상 위에 뜨도록 Z-Order를 높게 고정
		ActiveTutorialWidget->AddToViewport(1000);
	}

	UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] 튜토리얼 위젯 InitializeStep 호출 전"));
	if (!ActiveTutorialWidget->InitializeStep(StepData))
	{
		// viewport에 없던 위젯 초기화 실패 시 빈 popup을 화면에 남기지 않음
		if (!bWasInViewport)
		{
			ActiveTutorialWidget->RemoveFromParent();
		}
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialStep] IsInViewport=%s, Visibility=%s, WidgetClass=%s"),
		ActiveTutorialWidget->IsInViewport() ? TEXT("true") : TEXT("false"),
		*UEnum::GetValueAsString(ActiveTutorialWidget->GetVisibility()),
		*ActiveTutorialWidget->GetClass()->GetName());

	return true;
}

// global hide는 legacy 호출 계약대로 현재 표시 종류와 무관하게 닫음
void UTutorialManagerSubsystem::HideTutorial()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TutorialDismissTimerHandle);
	}
	if (ActiveTutorialWidget && ActiveTutorialWidget->IsInViewport())
	{
		ActiveTutorialWidget->RemoveFromParent();
	}
	ActivePresentationSerial = 0;
}

// role 불일치나 로컬 UI 실패 시 기존 popup과 serial을 보존
void UTutorialManagerSubsystem::ShowTutorialPresentation(const UTutorialStepData* StepData, uint32 PresentationSerial)
{
	if (PresentationSerial != 0 && TryShowTutorialStep(StepData))
	{
		ActivePresentationSerial = PresentationSerial;
		ScheduleTutorialDismiss(StepData, ActivePresentationSerial);
	}
}

// 로컬 popup 초기화 성공 뒤에만 TutorialId를 저장해 역할 불일치와 UI 실패가 이력을 소비하지 않게 함
bool UTutorialManagerSubsystem::ShowTutorialOnce(FName TutorialId, const UTutorialStepData* StepData, uint32 PresentationSerial)
{
	if (TutorialId == NAME_None || !StepData)
	{
		return false;
	}

	if (!TutorialProgress)
	{
		LoadTutorialProgress();
	}
	if (IsRunningInPIE() && !bUseTransientTutorialProgress)
	{
		// Initialize 시점 world가 아직 없던 PIE도 이전 SaveGame 이력을 읽지 않게 분리
		TutorialProgress = Cast<UTutorialProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UTutorialProgressSaveGame::StaticClass()));
		bUseTransientTutorialProgress = true;
	}
	if (!TutorialProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::ShowTutorialOnce] tutorial SaveGame 생성 실패"));
		return false;
	}
	if (TutorialProgress->HasShownTutorial(TutorialId))
	{
		return false;
	}

	if (!TryShowTutorialStep(StepData))
	{
		return false;
	}

	ActivePresentationSerial = PresentationSerial != 0 ? PresentationSerial : AllocateLocalPresentationSerial();
	ScheduleTutorialDismiss(StepData, ActivePresentationSerial);
	TutorialProgress->MarkTutorialShown(TutorialId);
	SaveTutorialProgress();
	return true;
}

// server serial과 충돌하지 않도록 direct client presentation은 높은 값부터 역순으로 사용
uint32 UTutorialManagerSubsystem::AllocateLocalPresentationSerial()
{
	if (NextLocalPresentationSerial == 0)
	{
		NextLocalPresentationSerial = MAX_uint32;
	}

	return NextLocalPresentationSerial--;
}

// 새 tutorial이 표시되면 이전 local timer를 교체하고 자신의 serial만 종료 대상으로 보존
void UTutorialManagerSubsystem::ScheduleTutorialDismiss(const UTutorialStepData* StepData, uint32 ExpectedPresentationSerial)
{
	UWorld* World = GetWorld();
	if (!World || !StepData || ExpectedPresentationSerial == 0)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TutorialDismissTimerHandle);
	FTimerDelegate DismissDelegate;
	DismissDelegate.BindUObject(this, &UTutorialManagerSubsystem::HandleTutorialDismissTimerElapsed, ExpectedPresentationSerial);
	World->GetTimerManager().SetTimer(
		TutorialDismissTimerHandle,
		DismissDelegate,
		FMath::Max(StepData->GetDisplayDurationSeconds(), 0.1f),
		false);
}

// 이전 timer는 ExpectedPresentationSerial 비교로 최신 popup을 닫지 못함
void UTutorialManagerSubsystem::HandleTutorialDismissTimerElapsed(uint32 ExpectedPresentationSerial)
{
	HideTutorialPresentation(ExpectedPresentationSerial);
}

// 이전 trigger timer가 더 최신 popup을 닫지 못하도록 serial 일치만 허용
void UTutorialManagerSubsystem::HideTutorialPresentation(uint32 ExpectedPresentationSerial)
{
	if (ExpectedPresentationSerial == 0 || ActivePresentationSerial != ExpectedPresentationSerial)
	{
		return;
	}

	HideTutorial();
}

// travel 뒤 새 GameState serial이 이전 world presentation과 충돌하지 않도록 world 소유 상태를 폐기
void UTutorialManagerSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;

	if (!ActiveTutorialWidget || ActiveTutorialWidget->GetWorld() != World)
	{
		return;
	}

	HideTutorial();
	ActiveTutorialWidget = nullptr;
}

// PIE는 매 실행 fresh tutorial 검증을 위해 disk slot 대신 메모리 이력만 사용
void UTutorialManagerSubsystem::LoadTutorialProgress()
{
	bUseTransientTutorialProgress = IsRunningInPIE();
	if (bUseTransientTutorialProgress)
	{
		TutorialProgress = Cast<UTutorialProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UTutorialProgressSaveGame::StaticClass()));
		return;
	}

	TutorialProgress = Cast<UTutorialProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(TutorialProgressSlotName, TutorialProgressUserIndex));
	if (!TutorialProgress)
	{
		TutorialProgress = Cast<UTutorialProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UTutorialProgressSaveGame::StaticClass()));
	}
}

// PIE 메모리 이력은 저장하지 않고 packaged/standalone 이력만 다음 실행까지 보존
void UTutorialManagerSubsystem::SaveTutorialProgress() const
{
	if (bUseTransientTutorialProgress || IsRunningInPIE())
	{
		return;
	}

	if (TutorialProgress && !UGameplayStatics::SaveGameToSlot(TutorialProgress, TutorialProgressSlotName, TutorialProgressUserIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialManagerSubsystem::SaveTutorialProgress] tutorial 진행 저장 실패"));
	}
}

// Editor world 중 PIE만 저장 이력을 격리해 standalone과 packaged의 영구 1회 정책을 보존
bool UTutorialManagerSubsystem::IsRunningInPIE() const
{
	const UWorld* World = GetWorld();
	return World && World->WorldType == EWorldType::PIE;
}
