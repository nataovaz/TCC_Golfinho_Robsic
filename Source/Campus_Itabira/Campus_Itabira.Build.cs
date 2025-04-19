// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Campus_Itabira : ModuleRules
{
    public Campus_Itabira(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore",
            "rclUE",
            "CesiumRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
