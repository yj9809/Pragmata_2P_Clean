#include "UI/TutorialStepViewModel.h"

#include "Data/TutorialStepData.h"
#include "Engine/Texture2D.h"

// Model의 StepTitle/PreviewImage/DescriptionText+KeyBindings를 View 바인딩용 필드로 가공
void UTutorialStepViewModel::InitFromStepData(const UTutorialStepData* StepData)
{
	if (!StepData)
	{
		return;
	}

	SetTitle(StepData->GetStepTitle());
	SetPreviewImage(StepData->GetPreviewImage().LoadSynchronous());

	FString AssembledDescription = StepData->GetDescriptionText().ToString();
	for (const FTutorialKeyBinding& KeyBinding : StepData->GetKeyBindings())
	{
		AssembledDescription += FString::Printf(TEXT("\n%s"), *KeyBinding.ActionLabel.ToString());
		// 아이콘 row가 없는 IMC 입력은 텍스트 키 표기만 유지
		if (!KeyBinding.IconStyleRowName.IsNone())
		{
			AssembledDescription += FString::Printf(TEXT(": <img id=\"%s\"/>"), *KeyBinding.IconStyleRowName.ToString());
		}
	}

	SetDescription(FText::FromString(AssembledDescription));
}

void UTutorialStepViewModel::SetTitle(const FText& NewTitle)
{
	UE_MVVM_SET_PROPERTY_VALUE(Title, NewTitle);
}

void UTutorialStepViewModel::SetPreviewImage(UTexture2D* NewPreviewImage)
{
	UE_MVVM_SET_PROPERTY_VALUE(PreviewImage, NewPreviewImage);
}

void UTutorialStepViewModel::SetDescription(const FText& NewDescription)
{
	UE_MVVM_SET_PROPERTY_VALUE(Description, NewDescription);
}
