#include "UBFGameInstance.h"

#include "UBFPlayerData.h"
#include "HAL/PlatformMisc.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <TlHelp32.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
	DWORD FindParentProcessId(DWORD ProcessId)
	{
		HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (Snapshot == INVALID_HANDLE_VALUE)
		{
			return 0;
		}

		PROCESSENTRY32 Entry{};
		Entry.dwSize = sizeof(Entry);
		if (Process32First(Snapshot, &Entry))
		{
			do
			{
				if (Entry.th32ProcessID == ProcessId)
				{
					const DWORD ParentProcessId = Entry.th32ParentProcessID;
					CloseHandle(Snapshot);
					return ParentProcessId;
				}
			}
			while (Process32Next(Snapshot, &Entry));
		}
		CloseHandle(Snapshot);
		return 0;
	}

	FString GetProcessImagePath(DWORD ProcessId)
	{
		HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, ProcessId);
		if (Process == nullptr)
		{
			return FString();
		}

		TCHAR ImagePath[32768]{};
		DWORD PathLength = UE_ARRAY_COUNT(ImagePath);
		const BOOL HasImagePath = QueryFullProcessImageName(Process, 0, ImagePath, &PathLength);
		CloseHandle(Process);
		if (!HasImagePath)
		{
			return FString();
		}
		return FString(ImagePath);
	}

	bool IsStartedByUbfLauncher()
	{
		DWORD ProcessId = GetCurrentProcessId();
		for (int32 Depth = 0; Depth < 5; ++Depth)
		{
			ProcessId = FindParentProcessId(ProcessId);
			if (ProcessId == 0)
			{
				return false;
			}

			const FString ImagePath = GetProcessImagePath(ProcessId);
			if (ImagePath.IsEmpty())
			{
				return false;
			}
			if (FPaths::GetCleanFilename(ImagePath).Equals(TEXT("UBFLauncher.exe"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
#endif
}

void UUBFGameInstance::Init()
{
	Super::Init();
	SessionUserName = FPlatformMisc::GetEnvironmentVariable(TEXT("UBF_LAUNCHER_USERNAME")).TrimStartAndEnd();

#if UE_BUILD_SHIPPING && PLATFORM_WINDOWS
	if (!IsStartedByUbfLauncher() || SessionUserName.IsEmpty())
	{
		FPlatformMisc::MessageBoxExt(
			EAppMsgType::Ok,
			TEXT("Abre UBF desde UBFLauncher para iniciar el juego."),
			TEXT("UBF"));
		FPlatformMisc::RequestExit(false);
		return;
	}
#endif

	if (UUBFPlayerDataSubsystem* PlayerData = GetSubsystem<UUBFPlayerDataSubsystem>())
	{
		if (SessionUserName.IsEmpty())
		{
			// In editor/development, keep the saved profile if no launcher is supplying a name.
			PlayerData->EnsurePlayerName(TEXT(""));
		}
		else
		{
			PlayerData->SyncPlayerNameFromLauncher(SessionUserName);
		}
	}
}
