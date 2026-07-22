#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/PragmataTypes.h"
#include "DialogueWidget.generated.h"

class UTextBlock;
class UWidget;

// 대사 한 줄이 자연 종료됐을 때 다음 줄 진행 타이밍을 알림
// 수동 HideDialogue로 끊은 경우에는 broadcast하지 않음
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueLineFinished, const FDialogueLine&, FinishedLine);

// 화자와 대사를 표시하는 자막형 위젯
// 표시 순서와 타이밍은 게임 로직이 PlayLine으로 밀어넣고, 레이아웃은 WBP가 담당
UCLASS()
class PRAGMATA_2P_API UDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임 로직이 타이밍에 맞춰 한 줄을 표시
	UFUNCTION(BlueprintCallable, Category="Pragmata|Dialogue")
	void PlayLine(const FDialogueLine& InLine, float DisplaySeconds = -1.f);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Dialogue")
	void HideDialogue();

	// 표시 시간이 자연 종료됐을 때만 대사 시퀀스 제어에 broadcast
	UPROPERTY(BlueprintAssignable, Category="Pragmata|Dialogue")
	FOnDialogueLineFinished OnLineFinished;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP가 타이프라이터/페이드 인 등 자체 연출을 붙이는 hook
	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Dialogue")
	void OnLinePlayed(const FDialogueLine& InLine);

private:
	void ApplyDialogueVisibility(ESlateVisibility NewVisibility);

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UWidget> Root = nullptr;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> SpeakerText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UTextBlock> LineText = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Dialogue", meta=(AllowPrivateAccess="true", ClampMin="0.1", Units="s"))
	float DefaultDisplaySeconds = 4.f;

	FDialogueLine CurrentLine;
	float HideAtWorldSeconds = 0.f;
	bool bDialogueVisible = false;
};
