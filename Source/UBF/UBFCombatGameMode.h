#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UBFCombatGameMode.generated.h"

class AController;
class APlayerController;
class AUBFBotAIController;
class AUBFCombatCharacter;
class AUBFGoldenGolemPickup;
class AUBFCombatPlayerController;

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
	void NotifyPlayerReadyChanged();
	void NotifyGoldenBearerPickedUp(AUBFCombatCharacter* Carrier, AUBFGoldenGolemPickup* Pickup);
	void NotifyGoldenBearerEliminated(AUBFCombatCharacter* Carrier);
	void NotifyGoldenGolemEliminated(AUBFCombatCharacter* Golem, AUBFCombatCharacter* LastHitBy);
	void NotifyMasterGolemDamaged(int32 TeamId, AUBFCombatCharacter* Attacker, float HealthRatio);
	void NotifyMasterGolemEliminated(int32 TeamId);
	void HandleSurrender(AUBFCombatPlayerController* PlayerController);
	void HandleAbandonRoom(AUBFCombatPlayerController* PlayerController);
	bool IsMatchFinished() const { return bMatchFinished; }
	bool IsRoundActive() const { return bRoundActive; }
	bool IsGolemMode() const { return bGolemMode; }
	float GetMatchDuration() const;

private:
	void CheckMatchOutcome();
	void StartRoundCountdown();
	void StartRound();
	void EndRound(int32 WinningTeamId, bool bIsDraw);
	void ContinueAfterRoundResults();
	void DestroyGolemObjectives();
	void FinishMatch(int32 WinningTeamId, bool bIsDraw);
	void ReconcileBotRoster();
	int32 GetPrimaryHumanTeam() const;
	void GetHumanTeamCounts(AController* ExcludedController, int32 OutCounts[2]) const;
	FTransform GetTeamSpawnTransform(int32 TeamId, int32 SlotIndex) const;
	AUBFBotAIController* SpawnBot(int32 TeamId, int32 SlotIndex);
	void SpawnGolemObjectives();
	void SpawnGoldenGolem(const FVector& Location);
	void ReturnToFrontEnd();
	void ClearGoldenBearer();
	void LockFighterForTransition(AUBFCombatCharacter* Fighter);
	void UpdatePlayerReadiness();
	void ResetPlayerReadiness();
	bool AreAllPlayersReady() const;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Match", meta=(ClampMin="1", ClampMax="5"))
	int32 TeamSize = 1;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Match")
	FName DefaultCharacterId = TEXT("Wizz");

	UPROPERTY(EditDefaultsOnly, Category="UBF|Bots")
	EUBFBotFillMode BotFillMode = EUBFBotFillMode::OpponentTeam;

	UPROPERTY(EditDefaultsOnly, Category="UBF|Bots")
	FName BotDifficulty = TEXT("NORMAL");

	int32 NextBotNumber = 1;
	TWeakObjectPtr<AUBFCombatCharacter> CurrentGoldenBearer;
	FTimerHandle MatchFlowTimer;
	int32 RoundNumber = 1;
	int32 RoundWins[2] = { 0, 0 };
	bool bGolemMode = false;
	bool bRoundActive = false;
	FName HumanCharacterId = TEXT("Wizz");
	bool bMatchFinished = false;
	bool bOutcomeCheckPending = false;
	bool bReturnToFrontEndAfterRound = false;
	float MatchStartTime = 0.0f;
	bool bMasterFirstHitAnnounced[2] = { false, false };
	bool bMasterHalfHealthAnnounced[2] = { false, false };
	bool bMasterCriticalHealthAnnounced[2] = { false, false };
};
