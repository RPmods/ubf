#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "UBFCombatGameState.generated.h"

UCLASS()
class UBF_API AUBFCombatGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AUBFCombatGameState();
	void SetGolemMode(bool bEnabled);
	void SetTeamScore(int32 TeamId, int32 Score);
	bool IsGolemMode() const { return bGolemMode; }
	int32 GetTeamScore(int32 TeamId) const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	bool bGolemMode = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	int32 TeamAScore = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	int32 TeamBScore = 0;
};
