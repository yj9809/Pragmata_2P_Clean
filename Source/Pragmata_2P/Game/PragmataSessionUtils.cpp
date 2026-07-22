#include "Game/PragmataSessionUtils.h"

namespace
{
	// 초대 코드 alphabet은 혼동 문자를 제외
	const TCHAR RoomCodeAlphabet[] = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");

	// local preselect를 서버 lobby의 PlayerState 초기 선택값으로 넘김
	FString BuildPreferredRoleOption(EPragmataPlayerRole PreferredRole)
	{
		if (PreferredRole == EPragmataPlayerRole::Hugh)
		{
			return TEXT("?PreferredRole=Hugh");
		}
		if (PreferredRole == EPragmataPlayerRole::Diana)
		{
			return TEXT("?PreferredRole=Diana");
		}

		return FString();
	}
}

FString PragmataSessionUtils::GenerateRoomCode()
{
	// GUID entropy를 혼동 문자 없는 6자리 alphabet으로 접어 lobby 초대 코드를 생성
	constexpr int32 AlphabetLength = UE_ARRAY_COUNT(RoomCodeAlphabet) - 1;
	const FGuid Guid = FGuid::NewGuid();
	uint64 Value = (static_cast<uint64>(Guid.A) << 32) | static_cast<uint64>(Guid.B);

	FString Result;
	Result.Reserve(RoomCodeLength);
	for (int32 Index = 0; Index < RoomCodeLength; ++Index)
	{
		if (Value == 0)
		{
			Value = (static_cast<uint64>(Guid.C) << 32) | static_cast<uint64>(Guid.D);
		}

		const int32 AlphabetIndex = static_cast<int32>(Value % AlphabetLength);
		Result.AppendChar(RoomCodeAlphabet[AlphabetIndex]);
		Value /= AlphabetLength;
	}

	return Result;
}

FString PragmataSessionUtils::NormalizeRoomCode(const FString& RoomCode)
{
	// 사용자가 입력한 공백/하이픈 표기를 제거해 session search key와 비교
	FString Normalized;
	Normalized.Reserve(RoomCode.Len());

	const FString UpperRoomCode = RoomCode.ToUpper();
	for (const TCHAR Character : UpperRoomCode)
	{
		if (FChar::IsWhitespace(Character) || Character == TCHAR('-'))
		{
			continue;
		}

		Normalized.AppendChar(Character);
	}

	return Normalized;
}

bool PragmataSessionUtils::IsValidRoomCode(const FString& RoomCode)
{
	// travel option과 session setting에 넣기 전 6자리 초대 코드 contract를 검증
	if (RoomCode.Len() != RoomCodeLength)
	{
		return false;
	}

	for (const TCHAR Character : RoomCode)
	{
		if (!FCString::Strchr(RoomCodeAlphabet, Character))
		{
			return false;
		}
	}

	return true;
}

FString PragmataSessionUtils::BuildHughListenTravelURL(const FString& MapPath, const FString& RoomCode, const FString& GameplayGameModePath)
{
	// legacy Hugh host entrypoint는 gameplay GameMode와 Hugh role을 직접 지정
	return FString::Printf(TEXT("%s?%s"), *MapPath, *BuildHughListenOpenLevelOptions(RoomCode, GameplayGameModePath));
}

FString PragmataSessionUtils::BuildHughListenOpenLevelOptions(const FString& RoomCode, const FString& GameplayGameModePath)
{
	// OpenLevel은 map path와 option string을 분리해 받으므로 leading '?' 없이 반환
	return FString::Printf(TEXT("listen?game=%s?PragmataRole=Hugh?RoomCode=%s"), *GameplayGameModePath, *RoomCode);
}

FString PragmataSessionUtils::BuildDianaJoinTravelURL(const FString& ConnectInfo, const FString& RoomCode)
{
	// legacy Diana join entrypoint는 connect URL에 Diana role과 room code만 추가
	return FString::Printf(TEXT("%s?PragmataRole=Diana?RoomCode=%s"), *ConnectInfo, *RoomCode);
}

// Host가 고른 character preselect를 listen server 초기 접속 option에 포함
FString PragmataSessionUtils::BuildLobbyListenTravelURL(const FString& MapPath, const FString& RoomCode, EPragmataPlayerRole PreferredRole)
{
	return FString::Printf(TEXT("%s?%s"), *MapPath, *BuildLobbyListenOpenLevelOptions(RoomCode, PreferredRole));
}

FString PragmataSessionUtils::BuildLobbyListenOpenLevelOptions(const FString& RoomCode, EPragmataPlayerRole PreferredRole)
{
	// OpenLevel로 listen lobby를 열 때 session 광고보다 먼저 NetDriver port를 확보
	return FString::Printf(TEXT("listen?RoomCode=%s%s"), *RoomCode, *BuildPreferredRoleOption(PreferredRole));
}

// Join client의 preselect를 서버 lobby의 InitNewPlayer 경로로 전달
FString PragmataSessionUtils::BuildLobbyJoinTravelURL(const FString& ConnectInfo, const FString& RoomCode, EPragmataPlayerRole PreferredRole)
{
	return FString::Printf(TEXT("%s?RoomCode=%s%s"), *ConnectInfo, *RoomCode, *BuildPreferredRoleOption(PreferredRole));
}

FString PragmataSessionUtils::BuildGameTravelURL(const FString& MapPath, const FString& RoomCode, const FString& GameplayGameModePath)
{
	// lobby에서 gameplay map으로 server travel할 때 GameMode와 room code를 유지
	if (RoomCode.IsEmpty())
	{
		return FString::Printf(TEXT("%s?game=%s"), *MapPath, *GameplayGameModePath);
	}

	return FString::Printf(TEXT("%s?game=%s?RoomCode=%s"), *MapPath, *GameplayGameModePath, *RoomCode);
}
