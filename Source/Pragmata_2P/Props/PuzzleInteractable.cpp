#include "Props/PuzzleInteractable.h"

#include "Animation/AnimationAsset.h"
#include "Components/DianaNodeInteractionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Puzzle/MapHackingPuzzleComponent.h"

APuzzleInteractable::APuzzleInteractable()
{
	PuzzleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PuzzleMesh"));
	RootComponent = PuzzleMesh;

	PuzzleComponent = CreateDefaultSubobject<UMapHackingPuzzleComponent>(TEXT("MapHackingPuzzleComponent"));

	bReplicates = true;
	bIsRepeatable = false; // 기본값은 1회성 퍼즐
	MaxHackCount = 2;
	CompletedHackCount = 0;
	bIsSolved = false;
	bInteractionEnabled = true;
	bCanBeScanned = true;
	ScanTargetKind = EPragmataScanTargetKind::MapHackNode;
	ScanTargetId = TEXT("MapHackNode");
	ScanDisplayText = NSLOCTEXT("Pragmata", "MapHackNodeScanDisplayText", "Map Hack Node");
	DianaInteractionDistance = 5000.f;
}

void APuzzleInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APuzzleInteractable, bIsRepeatable);
	DOREPLIFETIME(APuzzleInteractable, MaxHackCount);
	DOREPLIFETIME(APuzzleInteractable, bIsSolved);
	DOREPLIFETIME(APuzzleInteractable, CompletedHackCount);
	DOREPLIFETIME(APuzzleInteractable, bInteractionEnabled);
	DOREPLIFETIME(APuzzleInteractable, CompleteAnimationSerial);
	DOREPLIFETIME(APuzzleInteractable, ResetAnimationSerial);
}

void APuzzleInteractable::Interact_Implementation(AActor* Interactor)
{
	// 서버가 현재 재무장과 잔여 횟수를 검증한 퍼즐만 상호작용을 허용
	if (!HasAuthority() || !CanInteractWithPuzzle())
	{
		return;
	}

	// map hack component가 있으면 Diana component가 서버 puzzle 진행을 소유
	if (FindComponentByClass<UMapHackingPuzzleComponent>())
	{
		if (Interactor)
		{
			if (UDianaNodeInteractionComponent* DianaNodeInteraction = Interactor->FindComponentByClass<UDianaNodeInteractionComponent>())
			{
				DianaNodeInteraction->TryStartMapHackFromInteractable(this);
				return;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[PuzzleInteractable::Interact_Implementation] Map hack node requires DianaNodeInteractionComponent: %s"), *GetNameSafe(this));
		return;
	}

	CompletePuzzle();
}

FPragmataScanResult APuzzleInteractable::GetPragmataScanResult_Implementation(AActor* ScanRequester) const
{
	// 해결된 1회성 node는 다음 scan에서 POI로 다시 띄우지 않음
	if (IsPuzzleSolved())
	{
		return FPragmataScanResult();
	}

	return Super::GetPragmataScanResult_Implementation(ScanRequester);
}

void APuzzleInteractable::SetPuzzleInteractionEnabled(bool bNewEnabled)
{
	// elevator 같은 progression actor가 반복 퍼즐의 재상호작용 가능 시점을 서버에서 제어
	if (!HasAuthority() || bInteractionEnabled == bNewEnabled)
	{
		return;
	}

	bInteractionEnabled = bNewEnabled;
	OnPuzzleInteractionEnabledChanged(bInteractionEnabled);
	ForceNetUpdate();
}

// BossRouteElevator 같은 parent actor가 child puzzle의 재해킹 정책을 server에서 확정
void APuzzleInteractable::ConfigureRepeatHacking(bool bNewRepeatable, int32 NewMaxHackCount)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsRepeatable = bNewRepeatable;
	MaxHackCount = FMath::Max(1, NewMaxHackCount);
	ForceNetUpdate();
}

void APuzzleInteractable::CompletePuzzle()
{
	// 서버가 시도 횟수를 확정해 재사용 퍼즐의 상호작용 상태를 보호
	if (!HasAuthority() || !CanInteractWithPuzzle())
	{
		return;
	}

	// 모든 시도를 완료 상태로 닫고 재사용 퍼즐만 Reset으로 다음 시도를 허용
	++CompletedHackCount;
	bIsSolved = true;
	OnPuzzleSolvedChanged(bIsSolved);

	// 퍼즐 완료를 실제 상호작용 발동으로 보고 연결된 동작 액터에 알림
	// 서버 완료 시점에 로컬 재생 후 serial 복제로 client 재생을 맞춤
	PlayCompleteAnimation();
	++CompleteAnimationSerial;

	BroadcastInteractionActivated(nullptr);

	for (AActor* Target : TargetActors)
	{
		if (Target && Target->Implements<UPuzzleTargetInterface>())
		{
			IPuzzleTargetInterface::Execute_OnPuzzleTriggered(Target);
		}
	}

	ForceNetUpdate();
}

void APuzzleInteractable::ResetPuzzleForNextAttempt()
{
	// 체크한 반복 퍼즐만 잔여 횟수가 있을 때 다음 시도로 재무장
	if (!HasAuthority() || !bIsRepeatable || !HasRemainingHackAttempts())
	{
		return;
	}

	if (bIsSolved)
	{
		bIsSolved = false;
		OnPuzzleSolvedChanged(bIsSolved);
	}

	if (PuzzleComponent)
	{
		PuzzleComponent->CancelHack();
	}

	PlayCompleteAnimationReverse();
	++ResetAnimationSerial;
	ForceNetUpdate();
}

void APuzzleInteractable::OnRep_IsSolved()
{
	// 서버 완료 상태를 받은 client가 BP 표시를 갱신
	OnPuzzleSolvedChanged(bIsSolved);
}

void APuzzleInteractable::OnRep_InteractionEnabled()
{
	OnPuzzleInteractionEnabledChanged(bInteractionEnabled);
}

void APuzzleInteractable::OnRep_CompleteAnimationSerial()
{
	// 완료 serial 변화는 client 로컬 skeletal mesh 애니메이션 재생 신호
	if (CompleteAnimationSerial > 0)
	{
		PlayCompleteAnimation();
	}
}

void APuzzleInteractable::OnRep_ResetAnimationSerial()
{
	// reset serial 변화는 client 로컬 skeletal mesh 역재생 신호
	if (ResetAnimationSerial > 0)
	{
		PlayCompleteAnimationReverse();
	}
}

void APuzzleInteractable::PlayCompleteAnimation()
{
	if (!PuzzleMesh || !CompleteAnimation)
	{
		return;
	}

	PuzzleMesh->PlayAnimation(CompleteAnimation, false);
	// PlayAnimation이 single node 재생 상태를 갱신한 뒤 속도를 적용
	PuzzleMesh->SetPlayRate(CompleteAnimationPlayRate);
}

void APuzzleInteractable::PlayCompleteAnimationReverse()
{
	if (!PuzzleMesh || !CompleteAnimation)
	{
		return;
	}

	PuzzleMesh->PlayAnimation(CompleteAnimation, false);
	PuzzleMesh->SetPosition(CompleteAnimation->GetPlayLength(), false);
	PuzzleMesh->SetPlayRate(-FMath::Abs(CompleteAnimationPlayRate));
}
