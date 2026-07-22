#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "Data/PragmataTypes.h"
#include "DianaScanComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDianaScanResultSignature, const FPragmataScanResult&, ScanResult);

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UDianaScanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDianaScanComponent();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana|Scan")
	void RequestScan();

	UPROPERTY(BlueprintAssignable, Category="Pragmata|Diana|Scan")
	FDianaScanResultSignature OnScanResultUpdated;

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayScanAnimation();

	void PlayScanAnimation();
	UFUNCTION(Server, Reliable)
	void Server_RequestScan();

	bool ResolveAreaScanOrigin(FVector& OutScanOrigin) const;
	bool CanStartAreaScan(float CurrentTime, FString* OutRejectReason = nullptr) const;
	void CollectAreaScanResults(const FVector& ScanOrigin, TArray<FPragmataScanResult>& OutScanResults) const;
	void SendAreaScanResultsToPlayers(const TArray<FPragmataScanResult>& ScanResults, const FVector& ScanOrigin) const;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Scan|Animation")
	TSoftObjectPtr<class UAnimSequenceBase> ScanAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Scan", meta=(ClampMin="0"))
	float ScanRadius = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Scan", meta=(ClampMin="0"))
	float ScanDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Scan", meta=(ClampMin="0"))
	float ScanCooldownAfterDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Scan", meta=(ClampMin="1"))
	int32 MaxScanResults = 16;

	float NextAllowedScanTime = 0.f;
};
