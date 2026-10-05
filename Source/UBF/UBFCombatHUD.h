#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UBFCombatHUD.generated.h"

class UFileMediaSource;
class UAudioComponent;
class UMediaPlayer;
class UMediaSoundComponent;
class USoundWave;
struct FUBFMatchCueEvent;
enum class EUBFMatchPhase : uint8;

/** Small combat HUD for the first local arena build. */
UCLASS()
class UBF_API AUBFCombatHUD : public AHUD
{
	GENERATED_BODY()

public:
	AUBFCombatHUD();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
	void HandleCombatMenuClick(const FVector2D& ScreenPosition);

private:
	void HandleMatchPhaseTransition(EUBFMatchPhase NewPhase);
	void HandleMatchCueEvent(const FUBFMatchCueEvent& Event);
	void QueueMatchCueAudio(const FString& FileName, float VolumeMultiplier,
		const FString& Announcement, const FLinearColor& AnnouncementColor, float AnnouncementDuration = 3.5f);
	void PlayNextQueuedCueAudio();
	void StartMatchAudio(const FString& FileName, bool bLooping, float VolumeMultiplier = 0.60f);
	void StopMatchAudio();
	void StartNextBattleTrack();
	int32 GetLocalTeamId() const;

	UFUNCTION()
	void OnMatchAudioOpened(FString OpenedUrl);

	UFUNCTION()
	void OnMatchAudioOpenFailed(FString FailedUrl);

	UFUNCTION()
	void OnMatchAudioEnded();

	UFUNCTION()
	void StopRoundResultAudio();

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> MatchAudioPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaSoundComponent> MatchAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFileMediaSource> CurrentMatchAudioSource;

	UPROPERTY(Transient)
	TObjectPtr<USoundWave> FirstRoundIntroSound;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FirstRoundIntroAudioComponent;

	EUBFMatchPhase LastObservedPhase;
	int32 LastObservedRound = 0;
	int32 LastObservedMatchCueSequence = 0;
	int32 LastBattleTrackIndex = INDEX_NONE;
	bool bBattleMusicActive = false;
	bool bPlayingCueAudio = false;
	bool bFirstRoundIntroPlayed = false;
	TArray<TPair<FString, float>> PendingCueAudioFiles;
	FString MatchAnnouncementText;
	FLinearColor MatchAnnouncementColor = FLinearColor::White;
	float MatchAnnouncementRemaining = 0.0f;
	float MatchAnnouncementDuration = 0.0f;
	FTimerHandle RoundResultAudioTimer;
};
