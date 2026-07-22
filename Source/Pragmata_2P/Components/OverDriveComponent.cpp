// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/OverDriveComponent.h"

#include "Characters/HughCharacter.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/DianaAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemy/MonsterBase.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"
#include "Props/OverDrivePulseRingActor.h"
#include "EngineUtils.h"

// Sets default values for this component's properties
UOverDriveComponent::UOverDriveComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	OverDriveBodyAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Character/Diana/Animations/Grapple/AS_Diana_Grapple_0010_HacK_SP.AS_Diana_Grapple_0010_HacK_SP")));
	OverDriveFaceAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Character/Diana/Animations/Facial/Grapple/AS_Diana_Face_Grapple_SPHack.AS_Diana_Face_Grapple_SPHack")));
}

// Called when the game starts
void UOverDriveComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		OverDrivePercent = FMath::Clamp(OverDrivePercent, 0.f, 1.f);
	}
	
}

void UOverDriveComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UOverDriveComponent, OverDrivePercent);
	DOREPLIFETIME(UOverDriveComponent, bActivationPending);
}

void UOverDriveComponent::RequestOverDrive()
{
	if (const ADianaOperatorPawn* DianaPawn = Cast<ADianaOperatorPawn>(GetOwner()))
	{
		if (!DianaPawn->CanUseDianaActionIntent())
		{
			return;
		}
	}

	Server_RequestOverDrive();
}

void UOverDriveComponent::Server_RequestOverDrive_Implementation()
{
	// server는 Diana role과 gauge 상태를 확정한 뒤에만 연출 lock을 시작
	ADianaOperatorPawn* DianaPawn = Cast<ADianaOperatorPawn>(GetOwner());
	APragmataPlayerController* PlayerController = DianaPawn ? Cast<APragmataPlayerController>(DianaPawn->GetController()) : nullptr;
	APragmataPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	if (!DianaPawn || !PlayerState || !GameState || bActivationPending
		|| PlayerState->GetAssignedRole() != EPragmataPlayerRole::Diana
		|| OverDrivePercent < 1.f - UE_KINDA_SMALL_NUMBER
		|| GameState->AreDianaActionsLocked()
		|| DianaPawn->IsHackActionActive())
	{
		return;
	}

	bActivationPending = true;
	GameState->SetDianaActionsLocked(true);
	DianaPawn->SetAimingState(false);
	DianaPawn->CancelPrecisionCleanse();
	DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::OverDriveFire);
	Multicast_PlayOverDriveCinematic();
}

void UOverDriveComponent::Multicast_PlayOverDriveCinematic_Implementation()
{
	// 연출은 Diana owning client의 기존 camera를 유지한 채 local presentation만 재생
	PlayOverDriveCinematic();
}

void UOverDriveComponent::PlayOverDriveCinematic()
{
	// sequence asset 의존 없이 공통 timer가 server cast 시점을 결정
	UWorld* World = GetWorld();
	if (!World)
	{
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			AbortOverDrive();
		}
		return;
	}

	PlayLocalDianaOverDrivePresentation();
	World->GetTimerManager().SetTimer(CinematicFallbackTimerHandle, this,
		&ThisClass::HandleOverDriveCinematicFinished, CinematicDuration, false);
}

void UOverDriveComponent::PlayLocalDianaOverDrivePresentation()
{
	// Diana owning client만 기존 camera를 유지한 채 skill animation과 input lock을 적용
	ADianaOperatorPawn* DianaPawn = Cast<ADianaOperatorPawn>(GetOwner());
	if (!DianaPawn || !DianaPawn->IsLocallyControlled())
	{
		return;
	}

	if (APragmataPlayerController* PlayerController = Cast<APragmataPlayerController>(DianaPawn->GetController()))
	{
		PlayerController->SetIgnoreLookInput(true);
		DianaPawn->DisableInput(PlayerController);
		CinematicInputLockedPawn = DianaPawn;

		const APragmataGameState* GameState = GetWorld()->GetGameState<APragmataGameState>();
		if (AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr)
		{
			if (USkeletalMeshComponent* DianaMesh = HughCharacter->GetDianaAvatarMesh())
			{
				if (UDianaAnimInstance* DianaAnim = Cast<UDianaAnimInstance>(DianaMesh->GetAnimInstance()))
				{
					DianaAnim->PlaySkillBodyAnimation(OverDriveBodyAnimation.LoadSynchronous());
					DianaAnim->PlaySkillFaceAnimation(OverDriveFaceAnimation.LoadSynchronous());
				}
			}
		}
	}
}

void UOverDriveComponent::EndLocalDianaOverDrivePresentation()
{
	// 정상 종료와 abort가 같은 local input 복구 경로를 사용
	ADianaOperatorPawn* DianaPawn = Cast<ADianaOperatorPawn>(GetOwner());
	if (DianaPawn && DianaPawn->IsLocallyControlled())
	{
		if (APragmataPlayerController* PlayerController = Cast<APragmataPlayerController>(DianaPawn->GetController()))
		{
			PlayerController->SetIgnoreLookInput(false);
			if (CinematicInputLockedPawn.Get() == DianaPawn)
			{
				DianaPawn->EnableInput(PlayerController);
			}
		}
	}
	CinematicInputLockedPawn = nullptr;

	const APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	if (AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr)
	{
		if (USkeletalMeshComponent* DianaMesh = HughCharacter->GetDianaAvatarMesh())
		{
			if (UDianaAnimInstance* DianaAnim = Cast<UDianaAnimInstance>(DianaMesh->GetAnimInstance()))
			{
				DianaAnim->StopSkillBodyAnimation();
				DianaAnim->PlayIdleFaceAnimation();
			}
		}
	}
}

void UOverDriveComponent::HandleOverDriveCinematicFinished()
{
	// server만 연출 종료 뒤 world hack effect를 한 번 확정
	EndLocalDianaOverDrivePresentation();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		FinishOverDrive();
	}
}

void UOverDriveComponent::FinishOverDrive()
{
	// Hugh 위치를 effect 중심으로 고정해 ring과 enemy hack 범위를 일치
	if (!bActivationPending || !GetWorld())
	{
		return;
	}

	bActivationPending = false;
	SetOverDrivePercent(0.f);
	APragmataGameState* GameState = GetWorld()->GetGameState<APragmataGameState>();
	if (GameState)
	{
		GameState->SetDianaActionsLocked(false);
	}

	const AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr;
	if (!HughCharacter)
	{
		return;
	}

	const FVector EffectCenter = HughCharacter->GetActorLocation();
	SpawnOverDriveRing(EffectCenter);

	for (TActorIterator<AMonsterBase> It(GetWorld()); It; ++It)
	{
		AMonsterBase* Monster = *It;
		if (Monster && FVector::DistSquared(EffectCenter, Monster->GetActorLocation()) <= FMath::Square(EffectRadius))
		{
			Monster->ApplyTimedHackState(TimedHackDuration);
		}
	}
}

void UOverDriveComponent::SpawnOverDriveRing(const FVector& Center)
{
	// replicated mesh ring은 floor보다 높은 Z에서 양 client에 표시
	if (UWorld* World = GetWorld())
	{
		World->SpawnActor<AOverDrivePulseRingActor>(AOverDrivePulseRingActor::StaticClass(), Center + FVector(0.f, 0.f, RingHeightOffset), FRotator::ZeroRotator);
	}
}

void UOverDriveComponent::AbortOverDrive()
{
	bActivationPending = false;
	EndLocalDianaOverDrivePresentation();
	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		GameState->SetDianaActionsLocked(false);
	}
}

void UOverDriveComponent::AddOverDrive(float Amount)
{
	SetOverDrivePercent(OverDrivePercent + Amount);
}

void UOverDriveComponent::SetOverDrivePercent(float NewPercent)
{
	AActor* Owner = GetOwner();
	
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	
	const float PreviousPercent = OverDrivePercent;
	const float ClampedPercent = FMath::Clamp(NewPercent, 0.f, 1.f);
	
	if (FMath::IsNearlyEqual(OverDrivePercent, ClampedPercent))
	{
		return;
	}
	
	OverDrivePercent = ClampedPercent;
	if (PreviousPercent < 1.f - UE_KINDA_SMALL_NUMBER && OverDrivePercent >= 1.f - UE_KINDA_SMALL_NUMBER)
	{
		if (ADianaOperatorPawn* DianaPawn = Cast<ADianaOperatorPawn>(Owner))
		{
			DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::OverDriveReady);
		}
	}
	BroadcastOverDriveChanged();
}


void UOverDriveComponent::OnRep_OverDrivePercent()
{
	BroadcastOverDriveChanged();
}

void UOverDriveComponent::BroadcastOverDriveChanged()
{
	OnOverDriveChanged.Broadcast(OverDrivePercent);
}


