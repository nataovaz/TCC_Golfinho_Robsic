// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Campus_Itabira : ModuleRules
{
    public Campus_Itabira(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;
        
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "rclUE",
            "CesiumRuntime",
            "UMG"
        });


        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
