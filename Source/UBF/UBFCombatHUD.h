#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UBFCombatHUD.generated.h"

/** Small combat HUD for the first local arena build. */
UCLASS()
class UBF_API AUBFCombatHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
