#include "UBFPresentation.h"

#include "UBFGameInstance.h"
#include "UBFPlayerData.h"
#include "UBFCharacterDefinition.h"
#include "Brushes/SlateColorBrush.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundWave.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TextureResource.h"

namespace UBFPresentation
{
	DEFINE_LOG_CATEGORY_STATIC(LogUBFPresentation, Log, All);

	const FLinearColor AccentColor(0.92f, 0.04f, 0.10f, 1.0f);
	const FLinearColor GoldColor(0.94f, 0.70f, 0.22f, 1.0f);
	const FLinearColor BackdropColor(0.012f, 0.018f, 0.028f, 1.0f);
	const FLinearColor PanelColor(0.028f, 0.038f, 0.055f, 0.88f);
	const FLinearColor SubtlePanelColor(0.05f, 0.065f, 0.085f, 0.76f);
	const FLinearColor TextColor(0.92f, 0.94f, 0.97f, 1.0f);
	const FLinearColor MutedTextColor(0.62f, 0.68f, 0.74f, 1.0f);
	const TCHAR* QualityLabels[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH") };
	constexpr float FrameLimitValues[] = { 30.0f, 60.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f, 0.0f };
	const TCHAR* FrameLimitLabels[] = { TEXT("30 FPS"), TEXT("60 FPS"), TEXT("90 FPS"), TEXT("120 FPS"),
		TEXT("144 FPS"), TEXT("165 FPS"), TEXT("240 FPS"), TEXT("SIN LÍMITE") };
	constexpr int32 FrameLimitCount = 8;
	constexpr float IntroAndLoopVolume = 0.60f;
	constexpr float TitleVoiceVolume = 1.00f;
	const FSoftObjectPath IntroMusicPath(TEXT("/Game/Presentation/Audio/musicintro.musicintro"));
	const FSoftObjectPath LoopMusicPath(TEXT("/Game/Presentation/Audio/musicintrobucle.musicintrobucle"));
	const FSoftObjectPath TitleVoicePath(TEXT("/Game/Presentation/Audio/introdution_ubf.introdution_ubf"));
	// WmfMedia rounds some H.264 durations down to the nearest decoded sample.  The
	// source clip is 13.240 seconds (00:00:13:06 at 25 fps), but Windows can report
	// 13.200 seconds.  Keep the authored cut and only accept this narrow backend drift.
	const FTimespan MediaDurationTolerance = FTimespan::FromMilliseconds(100);

	UFileMediaSource* MakeFileSource(UObject* Outer, const FString& Path)
	{
		UFileMediaSource* Source = NewObject<UFileMediaSource>(Outer);
		Source->SetFilePath(Path);
		return Source;
	}

	UMediaPlayer* MakePlayer(UObject* Outer)
	{
		UMediaPlayer* Player = NewObject<UMediaPlayer>(Outer);
		Player->PlayOnOpen = false;
		Player->NativeAudioOut = false;
		return Player;
	}

	UMediaTexture* MakeTexture(UObject* Outer, UMediaPlayer* Player)
	{
		UMediaTexture* Texture = NewObject<UMediaTexture>(Outer);
		Texture->AutoClear = true;
		Texture->SetMediaPlayer(Player);
		Texture->UpdateResource();
		return Texture;
	}

	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Parent, UWidget* Widget, const FAnchors& Anchors,
		const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
	{
		if (!Parent || !Widget)
		{
			return nullptr;
		}

		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Parent->AddChild(Widget));
		if (Slot)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
		return Slot;
	}

	UCanvasPanelSlot* FillCanvas(UCanvasPanel* Parent, UWidget* Widget, const FAnchors& Anchors, int32 ZOrder)
	{
		if (!Parent || !Widget)
		{
			return nullptr;
		}

		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Parent->AddChild(Widget));
		if (Slot)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetZOrder(ZOrder);
		}
		return Slot;
	}

	void AddPageLine(UVerticalBox* Page, UTextBlock* Line, float BottomPadding = 12.0f)
	{
		if (Page && Line)
		{
			if (UVerticalBoxSlot* Slot = Page->AddChildToVerticalBox(Line))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
			}
		}
	}
}

AUBFPresentationGameMode::AUBFPresentationGameMode()
{
	PlayerControllerClass = AUBFPresentationController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void AUBFPresentationController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Controlador de presentación iniciado."));

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));

	// Existing local settings can override DefaultEngine.ini.  Keep the menu bounded even
	// when a prior uncapped third-person session wrote FrameRateLimit=0. Only initialize
	// defaults for a fresh or invalid settings file; saved player choices must survive boot.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (Settings)
	{
		if (Settings->GetOverallScalabilityLevel() < 0)
		{
			Settings->SetOverallScalabilityLevel(0);
			Settings->SetFrameRateLimit(60.0f);
			Settings->SetVSyncEnabled(true);
		}
		Settings->ApplySettings(false);
	}

	PresentationWidget = CreateWidget<UUBFPresentationWidget>(this);
	if (PresentationWidget)
	{
		PresentationWidget->AddToViewport(1000);
	}
	else
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("No se pudo crear el widget de presentación."));
	}
}

void UUBFPresentationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildInterface();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Árbol visual de presentación construido."));
}

FReply UUBFPresentationWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!PendingInputRebindAction.IsNone())
	{
		CaptureInputBinding(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UUBFPresentationWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!PendingInputRebindAction.IsNone())
	{
		CaptureInputBinding(InMouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UUBFPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UUBFPlayerDataSubsystem* PlayerData = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr)
	{
		PlayerData->OnPlayerDataChanged.AddUObject(this, &UUBFPresentationWidget::RefreshPlayerData);
	}

	PrepareResources();
}

void UUBFPresentationWidget::NativeDestruct()
{
	bTearingDown = true;
	if (UUBFPlayerDataSubsystem* PlayerData = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr)
	{
		PlayerData->OnPlayerDataChanged.RemoveAll(this);
	}

	ResetPresentationResources();
	Super::NativeDestruct();
}

UTextBlock* UUBFPresentationWidget::CreateText(const FName& Name, const FString& Text, int32 FontSize,
	const FLinearColor& Color, bool bBold) const
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetText(FText::FromString(Text));
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetAutoWrapText(true);
	TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FontSize));
	return TextBlock;
}

UBorder* UUBFPresentationWidget::CreatePanel(const FName& Name, const FLinearColor& FillColor) const
{
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Panel->SetBrush(FSlateColorBrush(FillColor));
	Panel->SetPadding(FMargin(22.0f));
	return Panel;
}

UButton* UUBFPresentationWidget::CreateMenuButton(const FName& Name, const FString& Label) const
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	FButtonStyle Style;
	Style.Normal = FSlateColorBrush(FLinearColor(0.012f, 0.020f, 0.032f, 0.42f));
	Style.Hovered = FSlateColorBrush(FLinearColor(0.28f, 0.018f, 0.055f, 0.78f));
	Style.Pressed = FSlateColorBrush(UBFPresentation::AccentColor);
	Style.Disabled = FSlateColorBrush(FLinearColor(0.02f, 0.025f, 0.035f, 0.20f));
	Style.NormalPadding = FMargin(13.0f, 6.0f);
	Style.PressedPadding = FMargin(13.0f, 6.0f);
	Button->SetStyle(Style);

	UHorizontalBox* ButtonLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
		FName(*(Name.ToString() + TEXT("_Layout"))));
	USizeBox* AccentRail = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		FName(*(Name.ToString() + TEXT("_AccentRail"))));
	AccentRail->SetWidthOverride(3.0f);
	UBorder* AccentFill = CreatePanel(FName(*(Name.ToString() + TEXT("_AccentFill"))), UBFPresentation::AccentColor);
	AccentFill->SetPadding(FMargin(0.0f));
	AccentRail->SetContent(AccentFill);
	if (UHorizontalBoxSlot* RailSlot = ButtonLayout->AddChildToHorizontalBox(AccentRail))
	{
		RailSlot->SetPadding(FMargin(0.0f, 0.0f, 11.0f, 0.0f));
	}

	UTextBlock* ButtonText = CreateText(FName(*(Name.ToString() + TEXT("_Text"))), Label, 13,
		UBFPresentation::TextColor, true);
	ButtonText->SetJustification(ETextJustify::Left);
	ButtonLayout->AddChildToHorizontalBox(ButtonText);
	Button->SetContent(ButtonLayout);
	return Button;
}

void UUBFPresentationWidget::BuildInterface()
{
	if (!WidgetTree)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PresentationRoot"));
	WidgetTree->RootWidget = RootCanvas;

	Backdrop = CreatePanel(TEXT("LoadingBackdrop"), UBFPresentation::BackdropColor);
	PositionWidget(Backdrop, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 0);

	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PresentationVideo"));
	PositionWidget(VideoImage, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 1);
	VideoImage->SetVisibility(ESlateVisibility::Collapsed);

	VideoTint = CreatePanel(TEXT("VideoTint"), FLinearColor(0.01f, 0.012f, 0.02f, 0.065f));
	PositionWidget(VideoTint, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 2);
	VideoTint->SetVisibility(ESlateVisibility::Collapsed);

	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Logo"));
	PositionWidget(LogoImage, FAnchors(0.5f, 0.37f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(520.0f, 292.0f), 5);
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);

	StatusText = CreateText(TEXT("LoadingStatus"), TEXT("PREPARANDO PRESENTACIÓN UBF"), 17,
		UBFPresentation::TextColor, true);
	PositionWidget(StatusText, FAnchors(0.5f, 0.68f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(920.0f, 46.0f), 6);
	StatusText->SetJustification(ETextJustify::Center);

	DevelopmentErrorText = CreateText(TEXT("DevelopmentError"), TEXT(""), 18,
		UBFPresentation::AccentColor, true);
	PositionWidget(DevelopmentErrorText, FAnchors(0.5f, 0.63f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(980.0f, 145.0f), 8);
	DevelopmentErrorText->SetJustification(ETextJustify::Center);
	DevelopmentErrorText->SetVisibility(ESlateVisibility::Collapsed);

	RetryButton = CreateMenuButton(TEXT("RetryButton"), TEXT("REINTENTAR"));
	PositionWidget(RetryButton, FAnchors(0.5f, 0.78f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(250.0f, 56.0f), 9);
	RetryButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnRetryClicked);
	RetryButton->SetVisibility(ESlateVisibility::Collapsed);

	BuildMenu();

	RewardToastText = CreateText(TEXT("RewardToast"), TEXT(""), 19, UBFPresentation::GoldColor, true);
	PositionWidget(RewardToastText, FAnchors(0.5f, 0.11f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(760.0f, 130.0f), 20);
	RewardToastText->SetJustification(ETextJustify::Center);
	RewardToastText->SetVisibility(ESlateVisibility::Collapsed);
}

void UUBFPresentationWidget::BuildMenu()
{
	MenuRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuRoot"));
	PositionWidget(MenuRoot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 10);
	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);

	// The home screen is a game HUD layered over the background video.  It deliberately
	// avoids the large left and right application panels used by the previous launcher-like UI.
	UTextBlock* NavigationCaption = CreateText(TEXT("NavigationCaption"), TEXT("// SELECT DISCIPLINE"), 12,
		UBFPresentation::GoldColor, true);
	PositionWidget(NavigationCaption, FAnchors(0.055f, 0.465f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D(420.0f, 28.0f), 1);

	UVerticalBox* NavigationBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Navigation"));
	PositionWidget(NavigationBox, FAnchors(0.055f, 0.495f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D(385.0f, 330.0f), 1);

	auto AddNavigationButton = [&NavigationBox](UButton* Button)
	{
		if (UVerticalBoxSlot* NavigationSlot = NavigationBox->AddChildToVerticalBox(Button))
		{
			NavigationSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		}
	};

	UButton* PlayButton = CreateMenuButton(TEXT("PlayButton"), TEXT("01  //  COMBAT\nONLINE · CUSTOM"));
	PlayButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPlayClicked);
	AddNavigationButton(PlayButton);
	UButton* CharacterButton = CreateMenuButton(TEXT("CharacterButton"), TEXT("02  //  FIGHTERS\nTRAINING · LOADOUT"));
	CharacterButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCharacterClicked);
	AddNavigationButton(CharacterButton);
	UButton* InventoryButton = CreateMenuButton(TEXT("InventoryButton"), TEXT("03  //  ARMORY\nEQUIP · COLLECTION"));
	InventoryButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnInventoryClicked);
	AddNavigationButton(InventoryButton);
	UButton* ProfileButton = CreateMenuButton(TEXT("ProfileButton"), TEXT("04  //  PROFILE\nRECORD · PROGRESS"));
	ProfileButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnProfileClicked);
	AddNavigationButton(ProfileButton);

	UVerticalBox* UtilityBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UtilityNavigation"));
	if (UCanvasPanelSlot* UtilitySlot = Cast<UCanvasPanelSlot>(MenuRoot->AddChild(UtilityBox)))
	{
		UtilitySlot->SetAnchors(FAnchors(0.055f, 0.735f, 0.345f, 0.985f));
		UtilitySlot->SetOffsets(FMargin(0.0f));
		UtilitySlot->SetZOrder(1);
	}
	auto AddUtilityButton = [&UtilityBox](UButton* Button)
	{
		if (UVerticalBoxSlot* UtilitySlot = UtilityBox->AddChildToVerticalBox(Button))
		{
			UtilitySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
	};

	UButton* ShopButton = CreateMenuButton(TEXT("ShopButton"), TEXT("SHOP  //  COLLECTION"));
	ShopButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopClicked);
	AddUtilityButton(ShopButton);
	UButton* GachaButton = CreateMenuButton(TEXT("GachaButton"), TEXT("GACHA  //  TICKETS"));
	GachaButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnGachaClicked);
	AddUtilityButton(GachaButton);
	UButton* CodesButton = CreateMenuButton(TEXT("CodesButton"), TEXT("GIFT CODES"));
	CodesButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCodesClicked);
	AddUtilityButton(CodesButton);
	UButton* SettingsButton = CreateMenuButton(TEXT("SettingsButton"), TEXT("OPTIONS"));
	SettingsButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnSettingsClicked);
	AddUtilityButton(SettingsButton);
	UButton* ExitButton = CreateMenuButton(TEXT("ExitButton"), TEXT("EXIT GAME"));
	ExitButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnExitClicked);
	AddUtilityButton(ExitButton);

	UBorder* AccountPanel = CreatePanel(TEXT("AccountPanel"), FLinearColor(0.015f, 0.024f, 0.036f, 0.64f));
	AccountPanel->SetPadding(FMargin(13.0f, 9.0f));
	UBFPresentation::FillCanvas(MenuRoot, AccountPanel, FAnchors(0.710f, 0.055f, 0.945f, 0.145f), 2);
	AccountSummaryText = CreateText(TEXT("AccountSummary"), TEXT(""), 14, UBFPresentation::TextColor, true);
	AccountPanel->SetContent(AccountSummaryText);

	UBorder* ContentPanel = CreatePanel(TEXT("ContentPanel"), FLinearColor(0.012f, 0.020f, 0.032f, 0.68f));
	ContentPanel->SetPadding(FMargin(24.0f, 20.0f));
	UBFPresentation::FillCanvas(MenuRoot, ContentPanel, FAnchors(0.385f, 0.490f, 0.885f, 0.805f), 2);
	UVerticalBox* ContentLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentLayout"));
	ContentPanel->SetContent(ContentLayout);

	PageTitleText = CreateText(TEXT("CurrentPageTitle"), TEXT("UNIVERSAL BREAKING FIGHTERS"), 22,
		UBFPresentation::TextColor, true);
	if (UVerticalBoxSlot* TitleSlot = ContentLayout->AddChildToVerticalBox(PageTitleText))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 17.0f));
	}

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("PageSwitcher"));
	if (UVerticalBoxSlot* SwitcherSlot = ContentLayout->AddChildToVerticalBox(PageSwitcher))
	{
		SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	BuildPages();
}

UVerticalBox* UUBFPresentationWidget::CreatePage(const FName& Name, const FString& Title, const FString& Subtitle)
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
	UTextBlock* TitleText = CreateText(FName(*(Name.ToString() + TEXT("_Title"))), Title, 20,
		UBFPresentation::AccentColor, true);
	UBFPresentation::AddPageLine(Page, TitleText, 8.0f);
	UTextBlock* SubtitleText = CreateText(FName(*(Name.ToString() + TEXT("_Subtitle"))), Subtitle, 14,
		UBFPresentation::MutedTextColor, false);
	UBFPresentation::AddPageLine(Page, SubtitleText, 24.0f);
	return Page;
}

void UUBFPresentationWidget::BuildPages()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const int32 CurrentQuality = Settings->GetOverallScalabilityLevel();
		if (CurrentQuality >= 0)
		{
			SelectedQualityIndex = FMath::Clamp(CurrentQuality, 0, 2);
		}

		const float CurrentFrameLimit = Settings->GetFrameRateLimit();
		float ClosestFrameLimitDelta = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < UBFPresentation::FrameLimitCount; ++Index)
		{
			const float Delta = FMath::Abs(UBFPresentation::FrameLimitValues[Index] - CurrentFrameLimit);
			if (Delta < ClosestFrameLimitDelta)
			{
				ClosestFrameLimitDelta = Delta;
				SelectedFrameLimitIndex = Index;
			}
		}
		bVSyncEnabled = Settings->IsVSyncEnabled();
	}

	auto AddPage = [this](UVerticalBox* Page)
	{
		PageSwitcher->AddChild(Page);
	};

	UVerticalBox* HomePage = CreatePage(TEXT("HomePage"), TEXT("UNIVERSAL BREAKING FIGHTERS"),
		TEXT("OPEN BETA // FIGHTER NETWORK"));
	UBFPresentation::AddPageLine(HomePage, CreateText(TEXT("HomeIntro"),
		TEXT("Elige una disciplina. Cada módulo conserva tu identidad de combatiente y tu progreso local."),
		16, UBFPresentation::TextColor), 22.0f);
	UBFPresentation::AddPageLine(HomePage, CreateText(TEXT("HomeStatus"),
		TEXT("NETWORK STATUS  //  UBF CUSTOM READY\nSESSION PROFILE  //  LOCAL DATA LINKED"),
		15, UBFPresentation::GoldColor, true));
	AddPage(HomePage);

	UVerticalBox* PlayPage = CreatePage(TEXT("PlayPage"), TEXT("COMBAT"),
		TEXT("Configura una partida local con IA."));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("ServerList"),
		TEXT("PRÁCTICA DE COMBATE  //  LISTA\nHasta 6 luchadores · máximo 3 por equipo"),
		15, UBFPresentation::TextColor), 12.0f);
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("RoomFlow"),
		TEXT("Los bots ocupan las plazas libres y pelean por equipos."),
		13, UBFPresentation::MutedTextColor), 10.0f);

	auto CreateCycleButton = [this](UHorizontalBox* Row, const FName& Name, const FString& Label)
	{
		UButton* Button = CreateMenuButton(Name, Label);
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("_Size"))));
		ButtonSize->SetWidthOverride(38.0f);
		ButtonSize->SetHeightOverride(38.0f);
		ButtonSize->SetContent(Button);
		Row->AddChildToHorizontalBox(ButtonSize);
		return Button;
	};

	UHorizontalBox* FormatRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MatchFormatRow"));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("MatchFormatLabel"), TEXT("FORMATO"),
		12, UBFPresentation::GoldColor, true), 5.0f);
	MatchFormatValueText = CreateText(TEXT("MatchFormatValue"), TEXT("1v1 · 1 por equipo"),
		15, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* ValueSlot = FormatRow->AddChildToHorizontalBox(MatchFormatValueText))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	CreateCycleButton(FormatRow, TEXT("FormatPrevious"), TEXT("‹"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousTeamSizeClicked);
	CreateCycleButton(FormatRow, TEXT("FormatNext"), TEXT("›"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextTeamSizeClicked);
	if (UVerticalBoxSlot* FormatRowSlot = PlayPage->AddChildToVerticalBox(FormatRow))
	{
		FormatRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	UHorizontalBox* GameModeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GameModeRow"));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("GameModeLabel"), TEXT("MODO DE JUEGO"),
		12, UBFPresentation::GoldColor, true), 5.0f);
	GameModeValueText = CreateText(TEXT("GameModeValue"), TEXT("ARENA"), 14, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* ModeValueSlot = GameModeRow->AddChildToHorizontalBox(GameModeValueText))
	{
		ModeValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ModeValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	CreateCycleButton(GameModeRow, TEXT("GameModePrevious"), TEXT("‹"))
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousGameModeClicked);
	CreateCycleButton(GameModeRow, TEXT("GameModeNext"), TEXT("›"))
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextGameModeClicked);
	if (UVerticalBoxSlot* GameModeSlot = PlayPage->AddChildToVerticalBox(GameModeRow))
	{
		GameModeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	UHorizontalBox* BotFillRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BotFillRow"));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("BotFillLabel"), TEXT("LLENADO DE PLAZAS"),
		12, UBFPresentation::GoldColor, true), 5.0f);
	BotFillValueText = CreateText(TEXT("BotFillValue"), TEXT("RIVAL COMPLETO CON IA"),
		14, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* ValueSlot = BotFillRow->AddChildToHorizontalBox(BotFillValueText))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	CreateCycleButton(BotFillRow, TEXT("BotFillPrevious"), TEXT("‹"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousBotFillClicked);
	CreateCycleButton(BotFillRow, TEXT("BotFillNext"), TEXT("›"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextBotFillClicked);
	if (UVerticalBoxSlot* BotFillRowSlot = PlayPage->AddChildToVerticalBox(BotFillRow))
	{
		BotFillRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UHorizontalBox* BotDifficultyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BotDifficultyRow"));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("BotDifficultyLabel"), TEXT("DIFICULTAD DE LA IA"),
		12, UBFPresentation::GoldColor, true), 5.0f);
	BotDifficultyValueText = CreateText(TEXT("BotDifficultyValue"), TEXT("NORMAL"), 14, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* DifficultyValueSlot = BotDifficultyRow->AddChildToHorizontalBox(BotDifficultyValueText))
	{
		DifficultyValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DifficultyValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	CreateCycleButton(BotDifficultyRow, TEXT("BotDifficultyPrevious"), TEXT("‹"))
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousBotDifficultyClicked);
	CreateCycleButton(BotDifficultyRow, TEXT("BotDifficultyNext"), TEXT("›"))
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextBotDifficultyClicked);
	if (UVerticalBoxSlot* DifficultyRowSlot = PlayPage->AddChildToVerticalBox(BotDifficultyRow))
	{
		DifficultyRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	MatchSetupHintText = CreateText(TEXT("MatchSetupHint"), TEXT("La IA buscará y atacará combatientes del otro equipo."),
		12, UBFPresentation::MutedTextColor);
	UBFPresentation::AddPageLine(PlayPage, MatchSetupHintText, 3.0f);
	UButton* TrainingButton = CreateMenuButton(TEXT("TrainingButton"), TEXT("INICIAR COMBATE"));
	TrainingButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnTrainingClicked);
	if (UVerticalBoxSlot* TrainingButtonSlot = PlayPage->AddChildToVerticalBox(TrainingButton))
	{
		TrainingButtonSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
	AddPage(PlayPage);
	RefreshMatchSetupText();

	UVerticalBox* CharacterPage = CreatePage(TEXT("CharacterPage"), TEXT("FIGHTERS"),
		TEXT("Elige un estilo de combate. Los personajes repetidos están permitidos."));
	if (UUBFCharacterCatalogSubsystem* Catalog = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFCharacterCatalogSubsystem>() : nullptr)
	{
		CharacterIds = Catalog->GetCharacterIds();
	}
	if (const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr)
	{
		const int32 SavedIndex = CharacterIds.IndexOfByKey(PlayerData->GetSelectedCharacterId());
		if (SavedIndex != INDEX_NONE)
		{
			SelectedCharacterIndex = SavedIndex;
		}
	}
	UHorizontalBox* CharacterSelectRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CharacterSelectRow"));
	CreateCycleButton(CharacterSelectRow, TEXT("CharacterPrevious"), TEXT("‹"))
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousCharacterClicked);
	CharacterNameText = CreateText(TEXT("CharacterName"), TEXT("WIZZ"),
		22, UBFPresentation::GoldColor, true);
	if (UHorizontalBoxSlot* NameSlot = CharacterSelectRow->AddChildToHorizontalBox(CharacterNameText))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetHorizontalAlignment(HAlign_Center);
	}
	CreateCycleButton(CharacterSelectRow, TEXT("CharacterNext"), TEXT("›"))
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextCharacterClicked);
	if (UVerticalBoxSlot* CharacterSelectSlot = CharacterPage->AddChildToVerticalBox(CharacterSelectRow))
	{
		CharacterSelectSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 14.0f));
	}
	CharacterDetailsText = CreateText(TEXT("CharacterDetails"), TEXT(""),
		14, UBFPresentation::TextColor);
	UBFPresentation::AddPageLine(CharacterPage, CharacterDetailsText, 18.0f);
	UBFPresentation::AddPageLine(CharacterPage, CreateText(TEXT("CharacterHint"),
		TEXT("Q · habilidad primaria     E · habilidad secundaria     F · Ultimate\nLas asignaciones se pueden cambiar desde Opciones → Controles."),
		12, UBFPresentation::MutedTextColor));
	AddPage(CharacterPage);
	RefreshCharacterSelection();

	UVerticalBox* InventoryPage = CreatePage(TEXT("InventoryPage"), TEXT("ARMORY"),
		TEXT("Equipo, accesorios, consumibles y objetos de evento."));
	InventorySummaryText = CreateText(TEXT("InventorySummary"), TEXT(""), 16, UBFPresentation::TextColor);
	UBFPresentation::AddPageLine(InventoryPage, InventorySummaryText, 20.0f);
	UBFPresentation::AddPageLine(InventoryPage, CreateText(TEXT("InventoryHint"),
		TEXT("Los objetos obtenidos quedan ligados a este perfil local."),
		14, UBFPresentation::MutedTextColor));
	AddPage(InventoryPage);

	UVerticalBox* ProfilePage = CreatePage(TEXT("ProfilePage"), TEXT("PROFILE"),
		TEXT("Identidad, progreso y registro de combate."));
	ProfileSummaryText = CreateText(TEXT("ProfileSummary"), TEXT(""), 16, UBFPresentation::TextColor);
	UBFPresentation::AddPageLine(ProfilePage, ProfileSummaryText, 20.0f);
	UBFPresentation::AddPageLine(ProfilePage, CreateText(TEXT("ProfileHint"),
		TEXT("Tu cuenta mantiene el oro, los puntos de evento, los tickets y los objetos obtenidos."),
		14, UBFPresentation::MutedTextColor));
	AddPage(ProfilePage);

	UVerticalBox* ShopPage = CreatePage(TEXT("ShopPage"), TEXT("SHOP"),
		TEXT("Equipo y objetos de evento para tu perfil local."));
	ShopBalanceText = CreateText(TEXT("ShopBalance"), TEXT(""), 13, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(ShopPage, ShopBalanceText, 8.0f);
	UScrollBox* ShopScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ShopScroll"));
	ShopScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* ShopLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopLayout"));
	ShopScroll->AddChild(ShopLayout);
	if (UVerticalBoxSlot* ShopScrollSlot = ShopPage->AddChildToVerticalBox(ShopScroll))
	{
		ShopScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UBFPresentation::AddPageLine(ShopLayout, CreateText(TEXT("GoldShopSection"), TEXT("EQUIPO  //  ORO"),
		12, UBFPresentation::GoldColor, true), 4.0f);
	UButton* BasicArmorButton = CreateMenuButton(TEXT("ShopBasicArmor"), TEXT("ARMADURA BÁSICA   ·   1,200 ORO"));
	BasicArmorButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyBasicArmorClicked);
	UBFPresentation::AddPageLine(ShopLayout, CreateText(TEXT("BasicArmorDescription"), TEXT("Protección inicial para el combatiente."),
		12, UBFPresentation::MutedTextColor), 2.0f);
	ShopLayout->AddChildToVerticalBox(BasicArmorButton);
	UButton* BasicRingButton = CreateMenuButton(TEXT("ShopBasicRing"), TEXT("ANILLO BÁSICO   ·   850 ORO"));
	BasicRingButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyBasicRingClicked);
	ShopLayout->AddChildToVerticalBox(BasicRingButton);
	UButton* BasicNecklaceButton = CreateMenuButton(TEXT("ShopBasicNecklace"), TEXT("COLLAR BÁSICO   ·   1,500 ORO"));
	BasicNecklaceButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyBasicNecklaceClicked);
	ShopLayout->AddChildToVerticalBox(BasicNecklaceButton);
	UBFPresentation::AddPageLine(ShopLayout, CreateText(TEXT("EventShopSection"), TEXT("EVENTO  //  PUNTOS DE EVENTO"),
		12, UBFPresentation::GoldColor, true), 4.0f);
	UButton* EventRingButton = CreateMenuButton(TEXT("ShopEventRing"), TEXT("ANILLO DE EVENTO   ·   500 PUNTOS"));
	EventRingButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyEventRingClicked);
	ShopLayout->AddChildToVerticalBox(EventRingButton);
	UButton* EventNecklaceButton = CreateMenuButton(TEXT("ShopEventNecklace"), TEXT("COLLAR DE EVENTO   ·   800 PUNTOS"));
	EventNecklaceButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyEventNecklaceClicked);
	ShopLayout->AddChildToVerticalBox(EventNecklaceButton);
	UButton* EventFrameButton = CreateMenuButton(TEXT("ShopEventFrame"), TEXT("MARCO DE PERFIL   ·   350 PUNTOS"));
	EventFrameButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyEventFrameClicked);
	ShopLayout->AddChildToVerticalBox(EventFrameButton);
	ShopConfirmationPanel = CreatePanel(TEXT("ShopConfirmationPanel"), UBFPresentation::SubtlePanelColor);
	ShopConfirmationPanel->SetPadding(FMargin(12.0f));
	UVerticalBox* ShopConfirmationLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopConfirmationLayout"));
	ShopConfirmationText = CreateText(TEXT("ShopConfirmationText"), TEXT(""), 13, UBFPresentation::TextColor, true);
	UBFPresentation::AddPageLine(ShopConfirmationLayout, ShopConfirmationText, 8.0f);
	UHorizontalBox* ShopConfirmationButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopConfirmationButtons"));
	UButton* ConfirmShopPurchaseButton = CreateMenuButton(TEXT("ConfirmShopPurchase"), TEXT("CONFIRMAR"));
	ConfirmShopPurchaseButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnConfirmShopPurchaseClicked);
	UButton* CancelShopPurchaseButton = CreateMenuButton(TEXT("CancelShopPurchase"), TEXT("CANCELAR"));
	CancelShopPurchaseButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCancelShopPurchaseClicked);
	ShopConfirmationButtons->AddChildToHorizontalBox(ConfirmShopPurchaseButton);
	ShopConfirmationButtons->AddChildToHorizontalBox(CancelShopPurchaseButton);
	ShopConfirmationLayout->AddChildToVerticalBox(ShopConfirmationButtons);
	ShopConfirmationPanel->SetContent(ShopConfirmationLayout);
	ShopConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	ShopLayout->AddChildToVerticalBox(ShopConfirmationPanel);
	ShopPurchaseResultText = CreateText(TEXT("ShopPurchaseResult"), TEXT(""), 13, UBFPresentation::TextColor, true);
	UBFPresentation::AddPageLine(ShopLayout, ShopPurchaseResultText, 10.0f);
	AddPage(ShopPage);

	UVerticalBox* GachaPage = CreatePage(TEXT("GachaPage"), TEXT("GACHA"),
		TEXT("Acumula tickets para futuras invocaciones y recompensas."));
	UBorder* GachaBalancePanel = CreatePanel(TEXT("GachaBalancePanel"), UBFPresentation::SubtlePanelColor);
	GachaBalancePanel->SetPadding(FMargin(20.0f, 18.0f));
	GachaSummaryText = CreateText(TEXT("GachaSummary"), TEXT(""), 20, UBFPresentation::GoldColor, true);
	GachaBalancePanel->SetContent(GachaSummaryText);
	if (UVerticalBoxSlot* GachaBalanceSlot = GachaPage->AddChildToVerticalBox(GachaBalancePanel))
	{
		GachaBalanceSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}
	UBFPresentation::AddPageLine(GachaPage, CreateText(TEXT("GachaHint"),
		TEXT("Los tickets se guardan en tu perfil local. El gasto de tickets todavía no está habilitado; no se consumirá ninguno desde este apartado."),
		14, UBFPresentation::MutedTextColor), 10.0f);
	UBFPresentation::AddPageLine(GachaPage, CreateText(TEXT("GachaSources"),
		TEXT("Puedes reunir tickets con los códigos OPENBETALESTGO y RPMODSGAMESBONUS."),
		13, UBFPresentation::TextColor));
	AddPage(GachaPage);

	UVerticalBox* CodesPage = CreatePage(TEXT("CodesPage"), TEXT("CÓDIGOS DE REGALO"),
		TEXT("Cada código se puede canjear una sola vez por perfil local."));
	UScrollBox* CodesScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CodesScroll"));
	CodesScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* CodesLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CodesLayout"));
	CodesScroll->AddChild(CodesLayout);
	if (UVerticalBoxSlot* CodesScrollSlot = CodesPage->AddChildToVerticalBox(CodesScroll))
	{
		CodesScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UBorder* InputPanel = CreatePanel(TEXT("GiftInputPanel"), UBFPresentation::SubtlePanelColor);
	GiftCodeInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("GiftCodeInput"));
	GiftCodeInput->SetHintText(FText::FromString(TEXT("ESCRIBE TU CÓDIGO")));
	FEditableTextBoxStyle GiftCodeStyle = GiftCodeInput->GetWidgetStyle();
	GiftCodeStyle.BackgroundImageNormal = FSlateColorBrush(FLinearColor(0.015f, 0.022f, 0.035f, 0.92f));
	GiftCodeStyle.BackgroundImageHovered = FSlateColorBrush(FLinearColor(0.05f, 0.065f, 0.09f, 0.96f));
	GiftCodeStyle.BackgroundImageFocused = FSlateColorBrush(FLinearColor(0.12f, 0.025f, 0.045f, 0.98f));
	GiftCodeStyle.SetForegroundColor(FSlateColor(UBFPresentation::TextColor));
	GiftCodeStyle.SetFocusedForegroundColor(FSlateColor(UBFPresentation::TextColor));
	GiftCodeStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 17));
	GiftCodeStyle.SetPadding(FMargin(18.0f, 12.0f));
	GiftCodeInput->SetWidgetStyle(GiftCodeStyle);
	GiftCodeInput->SetForegroundColor(UBFPresentation::TextColor);
	InputPanel->SetContent(GiftCodeInput);
	if (UVerticalBoxSlot* InputPanelSlot = CodesLayout->AddChildToVerticalBox(InputPanel))
	{
		InputPanelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	UButton* RedeemButton = CreateMenuButton(TEXT("RedeemGiftButton"), TEXT("CANJEAR CÓDIGO"));
	RedeemButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnRedeemClicked);
	if (UVerticalBoxSlot* RedeemButtonSlot = CodesLayout->AddChildToVerticalBox(RedeemButton))
	{
		RedeemButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}
	GiftCodeResultText = CreateText(TEXT("GiftCodeResult"), TEXT(""), 15, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(CodesLayout, GiftCodeResultText, 12.0f);
	UBorder* AvailableCodesPanel = CreatePanel(TEXT("AvailableCodesPanel"), UBFPresentation::SubtlePanelColor);
	AvailableCodesPanel->SetPadding(FMargin(14.0f, 12.0f));
	UTextBlock* AvailableCodesText = CreateText(TEXT("AvailableCodesText"),
		TEXT("CÓDIGOS DISPONIBLES  ·  01/10/2026\nOPENWELCOME2       +10,000 ORO  ·  +500 EVENTO\nINEEDMONEY21K       +21,000 ORO  ·  +250 EVENTO\nOPENBETALESTGO      +15,000 ORO  ·  +1,000 EVENTO  ·  +10 TICKETS\nMAKEONLYFAI23       1 ANILLO  ·  1 COLLAR ALEATORIOS  ·  +500 EVENTO\nRPMODSGAMESBONUS    +30,000 ORO  ·  +2,000 EVENTO  ·  +100 TICKETS"),
		12, UBFPresentation::TextColor);
	AvailableCodesText->SetAutoWrapText(true);
	AvailableCodesPanel->SetContent(AvailableCodesText);
	if (UVerticalBoxSlot* AvailableCodesSlot = CodesLayout->AddChildToVerticalBox(AvailableCodesPanel))
	{
		AvailableCodesSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	UBFPresentation::AddPageLine(CodesLayout, CreateText(TEXT("CodesHint"),
		TEXT("Las recompensas de oro, Puntos de Evento, tickets y objetos se guardan inmediatamente."),
		14, UBFPresentation::MutedTextColor));
	AddPage(CodesPage);

	UVerticalBox* SettingsPage = CreatePage(TEXT("SettingsPage"), TEXT("OPTIONS"),
		TEXT("Rendimiento y preferencias de pantalla."));
	auto AddSettingsCycleButton = [this](UHorizontalBox* Row, const FName& Name, const FString& Label)
	{
		UButton* Button = CreateMenuButton(Name, Label);
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("_Size"))));
		ButtonSize->SetWidthOverride(38.0f);
		ButtonSize->SetHeightOverride(38.0f);
		ButtonSize->SetContent(Button);
		Row->AddChildToHorizontalBox(ButtonSize);
		return Button;
	};
	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("QualityLabel"), TEXT("CALIDAD GRÁFICA"),
		12, UBFPresentation::GoldColor, true), 4.0f);
	UHorizontalBox* QualityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QualityRow"));
	QualityValueText = CreateText(TEXT("QualityValue"), TEXT("LOW"), 15, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* ValueSlot = QualityRow->AddChildToHorizontalBox(QualityValueText))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	AddSettingsCycleButton(QualityRow, TEXT("QualityPrevious"), TEXT("‹"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousQualityClicked);
	AddSettingsCycleButton(QualityRow, TEXT("QualityNext"), TEXT("›"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextQualityClicked);
	if (UVerticalBoxSlot* QualitySlot = SettingsPage->AddChildToVerticalBox(QualityRow))
	{
		QualitySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 9.0f));
	}

	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("FrameLimitLabel"), TEXT("LÍMITE DE FPS"),
		12, UBFPresentation::GoldColor, true), 4.0f);
	UHorizontalBox* FrameLimitRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FrameLimitRow"));
	FrameLimitValueText = CreateText(TEXT("FrameLimitValue"), TEXT("60 FPS"), 15, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* ValueSlot = FrameLimitRow->AddChildToHorizontalBox(FrameLimitValueText))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	AddSettingsCycleButton(FrameLimitRow, TEXT("FrameLimitPrevious"), TEXT("‹"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousFrameLimitClicked);
	AddSettingsCycleButton(FrameLimitRow, TEXT("FrameLimitNext"), TEXT("›"))->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextFrameLimitClicked);
	if (UVerticalBoxSlot* FrameLimitSlot = SettingsPage->AddChildToVerticalBox(FrameLimitRow))
	{
		FrameLimitSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 9.0f));
	}

	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("VSyncLabel"), TEXT("SINCRONIZACIÓN VERTICAL"),
		12, UBFPresentation::GoldColor, true), 4.0f);
	UHorizontalBox* VSyncRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VSyncRow"));
	VSyncValueText = CreateText(TEXT("VSyncValue"), TEXT("ACTIVADO"), 15, UBFPresentation::TextColor, true);
	if (UHorizontalBoxSlot* ValueSlot = VSyncRow->AddChildToHorizontalBox(VSyncValueText))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}
	UButton* VSyncButton = CreateMenuButton(TEXT("VSyncButton"), TEXT("CAMBIAR"));
	VSyncButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnToggleVSyncClicked);
	if (USizeBox* VSyncButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VSyncButtonSize")))
	{
		VSyncButtonSize->SetWidthOverride(120.0f);
		VSyncButtonSize->SetHeightOverride(38.0f);
		VSyncButtonSize->SetContent(VSyncButton);
		VSyncRow->AddChildToHorizontalBox(VSyncButtonSize);
	}
	if (UVerticalBoxSlot* VSyncSlot = SettingsPage->AddChildToVerticalBox(VSyncRow))
	{
		VSyncSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}
	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("ControlsLabel"), TEXT("CONTROLES DE HABILIDADES"),
		12, UBFPresentation::GoldColor, true), 4.0f);
	auto AddInputBindingRow = [this, SettingsPage](const FName& RowName, const FString& Label,
		TObjectPtr<UTextBlock>& KeyText) -> UButton*
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		UTextBlock* LabelText = CreateText(FName(*(RowName.ToString() + TEXT("_Label"))), Label,
			13, UBFPresentation::TextColor, true);
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		KeyText = CreateText(FName(*(RowName.ToString() + TEXT("_Key"))), TEXT("Q"),
			13, UBFPresentation::GoldColor, true);
		if (UHorizontalBoxSlot* KeySlot = Row->AddChildToHorizontalBox(KeyText))
		{
			KeySlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			KeySlot->SetPadding(FMargin(8.0f, 0.0f, 14.0f, 0.0f));
			KeySlot->SetVerticalAlignment(VAlign_Center);
		}
		UButton* ChangeButton = CreateMenuButton(FName(*(RowName.ToString() + TEXT("_Change"))), TEXT("CAMBIAR"));
		if (USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(RowName.ToString() + TEXT("_ButtonSize")))))
		{
			ButtonSize->SetWidthOverride(108.0f);
			ButtonSize->SetHeightOverride(38.0f);
			ButtonSize->SetContent(ChangeButton);
			Row->AddChildToHorizontalBox(ButtonSize);
		}
		if (UVerticalBoxSlot* RowSlot = SettingsPage->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
		return ChangeButton;
	};
	AddInputBindingRow(TEXT("PrimaryBindingRow"), TEXT("HABILIDAD PRIMARIA"), PrimaryBindingText)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnRebindPrimaryClicked);
	AddInputBindingRow(TEXT("SecondaryBindingRow"), TEXT("HABILIDAD SECUNDARIA"), SecondaryBindingText)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnRebindSecondaryClicked);
	AddInputBindingRow(TEXT("UltimateBindingRow"), TEXT("ULTIMATE"), UltimateBindingText)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnRebindUltimateClicked);
	UButton* ResetBindingsButton = CreateMenuButton(TEXT("ResetBindingsButton"), TEXT("RESTABLECER CONTROLES"));
	ResetBindingsButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnResetInputBindingsClicked);
	if (UVerticalBoxSlot* ResetBindingsSlot = SettingsPage->AddChildToVerticalBox(ResetBindingsButton))
	{
		ResetBindingsSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 8.0f));
	}
	InputBindingResultText = CreateText(TEXT("InputBindingResult"), TEXT("Las asignaciones se guardan en este perfil local."),
		12, UBFPresentation::MutedTextColor);
	UBFPresentation::AddPageLine(SettingsPage, InputBindingResultText, 8.0f);
	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("SettingsHint"),
		TEXT("LOW reduce el uso de GPU. Los cambios se guardan localmente y se aplican de inmediato."),
		13, UBFPresentation::MutedTextColor));
	AddPage(SettingsPage);
	RefreshSettingsText();
	RefreshInputBindingText();
}

void UUBFPresentationWidget::PositionWidget(UWidget* Widget, const FAnchors& Anchors,
	const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
{
	UBFPresentation::AddToCanvas(RootCanvas, Widget, Anchors, Alignment, Position, Size, ZOrder);
}

void UUBFPresentationWidget::FillWidget(UWidget* Widget, const FAnchors& Anchors, int32 ZOrder)
{
	UBFPresentation::FillCanvas(RootCanvas, Widget, Anchors, ZOrder);
}

bool UUBFPresentationWidget::LoadLogo()
{
	const FString LogoPath = FPaths::ProjectContentDir() / TEXT("Presentation/Images/logo.png");
	TArray<uint8> Compressed;
	if (!FFileHelper::LoadFileToArray(Compressed, *LogoPath))
	{
		ShowDevelopmentError(FString::Printf(TEXT("No se pudo leer logo.png:\n%s"), *LogoPath));
		return false;
	}

	IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Png = ImageWrapper.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Pixels;
	if (!Png.IsValid() || !Png->SetCompressed(Compressed.GetData(), Compressed.Num()) ||
		!Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
	{
		ShowDevelopmentError(TEXT("logo.png existe pero no se pudo decodificar."));
		return false;
	}

	LogoTexture = UTexture2D::CreateTransient(Png->GetWidth(), Png->GetHeight(), PF_B8G8R8A8);
	if (!LogoTexture)
	{
		ShowDevelopmentError(TEXT("No se pudo crear la textura temporal de logo.png."));
		return false;
	}
	LogoTexture->SRGB = true;
	void* TextureData = LogoTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num());
	LogoTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	LogoTexture->UpdateResource();
	LogoImage->SetBrushFromTexture(LogoTexture, false);
	bLogoReady = true;
	return true;
}

void UUBFPresentationWidget::ResetPresentationResources()
{
	for (UMediaPlayer* Player : { IntroVideoPlayer.Get(), BackgroundPlayer.Get() })
	{
		if (Player)
		{
			Player->Close();
		}
	}
	for (UAudioComponent* Component : { IntroAudioComponent.Get(), LoopAudioComponent.Get(), TitleVoiceAudioComponent.Get() })
	{
		if (Component)
		{
			Component->OnAudioFinished.RemoveDynamic(this, &UUBFPresentationWidget::OnIntroAudioEnded);
			Component->OnAudioFinished.RemoveDynamic(this, &UUBFPresentationWidget::OnTitleVoiceEnded);
			Component->Stop();
		}
	}
	if (PresentationAssetLoadHandle.IsValid())
	{
		PresentationAssetLoadHandle->ReleaseHandle();
		PresentationAssetLoadHandle.Reset();
	}

	IntroVideoSource = nullptr;
	BackgroundSource = nullptr;
	IntroVideoPlayer = nullptr;
	BackgroundPlayer = nullptr;
	IntroTexture = nullptr;
	BackgroundTexture = nullptr;
	IntroAudioComponent = nullptr;
	LoopAudioComponent = nullptr;
	TitleVoiceAudioComponent = nullptr;
	IntroSound = nullptr;
	LoopSound = nullptr;
	TitleVoiceSound = nullptr;
	bLogoReady = false;
	bSoundAssetsReady = false;
	bIntroVideoReady = false;
	bBackgroundVideoReady = false;
	bBackgroundPrerolled = false;
	bIntroPlaybackStarted = false;
	bIntroAudioStarted = false;
	bMenuStarted = false;
	bTitleVoiceStarted = false;
	bInteractiveMenuShown = false;
	BackgroundRevealElapsed = 0.0f;
}

void UUBFPresentationWidget::PrepareResources()
{
	bTearingDown = false;
	ResetPresentationResources();
	State = EPresentationState::Preloading;
	MenuElapsed = 0.0f;
	RewardToastRemaining = 0.0f;

	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	VideoImage->SetVisibility(ESlateVisibility::Collapsed);
	VideoTint->SetVisibility(ESlateVisibility::Collapsed);
	DevelopmentErrorText->SetVisibility(ESlateVisibility::Collapsed);
	RetryButton->SetVisibility(ESlateVisibility::Collapsed);
	RewardToastText->SetVisibility(ESlateVisibility::Collapsed);
	StatusText->SetText(FText::FromString(TEXT("PREPARANDO PRESENTACIÓN UBF")));
	StatusText->SetVisibility(ESlateVisibility::Visible);
	// The brand mark belongs to the reveal phase after the intro.  It must never
	// float or appear while resources are loading or while intro.mp4 is playing.
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* LogoSlot = Cast<UCanvasPanelSlot>(LogoImage->Slot))
	{
		LogoSlot->SetAnchors(FAnchors(0.5f, 0.37f));
		LogoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LogoSlot->SetPosition(FVector2D::ZeroVector);
		LogoSlot->SetSize(FVector2D(520.0f, 292.0f));
	}

	if (!LoadLogo() || State == EPresentationState::DevelopmentError)
	{
		return;
	}

	StartMediaPreload();
}

void UUBFPresentationWidget::StartMediaPreload()
{
	const FString IntroVideoPath = FPaths::ProjectContentDir() / TEXT("Presentation/Videos/intro.mp4");
	const FString BackgroundPath = FPaths::ProjectContentDir() / TEXT("Presentation/Videos/videobackground.mp4");
	if (!IFileManager::Get().FileExists(*IntroVideoPath))
	{
		ShowDevelopmentError(FString::Printf(TEXT("Recurso requerido ausente: intro.mp4\n%s"), *IntroVideoPath));
		return;
	}
	if (!IFileManager::Get().FileExists(*BackgroundPath))
	{
		ShowDevelopmentError(FString::Printf(TEXT("Recurso requerido ausente: videobackground.mp4\n%s"), *BackgroundPath));
		return;
	}

	TArray<FSoftObjectPath> SoundAssets = {
		UBFPresentation::IntroMusicPath,
		UBFPresentation::LoopMusicPath,
		UBFPresentation::TitleVoicePath
	};
	PresentationAssetLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoundAssets,
		FStreamableDelegate::CreateUObject(this, &UUBFPresentationWidget::OnSoundAssetsLoaded),
		FStreamableManager::AsyncLoadHighPriority);
	if (!PresentationAssetLoadHandle.IsValid())
	{
		ShowDevelopmentError(TEXT("No se pudo iniciar la carga asíncrona del audio de presentación."));
		return;
	}

	IntroVideoSource = UBFPresentation::MakeFileSource(this, IntroVideoPath);
	IntroVideoPlayer = UBFPresentation::MakePlayer(this);
	IntroTexture = UBFPresentation::MakeTexture(this, IntroVideoPlayer);
	IntroVideoPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnIntroVideoOpened);
	IntroVideoPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnIntroVideoOpenFailed);
	IntroVideoPlayer->OnEndReached.AddDynamic(this, &UUBFPresentationWidget::OnIntroVideoEnded);
	IntroVideoPlayer->OnPlaybackResumed.AddDynamic(this, &UUBFPresentationWidget::OnIntroPlaybackResumed);
	if (!IntroVideoPlayer->OpenSource(IntroVideoSource))
	{
		ShowDevelopmentError(TEXT("Unreal rechazó intro.mp4 al abrirlo."));
		return;
	}

	BackgroundSource = UBFPresentation::MakeFileSource(this, BackgroundPath);
	BackgroundPlayer = UBFPresentation::MakePlayer(this);
	BackgroundTexture = UBFPresentation::MakeTexture(this, BackgroundPlayer);
	BackgroundPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnBackgroundOpened);
	BackgroundPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnBackgroundOpenFailed);
	BackgroundPlayer->OnPlaybackResumed.AddDynamic(this, &UUBFPresentationWidget::OnBackgroundPlaybackResumed);
	if (!BackgroundPlayer->OpenSource(BackgroundSource))
	{
		ShowDevelopmentError(TEXT("Unreal rechazó videobackground.mp4 al abrirlo."));
	}
}

void UUBFPresentationWidget::OnSoundAssetsLoaded()
{
	if (State != EPresentationState::Preloading)
	{
		return;
	}

	IntroSound = Cast<USoundWave>(UBFPresentation::IntroMusicPath.ResolveObject());
	LoopSound = Cast<USoundWave>(UBFPresentation::LoopMusicPath.ResolveObject());
	TitleVoiceSound = Cast<USoundWave>(UBFPresentation::TitleVoicePath.ResolveObject());
	if (!IntroSound || !LoopSound || !TitleVoiceSound)
	{
		ShowDevelopmentError(TEXT("No se pudo cargar uno de los SoundWaves requeridos: musicintro, musicintrobucle o introdution_ubf."));
		return;
	}

	LoopSound->bLooping = true;
	IntroAudioComponent = UGameplayStatics::CreateSound2D(this, IntroSound, UBFPresentation::IntroAndLoopVolume, 1.0f, 0.0f, nullptr, false, false);
	LoopAudioComponent = UGameplayStatics::CreateSound2D(this, LoopSound, UBFPresentation::IntroAndLoopVolume, 1.0f, 0.0f, nullptr, false, false);
	TitleVoiceAudioComponent = UGameplayStatics::CreateSound2D(this, TitleVoiceSound, UBFPresentation::TitleVoiceVolume, 1.0f, 0.0f, nullptr, false, false);
	if (!IntroAudioComponent || !LoopAudioComponent || !TitleVoiceAudioComponent)
	{
		ShowDevelopmentError(TEXT("No se pudieron preparar los componentes de audio de presentación."));
		return;
	}
	IntroAudioComponent->OnAudioFinished.AddDynamic(this, &UUBFPresentationWidget::OnIntroAudioEnded);
	TitleVoiceAudioComponent->OnAudioFinished.AddDynamic(this, &UUBFPresentationWidget::OnTitleVoiceEnded);
	bSoundAssetsReady = true;
	TryStartIntro();
}

bool UUBFPresentationWidget::ValidateVideo(UMediaPlayer* Player, const FString& Label, bool bMustReachIntroCut)
{
	if (!Player || !Player->IsReady() || Player->GetNumTracks(EMediaPlayerTrack::Video) < 1)
	{
		ShowDevelopmentError(FString::Printf(TEXT("%s no tiene una pista de vídeo legible."), *Label));
		return false;
	}
	if (bMustReachIntroCut && Player->GetDuration() + UBFPresentation::MediaDurationTolerance < IntroCutTime)
	{
		ShowDevelopmentError(FString::Printf(TEXT("%s termina antes de 00:00:13:06."), *Label));
		return false;
	}
	return true;
}

void UUBFPresentationWidget::OnIntroVideoOpened(FString OpenedUrl)
{
	if (State != EPresentationState::Preloading || !ValidateVideo(IntroVideoPlayer, TEXT("intro.mp4"), true))
	{
		return;
	}

	IntroVideoPlayer->SetLooping(false);
	IntroVideoPlayer->SelectTrack(EMediaPlayerTrack::Audio, INDEX_NONE);
	IntroVideoPlayer->Seek(FTimespan::Zero());
	bIntroVideoReady = true;
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("intro.mp4 preparado: %s"), *OpenedUrl);
	TryStartIntro();
}

void UUBFPresentationWidget::OnIntroVideoOpenFailed(FString FailedUrl)
{
	if (!bTearingDown)
	{
		ShowDevelopmentError(FString::Printf(TEXT("Falló la apertura de intro.mp4:\n%s"), *FailedUrl));
	}
}

void UUBFPresentationWidget::OnIntroVideoEnded()
{
	if (State == EPresentationState::Intro && IntroVideoPlayer)
	{
		const FTimespan ReachedTime = IntroVideoPlayer->GetTime();
		const FTimespan ReportedDuration = IntroVideoPlayer->GetDuration();
		if (ReachedTime >= IntroCutTime || ReachedTime + UBFPresentation::MediaDurationTolerance >= IntroCutTime ||
			ReportedDuration + UBFPresentation::MediaDurationTolerance >= IntroCutTime)
		{
			if (ReachedTime < IntroCutTime)
			{
				UE_LOG(UBFPresentation::LogUBFPresentation, Warning,
					TEXT("WmfMedia redondeó el final de intro.mp4 a %.3f s; se conserva el corte al final del clip de 00:00:13:06."),
					ReachedTime.GetTotalSeconds());
			}
			BeginMenu();
		}
		else
		{
			ShowDevelopmentError(TEXT("intro.mp4 finalizó antes de alcanzar 00:00:13:06."));
		}
	}
}

void UUBFPresentationWidget::OnIntroPlaybackResumed()
{
	if (State == EPresentationState::Intro && bIntroPlaybackStarted)
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Log,
			TEXT("intro.mp4 confirmó reproducción; musicintro queda programada para el segundo %.3f."),
			IntroMusicStartTime.GetTotalSeconds());
	}
}

void UUBFPresentationWidget::OnBackgroundOpened(FString OpenedUrl)
{
	if (State != EPresentationState::Preloading || !ValidateVideo(BackgroundPlayer, TEXT("videobackground.mp4"), false))
	{
		return;
	}

	BackgroundPlayer->SetLooping(true);
	BackgroundPlayer->SelectTrack(EMediaPlayerTrack::Audio, INDEX_NONE);
	BackgroundPlayer->Seek(FTimespan::Zero());
	bBackgroundVideoReady = true;

	// Decode a real frame while hidden.  The readiness check below pauses it as soon as
	// MediaPlayer reports progress; no fixed-delay guess is used.
	if (!BackgroundPlayer->Play())
	{
		ShowDevelopmentError(TEXT("videobackground.mp4 se abrió pero no pudo iniciar su precarga."));
		return;
	}
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("videobackground.mp4 preparado: %s"), *OpenedUrl);
}

void UUBFPresentationWidget::OnBackgroundOpenFailed(FString FailedUrl)
{
	if (!bTearingDown)
	{
		ShowDevelopmentError(FString::Printf(TEXT("Falló la apertura de videobackground.mp4:\n%s"), *FailedUrl));
	}
}

void UUBFPresentationWidget::OnBackgroundPlaybackResumed()
{
	if (State == EPresentationState::MenuReveal && !bTitleVoiceStarted)
	{
		StartTitleVoice();
	}
}

void UUBFPresentationWidget::TryStartIntro()
{
	if (State != EPresentationState::Preloading || !bLogoReady || !bSoundAssetsReady ||
		!bIntroVideoReady || !bBackgroundVideoReady || !bBackgroundPrerolled)
	{
		return;
	}

	VideoImage->SetBrushResourceObject(IntroTexture);
	VideoImage->SetVisibility(ESlateVisibility::Visible);
	VideoTint->SetVisibility(ESlateVisibility::Collapsed);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);

	IntroVideoPlayer->Seek(FTimespan::Zero());
	State = EPresentationState::Intro;
	bIntroPlaybackStarted = true;
	bIntroAudioStarted = false;
	if (!IntroVideoPlayer->Play())
	{
		bIntroPlaybackStarted = false;
		ShowDevelopmentError(TEXT("intro.mp4 estaba preparado pero no pudo iniciar la reproducción."));
		return;
	}
}

void UUBFPresentationWidget::StartIntroAudio()
{
	if (State != EPresentationState::Intro || bIntroAudioStarted || !IntroAudioComponent || !IntroVideoPlayer)
	{
		return;
	}

	const float MediaOffsetSeconds = FMath::Max(0.0f, static_cast<float>(
		(IntroVideoPlayer->GetTime() - IntroMusicStartTime).GetTotalSeconds()));
	IntroAudioComponent->SetVolumeMultiplier(UBFPresentation::IntroAndLoopVolume);
	IntroAudioComponent->Play(MediaOffsetSeconds);
	bIntroAudioStarted = true;
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Presentación sincronizada: intro.mp4 alcanzó %.3f s; musicintro inició desde %.3f s a 60 %% (marca de inicio %.3f s)."),
		IntroVideoPlayer->GetTime().GetTotalSeconds(), MediaOffsetSeconds, IntroMusicStartTime.GetTotalSeconds());
}

void UUBFPresentationWidget::BeginMenu()
{
	if (State != EPresentationState::Intro || bMenuStarted || !BackgroundPlayer)
	{
		return;
	}
	bMenuStarted = true;

	if (IntroVideoPlayer)
	{
		IntroVideoPlayer->Pause();
	}
	BackgroundPlayer->Seek(FTimespan::Zero());
	VideoImage->SetBrushResourceObject(BackgroundTexture);
	VideoImage->SetVisibility(ESlateVisibility::Visible);
	VideoTint->SetVisibility(ESlateVisibility::Visible);
	State = EPresentationState::MenuReveal;
	MenuElapsed = 0.0f;
	BackgroundRevealElapsed = 0.0f;
	bTitleVoiceStarted = false;
	VideoImage->SetRenderOpacity(1.0f);
	VideoImage->SetRenderTranslation(FVector2D::ZeroVector);
	VideoTint->SetBrushColor(FLinearColor(1.0f, 0.18f, 0.24f, 0.78f));
	if (!BackgroundPlayer->Play())
	{
		ShowDevelopmentError(TEXT("No se pudo iniciar videobackground.mp4 durante la transición al menú."));
		return;
	}

	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	LogoImage->SetVisibility(ESlateVisibility::Visible);
	LogoImage->SetRenderOpacity(0.0f);
	LogoImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(0.92f), FVector2D::ZeroVector, 0.0f));
	if (UCanvasPanelSlot* LogoSlot = Cast<UCanvasPanelSlot>(LogoImage->Slot))
	{
		LogoSlot->SetAnchors(FAnchors(0.5f, 0.37f));
		LogoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LogoSlot->SetPosition(FVector2D::ZeroVector);
		LogoSlot->SetSize(FVector2D(520.0f, 292.0f));
	}
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Fase de revelación preparada: se iniciará la locución al confirmar videobackground.mp4."));
}

void UUBFPresentationWidget::StartTitleVoice()
{
	if (State != EPresentationState::MenuReveal || bTitleVoiceStarted || !TitleVoiceAudioComponent || !BackgroundPlayer)
	{
		return;
	}

	const float MediaOffsetSeconds = FMath::Max(0.0f, static_cast<float>(BackgroundPlayer->GetTime().GetTotalSeconds()));
	TitleVoiceAudioComponent->SetVolumeMultiplier(UBFPresentation::TitleVoiceVolume);
	TitleVoiceAudioComponent->Play(MediaOffsetSeconds);
	bTitleVoiceStarted = true;
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Fase de revelación sincronizada: videobackground.mp4 + introdution_ubf.mp3 con offset %.3f s."),
		MediaOffsetSeconds);
}

void UUBFPresentationWidget::EnterInteractiveMenu()
{
	if (State != EPresentationState::MenuReveal || bInteractiveMenuShown)
	{
		return;
	}

	bInteractiveMenuShown = true;
	State = EPresentationState::Menu;
	MenuElapsed = 0.0f;
	if (UCanvasPanelSlot* LogoSlot = Cast<UCanvasPanelSlot>(LogoImage->Slot))
	{
		LogoSlot->SetAnchors(FAnchors(0.045f, 0.045f));
		LogoSlot->SetAlignment(FVector2D::ZeroVector);
		LogoSlot->SetPosition(FVector2D::ZeroVector);
		LogoSlot->SetSize(FVector2D(325.0f, 166.0f));
	}
	LogoImage->SetRenderOpacity(1.0f);
	LogoImage->SetRenderTransform(FWidgetTransform());
	MenuRoot->SetVisibility(ESlateVisibility::Visible);
	SetMenuPage(EMenuPage::Home);
	RefreshPlayerData();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Fase interactiva del menú iniciada."));
}

void UUBFPresentationWidget::StartLoopMusic()
{
	if (bTearingDown || State == EPresentationState::DevelopmentError || !LoopAudioComponent ||
		LoopAudioComponent->IsPlaying())
	{
		return;
	}
	LoopAudioComponent->SetVolumeMultiplier(UBFPresentation::IntroAndLoopVolume);
	LoopAudioComponent->Play(0.0f);
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("musicintro finalizó; musicintrobucle inició a 60 %%."));
}

void UUBFPresentationWidget::OnIntroAudioEnded()
{
	StartLoopMusic();
}

void UUBFPresentationWidget::OnTitleVoiceEnded()
{
	EnterInteractiveMenu();
}

void UUBFPresentationWidget::ShowDevelopmentError(const FString& Reason)
{
	if (bTearingDown)
	{
		return;
	}

	State = EPresentationState::DevelopmentError;
	bMenuStarted = false;
	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	VideoImage->SetVisibility(ESlateVisibility::Collapsed);
	VideoTint->SetVisibility(ESlateVisibility::Collapsed);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	DevelopmentErrorText->SetText(FText::FromString(FString::Printf(
		TEXT("ASSET ERROR // PRESENTACIÓN DETENIDA\n%s\n\nRevisa Content/Presentation y vuelve a intentarlo."), *Reason)));
	DevelopmentErrorText->SetVisibility(ESlateVisibility::Visible);
	RetryButton->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("%s"), *Reason);
}

void UUBFPresentationWidget::SetMenuPage(EMenuPage NewPage)
{
	if (!PageSwitcher)
	{
		return;
	}

	ActivePage = NewPage;
	PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(NewPage));
	static const TCHAR* PageNames[] = {
		TEXT("UNIVERSAL BREAKING FIGHTERS"),
		TEXT("COMBAT"),
		TEXT("FIGHTERS"),
		TEXT("ARMORY"),
		TEXT("PROFILE"),
		TEXT("SHOP"),
		TEXT("GACHA TICKETS"),
		TEXT("GIFT CODES"),
		TEXT("OPTIONS")
	};
	PageTitleText->SetText(FText::FromString(PageNames[static_cast<int32>(NewPage)]));
	RefreshPlayerData();
}

void UUBFPresentationWidget::RefreshPlayerData()
{
	const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance() ?
		GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	const UUBFPlayerSaveGame* Data = PlayerData ? PlayerData->GetPlayerData() : nullptr;
	if (!Data)
	{
		return;
	}
	const int32 DisplayGold = bCurrencyRewardAnimating ? CurrencyDisplayedGold : Data->Gold;
	const int32 DisplayEventPoints = bCurrencyRewardAnimating ? CurrencyDisplayedEventPoints : Data->EventPoints;
	const int32 DisplayGachaTickets = bCurrencyRewardAnimating ? CurrencyDisplayedGachaTickets : Data->GachaTickets;

	const int32 NextLevelExperience = FMath::Max(1000, Data->AccountLevel * 1000);
	if (AccountSummaryText)
	{
		AccountSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  //  LV.%d\nXP %d / %d\nORO %s   ·   EVENTO %s   ·   TICKETS %d"),
			*Data->PlayerName.ToUpper(), Data->AccountLevel, Data->Experience, NextLevelExperience,
			*FText::AsNumber(DisplayGold).ToString(), *FText::AsNumber(DisplayEventPoints).ToString(), DisplayGachaTickets)));
	}
	if (ProfileSummaryText)
	{
		ProfileSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nLV.%d  //  XP %d / %d\n\nPARTIDAS %d\nVICTORIAS %d  ·  DERROTAS %d  ·  EMPATES %d\nDAÑO %s  ·  RECIBIDO %s\nKOs %d  ·  CAÍDAS %d  ·  MEJOR COMBO %d HIT\nPERSONAJE MÁS USADO — PENDIENTE"),
			*Data->PlayerName.ToUpper(), Data->AccountLevel, Data->Experience, NextLevelExperience,
			Data->MatchesPlayed, Data->MatchesWon, Data->MatchesLost, Data->MatchesDrawn,
			*FText::AsNumber(FMath::RoundToInt(Data->TotalDamageDealt)).ToString(),
			*FText::AsNumber(FMath::RoundToInt(Data->TotalDamageReceived)).ToString(),
			Data->TotalKnockouts, Data->TotalDeaths, Data->BestComboHits)));
	}
	if (InventorySummaryText)
	{
		const FString Items = Data->InventoryItems.IsEmpty()
			? TEXT("No tienes objetos todavía. Canjea un código o visita la tienda.")
			: FString::Join(Data->InventoryItems, TEXT("\n• "));
		InventorySummaryText->SetText(FText::FromString(FString::Printf(TEXT("OBJETOS LOCALES (%d)\n• %s"),
			Data->InventoryItems.Num(), *Items)));
	}
	if (GachaSummaryText)
	{
		GachaSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("TICKETS DE GACHA DISPONIBLES: %d"), DisplayGachaTickets)));
	}
	if (ShopBalanceText)
	{
		ShopBalanceText->SetText(FText::FromString(FString::Printf(
			TEXT("ORO  %s     ·     PUNTOS DE EVENTO  %s"),
			*FText::AsNumber(DisplayGold).ToString(), *FText::AsNumber(DisplayEventPoints).ToString())));
	}
}

void UUBFPresentationWidget::ShowRewardToast(const FString& Summary)
{
	if (!RewardToastText)
	{
		return;
	}
	RewardToastText->SetText(FText::FromString(FString::Printf(TEXT("RECOMPENSAS RECIBIDAS\n%s"), *Summary)));
	RewardToastText->SetRenderOpacity(0.0f);
	RewardToastText->SetRenderTranslation(FVector2D(0.0f, -24.0f));
	RewardToastText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, -24.0f), FVector2D(0.92f), FVector2D::ZeroVector, 0.0f));
	RewardToastText->SetVisibility(ESlateVisibility::Visible);
	RewardToastElapsed = 0.0f;
	RewardToastRemaining = 3.5f;
}

void UUBFPresentationWidget::StartCurrencyRewardAnimation(int32 PreviousGold, int32 PreviousEventPoints, int32 PreviousGachaTickets)
{
	const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance() ?
		GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	const UUBFPlayerSaveGame* Data = PlayerData ? PlayerData->GetPlayerData() : nullptr;
	if (!Data)
	{
		return;
	}

	CurrencyStartGold = PreviousGold;
	CurrencyStartEventPoints = PreviousEventPoints;
	CurrencyStartGachaTickets = PreviousGachaTickets;
	CurrencyTargetGold = Data->Gold;
	CurrencyTargetEventPoints = Data->EventPoints;
	CurrencyTargetGachaTickets = Data->GachaTickets;
	CurrencyDisplayedGold = CurrencyStartGold;
	CurrencyDisplayedEventPoints = CurrencyStartEventPoints;
	CurrencyDisplayedGachaTickets = CurrencyStartGachaTickets;
	CurrencyRewardElapsed = 0.0f;
	bCurrencyRewardAnimating = CurrencyTargetGold > CurrencyStartGold ||
		CurrencyTargetEventPoints > CurrencyStartEventPoints ||
		CurrencyTargetGachaTickets > CurrencyStartGachaTickets;
	RefreshPlayerData();
}

void UUBFPresentationWidget::OnPlayClicked()
{
	SetMenuPage(EMenuPage::Play);
}

void UUBFPresentationWidget::OnCharacterClicked()
{
	SetMenuPage(EMenuPage::Character);
}

void UUBFPresentationWidget::OnInventoryClicked()
{
	SetMenuPage(EMenuPage::Inventory);
}

void UUBFPresentationWidget::OnProfileClicked()
{
	SetMenuPage(EMenuPage::Profile);
}

void UUBFPresentationWidget::OnShopClicked()
{
	SetMenuPage(EMenuPage::Shop);
}

void UUBFPresentationWidget::OnGachaClicked()
{
	SetMenuPage(EMenuPage::Gacha);
}

void UUBFPresentationWidget::OnShopBuyBasicArmorClicked()
{
	SelectShopOffer(TEXT("basic_armor"), TEXT("¿Comprar Armadura básica por 1,200 de oro?"));
}

void UUBFPresentationWidget::OnShopBuyBasicRingClicked()
{
	SelectShopOffer(TEXT("basic_ring"), TEXT("¿Comprar Anillo básico por 850 de oro?"));
}

void UUBFPresentationWidget::OnShopBuyBasicNecklaceClicked()
{
	SelectShopOffer(TEXT("basic_necklace"), TEXT("¿Comprar Collar básico por 1,500 de oro?"));
}

void UUBFPresentationWidget::OnShopBuyEventRingClicked()
{
	SelectShopOffer(TEXT("event_ring"), TEXT("¿Canjear 500 Puntos de Evento por un Anillo de evento?"));
}

void UUBFPresentationWidget::OnShopBuyEventNecklaceClicked()
{
	SelectShopOffer(TEXT("event_necklace"), TEXT("¿Canjear 800 Puntos de Evento por un Collar de evento?"));
}

void UUBFPresentationWidget::OnShopBuyEventFrameClicked()
{
	SelectShopOffer(TEXT("event_profile_frame"), TEXT("¿Canjear 350 Puntos de Evento por un Marco de perfil?"));
}

void UUBFPresentationWidget::SelectShopOffer(const FString& ItemId, const FString& ConfirmationText)
{
	if (!ShopConfirmationPanel || !ShopConfirmationText || !ShopPurchaseResultText)
	{
		return;
	}
	PendingShopItemId = ItemId;
	ShopConfirmationText->SetText(FText::FromString(ConfirmationText));
	ShopPurchaseResultText->SetText(FText::GetEmpty());
	ShopConfirmationPanel->SetVisibility(ESlateVisibility::Visible);
}

void UUBFPresentationWidget::OnConfirmShopPurchaseClicked()
{
	UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	if (!PlayerData || PendingShopItemId.IsEmpty())
	{
		return;
	}

	const FUBFShopPurchaseResult Result = PlayerData->PurchasePrototypeShopItem(PendingShopItemId);
	if (ShopPurchaseResultText)
	{
		ShopPurchaseResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded
			? UBFPresentation::GoldColor : UBFPresentation::AccentColor));
		ShopPurchaseResultText->SetText(FText::FromString(Result.Message));
	}
	if (ShopConfirmationPanel)
	{
		ShopConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	PendingShopItemId.Reset();
	RefreshPlayerData();
}

void UUBFPresentationWidget::OnCancelShopPurchaseClicked()
{
	PendingShopItemId.Reset();
	if (ShopConfirmationPanel)
	{
		ShopConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UUBFPresentationWidget::OnCodesClicked()
{
	SetMenuPage(EMenuPage::Codes);
}

void UUBFPresentationWidget::OnSettingsClicked()
{
	SetMenuPage(EMenuPage::Settings);
}

void UUBFPresentationWidget::OnRebindPrimaryClicked()
{
	BeginInputRebind(TEXT("PrimarySkill"));
}

void UUBFPresentationWidget::OnRebindSecondaryClicked()
{
	BeginInputRebind(TEXT("SecondarySkill"));
}

void UUBFPresentationWidget::OnRebindUltimateClicked()
{
	BeginInputRebind(TEXT("Ultimate"));
}

void UUBFPresentationWidget::OnResetInputBindingsClicked()
{
	PendingInputRebindAction = NAME_None;
	UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	const bool bReset = PlayerData && PlayerData->ResetInputBindings();
	if (InputBindingResultText)
	{
		InputBindingResultText->SetText(FText::FromString(bReset
			? TEXT("Controles restablecidos: Q / E / F.")
			: TEXT("No se pudieron guardar los controles predeterminados.")));
		InputBindingResultText->SetColorAndOpacity(FSlateColor(bReset
			? UBFPresentation::GoldColor : UBFPresentation::AccentColor));
	}
	RefreshInputBindingText();
}

void UUBFPresentationWidget::BeginInputRebind(FName ActionId)
{
	PendingInputRebindAction = ActionId;
	if (InputBindingResultText)
	{
		InputBindingResultText->SetText(FText::FromString(TEXT("Presiona una tecla o un botón de ratón. ESC cancela.")));
		InputBindingResultText->SetColorAndOpacity(FSlateColor(UBFPresentation::GoldColor));
	}
	SetKeyboardFocus();
}

void UUBFPresentationWidget::CaptureInputBinding(const FKey& Key)
{
	if (Key == EKeys::Escape)
	{
		PendingInputRebindAction = NAME_None;
		if (InputBindingResultText)
		{
			InputBindingResultText->SetText(FText::FromString(TEXT("Cambio cancelado.")));
		}
		return;
	}
	UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	FString Message;
	const bool bSaved = PlayerData && PlayerData->SetInputBinding(PendingInputRebindAction, Key, Message);
	PendingInputRebindAction = NAME_None;
	if (InputBindingResultText)
	{
		InputBindingResultText->SetText(FText::FromString(Message.IsEmpty()
			? TEXT("No se pudo guardar el control.") : Message));
		InputBindingResultText->SetColorAndOpacity(FSlateColor(bSaved
			? UBFPresentation::GoldColor : UBFPresentation::AccentColor));
	}
	RefreshInputBindingText();
}

void UUBFPresentationWidget::RefreshInputBindingText()
{
	const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	if (!PlayerData)
	{
		return;
	}
	if (PrimaryBindingText)
	{
		PrimaryBindingText->SetText(PlayerData->GetInputBinding(TEXT("PrimarySkill")).GetDisplayName());
	}
	if (SecondaryBindingText)
	{
		SecondaryBindingText->SetText(PlayerData->GetInputBinding(TEXT("SecondarySkill")).GetDisplayName());
	}
	if (UltimateBindingText)
	{
		UltimateBindingText->SetText(PlayerData->GetInputBinding(TEXT("Ultimate")).GetDisplayName());
	}
}

void UUBFPresentationWidget::OnPreviousQualityClicked()
{
	SelectedQualityIndex = SelectedQualityIndex <= 0 ? 2 : SelectedQualityIndex - 1;
	ApplySettingsSelection();
}

void UUBFPresentationWidget::OnNextQualityClicked()
{
	SelectedQualityIndex = (SelectedQualityIndex + 1) % 3;
	ApplySettingsSelection();
}

void UUBFPresentationWidget::OnPreviousFrameLimitClicked()
{
	SelectedFrameLimitIndex = SelectedFrameLimitIndex <= 0
		? UBFPresentation::FrameLimitCount - 1 : SelectedFrameLimitIndex - 1;
	ApplySettingsSelection();
}

void UUBFPresentationWidget::OnNextFrameLimitClicked()
{
	SelectedFrameLimitIndex = (SelectedFrameLimitIndex + 1) % UBFPresentation::FrameLimitCount;
	ApplySettingsSelection();
}

void UUBFPresentationWidget::OnToggleVSyncClicked()
{
	bVSyncEnabled = !bVSyncEnabled;
	ApplySettingsSelection();
}

void UUBFPresentationWidget::RefreshSettingsText()
{
	if (QualityValueText)
	{
		QualityValueText->SetText(FText::FromString(UBFPresentation::QualityLabels[FMath::Clamp(SelectedQualityIndex, 0, 2)]));
	}
	if (FrameLimitValueText)
	{
		FrameLimitValueText->SetText(FText::FromString(UBFPresentation::FrameLimitLabels[
			FMath::Clamp(SelectedFrameLimitIndex, 0, UBFPresentation::FrameLimitCount - 1)]));
	}
	if (VSyncValueText)
	{
		VSyncValueText->SetText(FText::FromString(bVSyncEnabled ? TEXT("ACTIVADO") : TEXT("DESACTIVADO")));
	}
}

void UUBFPresentationWidget::ApplySettingsSelection()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(SelectedQualityIndex, 0, 2));
		Settings->SetFrameRateLimit(UBFPresentation::FrameLimitValues[
			FMath::Clamp(SelectedFrameLimitIndex, 0, UBFPresentation::FrameLimitCount - 1)]);
		Settings->SetVSyncEnabled(bVSyncEnabled);
		Settings->ApplySettings(false);
	}
	RefreshSettingsText();
}

void UUBFPresentationWidget::OnRedeemClicked()
{
	UUBFPlayerDataSubsystem* PlayerData = GetGameInstance() ?
		GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	if (!PlayerData || !GiftCodeInput || !GiftCodeResultText)
	{
		return;
	}

	const UUBFPlayerSaveGame* PreviousData = PlayerData->GetPlayerData();
	const int32 PreviousGold = PreviousData ? PreviousData->Gold : 0;
	const int32 PreviousEventPoints = PreviousData ? PreviousData->EventPoints : 0;
	const int32 PreviousGachaTickets = PreviousData ? PreviousData->GachaTickets : 0;
	const FUBFGiftCodeResult Result = PlayerData->RedeemGiftCode(GiftCodeInput->GetText().ToString());
	GiftCodeResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded ? UBFPresentation::GoldColor : UBFPresentation::AccentColor));
	GiftCodeResultText->SetText(FText::FromString(Result.bSucceeded
		? FString::Printf(TEXT("%s\n%s"), *Result.Message, *Result.RewardSummary)
		: Result.Message));
	if (Result.bSucceeded)
	{
		GiftCodeInput->SetText(FText::GetEmpty());
		ShowRewardToast(Result.RewardSummary);
		StartCurrencyRewardAnimation(PreviousGold, PreviousEventPoints, PreviousGachaTickets);
	}
}

void UUBFPresentationWidget::OnTrainingClicked()
{
	if (State == EPresentationState::Menu)
	{
		static const TCHAR* BotFillOptions[] = { TEXT("None"), TEXT("Opponent"), TEXT("All") };
		static const TCHAR* BotDifficultyOptions[] = { TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };
		const FName SelectedCharacterId = CharacterIds.IsValidIndex(SelectedCharacterIndex)
			? CharacterIds[SelectedCharacterIndex] : FName(TEXT("Wizz"));
		const FString Options = FString::Printf(
			TEXT("game=/Script/UBF.UBFCombatGameMode?TeamSize=%d?BotFill=%s?BotDifficulty=%s?CharacterID=%s?Mode=%s"),
			FMath::Clamp(SelectedTeamSize, 1, 3), BotFillOptions[FMath::Clamp(SelectedBotFillModeIndex, 0, 2)],
			BotDifficultyOptions[FMath::Clamp(SelectedBotDifficultyIndex, 0, 2)],
			*SelectedCharacterId.ToString(), SelectedGameModeIndex == 1 ? TEXT("Golem") : TEXT("Combat"));
		UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/UBF_Golem_Arena")), true, Options);
	}
}

void UUBFPresentationWidget::OnPreviousCharacterClicked()
{
	if (CharacterIds.IsEmpty()) return;
	SelectedCharacterIndex = SelectedCharacterIndex <= 0 ? CharacterIds.Num() - 1 : SelectedCharacterIndex - 1;
	if (UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr)
	{
		PlayerData->SetSelectedCharacterId(CharacterIds[SelectedCharacterIndex]);
	}
	RefreshCharacterSelection();
}

void UUBFPresentationWidget::OnNextCharacterClicked()
{
	if (CharacterIds.IsEmpty()) return;
	SelectedCharacterIndex = (SelectedCharacterIndex + 1) % CharacterIds.Num();
	if (UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr)
	{
		PlayerData->SetSelectedCharacterId(CharacterIds[SelectedCharacterIndex]);
	}
	RefreshCharacterSelection();
}

void UUBFPresentationWidget::RefreshCharacterSelection()
{
	if (!CharacterIds.IsValidIndex(SelectedCharacterIndex))
	{
		if (CharacterNameText) CharacterNameText->SetText(FText::FromString(TEXT("SIN PERSONAJES")));
		if (CharacterDetailsText) CharacterDetailsText->SetText(FText::GetEmpty());
		return;
	}

	UUBFCharacterCatalogSubsystem* Catalog = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFCharacterCatalogSubsystem>() : nullptr;
	const UUBFCharacterDefinition* Definition = Catalog
		? Catalog->FindCharacter(CharacterIds[SelectedCharacterIndex]) : nullptr;
	if (!Definition)
	{
		return;
	}
	if (CharacterNameText)
	{
		CharacterNameText->SetText(Definition->DisplayName);
	}
	if (CharacterDetailsText)
	{
		const FString PrimaryName = Definition->PrimarySkill
			? Definition->PrimarySkill->DisplayName.ToString() : TEXT("—");
		const FString SecondaryName = Definition->SecondarySkill
			? Definition->SecondarySkill->DisplayName.ToString() : TEXT("—");
		const FString UltimateName = Definition->Ultimate
			? Definition->Ultimate->DisplayName.ToString() : TEXT("—");
		const float PrimaryCost = Definition->PrimarySkill ? Definition->PrimarySkill->GaugeCost : 0.0f;
		const float SecondaryCost = Definition->SecondarySkill ? Definition->SecondarySkill->GaugeCost : 0.0f;
		const float UltimateCost = Definition->Ultimate ? Definition->Ultimate->GaugeCost : 0.0f;
		const FString Details = FString::Printf(TEXT("%s\n\nESTILO\n%s\n\nPASIVA · %s\n%s\n\nQ · %s  /  %.0f GAUGE\nE · %s  /  %.0f GAUGE\nF · %s  /  %.0f GAUGE"),
			*Definition->ShortDescription.ToString(), *Definition->CombatStyle.ToString(),
			*Definition->PassiveName.ToString(), *Definition->PassiveDescription.ToString(),
			*PrimaryName, PrimaryCost, *SecondaryName, SecondaryCost, *UltimateName, UltimateCost);
		CharacterDetailsText->SetText(FText::FromString(Details));
	}
}

void UUBFPresentationWidget::OnPreviousTeamSizeClicked()
{
	SelectedTeamSize = SelectedTeamSize <= 1 ? 3 : SelectedTeamSize - 1;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnNextTeamSizeClicked()
{
	SelectedTeamSize = SelectedTeamSize >= 3 ? 1 : SelectedTeamSize + 1;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnPreviousGameModeClicked()
{
	SelectedGameModeIndex = SelectedGameModeIndex <= 0 ? 1 : SelectedGameModeIndex - 1;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnNextGameModeClicked()
{
	SelectedGameModeIndex = (SelectedGameModeIndex + 1) % 2;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnPreviousBotFillClicked()
{
	SelectedBotFillModeIndex = SelectedBotFillModeIndex <= 0 ? 2 : SelectedBotFillModeIndex - 1;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnNextBotFillClicked()
{
	SelectedBotFillModeIndex = (SelectedBotFillModeIndex + 1) % 3;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnPreviousBotDifficultyClicked()
{
	SelectedBotDifficultyIndex = SelectedBotDifficultyIndex <= 0 ? 2 : SelectedBotDifficultyIndex - 1;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnNextBotDifficultyClicked()
{
	SelectedBotDifficultyIndex = (SelectedBotDifficultyIndex + 1) % 3;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::RefreshMatchSetupText()
{
	static const TCHAR* FormatLabels[] = { TEXT("1v1 · 1 por equipo"), TEXT("2v2 · 2 por equipo"), TEXT("3v3 · 3 por equipo") };
	static const TCHAR* GameModeLabels[] = { TEXT("ARENA · eliminación de equipo"), TEXT("GOLEM · portador y Master Golem") };
	static const TCHAR* BotLabels[] = { TEXT("SIN BOTS"), TEXT("RIVAL COMPLETO CON IA"), TEXT("LLENAR AMBOS EQUIPOS") };
	static const TCHAR* BotDifficultyLabels[] = { TEXT("FÁCIL"), TEXT("NORMAL"), TEXT("DIFÍCIL") };
	static const TCHAR* BotHints[] = {
		TEXT("No se añadirán combatientes controlados por IA."),
		TEXT("La IA completa las plazas libres del equipo rival."),
		TEXT("La IA completa las plazas libres de ambos equipos.")
	};
	if (MatchFormatValueText)
	{
		MatchFormatValueText->SetText(FText::FromString(FormatLabels[FMath::Clamp(SelectedTeamSize - 1, 0, 2)]));
	}
	if (GameModeValueText)
	{
		GameModeValueText->SetText(FText::FromString(GameModeLabels[FMath::Clamp(SelectedGameModeIndex, 0, 1)]));
	}
	if (BotFillValueText)
	{
		BotFillValueText->SetText(FText::FromString(BotLabels[FMath::Clamp(SelectedBotFillModeIndex, 0, 2)]));
	}
	if (BotDifficultyValueText)
	{
		BotDifficultyValueText->SetText(FText::FromString(
			BotDifficultyLabels[FMath::Clamp(SelectedBotDifficultyIndex, 0, 2)]));
	}
	if (MatchSetupHintText)
	{
		const FString BotHint = BotHints[FMath::Clamp(SelectedBotFillModeIndex, 0, 2)];
		const FString ModeHint = SelectedGameModeIndex == 1
			? TEXT("Recoge el orbe, protege al portador y destruye el Master Golem enemigo.")
			: FString();
		MatchSetupHintText->SetText(FText::FromString(ModeHint.IsEmpty()
			? BotHint : FString::Printf(TEXT("%s\n%s"), *ModeHint, *BotHint)));
	}
}

void UUBFPresentationWidget::OnRetryClicked()
{
	PrepareResources();
}

void UUBFPresentationWidget::OnExitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UUBFPresentationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (State == EPresentationState::Preloading && bBackgroundVideoReady && !bBackgroundPrerolled && BackgroundPlayer &&
		BackgroundPlayer->GetTime() > FTimespan::Zero())
	{
		BackgroundPlayer->Pause();
		BackgroundPlayer->Seek(FTimespan::Zero());
		bBackgroundPrerolled = true;
		UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("videobackground.mp4 entregó un frame y quedó preparado."));
		TryStartIntro();
	}

	if (State == EPresentationState::Intro && bIntroPlaybackStarted && IntroVideoPlayer &&
		IntroVideoPlayer->GetTime() >= IntroCutTime)
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("intro.mp4 llegó a 00:00:13:06; transición inmediata al menú."));
		BeginMenu();
		return;
	}

	// musicintro is deliberately delayed until its configured mark of intro.mp4.  The local
	// media offset keeps it aligned even when the decoder reports the frame a few ms late.
	if (State == EPresentationState::Intro && bIntroPlaybackStarted && !bIntroAudioStarted && IntroVideoPlayer &&
		IntroVideoPlayer->GetTime() >= IntroMusicStartTime)
	{
		StartIntroAudio();
	}

	if (State == EPresentationState::MenuReveal && !bTitleVoiceStarted && BackgroundPlayer &&
		BackgroundPlayer->GetTime() > FTimespan::Zero())
	{
		StartTitleVoice();
	}

	if (State == EPresentationState::MenuReveal)
	{
		MenuElapsed += InDeltaTime;
		BackgroundRevealElapsed += InDeltaTime;
		if (VideoTint && VideoImage)
		{
			const float FlashTime = BackgroundRevealElapsed;
			if (FlashTime < 0.055f)
			{
				VideoTint->SetBrushColor(FLinearColor(1.0f, 0.20f, 0.26f, 0.82f));
				VideoImage->SetRenderTranslation(FVector2D(FMath::FRandRange(-10.0f, 10.0f), 0.0f));
			}
			else if (FlashTime < 0.125f)
			{
				VideoTint->SetBrushColor(FLinearColor(0.04f, 0.01f, 0.03f, 0.05f));
				VideoImage->SetRenderTranslation(FVector2D(FMath::FRandRange(-5.0f, 5.0f), 0.0f));
			}
			else if (FlashTime < 0.215f)
			{
				VideoTint->SetBrushColor(FLinearColor(0.88f, 0.02f, 0.12f, 0.34f));
				VideoImage->SetRenderTranslation(FVector2D(FMath::FRandRange(-3.0f, 3.0f), 0.0f));
			}
			else if (FlashTime < 0.55f)
			{
				const float Settle = FMath::Clamp((FlashTime - 0.215f) / 0.335f, 0.0f, 1.0f);
				VideoTint->SetBrushColor(FLinearColor(0.01f, 0.012f, 0.02f, FMath::Lerp(0.22f, 0.065f, Settle)));
				VideoImage->SetRenderTranslation(FVector2D::ZeroVector);
			}
			else
			{
				VideoTint->SetBrushColor(FLinearColor(0.01f, 0.012f, 0.02f, 0.065f));
				VideoImage->SetRenderTranslation(FVector2D::ZeroVector);
			}
		}
		if (LogoImage)
		{
			const float Reveal = FMath::Clamp(MenuElapsed / 0.65f, 0.0f, 1.0f);
			const float EasedReveal = 1.0f - FMath::Square(1.0f - Reveal);
			LogoImage->SetRenderOpacity(EasedReveal);
			LogoImage->SetRenderTransform(FWidgetTransform(
				FVector2D(0.0f, FMath::Lerp(16.0f, 0.0f, EasedReveal)),
				FVector2D(FMath::Lerp(0.92f, 1.0f, EasedReveal)), FVector2D::ZeroVector, 0.0f));
		}
	}

	if (State == EPresentationState::Menu)
	{
		MenuElapsed += InDeltaTime;
		if (LogoImage)
		{
			FVector2D Translation(0.0f, FMath::Sin(MenuElapsed * 0.65f) * 2.5f);
			float Opacity = 1.0f;
			if (GlitchRemaining <= 0.0f && MenuElapsed >= NextGlitchTime)
			{
				GlitchRemaining = FMath::FRandRange(0.035f, 0.075f);
				NextGlitchTime = MenuElapsed + FMath::FRandRange(4.0f, 7.5f);
			}
			if (GlitchRemaining > 0.0f)
			{
				GlitchRemaining -= InDeltaTime;
				Translation.X += FMath::FRandRange(-5.0f, 5.0f);
				Opacity = FMath::FRandRange(0.88f, 1.0f);
			}
			LogoImage->SetRenderTransform(FWidgetTransform(Translation, FVector2D(1.0f), FVector2D::ZeroVector, 0.0f));
			LogoImage->SetRenderOpacity(Opacity);
		}
	}

	if (RewardToastRemaining > 0.0f && RewardToastText)
	{
		RewardToastRemaining -= InDeltaTime;
		RewardToastElapsed += InDeltaTime;
		const float FadeIn = FMath::Clamp(RewardToastElapsed / 0.22f, 0.0f, 1.0f);
		const float FadeOut = RewardToastRemaining < 0.55f ? FMath::Clamp(RewardToastRemaining / 0.55f, 0.0f, 1.0f) : 1.0f;
		RewardToastText->SetRenderOpacity(FadeIn * FadeOut);
		const float Pop = 1.0f - FMath::Square(1.0f - FadeIn);
		RewardToastText->SetRenderTransform(FWidgetTransform(
			FVector2D(0.0f, FMath::Lerp(-24.0f, 0.0f, Pop)),
			FVector2D(FMath::Lerp(0.92f, 1.0f, Pop)), FVector2D::ZeroVector, 0.0f));
		if (RewardToastRemaining <= 0.0f)
		{
			RewardToastText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (bCurrencyRewardAnimating)
	{
		CurrencyRewardElapsed += InDeltaTime;
		const float Progress = FMath::Clamp(CurrencyRewardElapsed / 1.2f, 0.0f, 1.0f);
		const float EasedProgress = Progress * Progress * (3.0f - 2.0f * Progress);
		CurrencyDisplayedGold = FMath::RoundToInt(FMath::Lerp(static_cast<float>(CurrencyStartGold), static_cast<float>(CurrencyTargetGold), EasedProgress));
		CurrencyDisplayedEventPoints = FMath::RoundToInt(FMath::Lerp(static_cast<float>(CurrencyStartEventPoints), static_cast<float>(CurrencyTargetEventPoints), EasedProgress));
		CurrencyDisplayedGachaTickets = FMath::RoundToInt(FMath::Lerp(static_cast<float>(CurrencyStartGachaTickets), static_cast<float>(CurrencyTargetGachaTickets), EasedProgress));
		if (AccountSummaryText)
		{
			const float Pulse = FMath::Sin(Progress * PI) * 0.10f;
			AccountSummaryText->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector,
				FVector2D(1.0f + Pulse), FVector2D::ZeroVector, 0.0f));
		}
		RefreshPlayerData();
		if (Progress >= 1.0f)
		{
			bCurrencyRewardAnimating = false;
			if (AccountSummaryText)
			{
				AccountSummaryText->SetRenderTransform(FWidgetTransform());
			}
			RefreshPlayerData();
		}
	}
}
