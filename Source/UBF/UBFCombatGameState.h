#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "UBFCombatGameState.generated.h"

UENUM(BlueprintType)
enum class EUBFMatchPhase : uint8
{
	LoadingSync,
	Countdown,
	InRound,
	RoundResults,
	MatchResults
};

UENUM(BlueprintType)
enum class EUBFMatchCueType : uint8
{
	None,
	GoldenGolemKilled,
	MasterGolemFirstHit,
	MasterGolemHalfHealth,
	MasterGolemCriticalHealth
};

USTRUCT(BlueprintType)
struct FUBFMatchCueEvent
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	EUBFMatchCueType Type = EUBFMatchCueType::None;

	/** Team that should receive a team-specific cue, or INDEX_NONE for both teams. */
	UPROPERTY()
	int32 TeamId = INDEX_NONE;

	/** Team responsible for the event, when applicable. */
	UPROPERTY()
	int32 SourceTeamId = INDEX_NONE;
};

UCLASS()
class UBF_API AUBFCombatGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AUBFCombatGameState();
	void SetGolemMode(bool bEnabled);
	void SetTeamScore(int32 TeamId, int32 Score);
	void SetRoundPhase(EUBFMatchPhase NewPhase, int32 NewRoundNumber, int32 NewRoundWinnerTeamId,
		float NewPhaseEndServerTime);
	void SetRoundWins(int32 TeamId, int32 Wins);
	void SetReadyPlayerCounts(int32 ReadyCount, int32 TotalCount);
	void AddMatchCue(EUBFMatchCueType Type, int32 TeamId = INDEX_NONE, int32 SourceTeamId = INDEX_NONE);
	const TArray<FUBFMatchCueEvent>& GetMatchCueEvents() const { return MatchCueEvents; }
	bool IsGolemMode() const { return bGolemMode; }
	int32 GetTeamScore(int32 TeamId) const;
	EUBFMatchPhase GetRoundPhase() const { return RoundPhase; }
	int32 GetRoundNumber() const { return RoundNumber; }
	int32 GetRoundWinnerTeamId() const { return RoundWinnerTeamId; }
	int32 GetRoundWins(int32 TeamId) const;
	int32 GetReadyPlayerCount() const { return ReadyPlayerCount; }
	int32 GetTotalPlayerCount() const { return TotalPlayerCount; }
	float GetPhaseTimeRemaining() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	bool bGolemMode = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	int32 TeamAScore = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Golem Mode")
	int32 TeamBScore = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	EUBFMatchPhase RoundPhase = EUBFMatchPhase::LoadingSync;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	int32 RoundNumber = 1;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	int32 RoundWinnerTeamId = INDEX_NONE;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	int32 TeamARoundWins = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	int32 TeamBRoundWins = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	int32 ReadyPlayerCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	int32 TotalPlayerCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	float PhaseEndServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category="UBF|Match")
	TArray<FUBFMatchCueEvent> MatchCueEvents;

	int32 NextMatchCueSequence = 1;
};
