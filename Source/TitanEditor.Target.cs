// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TitanEditorTarget : TargetRules
{
	public TitanEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.AddRange( new string[] { "Titan", "PhotoAlbum" } );
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	}
}
