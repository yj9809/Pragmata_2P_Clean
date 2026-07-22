#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Data/PragmataTypes.h"
#include "AnimNotifyState_AttackHitWindow.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;
class FPrimitiveDrawInterface;
struct FAnimNotifyEvent;
struct FAnimNotifyEventReference;

struct FAttackHitWindowRuntime
{
	// 이전 tick의 socket 위치
	FVector PreviousSocketLocation = FVector::ZeroVector;

	// 같은 공격 window에서 이미 맞은 actor
	TSet<TObjectKey<AActor>> HitActors;

	// 공격 시작 시 source interface에서 조회한 damage
	float SourceDamageAmount = 0.f;
};

UCLASS(meta=(DisplayName="Attack Hit Window"))
class PRAGMATA_2P_API UAnimNotifyState_AttackHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 공격 window 시작 시 socket 위치와 hit set 초기화
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	// 공격 window 동안 socket sweep으로 melee hit 판정
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	// 공격 window 종료 시 runtime state 제거
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	// montage preview viewport에 runtime sweep과 같은 sphere 표시
	virtual void DrawInEditor(
		FPrimitiveDrawInterface* PDI,
		USkeletalMeshComponent* MeshComp,
		const UAnimSequenceBase* Animation,
		const FAnimNotifyEvent& NotifyEvent
	) const override;
#endif

private:
	// server 권한 actor만 damage를 확정
	bool CanRunServerHitWindow(const USkeletalMeshComponent* MeshComp) const;

	// owner interface에서 현재 공격 damage 조회
	bool TryGetSourceDamage(const USkeletalMeshComponent* MeshComp, float& OutDamageAmount) const;

	// socket이 없을 때 component 위치로 fallback
	FVector GetAttackSocketLocation(const USkeletalMeshComponent* MeshComp) const;

	// sweep 결과를 damage spec으로 변환해 target에게 전달
	void DealDamageFromHit(USkeletalMeshComponent* MeshComp, FAttackHitWindowRuntime& RuntimeState, const FHitResult& Hit) const;

	// 공격 판정 중심 socket
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	FName AttackSocketName = TEXT("AttackSocket");

	// socket local space에서 sweep sphere 중심을 세부 조정
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(Units="cm", AllowPrivateAccess="true"))
	FVector AttackSocketOffset = FVector::ZeroVector;

	// sweep sphere 반지름
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(ClampMin="0", UIMin="0", Units="cm", AllowPrivateAccess="true"))
	float HitRadius = 35.f;

	// montage preview viewport에 표시할 sphere 색상
	UPROPERTY(EditAnywhere, Category="Pragmata|Preview", meta=(AllowPrivateAccess="true"))
	FLinearColor PreviewColor = FLinearColor::Red;

	// 선택 시 notify window 주변에서만 preview sphere 표시
	UPROPERTY(EditAnywhere, Category="Pragmata|Preview", meta=(AllowPrivateAccess="true"))
	bool bPreviewOnlyNearNotifyTime = false;

	// notify window 전후에 preview sphere를 추가로 표시할 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Preview", meta=(ClampMin="0.0", UIMin="0.0", Units="s", EditCondition="bPreviewOnlyNearNotifyTime", AllowPrivateAccess="true"))
	float PreviewTimeTolerance = 0.033f;

	// melee trace channel
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	// 자기 자신을 sweep에서 제외
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	bool bIgnoreOwner = true;

	// damage 종류
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	EPragmataDamageKind DamageKind = EPragmataDamageKind::Melee;

	// hit react 세기
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	EPragmataHitReactStrength HitReactStrength = EPragmataHitReactStrength::Medium;

	// hit react 방향
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	EPragmataHitDirection HitDirection = EPragmataHitDirection::Front;

	// mesh별 notify runtime state
	TMap<TObjectKey<USkeletalMeshComponent>, FAttackHitWindowRuntime> ActiveWindows;
};
