#include "Components/PairedAnimationComponent.h"

#include "Characters/HughCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/PairedAnimationSet.h"
#include "Game/PragmataGameState.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

namespace
{
FName GetDeathSectionName(EPragmataHitDirection Direction)
{
	switch (Direction)
	{
	case EPragmataHitDirection::Right:
		return TEXT("Right");
	case EPragmataHitDirection::Back:
		return TEXT("Back");
	case EPragmataHitDirection::Left:
		return TEXT("Left");
	case EPragmataHitDirection::Front:
	default:
		return TEXT("Front");
	}
}
}

UPairedAnimationComponent::UPairedAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DefaultAnimationSet = TSoftObjectPtr<UPairedAnimationSet>(FSoftObjectPath(
		TEXT("/Game/Data/DA_PairedAnimationSet.DA_PairedAnimationSet")));
}

// Hugh mesh를 명시 주입하지 않은 BP에서도 owner character mesh를 기본값으로 사용
void UPairedAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HughMesh)
	{
		if (const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
		{
			HughMesh = CharacterOwner->GetMesh();
		}
	}

	// BP component default가 비어 있어도 runtime DataAsset을 명시 경로에서 복구
	if (!AnimationSet && !DefaultAnimationSet.IsNull())
	{
		AnimationSet = DefaultAnimationSet.LoadSynchronous();
	}
}

// paired montage cue는 sequence id와 server time을 함께 복제해 늦은 클라이언트를 보정
void UPairedAnimationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPairedAnimationComponent, LastCue);
}

void UPairedAnimationComponent::InitializeMeshes(USkeletalMeshComponent* InHughMesh, USkeletalMeshComponent* InDianaMesh)
{
	// Hugh/Diana mesh를 외부에서 주입해 같은 cue를 두 캐릭터 montage에 적용
	HughMesh = InHughMesh;
	DianaMesh = InDianaMesh;
}

void UPairedAnimationComponent::TriggerPairedCue(EPairedAnimationCueType CueType, EPragmataHitReactStrength Strength, EPragmataHitDirection Direction)
{
	// paired cue는 서버에서만 생성하고, 클라이언트는 복제된 cue를 재생
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	LastCue.SequenceId++;
	LastCue.CueType = CueType;
	LastCue.Strength = Strength;
	LastCue.Direction = Direction;
	if (const APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		LastCue.ServerStartTime = GameState->GetServerWorldTimeSeconds();
	}
	else
	{
		LastCue.ServerStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	}

	LockDianaActionsForCue(LastCue);
	PlayCue(LastCue);
}

void UPairedAnimationComponent::OnRep_LastCue()
{
	// cue 복제 도착 시 server start time 기준 offset으로 montage를 맞춰 재생
	PlayCue(LastCue);
}

bool UPairedAnimationComponent::IsForcedDianaInterruptCue(const FPairedAnimationCue& Cue)
{
	// Diana 자유 조작을 끊어야 하는 강제 동기화 cue만 별도로 판정
	if (Cue.CueType == EPairedAnimationCueType::Death || Cue.CueType == EPairedAnimationCueType::MountedWatch)
	{
		return true;
	}

	if (Cue.CueType != EPairedAnimationCueType::HitReact)
	{
		return false;
	}

	return Cue.Strength == EPragmataHitReactStrength::Large
		|| Cue.Strength == EPragmataHitReactStrength::Death
		|| Cue.Strength == EPragmataHitReactStrength::ForcedPair; // Diana 독립 조작을 끊는 강제 연출
}

void UPairedAnimationComponent::PlayCue(const FPairedAnimationCue& Cue) const
{
	// Hugh는 모든 paired cue를 받고, Diana는 forced interrupt cue에서만 montage로 끊음
	if (!AnimationSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PairedAnimationComponent::PlayCue] Missing AnimationSet"));
		return;
	}

	const FPairedAnimationEntry* Entry = AnimationSet->FindEntry(Cue);
	if (!Entry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PairedAnimationComponent::PlayCue] Missing paired animation entry"));
		return;
	}

	const float StartOffset = GetCueStartOffset(Cue);
	if (Cue.CueType == EPairedAnimationCueType::Death || Cue.Strength == EPragmataHitReactStrength::Death)
	{
		if (AHughCharacter* HughCharacter = Cast<AHughCharacter>(GetOwner()))
		{
			HughCharacter->SetDianaAvatarDeathAttachment(true);
		}
	}
	if (HughMesh && Entry->HughMontage)
	{
		if (UAnimInstance* HughAnim = HughMesh->GetAnimInstance())
		{
			float HughStartPosition = StartOffset;
			if (Cue.CueType == EPairedAnimationCueType::Death || Cue.Strength == EPragmataHitReactStrength::Death)
			{
				const int32 SectionIndex = Entry->HughMontage->GetSectionIndex(GetDeathSectionName(Cue.Direction));
				if (SectionIndex != INDEX_NONE)
				{
					float SectionStartTime = 0.f;
					float SectionEndTime = 0.f;
					Entry->HughMontage->GetSectionStartAndEndTime(SectionIndex, SectionStartTime, SectionEndTime);
					HughStartPosition += SectionStartTime;
				}
			}
			HughAnim->Montage_Play(Entry->HughMontage, Entry->PlayRate, EMontagePlayReturnType::MontageLength, HughStartPosition, true);
		}
	}

	if (DianaMesh && Entry->DianaMontage)
	{
		if (IsForcedDianaInterruptCue(Cue))
		{
			if (UAnimInstance* DianaAnim = DianaMesh->GetAnimInstance())
			{
				DianaAnim->Montage_Play(Entry->DianaMontage, Entry->PlayRate, EMontagePlayReturnType::MontageLength, StartOffset, true);
			}
		}
	}
}

float UPairedAnimationComponent::GetCueStartOffset(const FPairedAnimationCue& Cue) const
{
	// 서버 시작 시간과 현재 server world time 차이로 늦게 받은 montage 위치를 보정
	if (!GetWorld() || Cue.ServerStartTime <= 0.f)
	{
		return 0.f;
	}

	if (const APragmataGameState* GameState = GetWorld()->GetGameState<APragmataGameState>())
	{
		return FMath::Max(0.f, GameState->GetServerWorldTimeSeconds() - Cue.ServerStartTime);
	}

	return FMath::Max(0.f, GetWorld()->GetTimeSeconds() - Cue.ServerStartTime);
}

void UPairedAnimationComponent::LockDianaActionsForCue(const FPairedAnimationCue& Cue)
{
	// hard hit/death/forced pair 동안 scan, hack, combat camera가 montage를 덮지 않게 잠금
	UWorld* World = GetWorld();
	APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!World || !GameState)
	{
		return;
	}

	if (!IsForcedDianaInterruptCue(Cue))
	{
		return;
	}

	GameState->SetDianaActionsLocked(true);
	World->GetTimerManager().ClearTimer(DianaActionLockTimerHandle);

	if (Cue.CueType != EPairedAnimationCueType::Death && Cue.Strength != EPragmataHitReactStrength::Death)
	{
		World->GetTimerManager().SetTimer(DianaActionLockTimerHandle, this, &UPairedAnimationComponent::ReleaseDianaActionLock, DefaultActionLockDuration, false);
	}
}

void UPairedAnimationComponent::ReleaseDianaActionLock()
{
	// 사망 외 forced cue가 끝나면 Diana 독립 조작을 다시 허용
	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		GameState->SetDianaActionsLocked(false);
	}
}
