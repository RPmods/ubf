#include "UBFGameInstance.h"

#include "HAL/PlatformMisc.h"

void UUBFGameInstance::Init()
{
	Super::Init();
	SessionUserName = FPlatformMisc::GetEnvironmentVariable(TEXT("UBF_LAUNCHER_USERNAME")).TrimStartAndEnd();
	if (SessionUserName.IsEmpty())
	{
		SessionUserName = TEXT("Usuario de desarrollo");
	}
}
