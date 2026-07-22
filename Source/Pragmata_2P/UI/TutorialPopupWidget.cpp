#include "UI/TutorialPopupWidget.h"

#include "MVVMSubsystem.h"
#include "UI/TutorialStepViewModel.h"
#include "View/MVVMView.h"

const FName UTutorialPopupWidget::ViewModelName = TEXT("TutorialStepViewModel");

// WBP가 Create Instance로 만들어둔 ViewModel을 조회해서 StepData만 채움 (다른 ViewModel들과 동일한 리졸브 패턴)
bool UTutorialPopupWidget::InitializeStep(const UTutorialStepData* StepData)
{
	if (!StepData)
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialPopupWidget::InitializeStep] WBP에 MVVM View/Binding이 없어 View extension을 찾지 못함"));
		return false;
	}

	UTutorialStepViewModel* ViewModel = Cast<UTutorialStepViewModel>(View->GetViewModel(ViewModelName).GetObject());
	if (!ViewModel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UTutorialPopupWidget::InitializeStep] WBP에서 TutorialStepViewModel을 찾지 못함 (View Models 패널 Creation Type=Create Instance 확인 필요)"));
		return false;
	}

	ViewModel->InitFromStepData(StepData);
	return true;
}
