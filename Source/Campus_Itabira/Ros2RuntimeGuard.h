#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "Misc/Paths.h"

#if PLATFORM_LINUX
#include <dlfcn.h>
#endif

namespace CampusRos2RuntimeGuard
{
inline void* LoadLibraryHandle(const TCHAR* LibraryName)
{
#if PLATFORM_LINUX
    FTCHARToUTF8 Utf8LibraryName(LibraryName);
    if (void* Handle = dlopen(Utf8LibraryName.Get(), RTLD_LAZY | RTLD_GLOBAL))
    {
        return Handle;
    }

    const FString PluginLibraryPath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("rclUE"),
        TEXT("ThirdParty"),
        TEXT("ros"),
        TEXT("lib"),
        LibraryName
    );

    FTCHARToUTF8 Utf8PluginLibraryPath(*PluginLibraryPath);
    if (void* Handle = dlopen(Utf8PluginLibraryPath.Get(), RTLD_LAZY | RTLD_GLOBAL))
    {
        return Handle;
    }
#endif
    return reinterpret_cast<void*>(1);
}

inline bool CanInitializeRos2()
{
#if PLATFORM_LINUX
    static TMap<FString, void*> LoadedLibraries;
    static const TCHAR* RequiredLibraries[] = {
        TEXT("libyaml.so"),
        TEXT("libspdlog.so.1"),
        TEXT("libtinyxml2.so.6"),
        TEXT("libssl.so.1.1"),
        TEXT("libcrypto.so.1.1")
    };

    TArray<FString> MissingLibraries;
    for (const TCHAR* RequiredLibrary : RequiredLibraries)
    {
        const FString LibraryKey(RequiredLibrary);
        if (!LoadedLibraries.Contains(LibraryKey))
        {
            if (void* Handle = LoadLibraryHandle(RequiredLibrary))
            {
                LoadedLibraries.Add(LibraryKey, Handle);
            }
        }

        if (!LoadedLibraries.Contains(LibraryKey))
        {
            MissingLibraries.Add(RequiredLibrary);
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
