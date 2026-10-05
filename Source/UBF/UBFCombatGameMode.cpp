#include "UBFCombatGameMode.h"

#include "UBFBotAIController.h"
#include "UBFCombatCharacter.h"
#include "UBFCombatGameState.h"
#include "UBFCharacterDefinition.h"
#include "UBFCombatHUD.h"
#include "UBFCombatPlayerController.h"
#include "UBFGoldenGolemPickup.h"
#include "UBFPlayerData.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr float RoundCountdownDuration = 3.0f;
	constexpr float RoundResultsDuration = 6.0f;
	constexpr int32 MaximumRounds = 3;
	constexpr int32 RoundsToWinMatch = 2;
}

AUBFCombatGameMode::AUBFCombatGameMode()
{
	DefaultPawnClass = AUBFCombatCharacter::StaticClass();
	PlayerControllerClass = AUBFCombatPlayerController::StaticClass();
	HUDClass = AUBFCombatHUD::StaticClass();
	GameStateClass = AUBFCombatGameState::StaticClass();
	bUseSeamlessTravel = true;
}

void AUBFCombatGameMode::BeginPlay()
{
	Super::BeginPlay();
	MatchStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetGolemMode(bGolemMode);
		MatchState->SetRoundPhase(EUBFMatchPhase::LoadingSync, RoundNumber, INDEX_NONE, 0.0f);
		MatchState->SetTeamScore(0, 0);
		MatchState->SetTeamScore(1, 0);
	}
	UpdatePlayerReadiness();
}

float AUBFCombatGameMode::GetMatchDuration() const
{
	return GetWorld() ? FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - MatchStartTime) : 0.0f;
}

void AUBFCombatGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bGolemMode = UGameplayStatics::ParseOption(Options, TEXT("Mode"))
		.Equals(TEXT("Golem"), ESearchCase::IgnoreCase);

	const FString TeamSizeOption = UGameplayStatics::ParseOption(Options, TEXT("TeamSize"));
	if (!TeamSizeOption.IsEmpty())
	{
		TeamSize = FMath::Clamp(FCString::Atoi(*TeamSizeOption), 1, 5);
	}
	const FString CharacterOption = UGameplayStatics::ParseOption(Options, TEXT("CharacterID"));
	if (!CharacterOption.IsEmpty())
	{
		HumanCharacterId = FName(*CharacterOption);
	}

	const FString BotFillOption = UGameplayStatics::ParseOption(Options, TEXT("BotFill"));
	if (BotFillOption.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		BotFillMode = EUBFBotFillMode::AllOpenSlots;
	}
	else if (BotFillOption.Equals(TEXT("None"), ESearchCase::IgnoreCase))
{
		BotFillMode = EUBFBotFillMode::None;
	}
	else if (BotFillOption.Equals(TEXT("Opponent"), ESearchCase::IgnoreCase))
	{
		BotFillMode = EUBFBotFillMode::OpponentTeam;
	}
	const FString DifficultyOption = UGameplayStatics::ParseOption(Options, TEXT("BotDifficulty"));
	if (DifficultyOption.Equals(TEXT("EASY"), ESearchCase::IgnoreCase))
	{
		BotDifficulty = TEXT("EASY");
	}
	else if (DifficultyOption.Equals(TEXT("HARD"), ESearchCase::IgnoreCase))
	{
		BotDifficulty = TEXT("HARD");
	}
}

void AUBFCombatGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (!HasAuthority() || !NewPlayer)
	{
		return;
	}
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>();
		MatchState && MatchState->GetRoundPhase() == EUBFMatchPhase::Countdown)
	{
		GetWorldTimerManager().ClearTimer(MatchFlowTimer);
		ResetPlayerReadiness();
		MatchState->SetRoundPhase(EUBFMatchPhase::LoadingSync, RoundNumber, INDEX_NONE, 0.0f);
	}

	if (AUBFCombatCharacter* Character = Cast<AUBFCombatCharacter>(NewPlayer->GetPawn()))
	{
		int32 HumanCounts[2] = { 0, 0 };
		GetHumanTeamCounts(NewPlayer, HumanCounts);
		const int32 TeamId = HumanCounts[0] <= HumanCounts[1] ? 0 : 1;
		Character->SetTeamId(TeamId);
		const FName RequestedCharacterId = HumanCharacterId.IsNone() ? DefaultCharacterId : HumanCharacterId;
		TSet<FName> TeammateCharacterIds;
		for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
		{
			const AUBFCombatCharacter* Teammate = *It;
			if (Teammate != Character && IsValid(Teammate) && !Teammate->IsMasterGolem()
				&& Teammate->GetTeamId() == TeamId)
			{
				TeammateCharacterIds.Add(Teammate->GetCharacterId());
			}
		}
		FName AssignedCharacterId = RequestedCharacterId;
		if (TeammateCharacterIds.Contains(AssignedCharacterId))
		{
			if (UGameInstance* Instance = GetGameInstance())
			{
				if (UUBFCharacterCatalogSubsystem* Catalog = Instance->GetSubsystem<UUBFCharacterCatalogSubsystem>())
				{
					AssignedCharacterId = Catalog->ChooseCharacterId(TeammateCharacterIds, HumanCounts[TeamId]);
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("El personaje pedido ya está elegido por un compañero; se asignó %s."),
				*AssignedCharacterId.ToString());
		}
		if (!AssignedCharacterId.IsNone())
		{
			Character->SetCharacterId(AssignedCharacterId);
		}
		Character->SetActorTransform(GetTeamSpawnTransform(TeamId, HumanCounts[TeamId]));
		if (!bRoundActive || (GetGameState<AUBFCombatGameState>()
			&& GetGameState<AUBFCombatGameState>()->GetRoundPhase() != EUBFMatchPhase::InRound))
		{
			LockFighterForTransition(Character);
		}
		else
		{
			Character->LockForRoundTransition();
		}
		UE_LOG(LogTemp, Log, TEXT("UBF combat player assigned to team %d (%d per team)."), TeamId, TeamSize);
	}

	ReconcileBotRoster();
	UpdatePlayerReadiness();
}

void AUBFCombatGameMode::Logout(AController* Exiting)
{
	const AUBFCombatCharacter* ExitingFighter = Exiting
		? Cast<AUBFCombatCharacter>(Exiting->GetPawn()) : nullptr;
	const int32 ExitingTeam = ExitingFighter ? FMath::Clamp(ExitingFighter->GetTeamId(), 0, 1) : INDEX_NONE;
	const bool bPlayerLeavingDuringMatch = bRoundActive && Cast<APlayerController>(Exiting) != nullptr;
	Super::Logout(Exiting);
	if (HasAuthority())
	{
		if (bPlayerLeavingDuringMatch && ExitingTeam != INDEX_NONE)
		{
			int32 HumanCounts[2] = { 0, 0 };
			GetHumanTeamCounts(nullptr, HumanCounts);
			const int32 RemainingHumans = HumanCounts[0] + HumanCounts[1];
			if (RemainingHumans > 0 && (HumanCounts[0] == 0 || HumanCounts[1] == 0
				|| HumanCounts[0] != HumanCounts[1]))
			{
				bReturnToFrontEndAfterRound = true;
				EndRound(INDEX_NONE, true);
				return;
			}
		}
		// Do not replace a leaver with a bot during active combat.
		if (!bRoundActive)
		{
			ReconcileBotRoster();
		}
		UpdatePlayerReadiness();
	}
}

void AUBFCombatGameMode::HandleSurrender(AUBFCombatPlayerController* PlayerController)
{
	if (!HasAuthority() || !bRoundActive || !IsValid(PlayerController))
	{
		return;
	}
	const AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>();
	AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(PlayerController->GetPawn());
	if (!MatchState || MatchState->GetRoundNumber() < 3
		|| MatchState->GetRoundPhase() != EUBFMatchPhase::InRound || !Fighter)
	{
		return;
	}
	bReturnToFrontEndAfterRound = true;
	EndRound(1 - FMath::Clamp(Fighter->GetTeamId(), 0, 1), false);
}

void AUBFCombatGameMode::HandleAbandonRoom(AUBFCombatPlayerController* PlayerController)
{
	if (!HasAuthority() || !IsValid(PlayerController) || bMatchFinished)
	{
		return;
	}
	bReturnToFrontEndAfterRound = true;
	if (bRoundActive)
	{
		EndRound(INDEX_NONE, true);
	}
	else
	{
		FinishMatch(INDEX_NONE, true);
	}
}

void AUBFCombatGameMode::NotifyPlayerReadyChanged()
{
	if (HasAuthority())
	{
		UpdatePlayerReadiness();
	}
}

void AUBFCombatGameMode::UpdatePlayerReadiness()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	int32 TotalPlayers = 0;
	int32 ReadyPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AUBFCombatPlayerController* Player = Cast<AUBFCombatPlayerController>(It->Get());
		if (!Player)
		{
			continue;
		}
		++TotalPlayers;
		ReadyPlayers += Player->IsReadyForRound() ? 1 : 0;
	}
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetReadyPlayerCounts(ReadyPlayers, TotalPlayers);
	}
	if (TotalPlayers > 0 && ReadyPlayers == TotalPlayers
		&& GetGameState<AUBFCombatGameState>()
		&& GetGameState<AUBFCombatGameState>()->GetRoundPhase() == EUBFMatchPhase::LoadingSync)
	{
		StartRoundCountdown();
	}
}

void AUBFCombatGameMode::ResetPlayerReadiness()
{
	if (!GetWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AUBFCombatPlayerController* Player = Cast<AUBFCombatPlayerController>(It->Get()))
		{
			Player->ResetReadyStateForNextRound();
		}
	}
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetReadyPlayerCounts(0, 0);
	}
}

bool AUBFCombatGameMode::AreAllPlayersReady() const
{
	if (!GetWorld())
	{
		return false;
	}
	int32 TotalPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AUBFCombatPlayerController* Player = Cast<AUBFCombatPlayerController>(It->Get());
		if (!Player)
		{
			continue;
		}
		++TotalPlayers;
		if (!Player->IsReadyForRound())
		{
			return false;
		}
	}
	return TotalPlayers > 0;
}

void AUBFCombatGameMode::NotifyFighterEliminated()
{
	if (!HasAuthority() || bMatchFinished || !bRoundActive || bOutcomeCheckPending || !GetWorld())
	{
		return;
	}

	// Defer one frame so every hit in the same area attack can resolve before
	// deciding whether both teams were eliminated simultaneously.
	bOutcomeCheckPending = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AUBFCombatGameMode::CheckMatchOutcome);
}

void AUBFCombatGameMode::NotifyGoldenBearerPickedUp(AUBFCombatCharacter* Carrier, AUBFGoldenGolemPickup* Pickup)
{
	// Compatibility path for any stale pickup actor left in a map. The sword
	// can only be awarded by the last hit on the actual Golden Golem.
	if (IsValid(Pickup))
	{
		Pickup->Destroy();
	}
}

void AUBFCombatGameMode::NotifyGoldenBearerEliminated(AUBFCombatCharacter* Carrier)
{
	if (!HasAuthority() || !bGolemMode || !Carrier || CurrentGoldenBearer.Get() != Carrier)
	{
		return;
	}
	ClearGoldenBearer();
	if (!bMatchFinished)
	{
		// Rakion's official Golem War flow respawns the neutral Golden Golem
		// after its sword carrier falls; a new last hit awards the next sword.
		SpawnGoldenGolem(FVector(0.0f, 0.0f, 232.0f));
	}
}

void AUBFCombatGameMode::NotifyGoldenGolemEliminated(AUBFCombatCharacter* Golem,
	AUBFCombatCharacter* LastHitBy)
{
	if (!HasAuthority() || !bGolemMode || !bRoundActive || bMatchFinished || !IsValid(Golem))
	{
		return;
	}
	Golem->Destroy();
	if (!IsValid(LastHitBy) || LastHitBy->IsMasterGolem() || LastHitBy->IsGoldenGolem()
		|| LastHitBy->GetCurrentHealth() <= 0.0f)
	{
		SpawnGoldenGolem(FVector(0.0f, 0.0f, 232.0f));
		return;
	}
	ClearGoldenBearer();
	CurrentGoldenBearer = LastHitBy;
	LastHitBy->SetGoldenBearer(true);
	LastHitBy->ShowCombatMessage(TEXT("ESPADA DORADA  ·  SOLO TÚ PUEDES DAÑAR AL GOLEM MAESTRO RIVAL"), 4.0f);
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->AddMatchCue(EUBFMatchCueType::GoldenGolemKilled, INDEX_NONE, LastHitBy->GetTeamId());
	}
	UE_LOG(LogTemp, Log, TEXT("Golem Mode: last hit on Golden Golem awarded the sword to %s (Team %d)."),
		*GetNameSafe(LastHitBy), LastHitBy->GetTeamId());
}

void AUBFCombatGameMode::NotifyMasterGolemDamaged(int32 TeamId, AUBFCombatCharacter* Attacker,
	float HealthRatio)
{
	if (!HasAuthority() || !bGolemMode || !bRoundActive || bMatchFinished
		|| (TeamId != 0 && TeamId != 1) || !IsValid(Attacker)
		|| !Attacker->IsGoldenBearer() || Attacker->GetTeamId() == TeamId)
	{
		return;
	}

	AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>();
	if (!MatchState)
	{
		return;
	}

	if (!bMasterFirstHitAnnounced[TeamId])
	{
		bMasterFirstHitAnnounced[TeamId] = true;
		MatchState->AddMatchCue(EUBFMatchCueType::MasterGolemFirstHit, TeamId, Attacker->GetTeamId());
	}
	if (!bMasterHalfHealthAnnounced[TeamId] && HealthRatio <= 0.50f)
	{
		bMasterHalfHealthAnnounced[TeamId] = true;
		MatchState->AddMatchCue(EUBFMatchCueType::MasterGolemHalfHealth, TeamId, Attacker->GetTeamId());
	}
	if (!bMasterCriticalHealthAnnounced[TeamId] && HealthRatio <= 0.10f)
	{
		bMasterCriticalHealthAnnounced[TeamId] = true;
		MatchState->AddMatchCue(EUBFMatchCueType::MasterGolemCriticalHealth, TeamId, Attacker->GetTeamId());
	}
}

void AUBFCombatGameMode::NotifyMasterGolemEliminated(int32 TeamId)
{
	if (HasAuthority() && bGolemMode && bRoundActive && !bMatchFinished && (TeamId == 0 || TeamId == 1))
	{
		UE_LOG(LogTemp, Log, TEXT("Golem Mode: team %d's Master Golem was defeated."), TeamId);
		EndRound(1 - TeamId, false);
	}
}

void AUBFCombatGameMode::SpawnGolemObjectives()
{
	if (!HasAuthority() || !bGolemMode || !bRoundActive || !GetWorld())
	{
		return;
	}
	for (int32 TeamId = 0; TeamId < 2; ++TeamId)
	{
		const float X = TeamId == 0 ? -1180.0f : 1180.0f;
		const FTransform SpawnTransform(FRotator(0.0f, TeamId == 0 ? 0.0f : 180.0f, 0.0f),
			FVector(X, 0.0f, 232.0f));
		if (AUBFCombatCharacter* MasterGolem = GetWorld()->SpawnActor<AUBFCombatCharacter>(
			AUBFCombatCharacter::StaticClass(), SpawnTransform))
		{
			MasterGolem->SetTeamId(TeamId);
			MasterGolem->SetAsMasterGolem();
		}
	}
	SpawnGoldenGolem(FVector(0.0f, 0.0f, 232.0f));
}

void AUBFCombatGameMode::SpawnGoldenGolem(const FVector& Location)
{
	if (HasAuthority() && bGolemMode && bRoundActive && GetWorld())
	{
		if (AUBFCombatCharacter* GoldenGolem = GetWorld()->SpawnActor<AUBFCombatCharacter>(
			AUBFCombatCharacter::StaticClass(), Location, FRotator::ZeroRotator))
		{
			GoldenGolem->SetAsGoldenGolem();
		}
	}
}

void AUBFCombatGameMode::ClearGoldenBearer()
{
	if (AUBFCombatCharacter* PreviousCarrier = CurrentGoldenBearer.Get())
	{
		PreviousCarrier->SetGoldenBearer(false);
	}
	CurrentGoldenBearer.Reset();
}

void AUBFCombatGameMode::StartRoundCountdown()
{
	if (!HasAuthority() || bMatchFinished || !GetWorld() || !AreAllPlayersReady())
	{
		return;
	}
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetRoundPhase(EUBFMatchPhase::Countdown, RoundNumber, INDEX_NONE,
			GetWorld()->GetTimeSeconds() + RoundCountdownDuration);
	}
	GetWorld()->GetTimerManager().SetTimer(MatchFlowTimer, this,
		&AUBFCombatGameMode::StartRound, RoundCountdownDuration, false);
}

void AUBFCombatGameMode::StartRound()
{
	if (!HasAuthority() || bMatchFinished || !GetWorld())
	{
		return;
	}
	bRoundActive = true;
	for (int32 TeamId = 0; TeamId < 2; ++TeamId)
	{
		bMasterFirstHitAnnounced[TeamId] = false;
		bMasterHalfHealthAnnounced[TeamId] = false;
		bMasterCriticalHealthAnnounced[TeamId] = false;
	}
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetRoundPhase(EUBFMatchPhase::InRound, RoundNumber, INDEX_NONE, 0.0f);
		if (bGolemMode)
		{
			MatchState->SetTeamScore(0, 0);
			MatchState->SetTeamScore(1, 0);
		}
	}
	if (bGolemMode)
	{
		SpawnGolemObjectives();
	}
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Fighter = *It;
		if (!IsValid(Fighter) || Fighter->IsMasterGolem())
		{
			continue;
		}
		if (UCharacterMovementComponent* Movement = Fighter->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		if (Cast<APlayerController>(Fighter->GetController()))
		{
			Fighter->UnlockForRoundStart();
		}
	}
	UE_LOG(LogTemp, Log, TEXT("UBF round %d started (series score %d-%d)."),
		RoundNumber, RoundWins[0], RoundWins[1]);
}

void AUBFCombatGameMode::EndRound(int32 WinningTeamId, bool bIsDraw)
{
	if (!HasAuthority() || bMatchFinished || !bRoundActive || !GetWorld())
	{
		return;
	}
	bRoundActive = false;
	bOutcomeCheckPending = false;
	ClearGoldenBearer();
	if (!bIsDraw && (WinningTeamId == 0 || WinningTeamId == 1))
	{
		++RoundWins[WinningTeamId];
		if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
		{
			MatchState->SetRoundWins(WinningTeamId, RoundWins[WinningTeamId]);
		}
	}
	DestroyGolemObjectives();
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetRoundPhase(EUBFMatchPhase::RoundResults, RoundNumber,
			bIsDraw ? INDEX_NONE : WinningTeamId, GetWorld()->GetTimeSeconds() + RoundResultsDuration);
	}
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Fighter = *It;
		if (IsValid(Fighter) && !Fighter->IsMasterGolem() && !Fighter->IsGoldenGolem())
		{
			LockFighterForTransition(Fighter);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("UBF round %d finished: %s (series %d-%d)."), RoundNumber,
		bIsDraw ? TEXT("draw") : *FString::Printf(TEXT("team %d wins"), WinningTeamId),
		RoundWins[0], RoundWins[1]);
	GetWorld()->GetTimerManager().SetTimer(MatchFlowTimer, this,
		&AUBFCombatGameMode::ContinueAfterRoundResults, RoundResultsDuration, false);
}

void AUBFCombatGameMode::ContinueAfterRoundResults()
{
	if (!HasAuthority() || bMatchFinished || !GetWorld())
	{
		return;
	}
	if (bReturnToFrontEndAfterRound || RoundWins[0] >= RoundsToWinMatch
		|| RoundWins[1] >= RoundsToWinMatch || RoundNumber >= MaximumRounds)
	{
		const int32 WinningTeamId = RoundWins[0] == RoundWins[1]
			? INDEX_NONE : (RoundWins[0] > RoundWins[1] ? 0 : 1);
		FinishMatch(WinningTeamId, WinningTeamId == INDEX_NONE);
		return;
	}

	++RoundNumber;
	int32 SlotsByTeam[2] = { 0, 0 };
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Fighter = *It;
		if (!IsValid(Fighter) || Fighter->IsMasterGolem() || Fighter->IsGoldenGolem())
		{
			continue;
		}
		const int32 TeamId = FMath::Clamp(Fighter->GetTeamId(), 0, 1);
		Fighter->ResetForNextRound(GetTeamSpawnTransform(TeamId, SlotsByTeam[TeamId]++));
		LockFighterForTransition(Fighter);
	}
	ResetPlayerReadiness();
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetRoundPhase(EUBFMatchPhase::LoadingSync, RoundNumber, INDEX_NONE, 0.0f);
		MatchState->SetTeamScore(0, 0);
		MatchState->SetTeamScore(1, 0);
	}
	UE_LOG(LogTemp, Log, TEXT("UBF preparing round %d at the team spawns."), RoundNumber);
	UpdatePlayerReadiness();
}

void AUBFCombatGameMode::DestroyGolemObjectives()
{
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		if (AUBFCombatCharacter* Fighter = *It; IsValid(Fighter)
			&& (Fighter->IsMasterGolem() || Fighter->IsGoldenGolem()))
		{
			Fighter->Destroy();
		}
	}
	for (TActorIterator<AUBFGoldenGolemPickup> It(GetWorld()); It; ++It)
	{
		if (AUBFGoldenGolemPickup* Pickup = *It; IsValid(Pickup))
		{
			Pickup->Destroy();
		}
	}
}

void AUBFCombatGameMode::LockFighterForTransition(AUBFCombatCharacter* Fighter)
{
	if (!IsValid(Fighter))
	{
		return;
	}
	if (UCharacterMovementComponent* Movement = Fighter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	if (AUBFBotAIController* BotController = Cast<AUBFBotAIController>(Fighter->GetController()))
	{
		BotController->StopMovement();
	}
	if (Cast<APlayerController>(Fighter->GetController()))
	{
		Fighter->LockForRoundTransition();
	}
}

void AUBFCombatGameMode::CheckMatchOutcome()
{
	bOutcomeCheckPending = false;
	if (!HasAuthority() || bMatchFinished || !bRoundActive || !GetWorld())
	{
		return;
	}

	int32 TeamTotals[2] = { 0, 0 };
	int32 TeamAlive[2] = { 0, 0 };
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		const AUBFCombatCharacter* Fighter = *It;
		if (!IsValid(Fighter) || Fighter->IsMasterGolem() || Fighter->IsGoldenGolem())
		{
			continue;
		}

		const int32 Team = FMath::Clamp(Fighter->GetTeamId(), 0, 1);
		++TeamTotals[Team];
		if (Fighter->GetCurrentHealth() > 0.0f)
		{
			++TeamAlive[Team];
		}
	}

	// Training mode and an unfilled roster have no opposing team, so they do
	// not end merely because the only fighter was knocked out.
	if (TeamTotals[0] == 0 || TeamTotals[1] == 0 || (TeamAlive[0] > 0 && TeamAlive[1] > 0))
	{
		return;
	}

	if (TeamAlive[0] == 0 && TeamAlive[1] == 0)
	{
		EndRound(INDEX_NONE, true);
	}
	else
	{
		EndRound(TeamAlive[0] > 0 ? 0 : 1, false);
	}
}

void AUBFCombatGameMode::FinishMatch(int32 WinningTeamId, bool bIsDraw)
{
	if (!HasAuthority() || bMatchFinished || !GetWorld())
	{
		return;
	}

	bMatchFinished = true;
	bRoundActive = false;
	GetWorld()->GetTimerManager().ClearTimer(MatchFlowTimer);
	ClearGoldenBearer();
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetRoundPhase(EUBFMatchPhase::MatchResults, RoundNumber,
			bIsDraw ? INDEX_NONE : WinningTeamId, 0.0f);
	}
	UE_LOG(LogTemp, Log, TEXT("UBF match finished: %s."), bIsDraw
		? TEXT("draw") : *FString::Printf(TEXT("team %d wins"), WinningTeamId));

	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		AUBFCombatCharacter* Fighter = *It;
		if (!IsValid(Fighter))
		{
			continue;
		}

		if (UCharacterMovementComponent* Movement = Fighter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
		if (AUBFBotAIController* BotController = Cast<AUBFBotAIController>(Fighter->GetController()))
		{
			BotController->StopMovement();
		}

		if (APlayerController* Player = Cast<APlayerController>(Fighter->GetController()))
		{
			Player->SetIgnoreMoveInput(true);
			Player->SetIgnoreLookInput(true);
			FUBFMatchRewardResult Rewards;
			if (Player->IsLocalController())
			{
				if (UGameInstance* Instance = GetGameInstance())
				{
					if (UUBFPlayerDataSubsystem* PlayerData = Instance->GetSubsystem<UUBFPlayerDataSubsystem>())
					{
						Rewards = PlayerData->GrantMatchRewards(
							Fighter->GetMatchDamageDealt(), Fighter->GetMatchDamageReceived(),
							Fighter->GetMatchKnockouts(), Fighter->GetMatchDeaths(), Fighter->GetMaximumComboHits(),
							!bIsDraw && Fighter->GetTeamId() == WinningTeamId, bIsDraw);
					}
				}
			}
			Fighter->ShowMatchResultMessage(bIsDraw
				? TEXT("EMPATE")
				: (Fighter->GetTeamId() == WinningTeamId ? TEXT("VICTORIA") : TEXT("DERROTA")),
				GetMatchDuration(), Rewards.Experience, Rewards.Gold, Rewards.EventPoints);
		}
	}
	GetWorldTimerManager().SetTimer(MatchFlowTimer, this,
		&AUBFCombatGameMode::ReturnToFrontEnd, RoundResultsDuration, false);
}

void AUBFCombatGameMode::ReturnToFrontEnd()
{
	if (HasAuthority() && GetWorld())
	{
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")));
	}
}

int32 AUBFCombatGameMode::GetPrimaryHumanTeam() const
{
	if (GetWorld())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* Player = It->Get())
			{
				if (const AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(Player->GetPawn()))
				{
					return Fighter->GetTeamId();
				}
			}
		}
	}
	return 0;
}

void AUBFCombatGameMode::GetHumanTeamCounts(AController* ExcludedController, int32 OutCounts[2]) const
{
	OutCounts[0] = 0;
	OutCounts[1] = 0;
	if (!GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!Player || Player == ExcludedController)
		{
			continue;
		}

		if (const AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(Player->GetPawn()))
		{
			++OutCounts[FMath::Clamp(Fighter->GetTeamId(), 0, 1)];
		}
	}
}

FTransform AUBFCombatGameMode::GetTeamSpawnTransform(int32 TeamId, int32 SlotIndex) const
{
	const int32 SafeTeamId = FMath::Clamp(TeamId, 0, 1);
	const float YOffset = (static_cast<float>(SlotIndex) - (static_cast<float>(TeamSize) - 1.0f) * 0.5f) * 220.0f;
	const float X = SafeTeamId == 0 ? -720.0f : 720.0f;
	const FRotator Rotation(0.0f, SafeTeamId == 0 ? 0.0f : 180.0f, 0.0f);
	return FTransform(Rotation, FVector(X, YOffset, 98.0f));
}

AUBFBotAIController* AUBFCombatGameMode::SpawnBot(int32 TeamId, int32 SlotIndex)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	const FTransform SpawnTransform = GetTeamSpawnTransform(TeamId, SlotIndex);
	TSet<FName> UsedCharacterIds;
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		const AUBFCombatCharacter* ExistingFighter = *It;
		if (IsValid(ExistingFighter) && !ExistingFighter->IsMasterGolem()
			&& ExistingFighter->GetTeamId() == TeamId)
		{
			UsedCharacterIds.Add(ExistingFighter->GetCharacterId());
		}
	}
	AUBFCombatCharacter* BotPawn = GetWorld()->SpawnActor<AUBFCombatCharacter>(
		AUBFCombatCharacter::StaticClass(), SpawnTransform);
	AUBFBotAIController* BotController = GetWorld()->SpawnActor<AUBFBotAIController>(
		AUBFBotAIController::StaticClass(), SpawnTransform);
	if (!BotPawn || !BotController)
	{
		if (BotPawn) BotPawn->Destroy();
		if (BotController) BotController->Destroy();
		return nullptr;
	}

	BotPawn->SetTeamId(TeamId);
	FName BotCharacterId = NAME_None;
	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UUBFCharacterCatalogSubsystem* Catalog = Instance->GetSubsystem<UUBFCharacterCatalogSubsystem>())
		{
			BotCharacterId = Catalog->ChooseCharacterId(UsedCharacterIds, SlotIndex);
		}
	}
	if (BotCharacterId.IsNone() || UsedCharacterIds.Contains(BotCharacterId))
	{
		UE_LOG(LogTemp, Error, TEXT("No hay un personaje único disponible para el bot del equipo %d."), TeamId);
		BotPawn->Destroy();
		BotController->Destroy();
		return nullptr;
	}
	BotPawn->SetCharacterId(BotCharacterId);
	const EUBFBotDifficulty Difficulty = BotDifficulty == TEXT("EASY") ? EUBFBotDifficulty::Easy
		: (BotDifficulty == TEXT("HARD") ? EUBFBotDifficulty::Hard : EUBFBotDifficulty::Normal);
	BotController->SetDifficulty(Difficulty);
	BotController->Possess(BotPawn);
	if (!bRoundActive)
	{
		LockFighterForTransition(BotPawn);
	}
	UE_LOG(LogTemp, Log, TEXT("Spawned UBF AI bot %d for team %d (slot %d)."), NextBotNumber++, TeamId, SlotIndex + 1);
	return BotController;
}

void AUBFCombatGameMode::ReconcileBotRoster()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	int32 HumanCounts[2] = { 0, 0 };
	GetHumanTeamCounts(nullptr, HumanCounts);

	int32 DesiredBotCounts[2] = { 0, 0 };
	if (BotFillMode == EUBFBotFillMode::OpponentTeam)
	{
		const int32 EnemyTeam = 1 - GetPrimaryHumanTeam();
		DesiredBotCounts[EnemyTeam] = FMath::Max(0, TeamSize - HumanCounts[EnemyTeam]);
	}
	else if (BotFillMode == EUBFBotFillMode::AllOpenSlots)
	{
		DesiredBotCounts[0] = FMath::Max(0, TeamSize - HumanCounts[0]);
		DesiredBotCounts[1] = FMath::Max(0, TeamSize - HumanCounts[1]);
	}

	TArray<AUBFBotAIController*> ExistingBots;
	for (TActorIterator<AUBFBotAIController> It(GetWorld()); It; ++It)
	{
		AUBFBotAIController* Bot = *It;
		AUBFCombatCharacter* Fighter = Bot ? Cast<AUBFCombatCharacter>(Bot->GetPawn()) : nullptr;
		if (Bot && Fighter)
		{
			ExistingBots.Add(Bot);
		}
	}

	int32 KeptBots[2] = { 0, 0 };
	for (AUBFBotAIController* Bot : ExistingBots)
	{
		AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(Bot->GetPawn());
		if (!Fighter)
		{
			Bot->Destroy();
			continue;
		}

		const int32 TeamId = FMath::Clamp(Fighter->GetTeamId(), 0, 1);
		if (KeptBots[TeamId] < DesiredBotCounts[TeamId])
		{
			++KeptBots[TeamId];
		}
		else
		{
			Fighter->Destroy();
			Bot->Destroy();
		}
	}

	for (int32 TeamId = 0; TeamId < 2; ++TeamId)
	{
		for (int32 SlotIndex = HumanCounts[TeamId] + KeptBots[TeamId];
			SlotIndex < HumanCounts[TeamId] + DesiredBotCounts[TeamId]; ++SlotIndex)
		{
			if (!SpawnBot(TeamId, SlotIndex))
			{
				break;
			}
		}
	}
}
