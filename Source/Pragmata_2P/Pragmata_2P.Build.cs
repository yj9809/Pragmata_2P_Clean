// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Pragmata_2P : ModuleRules
{
	public Pragmata_2P(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[]
		{
			"Pragmata_2P"
		});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "InputCore", "EnhancedInput", "AnimGraphRuntime", "OnlineSubsystem", "UMG", "ModelViewViewModel", "FieldNotification", "Niagara", "EngineCameras", "CinematicCamera", "AIModule", "SlateCore", "NavigationSystem", "LevelSequence" });

		PublicIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "ApplicationCore", "MovieScene", "OnlineBase", "OnlineSubsystemUtils", "Slate", "SlateCore" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
