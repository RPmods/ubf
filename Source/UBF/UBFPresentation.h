#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "MediaPlayer.h"
#include "UBFPresentation.generated.h"

class UButton;
class UCanvasPanel;
class UFileMediaSource;
class UImage;
class UMediaTexture;
class UTextBlock;
class UTexture2D;
class UBorder;

UCLASS()
class UBF_API AUBFPresentationGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUBFPresentationGameMode();
};

UCLASS()
class UBF_API AUBFPresentationController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<class UUBFPresentationWidget> PresentationWidget;
};

UCLASS()
class UBF_API UUBFPresentationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EPresentationState : uint8
	{
		Preparing,
		Intro,
		Menu,
		BetaError,
		DevelopmentError
	};

	void BuildInterface();
	void PrepareResources();
	bool LoadLogo();
	void TryStartIntro();
	void StartIntroPlayback();
	void OnIntroVideoSeekCompleted();
	void OnBackgroundSeekCompleted();
	void BeginMenu();
	void StartLoopMusic();
	void ShowDevelopmentError(const FText& Reason);
	void UpdateStatus(const FText& Message);
	void PositionWidget(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, const FVector2D& Size);

	UFUNCTION()
	void OnIntroVideoOpened(FString OpenedUrl);
	UFUNCTION()
	void OnIntroAudioOpened(FString OpenedUrl);
	UFUNCTION()
	void OnIntroAudioEnded();
	UFUNCTION()
	void OnLoopAudioOpened(FString OpenedUrl);
	UFUNCTION()
	void OnBackgroundOpened(FString OpenedUrl);
	UFUNCTION()
	void OnMediaOpenFailed(FString FailedUrl);
	UFUNCTION()
	void OnPlayClicked();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY()
	TObjectPtr<UBorder> Backdrop;
	UPROPERTY()
	TObjectPtr<UImage> VideoImage;
	UPROPERTY()
	TObjectPtr<UImage> LogoImage;
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY()
	TObjectPtr<UTextBlock> SessionText;
	UPROPERTY()
	TObjectPtr<UTextBlock> BetaErrorText;
	UPROPERTY()
	TObjectPtr<UButton> PlayButton;

	UPROPERTY()
	TObjectPtr<UFileMediaSource> IntroVideoSource;
	UPROPERTY()
	TObjectPtr<UFileMediaSource> IntroAudioSource;
	UPROPERTY()
	TObjectPtr<UFileMediaSource> LoopAudioSource;
	UPROPERTY()
	TObjectPtr<UFileMediaSource> BackgroundSource;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> IntroVideoPlayer;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> IntroAudioPlayer;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> LoopAudioPlayer;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> BackgroundPlayer;
	UPROPERTY()
	TObjectPtr<UMediaTexture> IntroTexture;
	UPROPERTY()
	TObjectPtr<UMediaTexture> BackgroundTexture;
	UPROPERTY()
	TObjectPtr<UTexture2D> LogoTexture;

	EPresentationState State = EPresentationState::Preparing;
	FTimespan IntroCutTime;
	float IntroFrameRate = 0.0f;
	float PreparationElapsed = 0.0f;
	float MenuElapsed = 0.0f;
	float NextGlitchTime = 4.0f;
	float GlitchRemaining = 0.0f;
	bool bIntroVideoReady = false;
	bool bIntroAudioReady = false;
	bool bLoopAudioReady = false;
	bool bLoopAudioStarted = false;
	bool bBackgroundReady = false;
	bool bResourcesValidated = false;
	bool bIntroSeekReady = false;
	bool bBackgroundSeekReady = false;
};
