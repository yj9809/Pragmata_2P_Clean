// Fill out your copyright notice in the Description page of Project Settings.

#include "Puzzle/EnemyHackingPuzzleComponent.h"

#include "Enemy/MonsterBase.h"
#include "Net/UnrealNetwork.h"

UEnemyHackingPuzzleComponent::UEnemyHackingPuzzleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	FEnemyHackDurationProfile& WatcherProfile = HackDurationProfiles.AddDefaulted_GetRef();
	WatcherProfile.EnemyType = EEnemyHackType::Watcher;
	WatcherProfile.DurationSecondsByVisitedNodeCount = { 6.f, 15.f };

	FEnemyHackDurationProfile& WalkerProfile = HackDurationProfiles.AddDefaulted_GetRef();
	WalkerProfile.EnemyType = EEnemyHackType::Walker;
	WalkerProfile.DurationSecondsByVisitedNodeCount = { 2.06f, 6.71f, 15.f };

	FEnemyHackDurationProfile& ExecutorProfile = HackDurationProfiles.AddDefaulted_GetRef();
	ExecutorProfile.EnemyType = EEnemyHackType::Executor;
	ExecutorProfile.DurationSecondsByVisitedNodeCount = { 1.10f, 4.55f, 9.10f, 15.f };

	FEnemyHackDurationProfile& SentinelProfile = HackDurationProfiles.AddDefaulted_GetRef();
	SentinelProfile.EnemyType = EEnemyHackType::Sentinel;
	SentinelProfile.DurationSecondsByVisitedNodeCount = { 1.f, 2.5f, 5.5f, 10.f, 15.f };
}

bool UEnemyHackingPuzzleComponent::IsOwnerHacked() const
{
	// 이 컴포넌트는 MonsterBase에 부착되며, hacked 상태는 소유 적이 단일 소스로 보유(복제됨)
	const AMonsterBase* OwnerMonster = Cast<AMonsterBase>(GetOwner());
	return OwnerMonster && OwnerMonster->IsHacked();
}

void UEnemyHackingPuzzleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnemyHackingPuzzleComponent, PuzzleState);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, CursorRepCounter);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, CachedGridSize);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, Grid);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, CursorPos);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, TargetPos);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, TotalNormalNodes);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, NormalNodesVisited);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, bDecodeNodeVisited);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, BaseHackDamage);
	DOREPLIFETIME(UEnemyHackingPuzzleComponent, bCorruptedPending);
}

bool UEnemyHackingPuzzleComponent::HasPuzzleAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

int32 UEnemyHackingPuzzleComponent::GetGridSize() const
{
	switch (EnemyType)
	{
	case EEnemyHackType::Watcher: return 3;
	case EEnemyHackType::Walker: return 4;
	case EEnemyHackType::Executor: return 5;
	case EEnemyHackType::Sentinel: return 8;
	default: return 4;
	}
}

void UEnemyHackingPuzzleComponent::SetBaseHackDamage(float InBaseHackDamage)
{
	if (!HasPuzzleAuthority()) return;
	BaseHackDamage = FMath::Max(0.f, InBaseHackDamage);
}

float UEnemyHackingPuzzleComponent::GetProjectedDamage() const
{
	// 기본 데미지에 가산분을 누적하되, 가산분이 매 노드 절반씩 감소.
	// 첫 노드 +base*1.0(총 ×2), 다음 +0.5, +0.25 ... (총배수 1→2→2.5→2.75, 3배 수렴)
	float TotalMultiplier = 1.f;
	float StepBonus = FirstNodeDamageMultiplier - 1.f;
	for (int32 i = 0; i < NormalNodesVisited; ++i)
	{
		TotalMultiplier += StepBonus;
		StepBonus *= 0.5f;
	}
	return BaseHackDamage * TotalMultiplier;
}

// 완료 경로의 파란 노드 수를 tier로 쓰며 초과 값은 마지막 기획 값으로 고정
float UEnemyHackingPuzzleComponent::GetResolvedHackDurationSeconds() const
{
	const FEnemyHackDurationProfile* MatchingProfile = nullptr;
	for (const FEnemyHackDurationProfile& Profile : HackDurationProfiles)
	{
		if (Profile.EnemyType != EnemyType)
		{
			continue;
		}

		if (MatchingProfile)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UEnemyHackingPuzzleComponent::GetResolvedHackDurationSeconds] Duplicate duration profile for EnemyType=%d"), static_cast<int32>(EnemyType));
			return 0.f;
		}

		MatchingProfile = &Profile;
	}

	if (!MatchingProfile || MatchingProfile->DurationSecondsByVisitedNodeCount.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEnemyHackingPuzzleComponent::GetResolvedHackDurationSeconds] Missing duration profile for EnemyType=%d"), static_cast<int32>(EnemyType));
		return 0.f;
	}

	const int32 DurationIndex = FMath::Clamp(NormalNodesVisited, 0, MatchingProfile->DurationSecondsByVisitedNodeCount.Num() - 1);
	const float ResolvedDurationSeconds = MatchingProfile->DurationSecondsByVisitedNodeCount[DurationIndex];
	if (ResolvedDurationSeconds <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UEnemyHackingPuzzleComponent::GetResolvedHackDurationSeconds] Invalid duration %.2f for EnemyType=%d NodeCount=%d"), ResolvedDurationSeconds, static_cast<int32>(EnemyType), NormalNodesVisited);
		return 0.f;
	}

	return ResolvedDurationSeconds;
}

float UEnemyHackingPuzzleComponent::GetEarnedHackDuration() const
{
	// UI 예상 게이지와 완료 시 gameplay timer가 같은 profile resolver를 사용
	return GetResolvedHackDurationSeconds();
}

bool UEnemyHackingPuzzleComponent::StartHack()
{
	if (!HasPuzzleAuthority()) return false;
	if (PuzzleState == EEnemyHackState::Active) return false;

	InitGrid();
	PuzzleState = EEnemyHackState::Active;
	CorruptedPendingDir = EHackDirection::None;
	CorruptedPendingCount = 0;
	bCorruptedPending = false;

	// 서버(리슨 서버 호스트) 로컬 표시. 원격 클라는 OnRep_PuzzleState에서 발동.
	BroadcastStarted();
	return true;
}

void UEnemyHackingPuzzleComponent::InitGrid()
{
	CachedGridSize = GetGridSize();
	const int32 TotalCells = CachedGridSize * CachedGridSize;

	Grid.Reset(TotalCells);
	Grid.SetNum(TotalCells);

	// 파란 노드 통계 리셋
	TotalNormalNodes = 0;
	NormalNodesVisited = 0;

	// 노란(Decode) 노드 방문 상태 리셋
	bDecodeNodeVisited = false;

	// 커서 시작: 항상 좌측 상단 (0, 0)
	CursorPos = FIntPoint(0, 0);
	GetCell(0, 0).CellState = EHackCellState::Cursor;

	// 경로 초기화 (시작 칸이 경로의 첫 칸)
	Path.Reset();
	Path.Add(CursorPos);

	// 타겟: 커서와 다른 랜덤 위치
	do
	{
		TargetPos = FIntPoint(FMath::RandRange(0, CachedGridSize - 1), FMath::RandRange(0, CachedGridSize - 1));
	}
	while (TargetPos == CursorPos);

	GetCell(TargetPos.X, TargetPos.Y).NodeType = EHackNodeType::Target;

	// 노란(Decode) 노드: 그리드당 항상 1개. 커서 시작칸(0,0)은 NodeType이 Empty지만 MoveCursor가
	// 그 칸에선 실행되지 않아 방문 플래그가 안 잡히므로 반드시 제외. 빈 칸 목록에서 균등 랜덤 선택해 확정 배치.
	{
		TArray<FIntPoint> FreeCells;
		FreeCells.Reserve(TotalCells);
		for (int32 Row = 0; Row < CachedGridSize; ++Row)
		{
			for (int32 Col = 0; Col < CachedGridSize; ++Col)
			{
				const FIntPoint Pos(Row, Col);
				if (Pos == CursorPos || GetCell(Row, Col).NodeType != EHackNodeType::Empty)
				{
					continue;
				}
				FreeCells.Add(Pos);
			}
		}
		if (FreeCells.Num() > 0)
		{
			const FIntPoint DecodePos = FreeCells[FMath::RandRange(0, FreeCells.Num() - 1)];
			GetCell(DecodePos.X, DecodePos.Y).NodeType = EHackNodeType::Decode;
		}
	}

	// 파란 노드(Normal): 그리드 크기와 동일한 개수 (3x3→3, 4x4→4, 6x6→6, 8x8→8)
	const int32 NormalCount = CachedGridSize;
	for (int32 i = 0; i < NormalCount; ++i)
	{
		FIntPoint Pos;
		int32 Attempts = 0;
		do
		{
			Pos = FIntPoint(FMath::RandRange(0, CachedGridSize - 1), FMath::RandRange(0, CachedGridSize - 1));
			++Attempts;
		}
		while ((Pos == CursorPos || Pos == TargetPos || GetCell(Pos.X, Pos.Y).NodeType != EHackNodeType::Empty) &&
			Attempts < 30);

		if (Attempts < 30)
		{
			GetCell(Pos.X, Pos.Y).NodeType = EHackNodeType::Normal;
			++TotalNormalNodes;
		}
	}

	// 정화 완료 적은 다음 puzzle부터 Corrupted node를 생성하지 않음
	const AMonsterBase* OwnerMonster = Cast<AMonsterBase>(GetOwner());
	if (OwnerMonster && OwnerMonster->IsTainted())
	{
		const int32 CorruptedCount = FMath::Max(1, TotalCells / 8);
		for (int32 i = 0; i < CorruptedCount; ++i)
		{
			FIntPoint Pos;
			int32 Attempts = 0;
			do
			{
				Pos = FIntPoint(FMath::RandRange(0, CachedGridSize - 1), FMath::RandRange(0, CachedGridSize - 1));
				++Attempts;
			}
			while ((Pos == CursorPos || Pos == TargetPos || GetCell(Pos.X, Pos.Y).NodeType != EHackNodeType::Empty) &&
				Attempts < 20);

			if (Attempts < 20)
			{
				GetCell(Pos.X, Pos.Y).NodeType = EHackNodeType::Corrupted;
			}
		}
	}
}

void UEnemyHackingPuzzleComponent::ProcessDirectionInput(EHackDirection InputDirection)
{
	if (!HasPuzzleAuthority()) return;
	if (PuzzleState != EEnemyHackState::Active) return;
	if (InputDirection == EHackDirection::None) return;

	const FIntPoint NewPos = CursorPos + DirectionToOffset(InputDirection);
	if (!IsValidPos(NewPos))
	{
		// 경계 밖: 진입 대기 입력 초기화
		CorruptedPendingDir = EHackDirection::None;
		CorruptedPendingCount = 0;
		if (bCorruptedPending)
		{
			bCorruptedPending = false;
			NotifyCursorMoved();
		}
		return;
	}

	// 새로 진입하려는 칸이 Corrupted면: 같은 방향 2회 연속 입력해야 진입.
	// (백트래킹 등 이미 지나온 칸은 게이트 없이 통과)
	const bool bEnteringCorrupted =
		GetCell(NewPos.X, NewPos.Y).NodeType == EHackNodeType::Corrupted && !Path.Contains(NewPos);

	if (bEnteringCorrupted)
	{
		if (CorruptedPendingDir == InputDirection)
		{
			CorruptedPendingCount++;
			if (CorruptedPendingCount >= 2)
			{
				// 2회째 — 진입
				CorruptedPendingDir = EHackDirection::None;
				CorruptedPendingCount = 0;
				MoveCursor(NewPos);
			}
			else
			{
				// 1회째 — 진입 대기 표시, 아직 이동 없음
				bCorruptedPending = true;
				NotifyCursorMoved();
			}
		}
		else
		{
			// 다른 방향 입력: 새 방향으로 1회 카운트 시작
			CorruptedPendingDir = InputDirection;
			CorruptedPendingCount = 1;
			bCorruptedPending = true;
			NotifyCursorMoved();
		}
		return;
	}

	// 일반 이동: Corrupted 진입이 아니면 즉시 이동
	CorruptedPendingDir = EHackDirection::None;
	CorruptedPendingCount = 0;
	MoveCursor(NewPos);
}

void UEnemyHackingPuzzleComponent::MoveCursor(FIntPoint NewPos)
{
	const int32 ExistingIdx = Path.IndexOfByKey(NewPos);

	if (ExistingIdx != INDEX_NONE)
	{
		// 이미 지나온 칸: 바로 직전 칸(경로의 끝에서 두 번째)으로의 한 칸 후퇴만 허용.
		// 그 외 자기 경로 위로는 이동 불가 — 중간을 끊지 않고 왔던 길을 그대로 되짚어야 함.
		if (ExistingIdx != Path.Num() - 2)
		{
			return;
		}

		// 한 칸 백트래킹: 현재 커서 칸(경로 마지막) 하나만 원상복구
		const FIntPoint RevertPos = Path.Last();
		FHackGridCell& RevertCell = GetCell(RevertPos.X, RevertPos.Y);
		if (RevertCell.NodeType == EHackNodeType::Normal)
		{
			--NormalNodesVisited;
		}
		// 백트래킹은 항상 한 칸만(경로 끝에서 두 번째로만) 허용되므로 이 되감기 칸이 노란 노드면
		// 경로에서 완전히 벗어난 것 → live 플래그 해제
		else if (RevertCell.NodeType == EHackNodeType::Decode)
		{
			bDecodeNodeVisited = false;
		}
		RevertCell.CellState = EHackCellState::Default;
		Path.Pop();

		CursorPos = NewPos;
		GetCell(CursorPos.X, CursorPos.Y).CellState = EHackCellState::Cursor;
	}
	else
	{
		// 전진: 이전 커서 칸을 경로(Visited)로 남기고 새 칸을 경로에 추가
		GetCell(CursorPos.X, CursorPos.Y).CellState = EHackCellState::Visited;

		CursorPos = NewPos;
		FHackGridCell& NewCell = GetCell(CursorPos.X, CursorPos.Y);
		if (NewCell.NodeType == EHackNodeType::Normal)
		{
			++NormalNodesVisited;
		}
		// 노란(Decode) 노드 진입 — 방어력 감소 신호 활성화 (그리드당 1개라 재방문 중복 없음)
		else if (NewCell.NodeType == EHackNodeType::Decode)
		{
			bDecodeNodeVisited = true;
			// Decode 보너스는 경로에 처음 진입한 server 시점에만 Hugh 피해 상태로 전달
			OnDecodeNodeVisited.Broadcast();
		}
		NewCell.CellState = EHackCellState::Cursor;
		Path.Add(CursorPos);
	}

	// 이동이 완료된 시점이므로 진입 대기 상태는 해제
	bCorruptedPending = false;
	NotifyCursorMoved();

	if (CursorPos == TargetPos)
	{
		CompleteHack();
	}
}

void UEnemyHackingPuzzleComponent::CompleteHack()
{
	PuzzleState = EEnemyHackState::Completed;
	OnHackCompleted.Broadcast(); // 서버 로컬. 원격 클라는 OnRep_PuzzleState에서 발동.
}

void UEnemyHackingPuzzleComponent::CancelHack()
{
	if (!HasPuzzleAuthority()) return;

	PuzzleState = EEnemyHackState::Idle;
	OnHackCancelled.Broadcast(); // 서버 로컬. 원격 클라는 OnRep_PuzzleState에서 발동.
}

void UEnemyHackingPuzzleComponent::BroadcastStarted()
{
	OnGridReady.Broadcast(CachedGridSize);
	OnHackStarted.Broadcast();
	OnCursorMoved.Broadcast(CursorPos, bCorruptedPending);
}

void UEnemyHackingPuzzleComponent::NotifyCursorMoved()
{
	// 서버: 카운터를 굴려 클라가 OnRep_CursorRep을 받게 함 (커서 위치가 같아도 알림 보장)
	++CursorRepCounter;
	OnCursorMoved.Broadcast(CursorPos, bCorruptedPending);
}

void UEnemyHackingPuzzleComponent::OnRep_PuzzleState(EEnemyHackState OldPuzzleState)
{
	if (PuzzleState == OldPuzzleState) return;

	if (PuzzleState == EEnemyHackState::Active && OldPuzzleState != EEnemyHackState::Active)
	{
		BroadcastStarted();
	}
	else if (PuzzleState == EEnemyHackState::Completed)
	{
		// Completed 복제가 CursorRep보다 먼저 오면 client가 마지막 이동 표시를 놓칠 수 있어 최종 위치를 먼저 알림
		OnCursorMoved.Broadcast(CursorPos, bCorruptedPending);
		OnHackCompleted.Broadcast();
	}
	else if (PuzzleState == EEnemyHackState::Idle && OldPuzzleState == EEnemyHackState::Active)
	{
		OnHackCancelled.Broadcast();
	}
}

void UEnemyHackingPuzzleComponent::OnRep_CursorRep()
{
	if (PuzzleState == EEnemyHackState::Active || PuzzleState == EEnemyHackState::Completed)
	{
		OnCursorMoved.Broadcast(CursorPos, bCorruptedPending);
	}
}

EHackNodeType UEnemyHackingPuzzleComponent::GetNodeType(int32 Row, int32 Col) const
{
	if (!Grid.IsValidIndex(Row * CachedGridSize + Col)) return EHackNodeType::Normal;
	return GetCell(Row, Col).NodeType;
}

EHackCellState UEnemyHackingPuzzleComponent::GetCellState(int32 Row, int32 Col) const
{
	if (!Grid.IsValidIndex(Row * CachedGridSize + Col)) return EHackCellState::Default;
	return GetCell(Row, Col).CellState;
}

FIntPoint UEnemyHackingPuzzleComponent::DirectionToOffset(EHackDirection Dir)
{
	switch (Dir)
	{
	case EHackDirection::Up: return FIntPoint(-1, 0);
	case EHackDirection::Down: return FIntPoint(1, 0);
	case EHackDirection::Left: return FIntPoint(0, -1);
	case EHackDirection::Right: return FIntPoint(0, 1);
	default: return FIntPoint(0, 0);
	}
}
