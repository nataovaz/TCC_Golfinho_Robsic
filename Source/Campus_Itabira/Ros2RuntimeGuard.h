#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

#if PLATFORM_LINUX
#include <dlfcn.h>
#include <initializer_list>
#endif

namespace CampusRos2RuntimeGuard
{
inline void AddRosRootIfValid(TArray<FString>& Roots, const FString& Candidate)
{
#if PLATFORM_LINUX
    if (Candidate.IsEmpty())
    {
        return;
    }

    const FString FullCandidate = FPaths::ConvertRelativePathToFull(Candidate);
    if (!FPaths::DirectoryExists(FPaths::Combine(FullCandidate, TEXT("lib"))) ||
        !FPaths::DirectoryExists(FPaths::Combine(FullCandidate, TEXT("include"))))
    {
        return;
    }

    if (!Roots.Contains(FullCandidate))
    {
        Roots.Add(FullCandidate);
    }
#endif
}

inline TArray<FString> GetRosRootCandidates()
{
    TArray<FString> Roots;

#if PLATFORM_LINUX
    AddRosRootIfValid(Roots, FPlatformMisc::GetEnvironmentVariable(TEXT("UE_ROS_ROOT")));
    AddRosRootIfValid(Roots, FPlatformMisc::GetEnvironmentVariable(TEXT("ROS_ROOT")));

    TArray<FString> AmentPrefixes;
    FPlatformMisc::GetEnvironmentVariable(TEXT("AMENT_PREFIX_PATH")).ParseIntoArray(AmentPrefixes, TEXT(":"), true);
    for (const FString& Prefix : AmentPrefixes)
    {
        AddRosRootIfValid(Roots, Prefix);
    }

    AddRosRootIfValid(Roots, TEXT("/opt/ros/jazzy"));

    const FString UserHome = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
    AddRosRootIfValid(Roots, FPaths::Combine(UserHome, TEXT("miniforge3"), TEXT("envs"), TEXT("ros_jazzy_env")));
    AddRosRootIfValid(Roots, FPaths::Combine(UserHome, TEXT("ros2_jazzy"), TEXT("ros2-linux")));
    AddRosRootIfValid(Roots, FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("rclUE"), TEXT("ThirdParty"), TEXT("ros")));
#endif

    return Roots;
}

inline void* LoadLibraryHandleFromCandidates(std::initializer_list<const TCHAR*> CandidateNames)
{
#if PLATFORM_LINUX
    TArray<FString> SearchTargets;
    for (const TCHAR* CandidateName : CandidateNames)
    {
        SearchTargets.Add(CandidateName);
    }

    for (const FString& RosRoot : GetRosRootCandidates())
    {
        for (const TCHAR* CandidateName : CandidateNames)
        {
            SearchTargets.Add(FPaths::Combine(RosRoot, TEXT("lib"), CandidateName));
        }
    }

    for (const FString& SearchTarget : SearchTargets)
    {
        FTCHARToUTF8 Utf8Path(*SearchTarget);
        if (void* Handle = dlopen(Utf8Path.Get(), RTLD_LAZY | RTLD_GLOBAL))
        {
            return Handle;
        }
    }
#endif
    return reinterpret_cast<void*>(1);
}

inline bool CanInitializeRos2()
{
#if PLATFORM_LINUX
    static TMap<FString, void*> LoadedLibraries;

    struct FRequiredLibrary
    {
        const TCHAR* Key;
        std::initializer_list<const TCHAR*> Candidates;
    };

    static const FRequiredLibrary RequiredLibraries[] = {
        {TEXT("yaml"),     {TEXT("libyaml.so"), TEXT("libyaml-0.so.2")}},
        {TEXT("spdlog"),   {TEXT("libspdlog.so"), TEXT("libspdlog.so.1"), TEXT("libspdlog.so.1.15")}},
        {TEXT("tinyxml2"), {TEXT("libtinyxml2.so"), TEXT("libtinyxml2.so.10"), TEXT("libtinyxml2.so.6")}},
        {TEXT("ssl"),      {TEXT("libssl.so"), TEXT("libssl.so.3"), TEXT("libssl.so.1.1")}},
        {TEXT("crypto"),   {TEXT("libcrypto.so"), TEXT("libcrypto.so.3"), TEXT("libcrypto.so.1.1")}},
    };

    TArray<FString> MissingLibraries;
    for (const FRequiredLibrary& RequiredLibrary : RequiredLibraries)
    {
        const FString LibraryKey(RequiredLibrary.Key);
        if (!LoadedLibraries.Contains(LibraryKey))
        {
            if (void* Handle = LoadLibraryHandleFromCandidates(RequiredLibrary.Candidates))
            {
                LoadedLibraries.Add(LibraryKey, Handle);
            }
        }

        if (!LoadedLibraries.Contains(LibraryKey))
        {
            MissingLibraries.Add(RequiredLibrary.Key);
        }
    }

    if (MissingLibraries.Num() > 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Skipping ROS2 initialization because required shared libraries are missing: %s"),
            *FString::Join(MissingLibraries, TEXT(", "))
        );
        return false;
    }
#endif

    return true;
}
}
