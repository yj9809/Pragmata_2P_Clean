#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Data/PragmataTypes.h"
#include "AnimNotify_AttackHitOverlap.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;
struct FAnimNotifyEventReference;
struct FAnimNotifyEvent;
struct FOverlapResult;
class FPrimitiveDrawInterface;

UCLASS(meta=(DisplayName="Attack Hit Overlap"))
class PRAGMATA_2P_API UAnimNotify_AttackHitOverlap : public UAnimNotify
{
	GENERATED_BODY()

public:
	// 타격 순간 socket 중심 overlap으로 melee hit 판정
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

#if WITH_EDITOR
	// 몽타주 preview viewport에 socket 기준 overlap box 표시
	virtual void DrawInEditor(
		FPrimitiveDrawInterface* PDI,
		USkeletalMeshComponent* MeshComp,
		const UAnimSequenceBase* Animation,
		const FAnimNotifyEvent& NotifyEvent
	) const override;
#endif

private:
	// server 권한 actor만 damage를 확정
	bool CanRunServerHitOverlap(const USkeletalMeshComponent* MeshComp) const;

	// owner interface에서 현재 공격 damage 조회
	bool TryGetSourceDamage(const USkeletalMeshComponent* MeshComp, float& OutDamageAmount) const;

	// 설정된 socket이 유효할 때 overlap world transform 반환
	bool TryGetOverlapTransform(const USkeletalMeshComponent* MeshComp, FTransform& OutTransform) const;

	// overlap 결과를 damage spec으로 변환해 target에게 전달
	void DealDamageFromOverlap(
		USkeletalMeshComponent* MeshComp,
		const FOverlapResult& Overlap,
		const FVector& OverlapLocation,
		float SourceDamageAmount
	) const;

	// 공격 판정 중심 socket, None이면 mesh component 위치 사용
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	FName AttackSocketName = TEXT("AttackSocket");

	// 순간 overlap box의 축별 half extent
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(ClampMin="0", UIMin="0", AllowPrivateAccess="true"))
	FVector HitBoxExtent = FVector(35.f);

	// montage preview viewport에 표시할 box 색상
	UPROPERTY(EditAnywhere, Category="Pragmata|Preview", meta=(AllowPrivateAccess="true"))
	FLinearColor PreviewColor = FLinearColor::Red;

	// 선택 시 notify 시점 주변에서만 preview box 표시
	UPROPERTY(EditAnywhere, Category="Pragmata|Preview", meta=(AllowPrivateAccess="true"))
	bool bPreviewOnlyNearNotifyTime = false;

	// notify 시점 전후에 preview box를 표시할 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Preview", meta=(ClampMin="0.0", UIMin="0.0", Units="s", EditCondition="bPreviewOnlyNearNotifyTime", AllowPrivateAccess="true"))
	float PreviewTimeTolerance = 0.033f;

	// melee overlap channel
	UPROPERTY(EditAnywhere, Category="Pragmata|Damage", meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	// 자기 자신을 overlap에서 제외
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
};
