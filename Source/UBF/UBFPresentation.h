#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UBFPresentation.generated.h"

class UAudioComponent;
class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UFileMediaSource;
class UHorizontalBox;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class USoundWave;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;
class UProgressBar;
class UWrapBox;
class AUBFDraftShowcaseActor;
struct FStreamableHandle;
struct FKeyEvent;
struct FPointerEvent;

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
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EPresentationState : uint8
	{
		Preloading,
		Intro,
		MenuReveal,
		Menu,
		Draft,
		LoadingLevel,
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
		Gacha,
		Codes,
		Settings
	};

	void BuildInterface();
	void BuildMenu();
	void BuildWelcomeScreen();
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
	void BeginMatchLoading();
	void BuildCharacterDraft();
	void DestroyDraftShowcase();
	void StartCharacterDraft();
	void RefreshCharacterDraft();
	void SetMenuPage(EMenuPage NewPage);
	void RefreshPlayerData();
	void ShowRewardToast(const FString& Summary);
	void StartCurrencyRewardAnimation(int32 PreviousGold, int32 PreviousEventPoints, int32 PreviousGachaTickets);
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
	void OnGachaClicked();

	UFUNCTION()
	void OnShopBuyBasicArmorClicked();

	UFUNCTION()
	void OnShopBuyBasicRingClicked();

	UFUNCTION()
	void OnShopBuyBasicNecklaceClicked();

	UFUNCTION()
	void OnShopBuyEventRingClicked();

	UFUNCTION()
	void OnShopBuyEventNecklaceClicked();

	UFUNCTION()
	void OnShopBuyEventFrameClicked();

	UFUNCTION()
	void OnConfirmShopPurchaseClicked();

	UFUNCTION()
	void OnCancelShopPurchaseClicked();

	UFUNCTION()
	void OnCodesClicked();

	UFUNCTION()
	void OnSettingsClicked();

	UFUNCTION()
	void OnPreviousQualityClicked();

	UFUNCTION()
	void OnNextQualityClicked();

	UFUNCTION()
	void OnPreviousFrameLimitClicked();

	UFUNCTION()
	void OnNextFrameLimitClicked();

	UFUNCTION()
	void OnRebindPrimaryClicked();

	UFUNCTION()
	void OnRebindSecondaryClicked();

	UFUNCTION()
	void OnRebindUltimateClicked();

	UFUNCTION()
	void OnResetInputBindingsClicked();

	UFUNCTION()
	void OnToggleVSyncClicked();

	UFUNCTION()
	void OnRedeemClicked();

	UFUNCTION()
	void OnTrainingClicked();
	UFUNCTION()
	void OnDraftReadyClicked();
	UFUNCTION()
	void OnDraftCancelClicked();
	UFUNCTION()
	void OnHomeClicked();
	UFUNCTION()
	void OnContinueFromWelcomeClicked();
	UFUNCTION()
	void OnContinueCharacterClicked();

	UFUNCTION()
	void OnPreviousCharacterClicked();

	UFUNCTION()
	void OnNextCharacterClicked();
	void RefreshCharacterSelection();

	UFUNCTION()
	void OnPreviousTeamSizeClicked();

	UFUNCTION()
	void OnNextTeamSizeClicked();

	UFUNCTION()
	void OnPreviousGameModeClicked();

	UFUNCTION()
	void OnNextGameModeClicked();

	UFUNCTION()
	void OnPreviousBotFillClicked();

	UFUNCTION()
	void OnNextBotFillClicked();

	UFUNCTION()
	void OnPreviousBotDifficultyClicked();

	UFUNCTION()
	void OnNextBotDifficultyClicked();

	void RefreshMatchSetupText();
	void RefreshSettingsText();
	void RefreshInputBindingText();
	void BeginInputRebind(FName ActionId);
	void CaptureInputBinding(const FKey& Key);
	void ApplySettingsSelection();
	void SelectShopOffer(const FString& ItemId, const FString& ConfirmationText);

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
	TObjectPtr<UCanvasPanel> WelcomeRoot;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> LoadingRoot;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> DraftRoot;

	UPROPERTY()
	TObjectPtr<UProgressBar> DraftTimerBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftTimerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftStatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftCharacterNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftCharacterRoleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftBlueRosterText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftRedRosterText;

	UPROPERTY()
	TObjectPtr<UImage> DraftPreviewImage;

	UPROPERTY(Transient)
	TObjectPtr<AUBFDraftShowcaseActor> DraftShowcaseActor;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftPrimarySkillText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftSecondarySkillText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftUltimateText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DraftPassiveText;

	UPROPERTY()
	TObjectPtr<UButton> DraftPreviousButton;

	UPROPERTY()
	TObjectPtr<UButton> DraftNextButton;

	UPROPERTY()
	TObjectPtr<UButton> DraftReadyButton;

	UPROPERTY()
	TObjectPtr<UButton> DraftCancelButton;

	UPROPERTY()
	TObjectPtr<UProgressBar> LoadingProgressBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> LoadingStageText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LoadingMapTitleText;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	UPROPERTY()
	TObjectPtr<UTextBlock> PageTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> AccountSummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> InventorySummaryText;

	UPROPERTY()
	TObjectPtr<UWrapBox> InventoryItemsGrid;

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
	TObjectPtr<UTextBlock> MatchFormatValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GameModeValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BotFillValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BotDifficultyValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> MatchSetupHintText;

	UPROPERTY()
	TObjectPtr<UTextBlock> RoomPreviewText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CharacterNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CharacterDetailsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CharacterPortraitMarkText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CharacterPortraitRoleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> QualityValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> FrameLimitValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> VSyncValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PrimaryBindingText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SecondaryBindingText;

	UPROPERTY()
	TObjectPtr<UTextBlock> UltimateBindingText;

	UPROPERTY()
	TObjectPtr<UTextBlock> InputBindingResultText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ShopBalanceText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ShopConfirmationText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ShopPurchaseResultText;

	UPROPERTY()
	TObjectPtr<UBorder> ShopConfirmationPanel;

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
	float CurrencyRewardElapsed = 0.0f;
	float LoadingElapsed = 0.0f;
	float DraftElapsed = 0.0f;
	float DraftCountdownRemaining = 0.0f;
	float WelcomeElapsed = 0.0f;
	float MenuEntranceElapsed = 0.0f;
	float PageEntranceElapsed = 0.0f;
	float CharacterEntryElapsed = 0.0f;
	int32 CurrencyStartGold = 0;
	int32 CurrencyStartEventPoints = 0;
	int32 CurrencyStartGachaTickets = 0;
	int32 CurrencyTargetGold = 0;
	int32 CurrencyTargetEventPoints = 0;
	int32 CurrencyTargetGachaTickets = 0;
	int32 CurrencyDisplayedGold = 0;
	int32 CurrencyDisplayedEventPoints = 0;
	int32 CurrencyDisplayedGachaTickets = 0;
	int32 SelectedTeamSize = 1;
	int32 SelectedGameModeIndex = 0;
	int32 SelectedBotFillModeIndex = 1;
	int32 SelectedBotDifficultyIndex = 1;
	int32 SelectedCharacterIndex = 0;
	TArray<FName> CharacterIds;
	int32 SelectedQualityIndex = 0;
	int32 SelectedFrameLimitIndex = 1;
	bool bVSyncEnabled = true;
	FString PendingShopItemId;
	FString PendingTrainingOptions;
	FString LastRenderedInventoryKey = TEXT("__not_rendered__");
	FName PendingInputRebindAction;
	uint32 InventoryWidgetBuildSerial = 0;
	bool bCurrencyRewardAnimating = false;
	bool bShowingWelcomeScreen = false;
	bool bLoadingScreenActive = false;
	bool bDraftReady = false;
	bool bDraftCountdownActive = false;
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
