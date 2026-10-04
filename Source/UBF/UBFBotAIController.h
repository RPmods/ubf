#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "UBFBotAIController.generated.h"

class AUBFCombatCharacter;

UENUM(BlueprintType)
enum class EUBFBotDifficulty : uint8
{
	Easy,
	Normal,
	Hard
};

/** Lightweight server-side opponent AI for the first UBF combat slice. */
UCLASS()
class UBF_API AUBFBotAIController : public AAIController
{
	GENERATED_BODY()

public:
	AUBFBotAIController();
	void SetDifficulty(EUBFBotDifficulty NewDifficulty) { Difficulty = NewDifficulty; }
	EUBFBotDifficulty GetDifficulty() const { return Difficulty; }

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	AUBFCombatCharacter* FindNearestEnemy(AUBFCombatCharacter* Fighter) const;

	TWeakObjectPtr<AUBFCombatCharacter> CurrentTarget;
	float NextTargetSearchTime = 0.0f;
	float NextAttackTime = 0.0f;
	float NextDodgeEvaluationTime = 0.0f;
	float ReactionReadyTime = 0.0f;
	float NextDiagnosticsTime = 0.0f;
	float LastProgressTime = 0.0f;
	float LastObservedDistance = TNumericLimits<float>::Max();
	float DetourEndTime = 0.0f;
	float NextDetourSide = 1.0f;
	FVector DetourDirection = FVector::ZeroVector;
	EUBFBotDifficulty Difficulty = EUBFBotDifficulty::Normal;
};
