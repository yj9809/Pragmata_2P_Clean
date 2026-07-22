#include "Game/PragmataSessionUtils.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Data/LobbyCharacterSkinSet.h"
#include "Game/PragmataMultiplayerSessionSubsystem.h"
#include "Game/PragmataPlayerState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPragmataRoomCodeNormalizeTest,
	"Pragmata.Session.RoomCode.NormalizeAndValidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// 초대 코드 정규화와 허용 문자 규칙이 세션 join 입력을 안정적으로 거르는지 검증
bool FPragmataRoomCodeNormalizeTest::RunTest(const FString& Parameters)
{
	const FString NormalizedCode = PragmataSessionUtils::NormalizeRoomCode(TEXT("ab-cd ef"));
	TestEqual(TEXT("Room code normalizes spacing, hyphen, and case"), NormalizedCode, TEXT("ABCDEF"));
	TestTrue(TEXT("Normalized invite alphabet is accepted"), PragmataSessionUtils::IsValidRoomCode(NormalizedCode));
	TestFalse(TEXT("Confusing characters are rejected"), PragmataSessionUtils::IsValidRoomCode(TEXT("ABCI1O")));

	const FString GeneratedCode = PragmataSessionUtils::GenerateRoomCode();
	TestEqual(TEXT("Generated room code length"), GeneratedCode.Len(), PragmataSessionUtils::RoomCodeLength);
	TestTrue(TEXT("Generated room code uses invite alphabet"), PragmataSessionUtils::IsValidRoomCode(GeneratedCode));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPragmataSessionTravelURLTest,
	"Pragmata.Session.TravelURL.RoleAndRoomCode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// lobby/game travel URL이 role과 room code handoff 값을 잃지 않는지 검증
bool FPragmataSessionTravelURLTest::RunTest(const FString& Parameters)
{
	const FString DefaultGameplayGameModePath = TEXT("/Game/Blueprints/BP_PragGameMode.BP_PragGameMode_C");
	const FString HughTravelURL = PragmataSessionUtils::BuildHughListenTravelURL(TEXT("/Game/Map/Demo"), TEXT("ABCDEF"), DefaultGameplayGameModePath);
	TestEqual(TEXT("Hugh host listen travel URL"), HughTravelURL, TEXT("/Game/Map/Demo?listen?game=/Game/Blueprints/BP_PragGameMode.BP_PragGameMode_C?PragmataRole=Hugh?RoomCode=ABCDEF"));
	const FString HughListenOptions = PragmataSessionUtils::BuildHughListenOpenLevelOptions(TEXT("ABCDEF"), DefaultGameplayGameModePath);
	TestEqual(TEXT("Hugh host OpenLevel options"), HughListenOptions, TEXT("listen?game=/Game/Blueprints/BP_PragGameMode.BP_PragGameMode_C?PragmataRole=Hugh?RoomCode=ABCDEF"));

	const FString DianaTravelURL = PragmataSessionUtils::BuildDianaJoinTravelURL(TEXT("127.0.0.1:7777"), TEXT("ABCDEF"));
	TestEqual(TEXT("Diana join travel URL carries role and code"), DianaTravelURL, TEXT("127.0.0.1:7777?PragmataRole=Diana?RoomCode=ABCDEF"));

	const FString LobbyListenURL = PragmataSessionUtils::BuildLobbyListenTravelURL(TEXT("/Game/Map/MatchLobby"), TEXT("ABCDEF"));
	TestEqual(TEXT("Lobby listen travel URL carries code only"), LobbyListenURL, TEXT("/Game/Map/MatchLobby?listen?RoomCode=ABCDEF"));
	const FString LobbyListenOptions = PragmataSessionUtils::BuildLobbyListenOpenLevelOptions(TEXT("ABCDEF"));
	TestEqual(TEXT("Lobby listen OpenLevel options carry code only"), LobbyListenOptions, TEXT("listen?RoomCode=ABCDEF"));

	const FString LobbyJoinURL = PragmataSessionUtils::BuildLobbyJoinTravelURL(TEXT("127.0.0.1:7777"), TEXT("ABCDEF"));
	TestEqual(TEXT("Lobby join travel URL carries code only"), LobbyJoinURL, TEXT("127.0.0.1:7777?RoomCode=ABCDEF"));

	const FString PreferredLobbyListenURL = PragmataSessionUtils::BuildLobbyListenTravelURL(TEXT("/Game/Map/MatchLobby"), TEXT("ABCDEF"), EPragmataPlayerRole::Diana);
	TestEqual(TEXT("Lobby listen travel URL carries preferred host role"), PreferredLobbyListenURL, TEXT("/Game/Map/MatchLobby?listen?RoomCode=ABCDEF?PreferredRole=Diana"));
	const FString PreferredLobbyListenOptions = PragmataSessionUtils::BuildLobbyListenOpenLevelOptions(TEXT("ABCDEF"), EPragmataPlayerRole::Diana);
	TestEqual(TEXT("Lobby listen OpenLevel options carry preferred host role"), PreferredLobbyListenOptions, TEXT("listen?RoomCode=ABCDEF?PreferredRole=Diana"));

	const FString PreferredLobbyJoinURL = PragmataSessionUtils::BuildLobbyJoinTravelURL(TEXT("127.0.0.1:7777"), TEXT("ABCDEF"), EPragmataPlayerRole::Hugh);
	TestEqual(TEXT("Lobby join travel URL carries preferred join role"), PreferredLobbyJoinURL, TEXT("127.0.0.1:7777?RoomCode=ABCDEF?PreferredRole=Hugh"));

	const FString GameTravelURL = PragmataSessionUtils::BuildGameTravelURL(TEXT("/Game/Map/Demo"), TEXT("ABCDEF"), DefaultGameplayGameModePath);
	TestEqual(TEXT("Game travel URL uses gameplay mode without per-player role"), GameTravelURL, TEXT("/Game/Map/Demo?game=/Game/Blueprints/BP_PragGameMode.BP_PragGameMode_C?RoomCode=ABCDEF"));

	const FString CustomGameTravelURL = PragmataSessionUtils::BuildGameTravelURL(TEXT("/Game/Map/ArenaSmoke"), FString(), TEXT("/Game/Blueprints/BP_CustomGameMode.BP_CustomGameMode_C"));
	TestEqual(TEXT("Game travel URL accepts custom gameplay mode"), CustomGameTravelURL, TEXT("/Game/Map/ArenaSmoke?game=/Game/Blueprints/BP_CustomGameMode.BP_CustomGameMode_C"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPragmataLobbyRoleHandoffTest,
	"Pragmata.Session.Lobby.RoleHandoffStoresRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Hugh/Diana 선택 순서가 바뀌어도 lobby handoff가 PlayerId와 slot fallback으로 보존되는지 검증
bool FPragmataLobbyRoleHandoffTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UPragmataMultiplayerSessionSubsystem* SessionSubsystem = NewObject<UPragmataMultiplayerSessionSubsystem>(TestGameInstance);
	TestNotNull(TEXT("Session subsystem"), SessionSubsystem);
	if (!SessionSubsystem)
	{
		return false;
	}

	SessionSubsystem->RegisterLobbyRoleHandoff(10, 1, EPragmataPlayerRole::Hugh, PragmataLobbySkinIds::ScribbleSuit);
	SessionSubsystem->RegisterLobbyRoleHandoff(20, 2, EPragmataPlayerRole::Diana, PragmataLobbySkinIds::DianaDefault);

	EPragmataPlayerRole FoundRole = EPragmataPlayerRole::None;
	FName FoundSkinId = NAME_None;
	TestTrue(TEXT("Hugh role is found by player id"), SessionSubsystem->FindLobbyRoleHandoff(10, 0, FoundRole, &FoundSkinId));
	TestEqual(TEXT("Hugh handoff role"), FoundRole, EPragmataPlayerRole::Hugh);
	TestEqual(TEXT("Hugh handoff skin"), FoundSkinId, PragmataLobbySkinIds::ScribbleSuit);

	FoundRole = EPragmataPlayerRole::None;
	FoundSkinId = NAME_None;
	TestTrue(TEXT("Diana role is found by lobby index fallback"), SessionSubsystem->FindLobbyRoleHandoff(-1, 2, FoundRole, &FoundSkinId));
	TestEqual(TEXT("Diana handoff role"), FoundRole, EPragmataPlayerRole::Diana);
	TestEqual(TEXT("Diana handoff skin"), FoundSkinId, PragmataLobbySkinIds::DianaDefault);

	FoundRole = EPragmataPlayerRole::None;
	FoundSkinId = NAME_None;
	TestTrue(TEXT("Ordered lobby handoff returns first player role"), SessionSubsystem->ConsumeNextLobbyRoleHandoff(FoundRole, &FoundSkinId));
	TestEqual(TEXT("First ordered handoff role"), FoundRole, EPragmataPlayerRole::Hugh);
	TestEqual(TEXT("First ordered handoff skin"), FoundSkinId, PragmataLobbySkinIds::ScribbleSuit);

	FoundRole = EPragmataPlayerRole::None;
	FoundSkinId = NAME_None;
	TestTrue(TEXT("Ordered lobby handoff returns second player role"), SessionSubsystem->ConsumeNextLobbyRoleHandoff(FoundRole, &FoundSkinId));
	TestEqual(TEXT("Second ordered handoff role"), FoundRole, EPragmataPlayerRole::Diana);
	TestEqual(TEXT("Second ordered handoff skin"), FoundSkinId, PragmataLobbySkinIds::DianaDefault);

	FoundRole = EPragmataPlayerRole::None;
	TestFalse(TEXT("Ordered lobby handoff is consumed once"), SessionSubsystem->ConsumeNextLobbyRoleHandoff(FoundRole));

	SessionSubsystem->ClearLobbyRoleHandoff();
	TestFalse(TEXT("Handoff clears stored roles"), SessionSubsystem->FindLobbyRoleHandoff(10, 1, FoundRole));
	TestFalse(TEXT("Ordered handoff clears stored roles"), SessionSubsystem->ConsumeNextLobbyRoleHandoff(FoundRole));

	// host가 Diana를 선택한 역순 조합도 gameplay travel에서 같은 순서로 복구
	SessionSubsystem->RegisterLobbyRoleHandoff(30, 1, EPragmataPlayerRole::Diana, PragmataLobbySkinIds::DianaDefault);
	SessionSubsystem->RegisterLobbyRoleHandoff(40, 2, EPragmataPlayerRole::Hugh, PragmataLobbySkinIds::DelphiSuit);

	FoundRole = EPragmataPlayerRole::None;
	TestTrue(TEXT("Diana host role is found by player id"), SessionSubsystem->FindLobbyRoleHandoff(30, 0, FoundRole));
	TestEqual(TEXT("Diana host handoff role"), FoundRole, EPragmataPlayerRole::Diana);

	FoundRole = EPragmataPlayerRole::None;
	TestTrue(TEXT("Hugh client role is found by lobby index fallback"), SessionSubsystem->FindLobbyRoleHandoff(-1, 2, FoundRole));
	TestEqual(TEXT("Hugh client handoff role"), FoundRole, EPragmataPlayerRole::Hugh);

	FoundRole = EPragmataPlayerRole::None;
	TestTrue(TEXT("Ordered handoff returns Diana host first"), SessionSubsystem->ConsumeNextLobbyRoleHandoff(FoundRole));
	TestEqual(TEXT("First swapped handoff role"), FoundRole, EPragmataPlayerRole::Diana);

	FoundRole = EPragmataPlayerRole::None;
	TestTrue(TEXT("Ordered handoff returns Hugh client second"), SessionSubsystem->ConsumeNextLobbyRoleHandoff(FoundRole));
	TestEqual(TEXT("Second swapped handoff role"), FoundRole, EPragmataPlayerRole::Hugh);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPragmataLobbyPlayerStateCopyTest,
	"Pragmata.Session.Lobby.PlayerStateCopyPreservesRole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// seamless travel 복사 과정에서 role과 lobby 상태가 새 PlayerState로 유지되는지 검증
bool FPragmataLobbyPlayerStateCopyTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PragmataPlayerStateCopyTest"));
	TestNotNull(TEXT("Test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	APragmataPlayerState* SourcePlayerState = TestWorld->SpawnActor<APragmataPlayerState>();
	APragmataPlayerState* CopiedPlayerState = TestWorld->SpawnActor<APragmataPlayerState>();
	TestNotNull(TEXT("Source player state"), SourcePlayerState);
	TestNotNull(TEXT("Copied player state"), CopiedPlayerState);
	if (!SourcePlayerState || !CopiedPlayerState)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	SourcePlayerState->SetAssignedRole(EPragmataPlayerRole::Diana);
	SourcePlayerState->SetActiveControlRole(EPragmataPlayerRole::Hugh);
	SourcePlayerState->SetLobbyPlayerIndex(2);
	SourcePlayerState->SetLobbySelectedRole(EPragmataPlayerRole::Diana);
	SourcePlayerState->SetLobbySelectedSkinId(PragmataLobbySkinIds::DianaDefault);
	SourcePlayerState->SetLobbyReady(true);
	SourcePlayerState->CopyProperties(CopiedPlayerState);

	TestEqual(TEXT("Assigned role survives seamless travel copy"), CopiedPlayerState->GetAssignedRole(), EPragmataPlayerRole::Diana);
	TestEqual(TEXT("Active control role survives seamless travel copy"), CopiedPlayerState->GetActiveControlRole(), EPragmataPlayerRole::Hugh);
	TestEqual(TEXT("Lobby index survives seamless travel copy"), CopiedPlayerState->GetLobbyPlayerIndex(), 2);
	TestEqual(TEXT("Lobby selected role survives seamless travel copy"), CopiedPlayerState->GetLobbySelectedRole(), EPragmataPlayerRole::Diana);
	TestEqual(TEXT("Lobby selected skin survives seamless travel copy"), CopiedPlayerState->GetLobbySelectedSkinId(), PragmataLobbySkinIds::DianaDefault);
	TestTrue(TEXT("Ready state survives seamless travel copy"), CopiedPlayerState->IsLobbyReady());

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPragmataLobbySkinSetTest,
	"Pragmata.Session.Lobby.SkinSetCyclesByRole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// lobby skin 목록은 Hugh 2종을 순환하고 Diana 단일 skin은 고정
bool FPragmataLobbySkinSetTest::RunTest(const FString& Parameters)
{
	ULobbyCharacterSkinSet* SkinSet = NewObject<ULobbyCharacterSkinSet>();
	TestNotNull(TEXT("Skin set"), SkinSet);
	if (!SkinSet)
	{
		return false;
	}

	TestEqual(TEXT("Hugh default skin"), SkinSet->ResolveDefaultSkinId(EPragmataPlayerRole::Hugh), PragmataLobbySkinIds::DelphiSuit);
	TestEqual(TEXT("Hugh cycles to scribble"), SkinSet->GetAdjacentSkinId(EPragmataPlayerRole::Hugh, PragmataLobbySkinIds::DelphiSuit, 1), PragmataLobbySkinIds::ScribbleSuit);
	TestEqual(TEXT("Hugh cycles back to default"), SkinSet->GetAdjacentSkinId(EPragmataPlayerRole::Hugh, PragmataLobbySkinIds::ScribbleSuit, 1), PragmataLobbySkinIds::DelphiSuit);
	TestEqual(TEXT("Diana single skin stays fixed"), SkinSet->GetAdjacentSkinId(EPragmataPlayerRole::Diana, PragmataLobbySkinIds::DianaDefault, 1), PragmataLobbySkinIds::DianaDefault);

	return true;
}

#endif
