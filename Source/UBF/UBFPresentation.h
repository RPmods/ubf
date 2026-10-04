#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "UBFPresentation.generated.h"

class UAudioComponent;
class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UFileMediaSource;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class USoundWave;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
struct FStreamableHandle;

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

/**
 * Lightweight front-end built in C++ while UBF's production UMG assets are prepared.
 * It owns only presentation state; player progress is provided by UUBFPlayerDataSubsystem.
 */
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
		Preloading,
		Intro,
		MenuReveal,
		Menu,
		DevelopmentError
	};

	enum class EMenuPage : uint8
	{
		Home,
		Play,
		Character,
		Inventory,
		Profile,
		Shop,
		Codes,
		Settings
	};

	void BuildInterface();
	void BuildMenu();
	void BuildPages();
	void PrepareResources();
	void ResetPresentationResources();
	void StartMediaPreload();
	void OnSoundAssetsLoaded();
	bool LoadLogo();
	bool ValidateVideo(UMediaPlayer* Player, const FString& Label, bool bMustReachIntroCut);
	void TryStartIntro();
	void StartIntroAudio();
	void BeginMenu();
	void StartTitleVoice();
	void EnterInteractiveMenu();
	void StartLoopMusic();
	void ShowDevelopmentError(const FString& Reason);
	void SetMenuPage(EMenuPage NewPage);
	void RefreshPlayerData();
	void ShowRewardToast(const FString& Summary);
	void PositionWidget(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position, const FVector2D& Size, int32 ZOrder);
	void FillWidget(UWidget* Widget, const FAnchors& Anchors, int32 ZOrder);
	UTextBlock* CreateText(const FName& Name, const FString& Text, int32 FontSize,
		const FLinearColor& Color, bool bBold = false) const;
	UButton* CreateMenuButton(const FName& Name, const FString& Label) const;
	UBorder* CreatePanel(const FName& Name, const FLinearColor& FillColor) const;
	UVerticalBox* CreatePage(const FName& Name, const FString& Title, const FString& Subtitle);

	UFUNCTION()
	void OnIntroVideoOpened(FString OpenedUrl);

	UFUNCTION()
	void OnIntroVideoOpenFailed(FString FailedUrl);

	UFUNCTION()
	void OnIntroVideoEnded();

	UFUNCTION()
	void OnIntroPlaybackResumed();

	UFUNCTION()
	void OnBackgroundOpened(FString OpenedUrl);

	UFUNCTION()
	void OnBackgroundOpenFailed(FString FailedUrl);

	UFUNCTION()
	void OnBackgroundPlaybackResumed();

	UFUNCTION()
	void OnIntroAudioEnded();

	UFUNCTION()
	void OnTitleVoiceEnded();

	UFUNCTION()
	void OnPlayClicked();

	UFUNCTION()
	void OnCharacterClicked();

	UFUNCTION()
	void OnInventoryClicked();

	UFUNCTION()
	void OnProfileClicked();

	UFUNCTION()
	void OnShopClicked();

	UFUNCTION()
	void OnCodesClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnRedeemClicked();

	UFUNCTION()
	void OnRetryClicked();

	UFUNCTION()
	void OnExitClicked();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UBorder> Backdrop;

	UPROPERTY()
	TObjectPtr<UImage> VideoImage;

	UPROPERTY()
	TObjectPtr<UBorder> VideoTint;

	UPROPERTY()
	TObjectPtr<UImage> LogoImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DevelopmentErrorText;

	UPROPERTY()
	TObjectPtr<UButton> RetryButton;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> MenuRoot;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	UPROPERTY()
	TObjectPtr<UTextBlock> PageTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> AccountSummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> InventorySummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ProfileSummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GachaSummaryText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> GiftCodeInput;

	UPROPERTY()
	TObjectPtr<UTextBlock> GiftCodeResultText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RewardToastText;

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
	TObjectPtr<USoundWave> TitleVoiceSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> IntroAudioComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopAudioComponent;

	UPROPERTY()
	TObjectPtr<UAudioComponent> TitleVoiceAudioComponent;

	TSharedPtr<FStreamableHandle> PresentationAssetLoadHandle;
	EPresentationState State = EPresentationState::Preloading;
	EMenuPage ActivePage = EMenuPage::Home;
	const FTimespan IntroCutTime = FTimespan::FromMilliseconds(13240);
	const FTimespan IntroMusicStartTime = FTimespan::FromSeconds(2.0);
	float MenuElapsed = 0.0f;
	float BackgroundRevealElapsed = 0.0f;
	float NextGlitchTime = 4.0f;
	float GlitchRemaining = 0.0f;
	float RewardToastRemaining = 0.0f;
	float RewardToastElapsed = 0.0f;
	bool bLogoReady = false;
	bool bSoundAssetsReady = false;
	bool bIntroVideoReady = false;
	bool bBackgroundVideoReady = false;
	bool bBackgroundPrerolled = false;
	bool bIntroPlaybackStarted = false;
	bool bIntroAudioStarted = false;
	bool bMenuStarted = false;
	bool bTitleVoiceStarted = false;
	bool bInteractiveMenuShown = false;
	bool bTearingDown = false;
};
