#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Enemy/PragmataMonsterMovementComponent.h"
#include "AnimNotifyState_BodySweepProfile.generated.h"

class AMonsterBase;
class FPrimitiveDrawInterface;
struct FAnimNotifyEvent;

UENUM(BlueprintType)
enum class EPragmataBodySweepProfileSource : uint8
{
	MovementComponentProfile UMETA(DisplayName="Movement Component Profile"),
	NotifyOverride UMETA(DisplayName="Notify Override")
};

// NotifyState 안에서 sphere 하나가 이동할 시작·최대·복귀 위치
USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPragmataAnimatedBodySweepSphere
{
	GENERATED_BODY()

public:
	// Notify Override에서 사용할 sphere 반지름, BP profile source에서는 무시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float Radius = 30.f;

	// Notify 시작 시 capsule 기준 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep", meta=(Units="cm"))
	FVector StartLocalOffset = FVector::ZeroVector;

	// 이동 구간이 끝났을 때 도달할 capsule 기준 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep", meta=(Units="cm"))
	FVector MaxLocalOffset = FVector::ZeroVector;

	// 복귀 구간이 끝난 뒤 root motion과 함께 유지할 capsule 기준 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body Sweep", meta=(Units="cm"))
	FVector ReturnLocalOffset = FVector::ZeroVector;
};

struct FBodySweepProfileNotifyRuntime
{
	float ElapsedTime = 0.f;
	float TotalDuration = 0.f;
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
	FTransform InitialMeshRelativeTransform = FTransform::Identity;
	FVector InitialTrackingBoneOwnerLocalLocation = FVector::ZeroVector;
	FVector InitialFollowSphereLocalOffset = FVector::ZeroVector;
	FVector TrackingBoneToSphereLocalOffset = FVector::ZeroVector;
	FName ResolvedTrackingBoneName = NAME_None;
	bool bMeshConstraintInitialized = false;
};

UCLASS(meta=(DisplayName="Monster Body Sweep Profile"))
class PRAGMATA_2P_API UAnimNotifyState_BodySweepProfile : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 자세가 길어지기 직전에 해당 monster의 body sweep profile 활성화
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference
	) override;

	// root motion이 없는 pose 이동 구간의 profile offset을 매 frame sweep
	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference
	) override;

	// 자세가 원래 크기로 돌아온 뒤 이 notify가 활성화한 profile 해제
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	// 선택한 monster BP의 profile을 montage preview mesh 좌표로 변환해 표시
	virtual void DrawInEditor(
		FPrimitiveDrawInterface* PDI,
		USkeletalMeshComponent* MeshComp,
		const UAnimSequenceBase* Animation,
		const FAnimNotifyEvent& NotifyEvent
	) const override;
#endif

private:
	FPragmataMonsterBodySweepProfile MakeNotifyOverrideProfile() const;
	// notify 시작 이후 경과 시간을 sphere별 선형 이동·유지·복귀 위치로 변환
	void EvaluateAnimatedSphereOffsets(
		float ElapsedTime,
		const FPragmataMonsterBodySweepProfile& BaseProfile,
		TArray<FVector>& OutLocalOffsets
	) const;
	bool InitializeMeshConstraint(
		USkeletalMeshComponent* MeshComp,
		const UPragmataMonsterMovementComponent* Movement,
		FBodySweepProfileNotifyRuntime& RuntimeState
	) const;
	bool UpdateTrackingBoneDrivenSphereOffset(
		USkeletalMeshComponent* MeshComp,
		const FBodySweepProfileNotifyRuntime& RuntimeState,
		TArray<FVector>& InOutLocalOffsets
	) const;
	void ApplyMeshConstraint(
		USkeletalMeshComponent* MeshComp,
		const UPragmataMonsterMovementComponent* Movement,
		const FBodySweepProfileNotifyRuntime& RuntimeState
	) const;

	// BP profile 조회 또는 notify 전용 sphere 사용 방식 선택
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep", meta=(AllowPrivateAccess="true"))
	EPragmataBodySweepProfileSource ProfileSource = EPragmataBodySweepProfileSource::MovementComponentProfile;

	// 활성 stack 식별자이자 MovementComponent profile 조회 이름
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep", meta=(AllowPrivateAccess="true"))
	FName ProfileName = NAME_None;

	// Notify Override에서 runtime과 montage preview에 함께 사용할 sphere 목록
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep", meta=(EditCondition="ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride && !bUseAnimatedProfileOffset", EditConditionHides, AllowPrivateAccess="true"))
	TArray<FPragmataMonsterBodySweepSphere> OverrideSpheres;

	// Notify Override sphere의 runtime debug와 montage preview 색상
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep", meta=(EditCondition="ProfileSource == EPragmataBodySweepProfileSource::NotifyOverride", EditConditionHides, AllowPrivateAccess="true"))
	FLinearColor OverridePreviewColor = FLinearColor(1.f, 0.35f, 0.f, 1.f);

	// notify 초반 mesh-only 이동에 맞춰 sphere별 위치 전환 사용
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Animated Offset", meta=(AllowPrivateAccess="true"))
	bool bUseAnimatedProfileOffset = false;

	// 각 sphere의 시작·최대·복귀 위치, BP profile source에서는 index 순서로 적용
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Animated Offset", meta=(EditCondition="bUseAnimatedProfileOffset", EditConditionHides, AllowPrivateAccess="true"))
	TArray<FPragmataAnimatedBodySweepSphere> AnimatedSpheres;

	// notify 시작 후 offset 이동을 시작하기 전 대기 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Animated Offset", meta=(EditCondition="bUseAnimatedProfileOffset", EditConditionHides, ClampMin="0.0", UIMin="0.0", Units="s", AllowPrivateAccess="true"))
	float OffsetStartDelay = 0.f;

	// StartLocalOffset에서 MaxLocalOffset까지 선형 이동할 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Animated Offset", meta=(EditCondition="bUseAnimatedProfileOffset", EditConditionHides, ClampMin="0.0", UIMin="0.0", Units="s", AllowPrivateAccess="true"))
	float MoveToOffsetDuration = 0.15f;

	// MaxLocalOffset 위치를 유지할 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Animated Offset", meta=(EditCondition="bUseAnimatedProfileOffset", EditConditionHides, ClampMin="0.0", UIMin="0.0", Units="s", AllowPrivateAccess="true"))
	float HoldOffsetDuration = 0.f;

	// MaxLocalOffset에서 ReturnLocalOffset까지 선형 복귀할 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Animated Offset", meta=(EditCondition="bUseAnimatedProfileOffset", EditConditionHides, ClampMin="0.0", UIMin="0.0", Units="s", AllowPrivateAccess="true"))
	float ReturnToBaseDuration = 0.15f;

	// 원본 animation의 tracking bone이 충돌에 막힌 sphere보다 더 진행하지 않도록 mesh 위치 보정
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(AllowPrivateAccess="true"))
	bool bConstrainMeshMotionToBodySweep = false;

	// 수동 시간 설정 대신 tracking bone의 실제 pose 이동량으로 선택 sphere 구동
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(EditCondition="bConstrainMeshMotionToBodySweep && !bUseAnimatedProfileOffset", EditConditionHides, AllowPrivateAccess="true"))
	bool bDriveFollowSphereFromTrackingBone = false;

	// 선택 sphere를 tracking bone 위치에 붙이고 bone local offset으로 세부 조정
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(EditCondition="bConstrainMeshMotionToBodySweep && bDriveFollowSphereFromTrackingBone && !bUseAnimatedProfileOffset", EditConditionHides, AllowPrivateAccess="true"))
	bool bAttachFollowSphereToTrackingBone = false;

	// bone에 붙인 sphere의 bone local 기준 보정 위치
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(EditCondition="bConstrainMeshMotionToBodySweep && bDriveFollowSphereFromTrackingBone && bAttachFollowSphereToTrackingBone && !bUseAnimatedProfileOffset", EditConditionHides, Units="cm", AllowPrivateAccess="true"))
	FVector TrackingBoneSphereLocalOffset = FVector::ZeroVector;

	// tracking bone과 짝지을 활성 profile sphere index
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(EditCondition="bConstrainMeshMotionToBodySweep", EditConditionHides, ClampMin="0", AllowPrivateAccess="true"))
	int32 MeshFollowSphereIndex = 0;

	// sphere에 붙여둘 몸통 bone 또는 socket, None이면 skeleton의 첫 root bone 사용
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(EditCondition="bConstrainMeshMotionToBodySweep", EditConditionHides, AllowPrivateAccess="true"))
	FName MeshTrackingBoneName = NAME_None;

	// actor local XYZ 중 mesh 역보정을 허용할 축, 뒤/앞 이동만 막을 때 X=1 Y=0 Z=0
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Mesh Constraint", meta=(EditCondition="bConstrainMeshMotionToBodySweep", EditConditionHides, ClampMin="0.0", ClampMax="1.0", AllowPrivateAccess="true"))
	FVector MeshCompensationAxisMask = FVector(1.f, 0.f, 0.f);

#if WITH_EDITORONLY_DATA
	// Montage Editor에서 capsule과 mesh 상대 transform 및 BP profile을 가져올 monster class
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Preview", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<AMonsterBase> PreviewMonsterClass;

	// notify 구간과 그 주변에서만 preview sphere 표시
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Preview", meta=(AllowPrivateAccess="true"))
	bool bPreviewOnlyNearNotifyTime = true;

	// notify 시작·종료 전후 preview 허용 시간
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Preview", meta=(ClampMin="0.0", UIMin="0.0", Units="s", EditCondition="bPreviewOnlyNearNotifyTime", AllowPrivateAccess="true"))
	float PreviewTimeTolerance = 0.033f;

	// montage preview sphere 선 굵기
	UPROPERTY(EditAnywhere, Category="Pragmata|Monster|Body Sweep|Preview", meta=(ClampMin="0.0", UIMin="0.0", AllowPrivateAccess="true"))
	float PreviewLineThickness = 2.f;
#endif

	// mesh별 notify 진행 시간 유지
	TMap<TObjectKey<USkeletalMeshComponent>, FBodySweepProfileNotifyRuntime> ActiveOffsetWindows;
};
