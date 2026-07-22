#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMetaData.h"
#include "UnreelMCPAnimMetaData.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class UNREELMCP_API UUnreelMCPAnimMetaData : public UAnimMetaData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnreelMCP")
	FString Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UnreelMCP")
	FString Value;
};
