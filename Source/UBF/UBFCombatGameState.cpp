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

void AUBFCombatGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUBFCombatGameState, bGolemMode);
	DOREPLIFETIME(AUBFCombatGameState, TeamAScore);
	DOREPLIFETIME(AUBFCombatGameState, TeamBScore);
}
