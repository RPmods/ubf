#include "UBFPresentation.h"

#include "UBFGameInstance.h"
#include "UBFPlayerData.h"
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
#include "Components/WidgetSwitcher.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
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
	// when a prior uncapped third-person session wrote FrameRateLimit=0.
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (Settings)
	{
		Settings->SetFrameRateLimit(60.0f);
		Settings->SetVSyncEnabled(true);
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
	BuildInterface();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Árbol visual de presentación construido."));
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
	Style.NormalPadding = FMargin(13.0f, 8.0f);
	Style.PressedPadding = FMargin(13.0f, 8.0f);
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
	PositionWidget(UtilityBox, FAnchors(0.055f, 0.815f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D(385.0f, 170.0f), 1);
	auto AddUtilityButton = [&UtilityBox](UButton* Button)
	{
		if (UVerticalBoxSlot* UtilitySlot = UtilityBox->AddChildToVerticalBox(Button))
		{
			UtilitySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
	};

	UButton* ShopButton = CreateMenuButton(TEXT("ShopButton"), TEXT("GACHA TICKETS"));
	ShopButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopClicked);
	AddUtilityButton(ShopButton);
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
		TEXT("Elige una red y prepara una sala de combate."));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("ServerList"),
		TEXT("PERU NODE        // OFFLINE\nCHILE NODE       // OFFLINE\nMIAMI NODE       // OFFLINE\n\nUBF CUSTOM       // READY\nEstablece una red privada y entra al listado de salas."),
		16, UBFPresentation::TextColor), 20.0f);
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("RoomFlow"),
		TEXT("NETWORK  →  ROOM LIST  →  FIGHT ROOM\nFormato de combate: 1v1, 2v2 y 3v3."),
		14, UBFPresentation::MutedTextColor));
	AddPage(PlayPage);

	UVerticalBox* CharacterPage = CreatePage(TEXT("CharacterPage"), TEXT("FIGHTERS"),
		TEXT("Roster de combate y preparación de estilo."));
	UBFPresentation::AddPageLine(CharacterPage, CreateText(TEXT("CharacterList"),
		TEXT("AKIRA  // Control · Burst · Cyan\nWIZZ   // Aggressive · Pressure · Crimson\nEILENE // Mobile DPS · Energy · Discipline"),
		17, UBFPresentation::TextColor), 20.0f);
	UBFPresentation::AddPageLine(CharacterPage, CreateText(TEXT("CharacterHint"),
		TEXT("Selecciona un combatiente para preparar su estilo, equipamiento y entrada."),
		14, UBFPresentation::MutedTextColor));
	AddPage(CharacterPage);

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

	UVerticalBox* ShopPage = CreatePage(TEXT("ShopPage"), TEXT("GACHA TICKETS"),
		TEXT("Tickets disponibles para futuras recompensas de colección."));
	GachaSummaryText = CreateText(TEXT("GachaSummary"), TEXT(""), 16, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(ShopPage, GachaSummaryText, 18.0f);
	UBFPresentation::AddPageLine(ShopPage, CreateText(TEXT("ShopHint"),
		TEXT("Tus tickets se almacenan de forma persistente en el perfil de UBF."),
		14, UBFPresentation::MutedTextColor));
	AddPage(ShopPage);

	UVerticalBox* CodesPage = CreatePage(TEXT("CodesPage"), TEXT("CÓDIGOS DE REGALO"),
		TEXT("Cada código se puede canjear una sola vez por perfil local."));
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
	if (UVerticalBoxSlot* InputPanelSlot = CodesPage->AddChildToVerticalBox(InputPanel))
	{
		InputPanelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	UButton* RedeemButton = CreateMenuButton(TEXT("RedeemGiftButton"), TEXT("CANJEAR CÓDIGO"));
	RedeemButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnRedeemClicked);
	if (UVerticalBoxSlot* RedeemButtonSlot = CodesPage->AddChildToVerticalBox(RedeemButton))
	{
		RedeemButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}
	GiftCodeResultText = CreateText(TEXT("GiftCodeResult"), TEXT(""), 15, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(CodesPage, GiftCodeResultText, 10.0f);
	UBFPresentation::AddPageLine(CodesPage, CreateText(TEXT("CodesHint"),
		TEXT("Las recompensas de oro, Puntos de Evento, tickets y objetos se guardan inmediatamente."),
		14, UBFPresentation::MutedTextColor));
	AddPage(CodesPage);

	UVerticalBox* SettingsPage = CreatePage(TEXT("SettingsPage"), TEXT("OPTIONS"),
		TEXT("Rendimiento, audio y preferencias del frontend."));
	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("SettingsData"),
		TEXT("FRONTEND MODE  //  LOW\nFRAME LIMIT    //  60 FPS + VSYNC\n\nAUDIO BUS      //  MENU · ROOM · MATCH"),
		16, UBFPresentation::TextColor), 18.0f);
	UBFPresentation::AddPageLine(SettingsPage, CreateText(TEXT("SettingsHint"),
		TEXT("El menú limita los FPS para evitar consumo continuo de GPU en una pantalla estática."),
		14, UBFPresentation::MutedTextColor));
	AddPage(SettingsPage);
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

	const int32 NextLevelExperience = FMath::Max(1000, Data->AccountLevel * 1000);
	if (AccountSummaryText)
	{
		AccountSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  //  LV.%d\nXP %d / %d\nORO %s   ·   EVENTO %s   ·   TICKETS %d"),
			*Data->PlayerName.ToUpper(), Data->AccountLevel, Data->Experience, NextLevelExperience,
			*FText::AsNumber(Data->Gold).ToString(), *FText::AsNumber(Data->EventPoints).ToString(), Data->GachaTickets)));
	}
	if (ProfileSummaryText)
	{
		ProfileSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nLV.%d  //  XP %d / %d\n\nPARTIDAS 0\nVICTORIAS 0\nDERROTAS 0\nPERSONAJE MÁS USADO — PENDIENTE"),
			*Data->PlayerName.ToUpper(), Data->AccountLevel, Data->Experience, NextLevelExperience)));
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
			TEXT("TICKETS DE GACHA DISPONIBLES: %d"), Data->GachaTickets)));
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
	RewardToastText->SetVisibility(ESlateVisibility::Visible);
	RewardToastElapsed = 0.0f;
	RewardToastRemaining = 3.5f;
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

void UUBFPresentationWidget::OnCodesClicked()
{
	SetMenuPage(EMenuPage::Codes);
}

void UUBFPresentationWidget::OnSettingsClicked()
{
	SetMenuPage(EMenuPage::Settings);
}

void UUBFPresentationWidget::OnRedeemClicked()
{
	UUBFPlayerDataSubsystem* PlayerData = GetGameInstance() ?
		GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	if (!PlayerData || !GiftCodeInput || !GiftCodeResultText)
	{
		return;
	}

	const FUBFGiftCodeResult Result = PlayerData->RedeemGiftCode(GiftCodeInput->GetText().ToString());
	GiftCodeResultText->SetColorAndOpacity(FSlateColor(Result.bSucceeded ? UBFPresentation::GoldColor : UBFPresentation::AccentColor));
	GiftCodeResultText->SetText(FText::FromString(Result.bSucceeded
		? FString::Printf(TEXT("%s\n%s"), *Result.Message, *Result.RewardSummary)
		: Result.Message));
	if (Result.bSucceeded)
	{
		GiftCodeInput->SetText(FText::GetEmpty());
		ShowRewardToast(Result.RewardSummary);
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
		RewardToastText->SetRenderTranslation(FVector2D(0.0f, FMath::Lerp(-24.0f, 0.0f, FadeIn)));
		if (RewardToastRemaining <= 0.0f)
		{
			RewardToastText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
