#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UBFPresentation.generated.h"

class UAudioComponent;
class UButton;
class UCanvasPanel;
class UFileMediaSource;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class USoundWave;
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
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EPresentationState : uint8
	{
		Intro,
		Menu,
		BetaError
	};

	void BuildInterface();
	void PrepareResources();
	bool LoadLogo();
	void BeginMenu();
	void StartLoopMusic();
	void StartBackgroundVideo();
	void PositionWidget(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, const FVector2D& Size, int32 ZOrder);

	UFUNCTION()
	void OnIntroVideoOpened(FString OpenedUrl);
	UFUNCTION()
	void OnIntroVideoOpenFailed(FString FailedUrl);
	UFUNCTION()
	void OnIntroAudioEnded();
	UFUNCTION()
	void OnBackgroundOpened(FString OpenedUrl);
	UFUNCTION()
	void OnBackgroundOpenFailed(FString FailedUrl);
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
	TObjectPtr<UFileMediaSource> BackgroundSource;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> IntroVideoPlayer;
	UPROPERTY()
	TObjectPtr<UMediaPlayer> BackgroundPlayer;
	UPROPERTY()
	TObjectPtr<UMediaTexture> IntroTexture;
	UPROPERTY()
	TObjectPtr<UMediaTexture> BackgroundTexture;
	UPROPERTY()
	TObjectPtr<UTexture2D> LogoTexture;
	UPROPERTY()
	TObjectPtr<USoundWave> IntroSound;
	UPROPERTY()
	TObjectPtr<USoundWave> LoopSound;
	UPROPERTY()
	TObjectPtr<UAudioComponent> IntroAudioComponent;
	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopAudioComponent;

	EPresentationState State = EPresentationState::Intro;
	FTimespan IntroCutTime = FTimespan::FromSeconds(13.2);
	float IntroElapsed = 0.0f;
	float MenuElapsed = 0.0f;
	float NextGlitchTime = 4.0f;
	float GlitchRemaining = 0.0f;
	bool bIntroVideoReady = false;
	bool bIntroVideoHasAdvanced = false;
	bool bBackgroundRequested = false;
};
