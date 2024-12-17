// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TitanTarget : TargetRules
{
	public TitanTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		ExtraModuleNames.AddRange(new string[] {
				"Titan",
				"PhotoAlbum",
				"TitanAbilities",
				"TitanMovement",
				"TitanRaft",
				"TitanCamera",
				"TitanFramework"
			});

		if (BuildEnvironment == TargetBuildEnvironment.Unique)
		{
			bUseLoggingInShipping = true;
		}
	}
}
