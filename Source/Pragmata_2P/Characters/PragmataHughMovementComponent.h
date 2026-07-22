#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PragmataHughMovementComponent.generated.h"

class FHughNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	explicit FHughNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement);

	// Hugh saved move 타입으로 prediction buffer 생성
	virtual FSavedMovePtr AllocateNewMove() override;
};

class FHughSavedMove : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

public:
	virtual void Clear() override;
	virtual void SetInitialPosition(ACharacter* Character) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

	// client replay와 서버 압축 flag 생성에 쓰는 Hugh 입력 snapshot
	uint8 bSavedWantsToAim : 1;
	uint8 bSavedWantsToRun : 1;
	uint8 bSavedWantsToHover : 1;
	uint8 bSavedJumpInputHeld : 1;
};

UCLASS()
class PRAGMATA_2P_API UPragmataHughMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPragmataHughMovementComponent();

	// 로컬 입력 intent를 다음 saved move에 실을 상태로 저장
	void SetWantsToAim(bool bNewWantsToAim);
	void SetWantsToRun(bool bNewWantsToRun);
	void SetWantsToHover(bool bNewWantsToHover);
	void SetJumpInputHeld(bool bNewJumpInputHeld);

	bool WantsToAim() const { return bWantsToAim; }
	bool WantsToRun() const { return bWantsToRun; }
	bool WantsToHover() const { return bWantsToHover; }
	bool IsJumpInputHeld() const { return bJumpInputHeld; }

	// 서버가 압축 flag에서 Hugh 입력 intent를 복원
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	// 클라이언트 prediction data를 Hugh 전용 saved move로 교체
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
	uint8 bWantsToAim : 1;
	uint8 bWantsToRun : 1;
	uint8 bWantsToHover : 1;
	uint8 bJumpInputHeld : 1;
};
