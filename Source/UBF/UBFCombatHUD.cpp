#include "UBFCombatHUD.h"

#include "UBFCombatGameState.h"
#include "UBFPlayerData.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UBFCombatCharacter.h"
#include "UBFCombatPlayerController.h"
#include "FileMediaSource.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

AUBFCombatHUD::AUBFCombatHUD()
	: LastObservedPhase(static_cast<EUBFMatchPhase>(MAX_uint8))
{
	PrimaryActorTick.bCanEverTick = true;
}

void AUBFCombatHUD::BeginPlay()
{
	Super::BeginPlay();
	FirstRoundIntroSound = LoadObject<USoundWave>(nullptr,
		TEXT("/Game/Presentation/Audio/introdution_ubf_normalized.introdution_ubf_normalized"));
	if (!FirstRoundIntroSound)
	{
		UE_LOG(LogTemp, Error, TEXT("No se pudo cargar el SoundWave normalizado de introdution_ubf."));
	}
	MatchAudioPlayer = NewObject<UMediaPlayer>(this, TEXT("UBFMatchAudioPlayer"));
	MatchAudioComponent = NewObject<UMediaSoundComponent>(this, TEXT("UBFMatchAudioComponent"));
	if (MatchAudioPlayer && MatchAudioComponent)
	{
		MatchAudioPlayer->PlayOnOpen = true;
		MatchAudioPlayer->SetLooping(false);
		MatchAudioPlayer->OnMediaOpened.AddDynamic(this, &AUBFCombatHUD::OnMatchAudioOpened);
		MatchAudioPlayer->OnMediaOpenFailed.AddDynamic(this, &AUBFCombatHUD::OnMatchAudioOpenFailed);
		MatchAudioPlayer->OnEndReached.AddDynamic(this, &AUBFCombatHUD::OnMatchAudioEnded);
		MatchAudioComponent->SetMediaPlayer(MatchAudioPlayer);
		MatchAudioComponent->SetVolumeMultiplier(0.60f);
		MatchAudioComponent->RegisterComponent();
		MatchAudioComponent->Start();
	}
}

void AUBFCombatHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MatchAudioPlayer)
	{
		MatchAudioPlayer->OnMediaOpened.RemoveDynamic(this, &AUBFCombatHUD::OnMatchAudioOpened);
		MatchAudioPlayer->OnMediaOpenFailed.RemoveDynamic(this, &AUBFCombatHUD::OnMatchAudioOpenFailed);
		MatchAudioPlayer->OnEndReached.RemoveDynamic(this, &AUBFCombatHUD::OnMatchAudioEnded);
	}
	StopMatchAudio();
	if (FirstRoundIntroAudioComponent)
	{
		FirstRoundIntroAudioComponent->Stop();
		FirstRoundIntroAudioComponent = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AUBFCombatHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AUBFCombatGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
	if (!MatchState)
	{
		return;
	}
	const EUBFMatchPhase CurrentPhase = MatchState->GetRoundPhase();
	const int32 CurrentRound = MatchState->GetRoundNumber();
	if (CurrentPhase != LastObservedPhase || CurrentRound != LastObservedRound)
	{
		LastObservedPhase = CurrentPhase;
		LastObservedRound = CurrentRound;
		HandleMatchPhaseTransition(CurrentPhase);
	}
	MatchAnnouncementRemaining = FMath::Max(0.0f, MatchAnnouncementRemaining - DeltaSeconds);
	for (const FUBFMatchCueEvent& Event : MatchState->GetMatchCueEvents())
	{
		if (Event.Sequence > LastObservedMatchCueSequence)
		{
			LastObservedMatchCueSequence = Event.Sequence;
			HandleMatchCueEvent(Event);
		}
	}
}

void AUBFCombatHUD::HandleMatchPhaseTransition(EUBFMatchPhase NewPhase)
{
	StopMatchAudio();
	switch (NewPhase)
	{
	case EUBFMatchPhase::LoadingSync:
		StartMatchAudio(TEXT("UBF_MAP_LOADING.wma"), false);
		break;
	case EUBFMatchPhase::InRound:
		bBattleMusicActive = true;
		MatchAnnouncementText = TEXT("¡LA BATALLA HA COMENZADO!");
		MatchAnnouncementColor = FLinearColor(0.42f, 0.88f, 1.0f, 1.0f);
		MatchAnnouncementDuration = 2.6f;
		MatchAnnouncementRemaining = MatchAnnouncementDuration;
		QueueMatchCueAudio(TEXT("round_introduction_1.wma"), 0.85f, FString(), FLinearColor::White);
		break;
	case EUBFMatchPhase::RoundResults:
	{
		const AUBFCombatGameState* MatchState = GetWorld()
			? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
		const int32 WinnerTeamId = MatchState ? MatchState->GetRoundWinnerTeamId() : INDEX_NONE;
		const int32 LocalTeamId = GetLocalTeamId();
		if (WinnerTeamId == 0 || WinnerTeamId == 1)
		{
			StartMatchAudio(WinnerTeamId == LocalTeamId
				? TEXT("UBF_ROUND_WIN_TEAM.wma") : TEXT("UBF_ROUND_LOST_TEAM.wma"), false);
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().SetTimer(RoundResultAudioTimer, this,
					&AUBFCombatHUD::StopRoundResultAudio, 3.5f, false);
			}
		}
		else
		{
			// A draw caused by a leaver/unfair roster still receives the loss sting
			// requested for both sides before returning to the front end.
			StartMatchAudio(TEXT("UBF_ROUND_LOST_TEAM.wma"), false);
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().SetTimer(RoundResultAudioTimer, this,
					&AUBFCombatHUD::StopRoundResultAudio, 3.5f, false);
			}
		}
		break;
	}
	case EUBFMatchPhase::Countdown:
	{
		const AUBFCombatGameState* MatchState = GetWorld()
			? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
		if (MatchState && MatchState->GetRoundNumber() == 1 && !bFirstRoundIntroPlayed)
		{
			bFirstRoundIntroPlayed = true;
			if (FirstRoundIntroSound)
			{
				FirstRoundIntroAudioComponent = UGameplayStatics::SpawnSound2D(this,
					FirstRoundIntroSound, 1.0f, 1.0f, 0.0f, nullptr, false, true);
				UE_LOG(LogTemp, Log, TEXT("Audio de introducción de la primera ronda iniciado a 100%%."));
			}
		}
		break;
	}
	case EUBFMatchPhase::MatchResults:
	default:
		break;
	}
}

void AUBFCombatHUD::StartMatchAudio(const FString& FileName, bool bLooping, float VolumeMultiplier)
{
	if (!MatchAudioPlayer || !MatchAudioComponent)
	{
		return;
	}
	MatchAudioPlayer->Close();
	const FString AudioPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectContentDir(), TEXT("Presentation"), TEXT("Audio"), FileName));
	CurrentMatchAudioSource = NewObject<UFileMediaSource>(this);
	CurrentMatchAudioSource->SetFilePath(AudioPath);
	MatchAudioComponent->SetVolumeMultiplier(FMath::Clamp(VolumeMultiplier, 0.0f, 1.0f));
	MatchAudioPlayer->SetLooping(bLooping);
	if (!MatchAudioPlayer->OpenSource(CurrentMatchAudioSource))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBF audio could not be opened: %s"), *AudioPath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UBF audio queued at %.0f%% volume: %s"),
			FMath::Clamp(VolumeMultiplier, 0.0f, 1.0f) * 100.0f, *FileName);
	}
}

void AUBFCombatHUD::HandleMatchCueEvent(const FUBFMatchCueEvent& Event)
{
	const int32 LocalTeamId = GetLocalTeamId();
	if (LocalTeamId != 0 && LocalTeamId != 1)
	{
		return;
	}

	if (Event.Type == EUBFMatchCueType::GoldenGolemKilled)
	{
		const bool bFriendlyKill = LocalTeamId == Event.SourceTeamId;
		QueueMatchCueAudio(bFriendlyKill
			? TEXT("round_friendly_kill_golem_gold.wma")
			: TEXT("round_enemy_kill_golem_gold.wma"), 0.85f,
			bFriendlyKill ? TEXT("¡TU EQUIPO OBTUVO LA ESPADA DORADA!")
				: TEXT("¡EL EQUIPO RIVAL TIENE LA ESPADA DORADA!"),
			bFriendlyKill ? FLinearColor(1.0f, 0.74f, 0.22f, 1.0f)
				: FLinearColor(1.0f, 0.36f, 0.40f, 1.0f));
		return;
	}

	if (Event.TeamId != LocalTeamId)
	{
		return;
	}

	FString Announcement;
	FString AudioFile;
	switch (Event.Type)
	{
	case EUBFMatchCueType::MasterGolemFirstHit:
		AudioFile = TEXT("round_enemyattack_teamgolem_announce.wma");
		Announcement = TEXT("¡EL GOLEM MAESTRO ESTÁ SIENDO ATACADO!");
		break;
	case EUBFMatchCueType::MasterGolemHalfHealth:
		AudioFile = TEXT("round_enemyattack_teamgolem_announce.wma");
		Announcement = TEXT("¡GOLEM MAESTRO AL 50% DE VIDA!");
		break;
	case EUBFMatchCueType::MasterGolemCriticalHealth:
		AudioFile = TEXT("round_enemyattack_teamgolem_announce.wma");
		Announcement = TEXT("¡PELIGRO! GOLEM MAESTRO AL 10% DE VIDA");
		break;
	default:
		return;
	}
	QueueMatchCueAudio(AudioFile, 0.85f, Announcement, FLinearColor(1.0f, 0.37f, 0.40f, 1.0f));
}

void AUBFCombatHUD::QueueMatchCueAudio(const FString& FileName, float VolumeMultiplier,
	const FString& Announcement, const FLinearColor& AnnouncementColor, float AnnouncementDuration)
{
	if (!Announcement.IsEmpty())
	{
		MatchAnnouncementText = Announcement;
		MatchAnnouncementColor = AnnouncementColor;
		MatchAnnouncementDuration = FMath::Max(0.1f, AnnouncementDuration);
		MatchAnnouncementRemaining = MatchAnnouncementDuration;
	}
	if (FileName.IsEmpty())
	{
		return;
	}

	PendingCueAudioFiles.Emplace(FileName, FMath::Clamp(VolumeMultiplier, 0.0f, 1.0f));
	if (!bPlayingCueAudio)
	{
		if (MatchAudioComponent)
		{
			MatchAudioComponent->Stop();
		}
		if (MatchAudioPlayer)
		{
			MatchAudioPlayer->Close();
		}
		bPlayingCueAudio = true;
		PlayNextQueuedCueAudio();
	}
}

void AUBFCombatHUD::PlayNextQueuedCueAudio()
{
	if (PendingCueAudioFiles.Num() > 0)
	{
		const TPair<FString, float> NextCue = PendingCueAudioFiles[0];
		PendingCueAudioFiles.RemoveAt(0, 1, EAllowShrinking::No);
		StartMatchAudio(NextCue.Key, false, NextCue.Value);
		return;
	}

	bPlayingCueAudio = false;
	if (bBattleMusicActive)
	{
		StartNextBattleTrack();
	}
	else if (MatchAudioPlayer)
	{
		MatchAudioPlayer->Close();
	}
}

void AUBFCombatHUD::StopMatchAudio()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RoundResultAudioTimer);
	}
	bBattleMusicActive = false;
	bPlayingCueAudio = false;
	PendingCueAudioFiles.Reset();
	if (MatchAudioComponent)
	{
		MatchAudioComponent->Stop();
	}
	if (MatchAudioPlayer)
	{
		MatchAudioPlayer->Close();
	}
}

void AUBFCombatHUD::StartNextBattleTrack()
{
	if (!bBattleMusicActive)
	{
		return;
	}
	int32 TrackIndex = FMath::RandRange(0, 2);
	if (TrackIndex == LastBattleTrackIndex)
	{
		TrackIndex = (TrackIndex + FMath::RandRange(1, 2)) % 3;
	}
	LastBattleTrackIndex = TrackIndex;
	const FString FileName = FString::Printf(TEXT("UBF_MUSICBATTLE_%02d.wma"), TrackIndex + 1);
	StartMatchAudio(FileName, false);
}

int32 AUBFCombatHUD::GetLocalTeamId() const
{
	const AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(PlayerOwner ? PlayerOwner->GetPawn() : nullptr);
	return Fighter ? Fighter->GetTeamId() : INDEX_NONE;
}

void AUBFCombatHUD::OnMatchAudioOpened(FString OpenedUrl)
{
	if (MatchAudioComponent && !MatchAudioComponent->IsPlaying())
	{
		MatchAudioComponent->Start();
	}
	if (MatchAudioPlayer)
	{
		MatchAudioPlayer->Play();
	}
	UE_LOG(LogTemp, Verbose, TEXT("UBF audio media opened: %s"), *OpenedUrl);
}

void AUBFCombatHUD::OnMatchAudioOpenFailed(FString FailedUrl)
{
	UE_LOG(LogTemp, Warning, TEXT("UBF audio media failed to open: %s"), *FailedUrl);
	if (bPlayingCueAudio)
	{
		PlayNextQueuedCueAudio();
	}
}

void AUBFCombatHUD::OnMatchAudioEnded()
{
	if (bPlayingCueAudio)
	{
		PlayNextQueuedCueAudio();
		return;
	}
	if (bBattleMusicActive)
	{
		StartNextBattleTrack();
	}
	else if (MatchAudioPlayer)
	{
		MatchAudioPlayer->Close();
	}
}

void AUBFCombatHUD::StopRoundResultAudio()
{
	if (MatchAudioComponent)
	{
		MatchAudioComponent->Stop();
	}
	if (MatchAudioPlayer)
	{
		MatchAudioPlayer->Close();
	}
}

void AUBFCombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float ScreenWidth = Canvas->ClipX;
	const float ScreenHeight = Canvas->ClipY;
	const FLinearColor Panel(0.012f, 0.020f, 0.032f, 0.88f);
	const FLinearColor White(0.92f, 0.95f, 0.98f, 1.0f);
	const FLinearColor Muted(0.60f, 0.68f, 0.76f, 1.0f);
	const FLinearColor Cyan(0.05f, 0.72f, 0.92f, 1.0f);
	const FLinearColor Crimson(0.88f, 0.035f, 0.12f, 1.0f);

	DrawRect(Panel, 34.0f, 32.0f, 360.0f, 132.0f);
	DrawText(TEXT("UBF  //  TRAINING ARENA"), White, 54.0f, 48.0f, GEngine->GetMediumFont(), 0.75f);
	DrawText(TEXT("SERVER-AUTHORITATIVE COMBAT"), Muted, 54.0f, 78.0f, GEngine->GetSmallFont(), 0.9f);

	AUBFCombatCharacter* Fighter = Cast<AUBFCombatCharacter>(PlayerOwner ? PlayerOwner->GetPawn() : nullptr);
	const AUBFCombatGameState* MatchState = GetWorld() ? GetWorld()->GetGameState<AUBFCombatGameState>() : nullptr;
	if (MatchState && MatchState->IsGolemMode())
	{
		const float ObjectiveWidth = 340.0f;
		const float ObjectiveX = (ScreenWidth - ObjectiveWidth) * 0.5f;
		const float ObjectiveY = ScreenWidth >= 1100.0f ? 32.0f : 170.0f;
		DrawRect(Panel, ObjectiveX, ObjectiveY, ObjectiveWidth, 58.0f);
		DrawRect(FLinearColor(1.0f, 0.62f, 0.08f, 1.0f), ObjectiveX, ObjectiveY, ObjectiveWidth, 3.0f);
		DrawText(TEXT("GOLEM WAR  ·  DESTRUYE EL MASTER RIVAL"), White,
			ObjectiveX + 36.0f, ObjectiveY + 14.0f, GEngine->GetMediumFont(), 0.78f);
		if (Fighter && Fighter->IsGoldenBearer())
		{
			DrawText(TEXT("ESPADA DORADA  ·  SOLO TÚ DAÑAS AL MASTER RIVAL"),
				FLinearColor(1.0f, 0.72f, 0.16f, 1.0f), ObjectiveX + 28.0f, ObjectiveY + 40.0f,
				GEngine->GetSmallFont(), 0.68f);
		}
		else
		{
			float GoldenGolemHealth = 0.0f;
			for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
			{
				if (const AUBFCombatCharacter* GoldenGolem = *It; GoldenGolem && GoldenGolem->IsGoldenGolem())
				{
					GoldenGolemHealth = GoldenGolem->GetCurrentHealth();
					break;
				}
			}
			DrawText(GoldenGolemHealth > 0.0f
				? FString::Printf(TEXT("GOLEM DORADO  ·  HP %.0f  ·  BUSCA EL ÚLTIMO GOLPE"), GoldenGolemHealth)
				: TEXT("PORTADOR ACTIVO  ·  PROTEGE A QUIEN TIENE LA ESPADA"),
				FLinearColor(1.0f, 0.72f, 0.16f, 1.0f), ObjectiveX + 20.0f, ObjectiveY + 40.0f,
				GEngine->GetSmallFont(), 0.65f);
		}
		if (Fighter)
		{
			for (TActorIterator<AUBFCombatCharacter> It(GetWorld()); It; ++It)
			{
				const AUBFCombatCharacter* EnemyMaster = *It;
				if (!EnemyMaster || !EnemyMaster->IsMasterGolem() || EnemyMaster->GetTeamId() == Fighter->GetTeamId())
				{
					continue;
				}
				const float BarX = (ScreenWidth - 420.0f) * 0.5f;
				DrawRect(Panel, BarX, ObjectiveY + 66.0f, 420.0f, 38.0f);
				DrawText(FString::Printf(TEXT("ENEMY MASTER GOLEM  ·  %03.0f HP"), EnemyMaster->GetCurrentHealth()),
					White, BarX + 14.0f, ObjectiveY + 72.0f, GEngine->GetSmallFont(), 0.78f);
				DrawRect(FLinearColor(0.18f, 0.10f, 0.11f, 1.0f), BarX + 14.0f, ObjectiveY + 91.0f, 392.0f, 5.0f);
				DrawRect(Crimson, BarX + 14.0f, ObjectiveY + 91.0f, 392.0f * EnemyMaster->GetHealthRatio(), 5.0f);
				break;
			}
		}
	}
	if (Fighter)
	{
		const float BarWidth = 246.0f;
		const FLinearColor HealthColor = Fighter->GetHealthRatio() > 0.30f ? Cyan : Crimson;
		DrawRect(FLinearColor(0.11f, 0.13f, 0.17f, 1.0f), 54.0f, 104.0f, BarWidth, 11.0f);
		DrawRect(HealthColor, 54.0f, 104.0f, BarWidth * Fighter->GetHealthRatio(), 11.0f);
		DrawText(FString::Printf(TEXT("HP %03.0f"), Fighter->GetCurrentHealth()), White,
			312.0f, 101.0f, GEngine->GetSmallFont(), 0.75f);

		DrawRect(FLinearColor(0.11f, 0.13f, 0.17f, 1.0f), 54.0f, 128.0f, BarWidth, 11.0f);
		DrawRect(Cyan, 54.0f, 128.0f, BarWidth * Fighter->GetSkillGaugeRatio(), 11.0f);
		DrawText(FString::Printf(TEXT("GAUGE %03.0f%%"), Fighter->GetCurrentSkillGauge()), Muted,
			312.0f, 125.0f, GEngine->GetSmallFont(), 0.75f);

		if (ScreenWidth >= 760.0f)
		{
			const float PanelX = ScreenWidth - 338.0f;
			DrawRect(Panel, PanelX, 32.0f, 304.0f, 124.0f);
			DrawRect(Cyan, PanelX, 32.0f, 3.0f, 124.0f);
			DrawText(FString::Printf(TEXT("FIGHTER  //  %s"), *Fighter->GetCharacterDisplayName().ToString()),
				White, PanelX + 18.0f, 44.0f, GEngine->GetMediumFont(), 0.78f);
			for (uint8 Slot = 0; Slot < 3; ++Slot)
			{
				const float RowY = 74.0f + static_cast<float>(Slot) * 24.0f;
				const float CooldownRatio = Fighter->GetAbilityCooldownRatio(Slot);
				const float Remaining = Fighter->GetAbilityCooldownRemaining(Slot);
				const FName ActionId = Slot == 0 ? FName(TEXT("PrimarySkill"))
					: (Slot == 1 ? FName(TEXT("SecondarySkill")) : FName(TEXT("Ultimate")));
				const TCHAR* DefaultLabel = Slot == 0 ? TEXT("Q") : (Slot == 1 ? TEXT("E") : TEXT("F"));
			const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
				? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
			const FString InputLabel = PlayerData
				? PlayerData->GetInputBinding(ActionId).GetDisplayName().ToString() : DefaultLabel;
				const FString AbilityName = Fighter->GetAbilityDisplayName(Slot).ToString();
				DrawText(FString::Printf(TEXT("%s  %s"), *InputLabel, *AbilityName), Muted,
					PanelX + 18.0f, RowY, GEngine->GetSmallFont(), 0.76f);
				DrawText(Remaining > 0.05f ? FString::Printf(TEXT("%.1fs"), Remaining) : TEXT("READY"),
					Remaining > 0.05f ? Crimson : Cyan, PanelX + 246.0f, RowY, GEngine->GetSmallFont(), 0.72f);
				DrawRect(FLinearColor(0.10f, 0.15f, 0.20f, 1.0f), PanelX + 18.0f, RowY + 15.0f, 268.0f, 3.0f);
				DrawRect(Cyan, PanelX + 18.0f, RowY + 15.0f, 268.0f * (1.0f - CooldownRatio), 3.0f);
			}
		}

		const FString Feedback = Fighter->GetCombatFeedbackMessage();
		if (!Feedback.IsEmpty() && !Fighter->HasMatchResult())
		{
			DrawText(Feedback, FLinearColor(1.0f, 0.78f, 0.28f, 1.0f), ScreenWidth * 0.5f - 120.0f,
				ScreenHeight * 0.78f, GEngine->GetMediumFont(), 1.0f);
		}
	}

	const bool bShowingMatchResult = Fighter && Fighter->HasMatchResult();
	if (bShowingMatchResult)
	{
		const float ResultWidth = FMath::Min(500.0f, ScreenWidth - 48.0f);
		const float ResultHeight = 420.0f;
		const float ResultX = (ScreenWidth - ResultWidth) * 0.5f;
		const float ResultY = (ScreenHeight - ResultHeight) * 0.5f;
		const FString& Result = Fighter->GetMatchResultLabel();
		const FLinearColor ResultColor = Result == TEXT("VICTORIA") ? Cyan
			: (Result == TEXT("DERROTA") ? Crimson : FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));

		DrawRect(FLinearColor(0.008f, 0.014f, 0.025f, 0.96f), ResultX, ResultY, ResultWidth, ResultHeight);
		DrawRect(ResultColor, ResultX, ResultY, ResultWidth, 4.0f);
		DrawText(Result, ResultColor, ResultX + 38.0f, ResultY + 26.0f, GEngine->GetLargeFont(), 1.35f);
		DrawText(TEXT("MATCH RESULT"), Muted, ResultX + 40.0f, ResultY + 78.0f, GEngine->GetSmallFont(), 0.9f);

		const int32 TotalSeconds = FMath::Max(0, FMath::RoundToInt(Fighter->GetMatchDuration()));
		const FString MatchTime = FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60);
		const FString StatLabels[] = { TEXT("KNOCKOUTS"), TEXT("DEATHS"), TEXT("DAMAGE DEALT"),
			TEXT("DAMAGE RECEIVED"), TEXT("MAX COMBO"), TEXT("MATCH TIME"),
			TEXT("XP REWARD"), TEXT("GOLD REWARD"), TEXT("EVENT POINTS") };
		const FString StatValues[] = {
			FString::FromInt(Fighter->GetMatchKnockouts()),
			FString::FromInt(Fighter->GetMatchDeaths()),
			FString::Printf(TEXT("%.0f"), Fighter->GetMatchDamageDealt()),
			FString::Printf(TEXT("%.0f"), Fighter->GetMatchDamageReceived()),
			FString::Printf(TEXT("%d HITS"), Fighter->GetMaximumComboHits()),
			MatchTime,
			FString::Printf(TEXT("+%d"), Fighter->GetMatchExperienceReward()),
			FString::Printf(TEXT("+%d"), Fighter->GetMatchGoldReward()),
			FString::Printf(TEXT("+%d"), Fighter->GetMatchEventReward())
		};
		DrawRect(FLinearColor(0.42f, 0.32f, 0.12f, 0.9f), ResultX + 40.0f, ResultY + 301.0f, ResultWidth - 80.0f, 1.0f);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(StatLabels); ++Index)
		{
			const float RowY = ResultY + 110.0f + Index * 30.0f;
			const FLinearColor RowColor = Index >= 6 ? FLinearColor(1.0f, 0.78f, 0.28f, 1.0f) : Muted;
			DrawText(StatLabels[Index], RowColor, ResultX + 40.0f, RowY, GEngine->GetSmallFont(), 0.9f);
			DrawText(StatValues[Index], White, ResultX + ResultWidth - 150.0f, RowY, GEngine->GetSmallFont(), 0.9f);
		}
	}

	if (MatchState && ScreenWidth >= 700.0f
		&& MatchState->GetRoundPhase() != EUBFMatchPhase::LoadingSync
		&& MatchState->GetRoundPhase() != EUBFMatchPhase::Countdown
		&& MatchState->GetRoundPhase() != EUBFMatchPhase::MatchResults)
	{
		const float ScoreWidth = 300.0f;
		const float ScoreX = (ScreenWidth - ScoreWidth) * 0.5f;
		DrawRect(Panel, ScoreX, 136.0f, ScoreWidth, 34.0f);
		DrawRect(Cyan, ScoreX, 136.0f, ScoreWidth, 2.0f);
		DrawText(FString::Printf(TEXT("RONDA %d / 3     A  %d  :  %d  B"),
			MatchState->GetRoundNumber(), MatchState->GetRoundWins(0), MatchState->GetRoundWins(1)),
			White, ScoreX + 29.0f, 146.0f, GEngine->GetSmallFont(), 0.92f);
	}

	const EUBFMatchPhase MatchPhase = MatchState
		? MatchState->GetRoundPhase() : EUBFMatchPhase::InRound;
	const bool bPreparingRound = MatchPhase == EUBFMatchPhase::LoadingSync
		|| MatchPhase == EUBFMatchPhase::Countdown;
	const bool bShowingRoundResult = MatchPhase == EUBFMatchPhase::RoundResults;
	if (bPreparingRound)
	{
		DrawRect(FLinearColor(0.006f, 0.012f, 0.022f, 0.84f), 0.0f, 0.0f, ScreenWidth, ScreenHeight);
		const float CardWidth = FMath::Min(690.0f, ScreenWidth - 48.0f);
		const float CardHeight = 252.0f;
		const float CardX = (ScreenWidth - CardWidth) * 0.5f;
		const float CardY = (ScreenHeight - CardHeight) * 0.5f;
		DrawRect(FLinearColor(0.018f, 0.035f, 0.055f, 0.94f), CardX, CardY, CardWidth, CardHeight);
		DrawRect(Cyan, CardX, CardY, CardWidth, 4.0f);
		DrawText(TEXT("UBF  //  ARENA"), Cyan, CardX + 36.0f, CardY + 32.0f,
			GEngine->GetSmallFont(), 1.0f);
		if (MatchPhase == EUBFMatchPhase::Countdown)
		{
			const int32 Countdown = FMath::Max(1, FMath::CeilToInt(MatchState->GetPhaseTimeRemaining()));
			DrawText(FString::Printf(TEXT("RONDA %d  ·  MANTÉN POSICIÓN"), MatchState->GetRoundNumber()),
				White, CardX + 36.0f, CardY + 78.0f, GEngine->GetMediumFont(), 0.94f);
			const float Pulse = 1.08f + 0.18f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 8.0f));
			const FString CountdownText = FString::FromInt(Countdown);
			float CountdownTextWidth = 0.0f;
			float CountdownTextHeight = 0.0f;
			Canvas->StrLen(GEngine->GetLargeFont(), CountdownText, CountdownTextWidth, CountdownTextHeight);
			const float CountdownX = (ScreenWidth - CountdownTextWidth * Pulse) * 0.5f;
			DrawText(CountdownText, Countdown == 1 ? Crimson : White, CountdownX, CardY + 102.0f,
				GEngine->GetLargeFont(), Pulse);
			DrawText(FString::Printf(TEXT("MARCADOR DE RONDAS    %d  :  %d"),
				MatchState->GetRoundWins(0), MatchState->GetRoundWins(1)), Muted,
				CardX + 38.0f, CardY + 160.0f, GEngine->GetSmallFont(), 0.88f);
		}
		else
		{
			DrawText(TEXT("PREPARANDO JUGADORES Y BOTS"), White,
				CardX + 36.0f, CardY + 82.0f, GEngine->GetLargeFont(), 0.88f);
			const AUBFCombatPlayerController* LocalController =
				Cast<AUBFCombatPlayerController>(GetOwningPlayerController());
			const FString ReadyText = FString::Printf(TEXT("JUGADORES LISTOS  %d / %d"),
				MatchState->GetReadyPlayerCount(), MatchState->GetTotalPlayerCount());
			DrawText(ReadyText, Muted, CardX + 38.0f, CardY + 145.0f, GEngine->GetSmallFont(), 0.90f);
			const bool bLocalPlayerReady = LocalController && LocalController->IsReadyForRound();
			DrawText(bLocalPlayerReady ? TEXT("LISTO  ·  ESPERANDO A LOS DEMÁS")
				: TEXT("PULSA  ENTER  O  A  EN EL MANDO  PARA MARCARTE LISTO"),
				bLocalPlayerReady ? Cyan : White,
				CardX + 38.0f, CardY + 174.0f, GEngine->GetSmallFont(), 0.90f);
		}
		const float BarWidth = CardWidth - 76.0f;
		const float Pulse = 0.35f + 0.65f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 4.0f));
		DrawRect(FLinearColor(0.09f, 0.14f, 0.19f, 1.0f), CardX + 38.0f, CardY + 205.0f, BarWidth, 7.0f);
		DrawRect(FLinearColor(Cyan.R, Cyan.G, Cyan.B, Pulse), CardX + 38.0f, CardY + 205.0f,
			BarWidth * (0.48f + 0.48f * Pulse), 7.0f);
	}
	else if (bShowingRoundResult && MatchState)
	{
		DrawRect(FLinearColor(0.006f, 0.012f, 0.022f, 0.68f), 0.0f, 0.0f, ScreenWidth, ScreenHeight);
		const float CardWidth = FMath::Min(600.0f, ScreenWidth - 48.0f);
		const float CardHeight = 310.0f;
		const float CardX = (ScreenWidth - CardWidth) * 0.5f;
		const float CardY = (ScreenHeight - CardHeight) * 0.5f;
		const int32 WinningTeamId = MatchState->GetRoundWinnerTeamId();
		const int32 LocalTeamId = Fighter ? Fighter->GetTeamId() : INDEX_NONE;
		const FString ResultLabel = WinningTeamId == INDEX_NONE ? TEXT("RONDA EMPATADA")
			: (WinningTeamId == LocalTeamId ? TEXT("RONDA GANADA") : TEXT("RONDA PERDIDA"));
		const FLinearColor ResultColor = WinningTeamId == INDEX_NONE
			? FLinearColor(1.0f, 0.72f, 0.22f, 1.0f) : (WinningTeamId == LocalTeamId ? Cyan : Crimson);
		DrawRect(FLinearColor(0.018f, 0.035f, 0.055f, 0.96f), CardX, CardY, CardWidth, CardHeight);
		DrawRect(ResultColor, CardX, CardY, CardWidth, 4.0f);
		DrawText(FString::Printf(TEXT("RESULTADO DE RONDA  %02d"), MatchState->GetRoundNumber()),
			Muted, CardX + 36.0f, CardY + 30.0f, GEngine->GetSmallFont(), 0.95f);
		DrawText(ResultLabel, ResultColor, CardX + 36.0f, CardY + 68.0f,
			GEngine->GetLargeFont(), 1.12f);
		DrawText(FString::Printf(TEXT("SERIE     AZUL  %d  :  %d  ROJO"),
			MatchState->GetRoundWins(0), MatchState->GetRoundWins(1)), White,
			CardX + 38.0f, CardY + 132.0f, GEngine->GetMediumFont(), 0.92f);
		if (Fighter)
		{
			DrawText(FString::Printf(TEXT("ESTADÍSTICAS TOTALES     KOs %d    CAÍDAS %d    DAÑO %.0f"),
				Fighter->GetMatchKnockouts(), Fighter->GetMatchDeaths(), Fighter->GetMatchDamageDealt()),
				Muted, CardX + 38.0f, CardY + 190.0f, GEngine->GetSmallFont(), 0.82f);
		}
		DrawText(TEXT("VOLVIENDO AL SPAWN PARA LA SIGUIENTE RONDA..."), White,
			CardX + 38.0f, CardY + 247.0f, GEngine->GetSmallFont(), 0.82f);
	}
	if (MatchAnnouncementRemaining > 0.0f && !bPreparingRound && !bShowingRoundResult && !bShowingMatchResult)
	{
		const float Fade = FMath::Clamp(MatchAnnouncementRemaining / 0.28f, 0.0f, 1.0f);
		const float Pulse = 1.0f + 0.045f * FMath::Sin(GetWorld()->GetTimeSeconds() * 7.0f);
		float AnnouncementWidth = 0.0f;
		float AnnouncementHeight = 0.0f;
		Canvas->StrLen(GEngine->GetMediumFont(), MatchAnnouncementText, AnnouncementWidth, AnnouncementHeight);
		const float CardWidth = FMath::Min(ScreenWidth - 48.0f, AnnouncementWidth * Pulse + 64.0f);
		const float CardX = (ScreenWidth - CardWidth) * 0.5f;
		const float CardY = ScreenHeight * 0.24f;
		DrawRect(FLinearColor(0.012f, 0.022f, 0.035f, 0.84f * Fade),
			CardX, CardY, CardWidth, 60.0f);
		DrawRect(FLinearColor(MatchAnnouncementColor.R, MatchAnnouncementColor.G,
			MatchAnnouncementColor.B, Fade), CardX, CardY, 4.0f, 60.0f);
		DrawText(MatchAnnouncementText, FLinearColor(MatchAnnouncementColor.R,
			MatchAnnouncementColor.G, MatchAnnouncementColor.B, Fade),
			CardX + 30.0f, CardY + 18.0f, GEngine->GetMediumFont(), Pulse);
	}

	const float InstructionsY = ScreenHeight - 70.0f;
	if (!bPreparingRound && !bShowingRoundResult)
	{
		DrawRect(Panel, 34.0f, InstructionsY - 10.0f, ScreenWidth - 68.0f, 44.0f);
	}
	FString Instructions = TEXT("ESC  RETURN TO MAIN MENU");
	if (!bShowingMatchResult && !bPreparingRound && !bShowingRoundResult)
	{
		const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
		const FString PrimaryKey = PlayerData ? PlayerData->GetInputBinding(TEXT("PrimarySkill")).GetDisplayName().ToString() : TEXT("Q");
		const FString SecondaryKey = PlayerData ? PlayerData->GetInputBinding(TEXT("SecondarySkill")).GetDisplayName().ToString() : TEXT("E");
		const FString UltimateKey = PlayerData ? PlayerData->GetInputBinding(TEXT("Ultimate")).GetDisplayName().ToString() : TEXT("F");
		Instructions = FString::Printf(TEXT("WASD MOVE   SPACE JUMP   CTRL DASH   LMB ATTACK   LMB+RMB GRAB   RMB CHARGE   PRIMARY %s   SECONDARY %s   ULTIMATE %s   ESC MENU"),
			*PrimaryKey, *SecondaryKey, *UltimateKey);
	}
	if (!bPreparingRound && !bShowingRoundResult)
	{
		DrawText(Instructions, White, 54.0f, InstructionsY, GEngine->GetSmallFont(), 0.85f);
	}

	if (const AUBFCombatPlayerController* CombatController =
		Cast<AUBFCombatPlayerController>(GetOwningPlayerController());
		CombatController && CombatController->IsCombatMenuOpen())
	{
		const float MenuWidth = 460.0f;
		const float MenuHeight = 330.0f;
		const float MenuX = (ScreenWidth - MenuWidth) * 0.5f;
		const float MenuY = (ScreenHeight - MenuHeight) * 0.5f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f), 0.0f, 0.0f, ScreenWidth, ScreenHeight);
		DrawRect(FLinearColor(0.015f, 0.026f, 0.042f, 0.97f), MenuX, MenuY, MenuWidth, MenuHeight);
		DrawRect(Cyan, MenuX, MenuY, MenuWidth, 4.0f);
		DrawText(TEXT("PAUSA  //  PARTIDA"), White, MenuX + 30.0f, MenuY + 24.0f,
			GEngine->GetLargeFont(), 0.9f);
		auto DrawMenuButton = [this, MenuX, MenuY](float OffsetY, const FString& Label,
			const FLinearColor& Color)
		{
			DrawRect(FLinearColor(0.07f, 0.11f, 0.15f, 1.0f), MenuX + 30.0f, MenuY + OffsetY, 400.0f, 52.0f);
			DrawRect(Color, MenuX + 30.0f, MenuY + OffsetY, 3.0f, 52.0f);
			DrawText(Label, Color, MenuX + 52.0f, MenuY + OffsetY + 17.0f,
				GEngine->GetMediumFont(), 0.78f);
		};
		DrawMenuButton(82.0f, TEXT("VOLVER A LA PARTIDA"), Cyan);
		DrawMenuButton(148.0f, CombatController->CanSurrender()
			? TEXT("RENDIRSE  ·  DISPONIBLE EN RONDA 3")
			: TEXT("RENDIRSE  ·  SE HABILITA EN RONDA 3"),
			CombatController->CanSurrender() ? Crimson : Muted);
		DrawMenuButton(214.0f, TEXT("ABANDONAR SALA  ·  EMPATE AUTOMÁTICO"),
			FLinearColor(1.0f, 0.67f, 0.27f, 1.0f));
		DrawText(TEXT("ESC  CERRAR"), Muted, MenuX + 30.0f, MenuY + 288.0f,
			GEngine->GetSmallFont(), 0.8f);
	}
}

void AUBFCombatHUD::HandleCombatMenuClick(const FVector2D& ScreenPosition)
{
	const AUBFCombatPlayerController* Controller =
		Cast<AUBFCombatPlayerController>(GetOwningPlayerController());
	if (!Canvas || !Controller || !Controller->IsCombatMenuOpen())
	{
		return;
	}
	const float MenuX = (Canvas->ClipX - 460.0f) * 0.5f;
	const float MenuY = (Canvas->ClipY - 330.0f) * 0.5f;
	if (ScreenPosition.X < MenuX + 30.0f || ScreenPosition.X > MenuX + 430.0f)
	{
		return;
	}
	if (ScreenPosition.Y >= MenuY + 82.0f && ScreenPosition.Y <= MenuY + 134.0f)
	{
		if (AUBFCombatPlayerController* MutableController =
			Cast<AUBFCombatPlayerController>(GetOwningPlayerController()))
		{
			MutableController->CloseCombatMenu();
		}
	}
	else if (ScreenPosition.Y >= MenuY + 148.0f && ScreenPosition.Y <= MenuY + 200.0f)
	{
		if (Controller->CanSurrender())
		{
			const_cast<AUBFCombatPlayerController*>(Controller)->RequestSurrender();
		}
	}
	else if (ScreenPosition.Y >= MenuY + 214.0f && ScreenPosition.Y <= MenuY + 266.0f)
	{
		const_cast<AUBFCombatPlayerController*>(Controller)->RequestAbandonRoom();
	}
}
