#include "UBFCombatGameMode.h"

#include "UBFBotAIController.h"
#include "UBFCombatCharacter.h"
#include "UBFCombatGameState.h"
#include "UBFCharacterDefinition.h"
#include "UBFCombatHUD.h"
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

AUBFCombatGameMode::AUBFCombatGameMode()
{
	DefaultPawnClass = AUBFCombatCharacter::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	HUDClass = AUBFCombatHUD::StaticClass();
	GameStateClass = AUBFCombatGameState::StaticClass();
	bUseSeamlessTravel = true;
}

void AUBFCombatGameMode::BeginPlay()
{
	Super::BeginPlay();
	MatchStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (bGolemMode)
	{
		if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
		{
			MatchState->SetGolemMode(true);
		}
		SpawnGolemObjectives();
	}
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
		TeamSize = FMath::Clamp(FCString::Atoi(*TeamSizeOption), 1, 3);
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

	if (AUBFCombatCharacter* Character = Cast<AUBFCombatCharacter>(NewPlayer->GetPawn()))
	{
		Character->SetCharacterId(HumanCharacterId.IsNone() ? DefaultCharacterId : HumanCharacterId);
		int32 HumanCounts[2] = { 0, 0 };
		GetHumanTeamCounts(NewPlayer, HumanCounts);
		const int32 TeamId = HumanCounts[0] <= HumanCounts[1] ? 0 : 1;
		Character->SetTeamId(TeamId);
		Character->SetActorTransform(GetTeamSpawnTransform(TeamId, HumanCounts[TeamId]));
		UE_LOG(LogTemp, Log, TEXT("UBF combat player assigned to team %d (%d per team)."), TeamId, TeamSize);
	}

	ReconcileBotRoster();
}

void AUBFCombatGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	if (HasAuthority())
	{
		ReconcileBotRoster();
	}
}

void AUBFCombatGameMode::NotifyFighterEliminated()
{
	if (!HasAuthority() || bMatchFinished || bOutcomeCheckPending || !GetWorld())
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
	if (!HasAuthority() || !bGolemMode || bMatchFinished || !IsValid(Carrier)
		|| Carrier->IsMasterGolem() || Carrier->GetCurrentHealth() <= 0.0f)
	{
		return;
	}
	ClearGoldenBearer();
	CurrentGoldenBearer = Carrier;
	Carrier->SetGoldenBearer(true);
	if (IsValid(Pickup))
	{
		Pickup->Destroy();
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(GoldenBearerScoreTimer, this,
			&AUBFCombatGameMode::ScoreGoldenBearer, FMath::Max(5.0f, BearerHoldSecondsToScore), false);
	}
	UE_LOG(LogTemp, Log, TEXT("Golem Mode: %s became the GoldenBearer for team %d."),
		*GetNameSafe(Carrier), Carrier->GetTeamId());
}

void AUBFCombatGameMode::NotifyGoldenBearerEliminated(AUBFCombatCharacter* Carrier)
{
	if (!HasAuthority() || !bGolemMode || !Carrier || CurrentGoldenBearer.Get() != Carrier)
	{
		return;
	}
	const FVector DropLocation = Carrier->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f);
	ClearGoldenBearer();
	if (!bMatchFinished)
	{
		SpawnGoldenGolemPickup(DropLocation);
	}
}

void AUBFCombatGameMode::NotifyMasterGolemEliminated(int32 TeamId)
{
	if (HasAuthority() && bGolemMode && !bMatchFinished && (TeamId == 0 || TeamId == 1))
	{
		UE_LOG(LogTemp, Log, TEXT("Golem Mode: team %d's Master Golem was defeated."), TeamId);
		FinishMatch(1 - TeamId, false);
	}
}

void AUBFCombatGameMode::SpawnGolemObjectives()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	for (int32 TeamId = 0; TeamId < 2; ++TeamId)
	{
		const float X = TeamId == 0 ? -1180.0f : 1180.0f;
		const FTransform SpawnTransform(FRotator(0.0f, TeamId == 0 ? 0.0f : 180.0f, 0.0f),
			FVector(X, 0.0f, 105.0f));
		if (AUBFCombatCharacter* MasterGolem = GetWorld()->SpawnActor<AUBFCombatCharacter>(
			AUBFCombatCharacter::StaticClass(), SpawnTransform))
		{
			MasterGolem->SetTeamId(TeamId);
			MasterGolem->SetAsMasterGolem();
		}
	}
	SpawnGoldenGolemPickup(FVector(0.0f, 0.0f, 90.0f));
}

void AUBFCombatGameMode::SpawnGoldenGolemPickup(const FVector& Location)
{
	if (HasAuthority() && bGolemMode && !bMatchFinished && GetWorld())
	{
		GetWorld()->SpawnActor<AUBFGoldenGolemPickup>(AUBFGoldenGolemPickup::StaticClass(),
			Location, FRotator::ZeroRotator);
	}
}

void AUBFCombatGameMode::ClearGoldenBearer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GoldenBearerScoreTimer);
	}
	if (AUBFCombatCharacter* PreviousCarrier = CurrentGoldenBearer.Get())
	{
		PreviousCarrier->SetGoldenBearer(false);
	}
	CurrentGoldenBearer.Reset();
}

void AUBFCombatGameMode::ScoreGoldenBearer()
{
	AUBFCombatCharacter* Carrier = CurrentGoldenBearer.Get();
	if (!HasAuthority() || !bGolemMode || bMatchFinished || !IsValid(Carrier)
		|| Carrier->GetCurrentHealth() <= 0.0f)
	{
		ClearGoldenBearer();
		return;
	}
	const int32 TeamId = FMath::Clamp(Carrier->GetTeamId(), 0, 1);
	const int32 NewScore = ++GolemTeamScores[TeamId];
	if (AUBFCombatGameState* MatchState = GetGameState<AUBFCombatGameState>())
	{
		MatchState->SetTeamScore(TeamId, NewScore);
	}
	Carrier->ShowCombatMessage(FString::Printf(TEXT("GOLDEN BEARER  %d / %d"),
		NewScore, GolemScoreToWin), 2.5f);
	ClearGoldenBearer();
	if (NewScore >= GolemScoreToWin)
	{
		FinishMatch(TeamId, false);
		return;
	}
	SpawnGoldenGolemPickup(FVector(0.0f, 0.0f, 90.0f));
}

void AUBFCombatGameMode::CheckMatchOutcome()
{
	bOutcomeCheckPending = false;
	if (!HasAuthority() || bMatchFinished || !GetWorld() || bGolemMode)
	{
		return;
	}

	int32 TeamTotals[2] = { 0, 0 };
	int32 TeamAlive[2] = { 0, 0 };
	for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
	{
		const AUBFCombatCharacter* Fighter = *It;
		if (!IsValid(Fighter))
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
		FinishMatch(INDEX_NONE, true);
	}
	else
	{
		FinishMatch(TeamAlive[0] > 0 ? 0 : 1, false);
	}
}

void AUBFCombatGameMode::FinishMatch(int32 WinningTeamId, bool bIsDraw)
{
	if (!HasAuthority() || bMatchFinished || !GetWorld())
	{
		return;
	}

	bMatchFinished = true;
	ClearGoldenBearer();
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
	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UUBFCharacterCatalogSubsystem* Catalog = Instance->GetSubsystem<UUBFCharacterCatalogSubsystem>())
		{
			const TArray<FName>& CharacterIds = Catalog->GetCharacterIds();
			if (!CharacterIds.IsEmpty())
			{
				BotPawn->SetCharacterId(CharacterIds[(NextBotNumber - 1) % CharacterIds.Num()]);
			}
		}
	}
	const EUBFBotDifficulty Difficulty = BotDifficulty == TEXT("EASY") ? EUBFBotDifficulty::Easy
		: (BotDifficulty == TEXT("HARD") ? EUBFBotDifficulty::Hard : EUBFBotDifficulty::Normal);
	BotController->SetDifficulty(Difficulty);
	BotController->Possess(BotPawn);
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
