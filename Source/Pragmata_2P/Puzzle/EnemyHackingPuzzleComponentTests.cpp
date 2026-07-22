#include "Puzzle/EnemyHackingPuzzleComponent.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool SetNormalNodesVisited(FAutomationTestBase& Test, UEnemyHackingPuzzleComponent* PuzzleComponent, int32 NodeCount)
	{
		FIntProperty* NodeCountProperty = FindFProperty<FIntProperty>(UEnemyHackingPuzzleComponent::StaticClass(), TEXT("NormalNodesVisited"));
		if (!Test.TestNotNull(TEXT("NormalNodesVisited property"), NodeCountProperty))
		{
			return false;
		}

		NodeCountProperty->SetPropertyValue_InContainer(PuzzleComponent, NodeCount);
		return true;
	}

	bool TestResolvedDuration(FAutomationTestBase& Test, UEnemyHackingPuzzleComponent* PuzzleComponent, EEnemyHackType EnemyType, int32 NodeCount, float ExpectedDuration)
	{
		PuzzleComponent->EnemyType = EnemyType;
		if (!SetNormalNodesVisited(Test, PuzzleComponent, NodeCount))
		{
			return false;
		}
		return Test.TestTrue(
			FString::Printf(TEXT("EnemyType=%d NodeCount=%d"), static_cast<int32>(EnemyType), NodeCount),
			FMath::IsNearlyEqual(PuzzleComponent->GetResolvedHackDurationSeconds(), ExpectedDuration));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyHackingPuzzleDurationTest,
	"Pragmata.Hacking.EnemyHackDuration.NodeProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// 적 종류와 완료 경로의 파란 노드 수가 기획 duration tier를 선택하는지 검증
bool FEnemyHackingPuzzleDurationTest::RunTest(const FString& Parameters)
{
	UEnemyHackingPuzzleComponent* PuzzleComponent = NewObject<UEnemyHackingPuzzleComponent>();
	TestNotNull(TEXT("Puzzle component"), PuzzleComponent);
	if (!PuzzleComponent)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Watcher, 0, 6.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Watcher, 1, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Watcher, 4, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Walker, 0, 2.06f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Walker, 1, 6.71f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Walker, 2, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Walker, 3, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Executor, 0, 1.10f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Executor, 1, 4.55f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Executor, 2, 9.10f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Executor, 3, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Executor, 4, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Watcher, -1, 6.f);

	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Sentinel, 0, 1.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Sentinel, 1, 2.5f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Sentinel, 2, 5.5f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Sentinel, 3, 10.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Sentinel, 4, 15.f);
	bPassed &= TestResolvedDuration(*this, PuzzleComponent, EEnemyHackType::Sentinel, 8, 15.f);
	PuzzleComponent->HackDurationProfiles.Reset();
	FEnemyHackDurationProfile& EmptyWalkerProfile = PuzzleComponent->HackDurationProfiles.AddDefaulted_GetRef();
	EmptyWalkerProfile.EnemyType = EEnemyHackType::Walker;
	PuzzleComponent->EnemyType = EEnemyHackType::Walker;
	bPassed &= TestTrue(TEXT("Empty profile falls back"), PuzzleComponent->GetResolvedHackDurationSeconds() <= 0.f);
	return bPassed;
}

#endif
