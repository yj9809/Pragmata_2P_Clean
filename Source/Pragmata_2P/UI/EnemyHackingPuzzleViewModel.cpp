// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/EnemyHackingPuzzleViewModel.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"
#include "Enemy/MonsterBase.h"

#include "GameFramework/Actor.h"

void UEnemyHackingPuzzleViewModel::SetHackTarget(AActor* TargetActor)
{
	// 타겟이 없으면 해제
	if (!TargetActor)
	{
		UnbindFromComponent();
		return;
	}

	// 타겟 적의 퍼즐 컴포넌트를 찾아 바인딩. 없으면(해킹 불가 액터 등) 해제.
	UEnemyHackingPuzzleComponent* PuzzleComp = TargetActor->FindComponentByClass<UEnemyHackingPuzzleComponent>();
	if (!PuzzleComp)
	{
		UnbindFromComponent();
		return;
	}

	BindToComponent(PuzzleComp);
}

void UEnemyHackingPuzzleViewModel::BindToComponent(UEnemyHackingPuzzleComponent* PuzzleComp)
{
	UnbindFromComponent();

	if (!PuzzleComp) return;

	BoundPuzzleComp = PuzzleComp;

	PuzzleComp->OnHackStarted.AddUObject(this, &UEnemyHackingPuzzleViewModel::HandleHackStarted);
	PuzzleComp->OnGridReady.AddUObject(this, &UEnemyHackingPuzzleViewModel::HandleGridReady);
	PuzzleComp->OnCursorMoved.AddUObject(this, &UEnemyHackingPuzzleViewModel::HandleCursorMoved);
	PuzzleComp->OnHackCompleted.AddUObject(this, &UEnemyHackingPuzzleViewModel::HandleHackCompleted);
	PuzzleComp->OnHackCancelled.AddUObject(this, &UEnemyHackingPuzzleViewModel::HandleHackCancelled);

	SyncFromComponent();
}

void UEnemyHackingPuzzleViewModel::UnbindFromComponent()
{
	if (BoundPuzzleComp.IsValid())
	{
		BoundPuzzleComp->OnHackStarted.RemoveAll(this);
		BoundPuzzleComp->OnGridReady.RemoveAll(this);
		BoundPuzzleComp->OnCursorMoved.RemoveAll(this);
		BoundPuzzleComp->OnHackCompleted.RemoveAll(this);
		BoundPuzzleComp->OnHackCancelled.RemoveAll(this);
	}
	BoundPuzzleComp.Reset();
	SetIsActive(false);
	SetGridSize(0);
	SetNormalNodesVisited(0);
	SetTotalNormalNodes(0);
	SetNormalProgressRatio(0.f);
	SetProjectedDamage(0.f);
	SetProjectedDamageText(FText::AsNumber(0));
	SetHackTimeRemaining(0.f, 1.f);
	SetHackCompletedHighlight(false);
	SetTargetHacked(false);
	CachedNodeTypes.Empty();
	CachedCellStates.Empty();
}

void UEnemyHackingPuzzleViewModel::SetHackTimeRemaining(float RemainingSeconds, float DurationSeconds, float FullGaugeRatio)
{
	const float SafeDuration = FMath::Max(DurationSeconds, UE_KINDA_SMALL_NUMBER);
	const float ClampedRemaining = FMath::Clamp(RemainingSeconds, 0.f, SafeDuration);
	const float ClampedGaugeRatio = FMath::Clamp(FullGaugeRatio, 0.f, 1.f);
	SetHackTimeRemainingSeconds(ClampedRemaining);
	SetHackTimeRemainingRatio((ClampedRemaining / SafeDuration) * ClampedGaugeRatio);
}

float UEnemyHackingPuzzleViewModel::GetHackTargetRemainingSeconds() const
{
	if (!BoundPuzzleComp.IsValid())
	{
		return 0.f;
	}
	const AMonsterBase* Monster = Cast<AMonsterBase>(BoundPuzzleComp->GetOwner());
	return Monster ? Monster->GetHackedRemainingSeconds() : 0.f;
}

float UEnemyHackingPuzzleViewModel::GetHackTargetDuration() const
{
	if (!BoundPuzzleComp.IsValid())
	{
		return 0.f;
	}
	const AMonsterBase* Monster = Cast<AMonsterBase>(BoundPuzzleComp->GetOwner());
	return Monster ? Monster->GetAppliedHackDuration() : 0.f;
}

float UEnemyHackingPuzzleViewModel::GetMaxHackDurationSeconds() const
{
	return BoundPuzzleComp.IsValid() ? BoundPuzzleComp->GetMaxHackDurationSeconds() : 0.f;
}

void UEnemyHackingPuzzleViewModel::StartEnemyHacking(UEnemyHackingPuzzleComponent* PuzzleComp)
{
	BindToComponent(PuzzleComp);

	// deprecated BP wrapper의 기존 standalone 동작 유지
	if (BoundPuzzleComp.IsValid())
	{
		BoundPuzzleComp->StartHack();
	}
}

void UEnemyHackingPuzzleViewModel::StopEnemyHacking()
{
	// deprecated BP wrapper의 기존 hard-cancel 동작 유지
	if (BoundPuzzleComp.IsValid())
	{
		BoundPuzzleComp->CancelHack();
	}

	UnbindFromComponent();
}

void UEnemyHackingPuzzleViewModel::OnCompletedAnimationFinished()
{
	// WBP 완료 애니 callback은 재타겟 후 늦게 도착할 수 있어 새 Active puzzle을 숨기지 않음
	if (IsBoundPuzzleActive())
	{
		return;
	}

	SetIsActive(false);
}

void UEnemyHackingPuzzleViewModel::OnCancelledAnimationFinished()
{
	// WBP 취소 애니 callback은 재타겟 후 늦게 도착할 수 있어 새 Active puzzle을 숨기지 않음
	if (IsBoundPuzzleActive())
	{
		return;
	}

	SetIsActive(false);
}

void UEnemyHackingPuzzleViewModel::HandleHackStarted()
{
	SetIsActive(true);
	SetCorruptedPending(false);
	SetHackCompletedHighlight(false);
	// 시작 시점의 타겟 hacked 여부를 고정 — 트리거 발동 전에 확정해 셀이 최신 값을 읽게 함
	SetTargetHacked(BoundPuzzleComp.IsValid() && BoundPuzzleComp->IsOwnerHacked());
	SetStartedTrigger(!bStartedTrigger);
}

void UEnemyHackingPuzzleViewModel::HandleGridReady(int32 InGridSize)
{
	SetGridSize(InGridSize);

	if (BoundPuzzleComp.IsValid())
	{
		SetTotalNormalNodes(BoundPuzzleComp->GetTotalNormalNodes());
		SetNormalNodesVisited(BoundPuzzleComp->GetNormalNodesVisited());
		UpdateNormalProgress();
		UpdateProjectedDamage();
	}

	RebuildCellCache();
}

void UEnemyHackingPuzzleViewModel::HandleCursorMoved(FIntPoint NewPos, bool bNewCorruptedPending)
{
	SetCursorRow(NewPos.X);
	SetCursorCol(NewPos.Y);
	SetCorruptedPending(bNewCorruptedPending);

	if (BoundPuzzleComp.IsValid())
	{
		SetNormalNodesVisited(BoundPuzzleComp->GetNormalNodesVisited());
		UpdateNormalProgress();
		UpdateProjectedDamage();
	}

	RebuildCellCache();
}

void UEnemyHackingPuzzleViewModel::HandleHackCompleted()
{
	// 완료 trigger 전에 최종 grid/cursor snapshot을 반영해 client 복제 순서 차이를 흡수
	SyncVisualSnapshotFromComponent();

	// 완료 시점의 최종 데미지를 확정해 진입점에 알림 (진입점이 서버 RPC로 실제 적용)
	UpdateProjectedDamage();

	// 복제 순서와 무관하게 최종 노드 수 기준 지속시간 채움을 확정. 몬스터의 hacked 타이밍이
	// 복제돼 오기 전(위젯 tick이 잔량을 받기 전)에도 게이지가 올바른 시작값에서 출발하게 함.
	if (BoundPuzzleComp.IsValid())
	{
		SetNormalNodesVisited(BoundPuzzleComp->GetNormalNodesVisited());
		UpdateNormalProgress();
	}
	SetHackTimeRemainingRatio(DurationFillRatio);	
	SetHackCompletedHighlight(true);
	OnHackCompletedWithDamage.Broadcast(ProjectedDamage);
	SetCompletedTrigger(!bCompletedTrigger);
	// SetIsActive(false)는 OnCompletedAnimationFinished()에서 호출
}

void UEnemyHackingPuzzleViewModel::HandleHackCancelled()
{
	// 이전 cancel 복제가 새 Active 복제 뒤에 처리되면 새 세션의 cancel 연출을 시작하지 않음
	if (IsBoundPuzzleActive())
	{
		return;
	}

	// IsActive(false)는 OnCancelledAnimationFinished()에서 호출 — 취소 애니 재생 후 패널 숨김
	SetHackTimeRemaining(0.f, 1.f);
	SetHackCompletedHighlight(false);
	SetCancelledTrigger(!bCancelledTrigger);
}

void UEnemyHackingPuzzleViewModel::SyncFromComponent()
{
	if (!BoundPuzzleComp.IsValid()) return;

	const EEnemyHackState PuzzleState = BoundPuzzleComp->GetPuzzleState();
	if (PuzzleState == EEnemyHackState::Idle)
	{
		SetIsActive(false);
		SetHackTimeRemaining(0.f, 1.f);
		SetHackCompletedHighlight(false);
		return;
	}

	SetIsActive(true);
	// 늦은/호스트 바인딩에서도 트리거 발동 전에 타겟 hacked 여부를 확정
	SetTargetHacked(BoundPuzzleComp->IsOwnerHacked());
	if (PuzzleState == EEnemyHackState::Active)
	{
		// listen server HUD는 시작 delegate를 놓칠 수 있어 현재 active 상태 동기화 때도 시작 trigger를 갱신
		SetHackCompletedHighlight(false);
		SetStartedTrigger(!bStartedTrigger);
	}
	SyncVisualSnapshotFromComponent();

	if (PuzzleState == EEnemyHackState::Completed)
	{
		SetHackCompletedHighlight(true);
		SetCompletedTrigger(!bCompletedTrigger);
	}
}

void UEnemyHackingPuzzleViewModel::SyncVisualSnapshotFromComponent()
{
	if (!BoundPuzzleComp.IsValid()) return;

	// 복제 property 도착 순서와 관계없이 UI cell cache가 component 최종 상태를 기준으로 갱신되게 함
	SetGridSize(BoundPuzzleComp->GetCurrentGridSize());

	const FIntPoint CurrentCursorPos = BoundPuzzleComp->GetCursorPos();
	SetCursorRow(CurrentCursorPos.X);
	SetCursorCol(CurrentCursorPos.Y);
	SetCorruptedPending(BoundPuzzleComp->IsCorruptedPending());
	SetTotalNormalNodes(BoundPuzzleComp->GetTotalNormalNodes());
	SetNormalNodesVisited(BoundPuzzleComp->GetNormalNodesVisited());
	UpdateNormalProgress();
	UpdateProjectedDamage();
	RebuildCellCache();
}

void UEnemyHackingPuzzleViewModel::RebuildCellCache()
{
	if (!BoundPuzzleComp.IsValid()) return;

	const int32 TotalCells = GridSize * GridSize;
	CachedNodeTypes.SetNum(TotalCells);
	CachedCellStates.SetNum(TotalCells);

	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			const int32 Index = Row * GridSize + Col;
			CachedNodeTypes[Index] = BoundPuzzleComp->GetNodeType(Row, Col);
			CachedCellStates[Index] = BoundPuzzleComp->GetCellState(Row, Col);
		}
	}

	SetGridRefreshTrigger(!bGridRefreshTrigger);
}

bool UEnemyHackingPuzzleViewModel::IsBoundPuzzleActive() const
{
	// 현재 bind된 component 상태를 최종 기준으로 삼아 stale UI callback을 구분
	return BoundPuzzleComp.IsValid() && BoundPuzzleComp->GetPuzzleState() == EEnemyHackState::Active;
}

EHackNodeType UEnemyHackingPuzzleViewModel::GetNodeTypeAt(int32 Index) const
{
	return CachedNodeTypes.IsValidIndex(Index) ? CachedNodeTypes[Index] : EHackNodeType::Normal;
}

EHackCellState UEnemyHackingPuzzleViewModel::GetCellStateAt(int32 Index) const
{
	return CachedCellStates.IsValidIndex(Index) ? CachedCellStates[Index] : EHackCellState::Default;
}

void UEnemyHackingPuzzleViewModel::SetIsActive(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsActive, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetGridSize(int32 NewSize)
{
	UE_MVVM_SET_PROPERTY_VALUE(GridSize, NewSize);
}

void UEnemyHackingPuzzleViewModel::SetCursorRow(int32 NewRow)
{
	UE_MVVM_SET_PROPERTY_VALUE(CursorRow, NewRow);
}

void UEnemyHackingPuzzleViewModel::SetCursorCol(int32 NewCol)
{
	UE_MVVM_SET_PROPERTY_VALUE(CursorCol, NewCol);
}

void UEnemyHackingPuzzleViewModel::SetCompletedTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCompletedTrigger, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetStartedTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bStartedTrigger, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetCancelledTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCancelledTrigger, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetCorruptedPending(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCorruptedPending, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetTargetHacked(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bTargetHacked, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetGridRefreshTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bGridRefreshTrigger, bNewValue);
}

void UEnemyHackingPuzzleViewModel::SetNormalNodesVisited(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(NormalNodesVisited, NewValue);
}

void UEnemyHackingPuzzleViewModel::SetTotalNormalNodes(int32 NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(TotalNormalNodes, NewValue);
}

void UEnemyHackingPuzzleViewModel::SetNormalProgressRatio(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(NormalProgressRatio, NewValue);
	RefreshDoorGaugeRatio();
}

void UEnemyHackingPuzzleViewModel::SetDoorGaugeRatio(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(DoorGaugeRatio, FMath::Clamp(NewValue, 0.f, 1.f));
}

void UEnemyHackingPuzzleViewModel::UpdateNormalProgress()
{
	const float Ratio = (TotalNormalNodes > 0)
		? static_cast<float>(NormalNodesVisited) / static_cast<float>(TotalNormalNodes)
		: 0.f;
	SetNormalProgressRatio(Ratio);
	
	// 게이지 증가량을 지속시간 차등에 맞춤 — 노드 개수 비율이 아닌 예상 지속시간 / 상한.
	if (BoundPuzzleComp.IsValid())
	{
		const float MaxDuration = FMath::Max(BoundPuzzleComp->GetMaxHackDurationSeconds(), UE_KINDA_SMALL_NUMBER);
		DurationFillRatio = FMath::Clamp(BoundPuzzleComp->GetEarnedHackDuration() / MaxDuration, 0.f, 1.f);
	}
	else
	{
		DurationFillRatio = 0.f;
	}
	RefreshDoorGaugeRatio();
}

void UEnemyHackingPuzzleViewModel::SetProjectedDamage(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectedDamage, NewValue);
}

void UEnemyHackingPuzzleViewModel::SetProjectedDamageText(const FText& NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(ProjectedDamageText, NewValue);
}

void UEnemyHackingPuzzleViewModel::SetHackTimeRemainingRatio(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(HackTimeRemainingRatio, FMath::Clamp(NewValue, 0.f, 1.f));
	RefreshDoorGaugeRatio();
}

void UEnemyHackingPuzzleViewModel::SetHackTimeRemainingSeconds(float NewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(HackTimeRemainingSeconds, FMath::Max(0.f, NewValue));
}

void UEnemyHackingPuzzleViewModel::SetHackCompletedHighlight(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHackCompletedHighlight, bNewValue);
	RefreshDoorGaugeRatio();
}

void UEnemyHackingPuzzleViewModel::RefreshDoorGaugeRatio()
{
	// 단계 판정은 완료 하이라이트 플래그가 아니라 몬스터의 복제된 hacked 상태(진실원)를 기준으로.
	// 이래야 UI를 닫았다 열어도(플래그가 초기화돼도) 실제 잔여 지속시간을 그대로 이어서 표시한다.
	if (GetHackTargetDuration() > UE_KINDA_SMALL_NUMBER)
	{
		// hacked 지속시간 진행 중 → 카운트다운(잔여/상한). tick이 매 프레임 갱신.
		SetDoorGaugeRatio(HackTimeRemainingRatio);
	}
	else if (IsBoundPuzzleActive())
	{
		// 퍼즐 진행 중 → 노드 진행률(방문/전체). 0노드면 0으로 시작.
		SetDoorGaugeRatio(DurationFillRatio);
	}
	else
	{
		// 완료 후 만료 / idle → 게이지 비움.
		SetDoorGaugeRatio(0.f);
	}
}

void UEnemyHackingPuzzleViewModel::UpdateProjectedDamage()
{
	const float NewDamage = BoundPuzzleComp.IsValid() ? BoundPuzzleComp->GetProjectedDamage() : 0.f;
	SetProjectedDamage(NewDamage);
	SetProjectedDamageText(FText::AsNumber(FMath::RoundToInt(NewDamage)));
}
