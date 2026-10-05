#include "UBFCombatGameState.h"

#include "Net/UnrealNetwork.h"

AUBFCombatGameState::AUBFCombatGameState()
{
	bReplicates = true;
}

void AUBFCombatGameState::SetGolemMode(bool bEnabled)
{
	if (HasAuthority())
	{
		bGolemMode = bEnabled;
	}
}

void AUBFCombatGameState::SetTeamScore(int32 TeamId, int32 Score)
{
	if (!HasAuthority())
	{
		return;
	}
	if (TeamId == 0)
	{
		TeamAScore = FMath::Max(0, Score);
	}
	else if (TeamId == 1)
	{
		TeamBScore = FMath::Max(0, Score);
	}
}

int32 AUBFCombatGameState::GetTeamScore(int32 TeamId) const
{
	return TeamId == 0 ? TeamAScore : (TeamId == 1 ? TeamBScore : 0);
}

void AUBFCombatGameState::SetRoundPhase(EUBFMatchPhase NewPhase, int32 NewRoundNumber,
	int32 NewRoundWinnerTeamId, float NewPhaseEndServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	RoundPhase = NewPhase;
	RoundNumber = FMath::Max(1, NewRoundNumber);
	RoundWinnerTeamId = NewRoundWinnerTeamId;
	PhaseEndServerTime = FMath::Max(0.0f, NewPhaseEndServerTime);
	ForceNetUpdate();
}

void AUBFCombatGameState::SetRoundWins(int32 TeamId, int32 Wins)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TeamId == 0)
	{
		TeamARoundWins = FMath::Max(0, Wins);
	}
	else if (TeamId == 1)
	{
		TeamBRoundWins = FMath::Max(0, Wins);
	}
	ForceNetUpdate();
}

void AUBFCombatGameState::SetReadyPlayerCounts(int32 ReadyCount, int32 TotalCount)
{
	if (!HasAuthority())
	{
		return;
	}
	TotalPlayerCount = FMath::Max(0, TotalCount);
	ReadyPlayerCount = FMath::Clamp(ReadyCount, 0, TotalPlayerCount);
	ForceNetUpdate();
}

void AUBFCombatGameState::AddMatchCue(EUBFMatchCueType Type, int32 TeamId, int32 SourceTeamId)
{
	if (!HasAuthority() || Type == EUBFMatchCueType::None)
	{
		return;
	}

	FUBFMatchCueEvent& Event = MatchCueEvents.AddDefaulted_GetRef();
	Event.Sequence = NextMatchCueSequence++;
	Event.Type = Type;
	Event.TeamId = TeamId;
	Event.SourceTeamId = SourceTeamId;
	constexpr int32 MaximumRetainedCueEvents = 24;
	if (MatchCueEvents.Num() > MaximumRetainedCueEvents)
	{
		MatchCueEvents.RemoveAt(0, MatchCueEvents.Num() - MaximumRetainedCueEvents, EAllowShrinking::No);
	}
	ForceNetUpdate();
}

int32 AUBFCombatGameState::GetRoundWins(int32 TeamId) const
{
	return TeamId == 0 ? TeamARoundWins : (TeamId == 1 ? TeamBRoundWins : 0);
}

float AUBFCombatGameState::GetPhaseTimeRemaining() const
{
	return PhaseEndServerTime > 0.0f
		? FMath::Max(0.0f, PhaseEndServerTime - GetServerWorldTimeSeconds())
		: 0.0f;
}

void AUBFCombatGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUBFCombatGameState, bGolemMode);
	DOREPLIFETIME(AUBFCombatGameState, TeamAScore);
	DOREPLIFETIME(AUBFCombatGameState, TeamBScore);
	DOREPLIFETIME(AUBFCombatGameState, RoundPhase);
	DOREPLIFETIME(AUBFCombatGameState, RoundNumber);
	DOREPLIFETIME(AUBFCombatGameState, RoundWinnerTeamId);
	DOREPLIFETIME(AUBFCombatGameState, TeamARoundWins);
	DOREPLIFETIME(AUBFCombatGameState, TeamBRoundWins);
	DOREPLIFETIME(AUBFCombatGameState, ReadyPlayerCount);
	DOREPLIFETIME(AUBFCombatGameState, TotalPlayerCount);
	DOREPLIFETIME(AUBFCombatGameState, PhaseEndServerTime);
	DOREPLIFETIME(AUBFCombatGameState, MatchCueEvents);
}
