#include "Characters/PragmataHughMovementComponent.h"

#include "GameFramework/Character.h"

// Hugh 입력 intent bit를 기본값으로 시작해 첫 saved move가 깨끗한 상태를 갖게 함
UPragmataHughMovementComponent::UPragmataHughMovementComponent()
{
	bWantsToAim = false;
	bWantsToRun = false;
	bWantsToHover = false;
	bJumpInputHeld = false;
}

// aim 입력 intent를 movement prediction custom flag에 실을 상태로 보관
void UPragmataHughMovementComponent::SetWantsToAim(bool bNewWantsToAim)
{
	bWantsToAim = bNewWantsToAim;
}

// run 입력 intent를 서버 movement update에서 같은 속도 상태로 재생하게 함
void UPragmataHughMovementComponent::SetWantsToRun(bool bNewWantsToRun)
{
	bWantsToRun = bNewWantsToRun;
}

// hover 입력 intent를 falling/flying 전환 prediction과 함께 전달
void UPragmataHughMovementComponent::SetWantsToHover(bool bNewWantsToHover)
{
	bWantsToHover = bNewWantsToHover;
}

// jump hold 상태를 자동 hover 판정에 쓰도록 saved move에 남김
void UPragmataHughMovementComponent::SetJumpInputHeld(bool bNewJumpInputHeld)
{
	bJumpInputHeld = bNewJumpInputHeld;
}

// 서버 movement update에서 Hugh 입력 intent를 디코딩
void UPragmataHughMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// 클라이언트 입력 intent 디코딩
	bWantsToAim = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToRun = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsToHover = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	bJumpInputHeld = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
}

// prediction system이 Hugh saved move를 사용하게 연결
FNetworkPredictionData_Client* UPragmataHughMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UPragmataHughMovementComponent* MutableThis = const_cast<UPragmataHughMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FHughNetworkPredictionData_Client(*this);
	}

	return ClientPredictionData;
}

// CharacterMovement prediction data의 saved move factory만 Hugh 전용으로 교체
FHughNetworkPredictionData_Client::FHughNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

// 새 movement sample마다 Hugh intent 저장 공간 생성
FSavedMovePtr FHughNetworkPredictionData_Client::AllocateNewMove()
{
	return FSavedMovePtr(new FHughSavedMove());
}

// saved move 재사용 전 Hugh intent bit 초기화
void FHughSavedMove::Clear()
{
	Super::Clear();

	bSavedWantsToAim = false;
	bSavedWantsToRun = false;
	bSavedWantsToHover = false;
	bSavedJumpInputHeld = false;
}

// movement sample 시작 시점의 Hugh intent 저장
void FHughSavedMove::SetInitialPosition(ACharacter* Character)
{
	Super::SetInitialPosition(Character);

	// 저장 move에 현재 intent 저장
	if (const UPragmataHughMovementComponent* HughMovement = Character ? Cast<UPragmataHughMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		bSavedWantsToAim = HughMovement->WantsToAim();
		bSavedWantsToRun = HughMovement->WantsToRun();
		bSavedWantsToHover = HughMovement->WantsToHover();
		bSavedJumpInputHeld = HughMovement->IsJumpInputHeld();
	}
}

// 클라이언트 재예측 전에 저장된 intent 복원
void FHughSavedMove::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	// 클라이언트 replay 전 intent 복원
	if (UPragmataHughMovementComponent* HughMovement = Character ? Cast<UPragmataHughMovementComponent>(Character->GetCharacterMovement()) : nullptr)
	{
		HughMovement->SetWantsToAim(bSavedWantsToAim);
		HughMovement->SetWantsToRun(bSavedWantsToRun);
		HughMovement->SetWantsToHover(bSavedWantsToHover);
		HughMovement->SetJumpInputHeld(bSavedJumpInputHeld);
	}
}

// 서버 RPC에 실을 custom flag bit 구성
uint8 FHughSavedMove::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	// 서버 전송 custom flag 인코딩
	if (bSavedWantsToAim)
	{
		Result |= FLAG_Custom_0;
	}
	if (bSavedWantsToRun)
	{
		Result |= FLAG_Custom_1;
	}
	if (bSavedWantsToHover)
	{
		Result |= FLAG_Custom_2;
	}
	if (bSavedJumpInputHeld)
	{
		Result |= FLAG_Custom_3;
	}

	return Result;
}

// intent가 다른 move는 합치지 않아 입력 전환 타이밍 보존
bool FHughSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	const FHughSavedMove* NewHughMove = static_cast<const FHughSavedMove*>(NewMove.Get());
	if (!NewHughMove)
	{
		return false;
	}

	if (bSavedWantsToAim != NewHughMove->bSavedWantsToAim
		|| bSavedWantsToRun != NewHughMove->bSavedWantsToRun
		|| bSavedWantsToHover != NewHughMove->bSavedWantsToHover
		|| bSavedJumpInputHeld != NewHughMove->bSavedJumpInputHeld)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}
