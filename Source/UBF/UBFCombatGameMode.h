#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UBFCombatGameMode.generated.h"

class AController;
class APlayerController;
class AUBFBotAIController;
class AUBFCombatCharacter;
class AUBFGoldenGolemPickup;

UENUM(BlueprintType)
enum class EUBFBotFillMode : uint8
{
	None,
	OpponentTeam,
	AllOpenSlots
};

UCLASS()
class UBF_API AUBFCombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUBFCombatGameMode();
	virtual void BeginPlay() override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void NotifyFighterEliminated();
	void NotifyGoldenBearerPickedUp(AUBFCombatCharacter* Carrier, AUBFGoldenGolemPickup* Pickup);
	void NotifyGoldenBearerEliminated(AUBFCombatCharacter* Carrier);
	void NotifyMasterGolemEliminated(int32 TeamId);
	bool IsMatchFinished() const { return bMatchFinished; }
	bool IsGolemMode() const { return bGolemMode; }
	float GetMatchDuration() const;

private:
	void CheckMatchOutcome();
	void FinishMatch(int32 WinningTeamId, bool bIsDraw);
	void ReconcileBotRoster();
	int32 GetPrimaryHumanTeam() const;
	void GetHumanTeamCounts(AController* ExcludedController, int32 OutCounts[2]) const;
	FTransform GetTeamSpawnTransform(int32 TeamId, int32 SlotIndex) const;
	AUBFBotAIController* SpawnBot(int32 TeamId, int32 SlotIndex);
	void SpawnGolemObjectives();
	void SpawnGoldenGolemPickup(const FVector& Location);
	void ScoreGoldenBearer();
	void ClearGoldenBearer();

	UPROPERTY(EditDefaultsOnly, Category="UBF|Match", meta=(ClampMin="1", ClampMax="3"))
	int32 TeamSize = 1;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Match")
	FName DefaultCharacterId = TEXT("Wizz");

	UPROPERTY(EditDefaultsOnly, Category="UBF|Bots")
	EUBFBotFillMode BotFillMode = EUBFBotFillMode::OpponentTeam;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Bots")
	FName BotDifficulty = TEXT("NORMAL");

	UPROPERTY(EditDefaultsOnly, Category="UBF|Golem Mode", meta=(ClampMin="1", ClampMax="5"))
	int32 GolemScoreToWin = 3;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Golem Mode", meta=(ClampMin="5.0"))
	float BearerHoldSecondsToScore = 25.0f;

	int32 NextBotNumber = 1;
	int32 GolemTeamScores[2] = { 0, 0 };
	TWeakObjectPtr<AUBFCombatCharacter> CurrentGoldenBearer;
	FTimerHandle GoldenBearerScoreTimer;
	bool bGolemMode = false;
	FName HumanCharacterId = TEXT("Wizz");
	bool bMatchFinished = false;
	bool bOutcomeCheckPending = false;
	float MatchStartTime = 0.0f;
};
