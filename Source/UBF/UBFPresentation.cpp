#include "UBFPresentation.h"

#include "UBFGameInstance.h"
#include "UBFPlayerData.h"
#include "UBFCharacterDefinition.h"
#include "UBFDraftShowcaseActor.h"
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
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
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
	const FLinearColor CardPanelColor(0.025f, 0.038f, 0.055f, 0.82f);
	const FLinearColor TextColor(0.92f, 0.94f, 0.97f, 1.0f);
	const FLinearColor MutedTextColor(0.62f, 0.68f, 0.74f, 1.0f);

	FSlateBrush MakeUIFrameBrush(UTexture2D* Texture, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		if (Texture)
		{
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.Margin = FMargin(0.105f);
			Brush.TintColor = FSlateColor(Tint);
		}
		return Brush;
	}

	void ApplyUIFrame(UBorder* Panel, UTexture2D* Texture, const FLinearColor& Tint = FLinearColor::White)
	{
		if (Panel && Texture)
		{
			Panel->SetBrush(MakeUIFrameBrush(Texture, Tint));
		}
	}
	const TCHAR* QualityLabels[] = { TEXT("BAJA"), TEXT("MEDIA"), TEXT("ALTA") };
	constexpr float FrameLimitValues[] = { 30.0f, 60.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f, 0.0f };
	const TCHAR* FrameLimitLabels[] = { TEXT("30 FPS"), TEXT("60 FPS"), TEXT("90 FPS"), TEXT("120 FPS"),
		TEXT("144 FPS"), TEXT("165 FPS"), TEXT("240 FPS"), TEXT("SIN LÍMITE") };
	constexpr int32 FrameLimitCount = 8;
	constexpr float IntroAndLoopVolume = 0.60f;
	constexpr float TitleVoiceVolume = 1.00f;
	const FSoftObjectPath IntroMusicPath(TEXT("/Game/Presentation/Audio/musicintro.musicintro"));
	const FSoftObjectPath LoopMusicPath(TEXT("/Game/Presentation/Audio/musicintrobucle.musicintrobucle"));
	const FSoftObjectPath TitleVoicePath(TEXT("/Game/Presentation/Audio/introdution_ubf_normalized.introdution_ubf_normalized"));
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
	LoadUIArtResources();
	BuildInterface();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Árbol visual de presentación construido."));
}

void UUBFPresentationWidget::LoadUIArtResources()
{
	UIFrameGoldTexture = LoadUITexture(TEXT("panel-frame-gold.png"));
	UIFrameRedTexture = LoadUITexture(TEXT("panel-frame-red.png"));
	UIFrameBlueTexture = LoadUITexture(TEXT("panel-frame-blue.png"));
	UIArmorIconTexture = LoadUITexture(TEXT("icon-armor.png"));
	UIWeaponIconTexture = LoadUITexture(TEXT("icon-weapon.png"));
	UIAccessoryIconTexture = LoadUITexture(TEXT("icon-accessory.png"));
	UICharacterIconTexture = LoadUITexture(TEXT("icon-character.png"));
	UIShopIconTexture = LoadUITexture(TEXT("icon-shop.png"));
	UISkillQIconTexture = LoadUITexture(TEXT("skill-q.png"));
	UISkillEIconTexture = LoadUITexture(TEXT("skill-e.png"));
	UISkillUltimateIconTexture = LoadUITexture(TEXT("skill-ultimate.png"));
	UISkillPassiveIconTexture = LoadUITexture(TEXT("skill-passive.png"));
}

UTexture2D* UUBFPresentationWidget::LoadUITexture(const FString& FileName) const
{
	const FString TexturePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Presentation/UIArt"), FileName);
	TArray<uint8> CompressedBytes;
	if (!FFileHelper::LoadFileToArray(CompressedBytes, *TexturePath) || CompressedBytes.IsEmpty())
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("No se pudo leer el recurso UI: %s"), *TexturePath);
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedBytes.GetData(), CompressedBytes.Num()))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("El recurso UI no es un PNG válido: %s"), *TexturePath);
		return nullptr;
	}

	TArray<uint8> PixelBytes;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, PixelBytes))
	{
		return nullptr;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
	{
		return nullptr;
	}
	Texture->SRGB = true;
	Texture->Filter = TF_Bilinear;
	Texture->NeverStream = true;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, PixelBytes.GetData(), PixelBytes.Num());
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

FReply UUBFPresentationWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bShowingClosedBetaNotice && InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCloseClosedBetaNoticeClicked();
		return FReply::Handled();
	}
	if (!PendingInputRebindAction.IsNone())
	{
		CaptureInputBinding(InKeyEvent.GetKey());
		return FReply::Handled();
	}
	if (State == EPresentationState::Draft)
	{
		if (InKeyEvent.GetKey() == EKeys::Left && !bDraftReady)
		{
			OnPreviousCharacterClicked();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Right && !bDraftReady)
		{
			OnNextCharacterClicked();
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			OnDraftCancelClicked();
			return FReply::Handled();
		}
		if (!bDraftReady && (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::SpaceBar ||
			InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom))
		{
			OnDraftReadyClicked();
			return FReply::Handled();
		}
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
	DestroyDraftShowcase();
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
	Style.Hovered = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture,
		FLinearColor(1.0f, 0.92f, 0.72f, 1.0f));
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
	BuildWelcomeScreen();
	BuildClosedBetaNotice();
	BuildCharacterDraft();

	LoadingRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapLoadingRoot"));
	PositionWidget(LoadingRoot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 30);
	LoadingRoot->SetVisibility(ESlateVisibility::Collapsed);
	UBorder* LoadingBackdrop = CreatePanel(TEXT("MapLoadingBackdrop"), FLinearColor(0.008f, 0.012f, 0.022f, 0.94f));
	LoadingBackdrop->SetPadding(FMargin(0.0f));
	UBFPresentation::FillCanvas(LoadingRoot, LoadingBackdrop, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 0);
	UBorder* LoadingCard = CreatePanel(TEXT("MapLoadingCard"), FLinearColor(0.025f, 0.038f, 0.055f, 0.94f));
	LoadingCard->SetPadding(FMargin(34.0f, 30.0f));
	UBFPresentation::AddToCanvas(LoadingRoot, LoadingCard, FAnchors(0.5f, 0.56f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(700.0f, 295.0f), 1);
	UVerticalBox* LoadingLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MapLoadingLayout"));
	LoadingCard->SetContent(LoadingLayout);
	UBFPresentation::AddPageLine(LoadingLayout, CreateText(TEXT("LoadingEyebrow"), TEXT("UBF  //  SALA LOCAL"),
		12, UBFPresentation::GoldColor, true), 12.0f);
	LoadingMapTitleText = CreateText(TEXT("LoadingMapTitle"), TEXT("ARENA GOLEM"),
		30, UBFPresentation::TextColor, true);
	UBFPresentation::AddPageLine(LoadingLayout, LoadingMapTitleText, 8.0f);
	LoadingStageText = CreateText(TEXT("LoadingStage"), TEXT("PREPARANDO COMBATIENTES..."),
		14, UBFPresentation::MutedTextColor, true);
	UBFPresentation::AddPageLine(LoadingLayout, LoadingStageText, 26.0f);
	LoadingProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("MapLoadingProgress"));
	LoadingProgressBar->SetFillColorAndOpacity(UBFPresentation::AccentColor);
	LoadingProgressBar->SetPercent(0.0f);
	LoadingLayout->AddChildToVerticalBox(LoadingProgressBar);
	UBFPresentation::AddPageLine(LoadingLayout, CreateText(TEXT("LoadingTip"),
		TEXT("Consejo: alterna ataque, esquiva y habilidades para crear una apertura."),
		12, UBFPresentation::MutedTextColor), 0.0f);

	RewardToastText = CreateText(TEXT("RewardToast"), TEXT(""), 19, UBFPresentation::GoldColor, true);
	PositionWidget(RewardToastText, FAnchors(0.5f, 0.11f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(760.0f, 130.0f), 20);
	RewardToastText->SetJustification(ETextJustify::Center);
	RewardToastText->SetVisibility(ESlateVisibility::Collapsed);
}

void UUBFPresentationWidget::BuildWelcomeScreen()
{
	WelcomeRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WelcomeScreenRoot"));
	PositionWidget(WelcomeRoot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 12);
	WelcomeRoot->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* WelcomeCard = CreatePanel(TEXT("WelcomeCard"), FLinearColor(0.012f, 0.018f, 0.028f, 0.77f));
	WelcomeCard->SetPadding(FMargin(34.0f, 22.0f));
	UBFPresentation::ApplyUIFrame(WelcomeCard, UIFrameGoldTexture,
		FLinearColor(1.0f, 0.87f, 0.60f, 0.98f));
	UBFPresentation::AddToCanvas(WelcomeRoot, WelcomeCard, FAnchors(0.5f, 0.79f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(660.0f, 220.0f), 0);
	UVerticalBox* WelcomeLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("WelcomeLayout"));
	WelcomeCard->SetContent(WelcomeLayout);
	UBFPresentation::AddPageLine(WelcomeLayout,
		CreateText(TEXT("WelcomeEyebrow"), TEXT("UBF  //  ACCESO A LA BETA"),
			12, UBFPresentation::GoldColor, true), 3.0f);
	WelcomeUserNameText = CreateText(TEXT("WelcomeUserName"), TEXT("JUGADOR"),
		22, UBFPresentation::TextColor, true);
	UBFPresentation::AddPageLine(WelcomeLayout, WelcomeUserNameText, 2.0f);
	UBFPresentation::AddPageLine(WelcomeLayout,
		CreateText(TEXT("WelcomePrompt"), TEXT("Tu perfil se sincroniza con UBF Launcher."),
			12, UBFPresentation::MutedTextColor), 13.0f);

	UButton* ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		TEXT("WelcomeContinueButton"));
	FButtonStyle WelcomeButtonStyle;
	WelcomeButtonStyle.Normal = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture,
		FLinearColor(1.0f, 0.84f, 0.46f, 1.0f));
	WelcomeButtonStyle.Hovered = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture,
		FLinearColor(1.0f, 0.96f, 0.78f, 1.0f));
	WelcomeButtonStyle.Pressed = FSlateColorBrush(FLinearColor(0.72f, 0.43f, 0.12f, 1.0f));
	WelcomeButtonStyle.Disabled = FSlateColorBrush(FLinearColor(0.16f, 0.15f, 0.13f, 0.7f));
	WelcomeButtonStyle.NormalPadding = FMargin(16.0f, 9.0f);
	WelcomeButtonStyle.PressedPadding = FMargin(16.0f, 11.0f, 16.0f, 7.0f);
	ContinueButton->SetStyle(WelcomeButtonStyle);
	UTextBlock* ContinueLabel = CreateText(TEXT("WelcomeContinueLabel"), TEXT("INICIAR JUEGO"),
		17, FLinearColor(0.10f, 0.075f, 0.035f, 1.0f), true);
	ContinueLabel->SetJustification(ETextJustify::Center);
	ContinueButton->SetContent(ContinueLabel);
	if (UVerticalBoxSlot* ButtonSlot = WelcomeLayout->AddChildToVerticalBox(ContinueButton))
	{
		ButtonSlot->SetPadding(FMargin(0.0f));
	}
	ContinueButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnContinueFromWelcomeClicked);
	WelcomeRoot->SetRenderOpacity(0.0f);
}

void UUBFPresentationWidget::BuildClosedBetaNotice()
{
	ClosedBetaRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
		TEXT("ClosedBetaNoticeRoot"));
	PositionWidget(ClosedBetaRoot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 55);
	ClosedBetaRoot->SetVisibility(ESlateVisibility::Collapsed);
	UBFPresentation::FillCanvas(ClosedBetaRoot,
		CreatePanel(TEXT("ClosedBetaDim"), FLinearColor(0.003f, 0.006f, 0.012f, 0.50f)),
		FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 0);

	UBorder* NoticeCard = CreatePanel(TEXT("ClosedBetaCard"), FLinearColor(0.018f, 0.026f, 0.039f, 0.96f));
	NoticeCard->SetPadding(FMargin(38.0f, 32.0f));
	UBFPresentation::ApplyUIFrame(NoticeCard, UIFrameGoldTexture,
		FLinearColor(1.0f, 0.85f, 0.52f, 1.0f));
	UBFPresentation::AddToCanvas(ClosedBetaRoot, NoticeCard, FAnchors(0.5f, 0.5f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(610.0f, 320.0f), 1);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		TEXT("ClosedBetaCloseButton"));
	FButtonStyle CloseStyle;
	CloseStyle.Normal = FSlateColorBrush(FLinearColor(0.035f, 0.045f, 0.060f, 0.82f));
	CloseStyle.Hovered = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture,
		FLinearColor(1.0f, 0.92f, 0.72f, 1.0f));
	CloseStyle.Pressed = FSlateColorBrush(UBFPresentation::AccentColor);
	CloseStyle.NormalPadding = FMargin(4.0f);
	CloseStyle.PressedPadding = FMargin(4.0f);
	CloseButton->SetStyle(CloseStyle);
	UTextBlock* CloseLabel = CreateText(TEXT("ClosedBetaCloseLabel"), TEXT("X"),
		17, UBFPresentation::TextColor, true);
	CloseLabel->SetJustification(ETextJustify::Center);
	CloseButton->SetContent(CloseLabel);
	UBFPresentation::AddToCanvas(ClosedBetaRoot, CloseButton, FAnchors(0.5f, 0.5f), FVector2D(0.5f),
		FVector2D(260.0f, -126.0f), FVector2D(44.0f, 44.0f), 2);
	CloseButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCloseClosedBetaNoticeClicked);

	UVerticalBox* NoticeLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("ClosedBetaLayout"));
	NoticeCard->SetContent(NoticeLayout);
	UBFPresentation::AddPageLine(NoticeLayout,
		CreateText(TEXT("ClosedBetaEyebrow"), TEXT("UNIVERSAL BREAKING FIGHTERS"),
			12, UBFPresentation::GoldColor, true), 11.0f);
	UTextBlock* NoticeTitle = CreateText(TEXT("ClosedBetaTitle"), TEXT("BETA CERRADA"),
		30, UBFPresentation::TextColor, true);
	UBFPresentation::AddPageLine(NoticeLayout, NoticeTitle, 10.0f);
	UBFPresentation::AddPageLine(NoticeLayout,
		CreateText(TEXT("ClosedBetaDescription"),
			TEXT("El acceso al juego está temporalmente limitado. Gracias por tu interés en UBF."),
			14, UBFPresentation::MutedTextColor), 9.0f);
	ClosedBetaProfileText = CreateText(TEXT("ClosedBetaProfile"), TEXT("PERFIL  //  JUGADOR"),
		12, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(NoticeLayout, ClosedBetaProfileText, 23.0f);

	UButton* ReturnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		TEXT("ClosedBetaReturnButton"));
	FButtonStyle ReturnStyle;
	ReturnStyle.Normal = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture,
		FLinearColor(0.82f, 0.68f, 0.42f, 0.92f));
	ReturnStyle.Hovered = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture, FLinearColor::White);
	ReturnStyle.Pressed = FSlateColorBrush(FLinearColor(0.72f, 0.43f, 0.12f, 1.0f));
	ReturnStyle.NormalPadding = FMargin(15.0f, 8.0f);
	ReturnStyle.PressedPadding = FMargin(15.0f, 10.0f, 15.0f, 6.0f);
	ReturnButton->SetStyle(ReturnStyle);
	UTextBlock* ReturnLabel = CreateText(TEXT("ClosedBetaReturnLabel"), TEXT("VOLVER AL MENÚ"),
		14, UBFPresentation::TextColor, true);
	ReturnLabel->SetJustification(ETextJustify::Center);
	ReturnButton->SetContent(ReturnLabel);
	if (UVerticalBoxSlot* ReturnSlot = NoticeLayout->AddChildToVerticalBox(ReturnButton))
	{
		ReturnSlot->SetPadding(FMargin(0.0f));
	}
	ReturnButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCloseClosedBetaNoticeClicked);
}

void UUBFPresentationWidget::BuildCharacterDraft()
{
	DraftRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CharacterDraftRoot"));
	PositionWidget(DraftRoot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 25);
	DraftRoot->SetVisibility(ESlateVisibility::Collapsed);
	UBFPresentation::FillCanvas(DraftRoot,
		CreatePanel(TEXT("CharacterDraftTint"), FLinearColor(0.004f, 0.008f, 0.016f, 0.74f)),
		FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 0);
	DraftPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DraftMannequinLineup"));
	DraftPreviewImage->SetColorAndOpacity(FLinearColor(0.82f, 0.88f, 0.98f, 0.90f));
	UBFPresentation::FillCanvas(DraftRoot, DraftPreviewImage, FAnchors(0.02f, 0.16f, 0.98f, 0.68f), 1);

	UTextBlock* Title = CreateText(TEXT("DraftTitle"), TEXT("SELECCIÓN DE ESCUADRÓN"),
		24, UBFPresentation::TextColor, true);
	Title->SetJustification(ETextJustify::Center);
	UBFPresentation::AddToCanvas(DraftRoot, Title, FAnchors(0.5f, 0.055f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(700.0f, 36.0f), 1);
	DraftTimerText = CreateText(TEXT("DraftTimer"), TEXT("SELECCIÓN  ·  00:30"),
		16, UBFPresentation::GoldColor, true);
	DraftTimerText->SetJustification(ETextJustify::Center);
	UBFPresentation::AddToCanvas(DraftRoot, DraftTimerText, FAnchors(0.5f, 0.12f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(420.0f, 28.0f), 1);
	DraftTimerBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DraftTimerBar"));
	DraftTimerBar->SetFillColorAndOpacity(UBFPresentation::AccentColor);
	DraftTimerBar->SetPercent(1.0f);
	UBFPresentation::AddToCanvas(DraftRoot, DraftTimerBar, FAnchors(0.5f, 0.175f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(650.0f, 7.0f), 1);

	auto MakeRosterPanel = [this](const FName& PanelName, const FString& TeamName,
		const FLinearColor& TeamColor, TObjectPtr<UTextBlock>& OutRoster)
	{
		UBorder* Panel = CreatePanel(PanelName, FLinearColor(0.014f, 0.024f, 0.038f, 0.86f));
		const bool bBlue = PanelName == FName(TEXT("DraftBluePanel"));
		UBFPresentation::ApplyUIFrame(Panel, bBlue ? UIFrameBlueTexture : UIFrameRedTexture);
		Panel->SetPadding(FMargin(20.0f, 18.0f));
		UBFPresentation::FillCanvas(DraftRoot, Panel,
			bBlue ? FAnchors(0.755f, 0.24f, 0.965f, 0.65f) : FAnchors(0.035f, 0.24f, 0.245f, 0.65f), 2);
		UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			FName(*(PanelName.ToString() + TEXT("Layout"))));
		Panel->SetContent(Layout);
		UBFPresentation::AddPageLine(Layout,
			CreateText(FName(*(PanelName.ToString() + TEXT("Title"))), TeamName, 15, TeamColor, true), 14.0f);
		OutRoster = CreateText(FName(*(PanelName.ToString() + TEXT("Roster"))), TEXT(""),
			13, UBFPresentation::TextColor, false);
		OutRoster->SetJustification(ETextJustify::Left);
		OutRoster->SetAutoWrapText(true);
		Layout->AddChildToVerticalBox(OutRoster);
	};
	MakeRosterPanel(TEXT("DraftRedPanel"), TEXT("TEAM A  ·  ROJO"),
		FLinearColor(1.0f, 0.30f, 0.35f, 1.0f), DraftRedRosterText);
	MakeRosterPanel(TEXT("DraftBluePanel"), TEXT("TEAM B  ·  AZUL  ·  RIVAL"),
		FLinearColor(0.28f, 0.78f, 0.95f, 1.0f), DraftBlueRosterText);

	UBorder* CharacterPanel = CreatePanel(TEXT("DraftCharacterPanel"), FLinearColor(0.018f, 0.033f, 0.051f, 0.44f));
	UBFPresentation::ApplyUIFrame(CharacterPanel, UIFrameGoldTexture);
	CharacterPanel->SetPadding(FMargin(18.0f, 14.0f));
	UBFPresentation::FillCanvas(DraftRoot, CharacterPanel, FAnchors(0.405f, 0.355f, 0.595f, 0.535f), 2);
	UTextBlock* CharacterLabel = CreateText(TEXT("DraftCharacterLabel"), TEXT("TU PERSONAJE"),
		11, UBFPresentation::GoldColor, true);
	UBFPresentation::AddToCanvas(DraftRoot, CharacterLabel, FAnchors(0.5f, 0.37f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(250.0f, 20.0f), 2);
	CharacterLabel->SetJustification(ETextJustify::Center);
	DraftCharacterNameText = CreateText(TEXT("DraftCharacterName"), TEXT("WIZZ"),
		30, UBFPresentation::TextColor, true);
	DraftCharacterNameText->SetJustification(ETextJustify::Center);
	UBFPresentation::AddToCanvas(DraftRoot, DraftCharacterNameText, FAnchors(0.5f, 0.416f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(340.0f, 38.0f), 3);
	DraftCharacterRoleText = CreateText(TEXT("DraftCharacterRole"), TEXT(""),
		13, UBFPresentation::MutedTextColor, false);
	DraftCharacterRoleText->SetJustification(ETextJustify::Center);
	UBFPresentation::AddToCanvas(DraftRoot, DraftCharacterRoleText, FAnchors(0.5f, 0.473f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(360.0f, 26.0f), 3);
	DraftPreviousButton = CreateMenuButton(TEXT("DraftPrevious"), TEXT("‹  ANTERIOR"));
	UBFPresentation::AddToCanvas(DraftRoot, DraftPreviousButton, FAnchors(0.5f, 0.5f), FVector2D(1.0f, 0.5f),
		FVector2D(-180.0f, 0.0f), FVector2D(104.0f, 36.0f), 4);
	DraftPreviousButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPreviousCharacterClicked);
	DraftNextButton = CreateMenuButton(TEXT("DraftNext"), TEXT("SIGUIENTE  ›"));
	UBFPresentation::AddToCanvas(DraftRoot, DraftNextButton, FAnchors(0.5f, 0.5f), FVector2D(0.0f, 0.5f),
		FVector2D(180.0f, 0.0f), FVector2D(104.0f, 36.0f), 4);
	DraftNextButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnNextCharacterClicked);

	auto MakeSkillPanel = [this](const FName& Name, const FString& Label, const FAnchors& Anchors,
		TObjectPtr<UTextBlock>& OutText, UTexture2D* IconTexture)
	{
		UBorder* Panel = CreatePanel(Name, FLinearColor(0.012f, 0.021f, 0.034f, 0.84f));
		UBFPresentation::ApplyUIFrame(Panel, UIFrameGoldTexture);
		Panel->SetPadding(FMargin(13.0f, 10.0f));
		UBFPresentation::FillCanvas(DraftRoot, Panel, Anchors, 1);
		UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("Layout"))));
		Panel->SetContent(Layout);
		UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("Header"))));
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("IconSize"))));
		IconSize->SetWidthOverride(26.0f);
		IconSize->SetHeightOverride(26.0f);
		UImage* SkillIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*(Name.ToString() + TEXT("Icon"))));
		if (IconTexture)
		{
			SkillIcon->SetBrushFromTexture(IconTexture, true);
		}
		IconSize->SetContent(SkillIcon);
		if (UHorizontalBoxSlot* IconSlot = Header->AddChildToHorizontalBox(IconSize))
		{
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* SkillLabel = CreateText(FName(*(Name.ToString() + TEXT("Label"))), Label, 11,
			UBFPresentation::GoldColor, true);
		if (UHorizontalBoxSlot* LabelSlot = Header->AddChildToHorizontalBox(SkillLabel))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* HeaderSlot = Layout->AddChildToVerticalBox(Header))
		{
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
		OutText = CreateText(FName(*(Name.ToString() + TEXT("Details"))), TEXT(""),
			10, UBFPresentation::TextColor, false);
		OutText->SetAutoWrapText(true);
		Layout->AddChildToVerticalBox(OutText);
	};
	MakeSkillPanel(TEXT("DraftQPanel"), TEXT("Q  ·  PRIMARIA"), FAnchors(0.025f, 0.68f, 0.265f, 0.90f), DraftPrimarySkillText, UISkillQIconTexture);
	MakeSkillPanel(TEXT("DraftEPanel"), TEXT("E  ·  SECUNDARIA"), FAnchors(0.27f, 0.68f, 0.51f, 0.90f), DraftSecondarySkillText, UISkillEIconTexture);
	MakeSkillPanel(TEXT("DraftUltimatePanel"), TEXT("F  ·  DEFINITIVA"), FAnchors(0.515f, 0.68f, 0.755f, 0.90f), DraftUltimateText, UISkillUltimateIconTexture);
	MakeSkillPanel(TEXT("DraftPassivePanel"), TEXT("PASIVA"), FAnchors(0.76f, 0.68f, 0.975f, 0.90f), DraftPassiveText, UISkillPassiveIconTexture);

	DraftStatusText = CreateText(TEXT("DraftStatus"), TEXT(""), 12, UBFPresentation::MutedTextColor, true);
	DraftStatusText->SetJustification(ETextJustify::Center);
	DraftStatusText->SetAutoWrapText(true);
	UBFPresentation::AddToCanvas(DraftRoot, DraftStatusText, FAnchors(0.5f, 0.805f), FVector2D(0.5f),
		FVector2D::ZeroVector, FVector2D(950.0f, 34.0f), 2);
	DraftCancelButton = CreateMenuButton(TEXT("DraftCancel"), TEXT("VOLVER A LA SALA"));
	UBFPresentation::AddToCanvas(DraftRoot, DraftCancelButton, FAnchors(0.5f, 0.91f), FVector2D(1.0f, 0.5f),
		FVector2D(-12.0f, 0.0f), FVector2D(210.0f, 52.0f), 2);
	DraftCancelButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnDraftCancelClicked);
	DraftReadyButton = CreateMenuButton(TEXT("DraftReady"), TEXT("CONFIRMAR Y LISTO"));
	UBFPresentation::AddToCanvas(DraftRoot, DraftReadyButton, FAnchors(0.5f, 0.91f), FVector2D(0.0f, 0.5f),
		FVector2D(12.0f, 0.0f), FVector2D(240.0f, 52.0f), 2);
	DraftReadyButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnDraftReadyClicked);
	DraftRoot->SetRenderOpacity(0.0f);
}

void UUBFPresentationWidget::DestroyDraftShowcase()
{
	if (IsValid(DraftShowcaseActor))
	{
		DraftShowcaseActor->Destroy();
	}
	DraftShowcaseActor = nullptr;
	if (DraftPreviewImage)
	{
		DraftPreviewImage->SetBrushResourceObject(nullptr);
	}
	if (CharacterPreviewImage)
	{
		CharacterPreviewImage->SetBrushResourceObject(nullptr);
	}
	if (InventoryCharacterPreviewImage)
	{
		InventoryCharacterPreviewImage->SetBrushResourceObject(nullptr);
	}
}

void UUBFPresentationWidget::EnsureMenuCharacterShowcase()
{
	if (!IsValid(DraftShowcaseActor) && GetWorld())
	{
		const FTransform ShowcaseTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 3000.0f));
		DraftShowcaseActor = GetWorld()->SpawnActor<AUBFDraftShowcaseActor>(
			AUBFDraftShowcaseActor::StaticClass(), ShowcaseTransform);
	}
	if (!IsValid(DraftShowcaseActor))
	{
		return;
	}
	DraftShowcaseActor->SetSoloPreviewMode(true);
	const bool bPreviewPageActive = ActivePage == EMenuPage::Character || ActivePage == EMenuPage::Inventory;
	DraftShowcaseActor->SetShowcaseActive(bPreviewPageActive);
	UTextureRenderTarget2D* PreviewTexture = DraftShowcaseActor->GetPreviewTexture();
	if (CharacterPreviewImage)
	{
		CharacterPreviewImage->SetBrushResourceObject(PreviewTexture);
	}
	if (InventoryCharacterPreviewImage)
	{
		InventoryCharacterPreviewImage->SetBrushResourceObject(PreviewTexture);
	}
}

void UUBFPresentationWidget::BuildMenu()
{
	MenuRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuRoot"));
	PositionWidget(MenuRoot, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 10);
	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);

	// Persistent top bar keeps the main sections in one predictable row, with settings
	// available as a separate control in the upper-right corner.
	UBorder* NavigationPanel = CreatePanel(TEXT("NavigationPanel"), FLinearColor(0.012f, 0.020f, 0.032f, 0.76f));
	NavigationPanel->SetPadding(FMargin(5.0f, 4.0f));
	UBFPresentation::FillCanvas(MenuRoot, NavigationPanel, FAnchors(0.195f, 0.043f, 0.792f, 0.137f), 1);
	UHorizontalBox* NavigationBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Navigation"));
	NavigationPanel->SetContent(NavigationBox);
	auto AddNavigationButton = [&NavigationBox](UButton* Button)
	{
		if (UHorizontalBoxSlot* NavigationSlot = NavigationBox->AddChildToHorizontalBox(Button))
		{
			NavigationSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NavigationSlot->SetPadding(FMargin(1.0f, 0.0f));
			NavigationSlot->SetVerticalAlignment(VAlign_Fill);
		}
	};
	auto CreateTopButton = [this](const FName& Name, const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style;
		Style.Normal = FSlateColorBrush(FLinearColor(0.008f, 0.014f, 0.024f, 0.06f));
		Style.Hovered = FSlateColorBrush(FLinearColor(0.30f, 0.025f, 0.065f, 0.74f));
		Style.Pressed = FSlateColorBrush(UBFPresentation::AccentColor);
		Style.Disabled = FSlateColorBrush(FLinearColor(0.02f, 0.025f, 0.035f, 0.20f));
		Style.NormalPadding = FMargin(4.0f, 5.0f);
		Style.PressedPadding = FMargin(4.0f, 5.0f);
		Button->SetStyle(Style);
		UTextBlock* ButtonText = CreateText(FName(*(Name.ToString() + TEXT("_Text"))), Label, 11,
			UBFPresentation::TextColor, true);
		ButtonText->SetJustification(ETextJustify::Center);
		Button->SetContent(ButtonText);
		return Button;
	};

	UButton* HomeButton = CreateTopButton(TEXT("HomeButton"), TEXT("INICIO"));
	HomeButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnHomeClicked);
	AddNavigationButton(HomeButton);
	UButton* PlayButton = CreateTopButton(TEXT("PlayButton"), TEXT("JUGAR"));
	PlayButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPlayClicked);
	AddNavigationButton(PlayButton);
	UButton* InventoryButton = CreateTopButton(TEXT("InventoryButton"), TEXT("ARMARIO"));
	InventoryButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnInventoryClicked);
	AddNavigationButton(InventoryButton);
	UButton* ShopButton = CreateTopButton(TEXT("ShopButton"), TEXT("TIENDA"));
	ShopButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopClicked);
	AddNavigationButton(ShopButton);
	UButton* GachaButton = CreateTopButton(TEXT("GachaButton"), TEXT("GACHA"));
	GachaButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnGachaClicked);
	AddNavigationButton(GachaButton);
	UButton* CodesButton = CreateTopButton(TEXT("CodesButton"), TEXT("CÓDIGOS"));
	CodesButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCodesClicked);
	AddNavigationButton(CodesButton);
	UButton* ProfileButton = CreateTopButton(TEXT("ProfileButton"), TEXT("PERFIL"));
	ProfileButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnProfileClicked);
	AddNavigationButton(ProfileButton);

	UButton* SettingsButton = CreateTopButton(TEXT("SettingsButton"), TEXT("AJUSTES"));
	SettingsButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnSettingsClicked);
	PositionWidget(SettingsButton, FAnchors(1.0f, 0.043f), FVector2D(1.0f, 0.0f),
		FVector2D(-12.0f, 0.0f), FVector2D(118.0f, 58.0f), 2);

	UBorder* AccountPanel = CreatePanel(TEXT("AccountPanel"), FLinearColor(0.015f, 0.024f, 0.036f, 0.72f));
	AccountPanel->SetPadding(FMargin(8.0f, 4.0f));
	UBFPresentation::FillCanvas(MenuRoot, AccountPanel, FAnchors(0.795f, 0.043f, 0.89f, 0.137f), 2);
	AccountSummaryText = CreateText(TEXT("AccountSummary"), TEXT("PERFIL LOCAL"), 11, UBFPresentation::TextColor, true);
	AccountPanel->SetContent(AccountSummaryText);

	UBorder* ContentPanel = CreatePanel(TEXT("ContentPanel"), FLinearColor(0.012f, 0.020f, 0.032f, 0.64f));
	ContentPanel->SetPadding(FMargin(34.0f, 27.0f));
	UBFPresentation::FillCanvas(MenuRoot, ContentPanel, FAnchors(0.385f, 0.165f, 0.965f, 0.94f), 2);
	UVerticalBox* ContentLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentLayout"));
	ContentPanel->SetContent(ContentLayout);

	PageTitleText = CreateText(TEXT("CurrentPageTitle"), TEXT("UBF  /  CENTRO"), 12,
		UBFPresentation::GoldColor, true);
	if (UVerticalBoxSlot* TitleSlot = ContentLayout->AddChildToVerticalBox(PageTitleText))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("PageSwitcher"));
	if (UVerticalBoxSlot* SwitcherSlot = ContentLayout->AddChildToVerticalBox(PageSwitcher))
	{
		SwitcherSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	BuildPages();
}

void UUBFPresentationWidget::OnHomeClicked()
{
	SetMenuPage(EMenuPage::Home);
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

	UVerticalBox* HomePage = CreatePage(TEXT("HomePage"), TEXT("UBF  /  CENTRO DE OPERACIONES"),
		TEXT("ELIGE TU ESCUADRÓN Y PREPARA EL COMBATE"));
	UHorizontalBox* HomeGrid = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HomeFeatureGrid"));
	if (UVerticalBoxSlot* HomeGridSlot = HomePage->AddChildToVerticalBox(HomeGrid))
	{
		HomeGridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		HomeGridSlot->SetPadding(FMargin(0.0f, 18.0f, 0.0f, 0.0f));
	}
	UBorder* HomePlayCard = CreatePanel(TEXT("HomePlayCard"), FLinearColor(0.018f, 0.032f, 0.052f, 0.78f));
	HomePlayCard->SetPadding(FMargin(28.0f, 28.0f));
	if (UHorizontalBoxSlot* PlayCardSlot = HomeGrid->AddChildToHorizontalBox(HomePlayCard))
	{
		PlayCardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PlayCardSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
		PlayCardSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* HomePlayLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HomePlayLayout"));
	HomePlayCard->SetContent(HomePlayLayout);
	UBFPresentation::AddPageLine(HomePlayLayout, CreateText(TEXT("HomePlayEyebrow"),
		TEXT("LISTO PARA COMBATIR  /  01"), 12, UBFPresentation::GoldColor, true), 22.0f);
	UBFPresentation::AddPageLine(HomePlayLayout, CreateText(TEXT("HomePlayHeadline"),
		TEXT("Tu equipo.\nTu siguiente ronda."), 30, UBFPresentation::TextColor, true), 18.0f);
	UBFPresentation::AddPageLine(HomePlayLayout, CreateText(TEXT("HomePlayDescription"),
		TEXT("Configura una sala local, decide el modo y formato, y comienza el draft de personajes antes de cargar el mapa."),
		15, UBFPresentation::MutedTextColor), 24.0f);
	UButton* HomePlayButton = CreateMenuButton(TEXT("HomePlayButton"), TEXT("CREAR SALA LOCAL  →"));
	HomePlayButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPlayClicked);
	if (UVerticalBoxSlot* HomePlaySlot = HomePlayLayout->AddChildToVerticalBox(HomePlayButton))
	{
		HomePlaySlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
	}
	UBorder* HomeStatusCard = CreatePanel(TEXT("HomeStatusCard"), FLinearColor(0.022f, 0.036f, 0.054f, 0.68f));
	HomeStatusCard->SetPadding(FMargin(20.0f, 24.0f));
	if (UHorizontalBoxSlot* StatusCardSlot = HomeGrid->AddChildToHorizontalBox(HomeStatusCard))
	{
		StatusCardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StatusCardSlot->SetPadding(FMargin(0.0f));
	}
	UVerticalBox* HomeStatusLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HomeStatusLayout"));
	HomeStatusCard->SetContent(HomeStatusLayout);
	UBFPresentation::AddPageLine(HomeStatusLayout, CreateText(TEXT("HomeStatusEyebrow"),
		TEXT("ESTADO DEL CAMPO"), 11, UBFPresentation::GoldColor, true), 18.0f);
	UBFPresentation::AddPageLine(HomeStatusLayout, CreateText(TEXT("HomeStatusLocal"),
		TEXT("BETA LOCAL  ·  DISPONIBLE"), 14, UBFPresentation::TextColor, true), 10.0f);
	UBFPresentation::AddPageLine(HomeStatusLayout, CreateText(TEXT("HomeStatusModes"),
		TEXT("ARENA\nGOLEM\n\nSALAS EN LÍNEA  ·  EN DESARROLLO"),
		13, UBFPresentation::MutedTextColor), 18.0f);
	UBFPresentation::AddPageLine(HomeStatusLayout, CreateText(TEXT("HomeStatusRoster"),
		TEXT("1—5 combatientes por equipo"), 12, UBFPresentation::GoldColor, true), 0.0f);
	AddPage(HomePage);

	UVerticalBox* PlayPage = CreatePage(TEXT("PlayPage"), TEXT("SALAS"),
		TEXT("CREA UNA PARTIDA LOCAL Y PREPARA LOS EQUIPOS"));
	UBFPresentation::AddPageLine(PlayPage, CreateText(TEXT("ServerList"),
		TEXT("ENTRENAMIENTO LOCAL  ·  HASTA 10 LUCHADORES  ·  5 POR EQUIPO"),
		12, UBFPresentation::GoldColor, true), 9.0f);
	USizeBox* RoomPreviewSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RoomPreviewSize"));
	RoomPreviewSize->SetHeightOverride(122.0f);
	UBorder* RoomPreviewPanel = CreatePanel(TEXT("RoomPreviewPanel"), FLinearColor(0.012f, 0.020f, 0.033f, 0.72f));
	UBFPresentation::ApplyUIFrame(RoomPreviewPanel, UIFrameGoldTexture);
	RoomPreviewPanel->SetPadding(FMargin(12.0f, 9.0f));
	UVerticalBox* RoomPreviewLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("RoomPreviewLayout"));
	RoomPreviewPanel->SetContent(RoomPreviewLayout);
	RoomPreviewText = CreateText(TEXT("RoomPreview"), TEXT("SALA LOCAL  //  ARENA  //  1v1"),
		11, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(RoomPreviewLayout, RoomPreviewText, 5.0f);
	UHorizontalBox* RoomTeamsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
		TEXT("RoomTeamsRow"));
	RoomPreviewLayout->AddChildToVerticalBox(RoomTeamsRow);
	auto MakeRoomTeamCard = [this, RoomTeamsRow](const FName& Name, const FString& Label,
		const FLinearColor& Color, bool bRedTeam, TObjectPtr<UTextBlock>& OutRoster)
	{
		UBorder* TeamPanel = CreatePanel(Name, FLinearColor(bRedTeam ? 0.12f : 0.018f,
			bRedTeam ? 0.024f : 0.065f, bRedTeam ? 0.032f : 0.12f, 0.70f));
		UBFPresentation::ApplyUIFrame(TeamPanel, bRedTeam ? UIFrameRedTexture : UIFrameBlueTexture);
		TeamPanel->SetPadding(FMargin(12.0f, 7.0f));
		UVerticalBox* TeamLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("Layout"))));
		TeamPanel->SetContent(TeamLayout);
		UBFPresentation::AddPageLine(TeamLayout, CreateText(FName(*(Name.ToString() + TEXT("Title"))),
			Label, 11, Color, true), 2.0f);
		OutRoster = CreateText(FName(*(Name.ToString() + TEXT("Roster"))), TEXT(""), 11,
			UBFPresentation::TextColor, false);
		OutRoster->SetAutoWrapText(true);
		TeamLayout->AddChildToVerticalBox(OutRoster);
		if (UHorizontalBoxSlot* TeamSlot = RoomTeamsRow->AddChildToHorizontalBox(TeamPanel))
		{
			TeamSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TeamSlot->SetPadding(FMargin(bRedTeam ? 0.0f : 6.0f, 0.0f, bRedTeam ? 6.0f : 0.0f, 0.0f));
		}
	};
	MakeRoomTeamCard(TEXT("RoomRedTeamPanel"), TEXT("TEAM A  ·  ROJO"),
		FLinearColor(1.0f, 0.30f, 0.35f, 1.0f), true, RoomRedRosterText);
	MakeRoomTeamCard(TEXT("RoomBlueTeamPanel"), TEXT("TEAM B  ·  AZUL"),
		FLinearColor(0.28f, 0.78f, 0.95f, 1.0f), false, RoomBlueRosterText);
	RoomPreviewSize->SetContent(RoomPreviewPanel);
	if (UVerticalBoxSlot* RoomPreviewSlot = PlayPage->AddChildToVerticalBox(RoomPreviewSize))
	{
		RoomPreviewSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

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

	UVerticalBox* CharacterPage = CreatePage(TEXT("CharacterPage"), TEXT("ELIGE TU COMBATIENTE"),
		TEXT("PASO 1  /  CADA INTEGRANTE DEBE ELEGIR UN PERSONAJE DIFERENTE"));
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
	UHorizontalBox* CharacterShowcaseRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CharacterShowcaseRow"));
	UBorder* CharacterDetailsCard = CreatePanel(TEXT("CharacterDetailsCard"), FLinearColor(0.018f, 0.027f, 0.043f, 0.89f));
	UBFPresentation::ApplyUIFrame(CharacterDetailsCard, UIFrameGoldTexture);
	CharacterDetailsCard->SetPadding(FMargin(18.0f, 15.0f));
	if (UHorizontalBoxSlot* DetailsSlot = CharacterShowcaseRow->AddChildToHorizontalBox(CharacterDetailsCard))
	{
		DetailsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DetailsSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		DetailsSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* CharacterDetailsLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("CharacterDetailsLayout"));
	CharacterDetailsCard->SetContent(CharacterDetailsLayout);
	CharacterPortraitRoleText = CreateText(TEXT("CharacterPortraitRole"), TEXT("ESTILO DE COMBATE"),
		11, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(CharacterDetailsLayout, CharacterPortraitRoleText, 7.0f);
	UScrollBox* CharacterInfoScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
		TEXT("CharacterInfoScroll"));
	if (UVerticalBoxSlot* CharacterScrollSlot = CharacterDetailsLayout->AddChildToVerticalBox(CharacterInfoScroll))
	{
		CharacterScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	CharacterDetailsText = CreateText(TEXT("CharacterDetails"), TEXT(""), 13, UBFPresentation::TextColor);
	CharacterDetailsText->SetAutoWrapText(true);
	CharacterDetailsText->SetLineHeightPercentage(1.12f);
	CharacterInfoScroll->AddChild(CharacterDetailsText);
	USizeBox* CharacterArtSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		TEXT("CharacterArtSize"));
	CharacterArtSize->SetMinDesiredWidth(400.0f);
	CharacterArtSize->SetMinDesiredHeight(400.0f);
	UBorder* CharacterArtCard = CreatePanel(TEXT("CharacterArtCard"), FLinearColor(0.008f, 0.015f, 0.027f, 0.56f));
	UBFPresentation::ApplyUIFrame(CharacterArtCard, UIFrameRedTexture);
	CharacterArtCard->SetPadding(FMargin(8.0f));
	CharacterPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CharacterPreviewImage"));
	CharacterPreviewImage->SetColorAndOpacity(FLinearColor::White);
	CharacterArtCard->SetContent(CharacterPreviewImage);
	CharacterArtSize->SetContent(CharacterArtCard);
	if (UHorizontalBoxSlot* ArtSlot = CharacterShowcaseRow->AddChildToHorizontalBox(CharacterArtSize))
	{
		ArtSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ArtSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UVerticalBoxSlot* ShowcaseSlot = CharacterPage->AddChildToVerticalBox(CharacterShowcaseRow))
	{
		ShowcaseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ShowcaseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}
	UBFPresentation::AddPageLine(CharacterPage, CreateText(TEXT("CharacterHint"),
		TEXT("Q PRINCIPAL   ·   E SECUNDARIA   ·   F DEFINITIVA   ·   Cambia las teclas en Ajustes / Controles."),
		11, UBFPresentation::MutedTextColor), 4.0f);
	UButton* ContinueCharacterButton = CreateMenuButton(TEXT("ContinueCharacterButton"), TEXT("CONTINUAR  ·  CONFIGURAR SALA"));
	ContinueCharacterButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnContinueCharacterClicked);
	if (UVerticalBoxSlot* CharacterContinueSlot = CharacterPage->AddChildToVerticalBox(ContinueCharacterButton))
	{
		CharacterContinueSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
	AddPage(CharacterPage);
	RefreshCharacterSelection();

	UVerticalBox* InventoryPage = CreatePage(TEXT("InventoryPage"), TEXT("INVENTARIO"),
		TEXT("TU COLECCIÓN  /  PERSONAJE, EQUIPO Y OBJETOS"));
	UHorizontalBox* InventoryLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
		TEXT("InventoryLayout"));
	if (UVerticalBoxSlot* InventoryLayoutSlot = InventoryPage->AddChildToVerticalBox(InventoryLayout))
	{
		InventoryLayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UBorder* InventoryCategoryPanel = CreatePanel(TEXT("InventoryCategoryPanel"), FLinearColor(0.014f, 0.022f, 0.037f, 0.85f));
	UBFPresentation::ApplyUIFrame(InventoryCategoryPanel, UIFrameGoldTexture);
	InventoryCategoryPanel->SetPadding(FMargin(8.0f, 10.0f));
	if (UHorizontalBoxSlot* CategorySlot = InventoryLayout->AddChildToHorizontalBox(InventoryCategoryPanel))
	{
		CategorySlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		CategorySlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* InventoryCategoryList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("InventoryCategoryList"));
	InventoryCategoryPanel->SetContent(InventoryCategoryList);
	InventoryCategoryText = CreateText(TEXT("InventoryCategory"), TEXT("ARMARIO"), 10,
		UBFPresentation::GoldColor, true);
	InventoryCategoryText->SetJustification(ETextJustify::Center);
	UBFPresentation::AddPageLine(InventoryCategoryList, InventoryCategoryText, 8.0f);
	auto AddInventoryCategory = [this, InventoryCategoryList](const FName& Name, const FString& Label,
		UTexture2D* Icon) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style;
		Style.Normal = FSlateColorBrush(FLinearColor(0.016f, 0.026f, 0.042f, 0.58f));
		Style.Hovered = UBFPresentation::MakeUIFrameBrush(UIFrameGoldTexture);
		Style.Pressed = FSlateColorBrush(FLinearColor(0.34f, 0.18f, 0.045f, 0.82f));
		Style.NormalPadding = FMargin(5.0f, 7.0f);
		Style.PressedPadding = Style.NormalPadding;
		Button->SetStyle(Style);
		UVerticalBox* ButtonContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("Content"))));
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			FName(*(Name.ToString() + TEXT("Icon"))));
		if (Icon) IconImage->SetBrushFromTexture(Icon, true);
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("IconSize"))));
		IconSize->SetWidthOverride(34.0f);
		IconSize->SetHeightOverride(34.0f);
		IconSize->SetContent(IconImage);
		if (UVerticalBoxSlot* IconSlot = ButtonContent->AddChildToVerticalBox(IconSize))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 4.0f));
		}
		UTextBlock* LabelText = CreateText(FName(*(Name.ToString() + TEXT("Label"))), Label, 9,
			UBFPresentation::TextColor, true);
		LabelText->SetJustification(ETextJustify::Center);
		ButtonContent->AddChildToVerticalBox(LabelText);
		Button->SetContent(ButtonContent);
		if (UVerticalBoxSlot* ButtonSlot = InventoryCategoryList->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
		return Button;
	};
	AddInventoryCategory(TEXT("InventoryAll"), TEXT("TODO"), UICharacterIconTexture)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnInventoryAllClicked);
	AddInventoryCategory(TEXT("InventoryArmor"), TEXT("ARMADURA"), UIArmorIconTexture)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnInventoryArmorClicked);
	AddInventoryCategory(TEXT("InventoryWeapons"), TEXT("ARMAS"), UIWeaponIconTexture)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnInventoryWeaponsClicked);
	AddInventoryCategory(TEXT("InventoryAccessories"), TEXT("ACCESORIOS"), UIAccessoryIconTexture)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnInventoryAccessoriesClicked);
	AddInventoryCategory(TEXT("InventoryCharacter"), TEXT("PERSONAJE"), UICharacterIconTexture)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnCharacterClicked);
	AddInventoryCategory(TEXT("InventoryShop"), TEXT("TIENDA"), UIShopIconTexture)
		->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopClicked);
	UBorder* InventoryItemsPanel = CreatePanel(TEXT("InventoryItemsPanel"), FLinearColor(0.012f, 0.020f, 0.033f, 0.68f));
	UBFPresentation::ApplyUIFrame(InventoryItemsPanel, UIFrameBlueTexture);
	InventoryItemsPanel->SetPadding(FMargin(13.0f, 12.0f));
	if (UHorizontalBoxSlot* ItemsPanelSlot = InventoryLayout->AddChildToHorizontalBox(InventoryItemsPanel))
	{
		ItemsPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ItemsPanelSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		ItemsPanelSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* InventoryItemsLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("InventoryItemsLayout"));
	InventoryItemsPanel->SetContent(InventoryItemsLayout);
	InventorySummaryText = CreateText(TEXT("InventorySummary"), TEXT("0 OBJETOS  ·  TODO"),
		12, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(InventoryItemsLayout, InventorySummaryText, 8.0f);
	UScrollBox* InventoryScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
		TEXT("InventoryScroll"));
	if (UVerticalBoxSlot* InventoryScrollSlot = InventoryItemsLayout->AddChildToVerticalBox(InventoryScroll))
	{
		InventoryScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	InventoryItemsGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("InventoryItemsGrid"));
	InventoryItemsGrid->SetInnerSlotPadding(FVector2D(7.0f, 7.0f));
	InventoryScroll->AddChild(InventoryItemsGrid);
	UBorder* InventoryCharacterPanel = CreatePanel(TEXT("InventoryCharacterPanel"), FLinearColor(0.014f, 0.021f, 0.036f, 0.82f));
	UBFPresentation::ApplyUIFrame(InventoryCharacterPanel, UIFrameGoldTexture);
	InventoryCharacterPanel->SetPadding(FMargin(9.0f));
	if (UHorizontalBoxSlot* CharacterPanelSlot = InventoryLayout->AddChildToHorizontalBox(InventoryCharacterPanel))
	{
		CharacterPanelSlot->SetPadding(FMargin(0.0f));
		CharacterPanelSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* InventoryCharacterLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
		TEXT("InventoryCharacterLayout"));
	InventoryCharacterPanel->SetContent(InventoryCharacterLayout);
	InventoryCharacterNameText = CreateText(TEXT("InventoryCharacterName"), TEXT("PERSONAJE"),
		13, UBFPresentation::GoldColor, true);
	InventoryCharacterNameText->SetJustification(ETextJustify::Center);
	UBFPresentation::AddPageLine(InventoryCharacterLayout, InventoryCharacterNameText, 2.0f);
	InventoryCharacterInfoText = CreateText(TEXT("InventoryCharacterInfo"), TEXT(""),
		10, UBFPresentation::MutedTextColor, false);
	InventoryCharacterInfoText->SetJustification(ETextJustify::Center);
	UBFPresentation::AddPageLine(InventoryCharacterLayout, InventoryCharacterInfoText, 6.0f);
	UImage* InventoryCharacterImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
		TEXT("InventoryCharacterImage"));
	InventoryCharacterImage->SetColorAndOpacity(FLinearColor::White);
	InventoryCharacterPreviewImage = InventoryCharacterImage;
	if (UVerticalBoxSlot* CharacterImageSlot = InventoryCharacterLayout->AddChildToVerticalBox(InventoryCharacterImage))
	{
		CharacterImageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UBFPresentation::AddPageLine(InventoryPage, CreateText(TEXT("InventoryHint"),
		TEXT("Colección local · los objetos se agrupan por categoría cuando el nombre permite reconocer su tipo."),
		10, UBFPresentation::MutedTextColor), 0.0f);
	AddPage(InventoryPage);

	UVerticalBox* ProfilePage = CreatePage(TEXT("ProfilePage"), TEXT("PERFIL"),
		TEXT("Identidad, progreso y registro de combate."));
	ProfileSummaryText = CreateText(TEXT("ProfileSummary"), TEXT(""), 16, UBFPresentation::TextColor);
	UBFPresentation::AddPageLine(ProfilePage, ProfileSummaryText, 20.0f);
	UBFPresentation::AddPageLine(ProfilePage, CreateText(TEXT("ProfileHint"),
		TEXT("Tu cuenta mantiene el oro, los puntos de evento, los tickets y los objetos obtenidos."),
		14, UBFPresentation::MutedTextColor));
	AddPage(ProfilePage);

	UVerticalBox* ShopPage = CreatePage(TEXT("ShopPage"), TEXT("TIENDA"),
		TEXT("Equipo y objetos de evento para tu perfil local."));
	ShopBalanceText = CreateText(TEXT("ShopBalance"), TEXT(""), 13, UBFPresentation::GoldColor, true);
	UBFPresentation::AddPageLine(ShopPage, ShopBalanceText, 8.0f);
	UWrapBox* ShopOffersGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ShopOffersGrid"));
	ShopOffersGrid->SetInnerSlotPadding(FVector2D(10.0f, 10.0f));
	if (UVerticalBoxSlot* ShopOffersSlot = ShopPage->AddChildToVerticalBox(ShopOffersGrid))
	{
		ShopOffersSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	auto CreateShopOffer = [this, ShopOffersGrid](const FName& Name, const FString& Title,
		const FString& Description, const FString& ActionLabel)
	{
		UBorder* Card = CreatePanel(FName(*(Name.ToString() + TEXT("_Card"))), UBFPresentation::CardPanelColor);
		Card->SetPadding(FMargin(14.0f, 12.0f));
		UVerticalBox* CardBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("_Body"))));
		Card->SetContent(CardBody);
		UBFPresentation::AddPageLine(CardBody, CreateText(FName(*(Name.ToString() + TEXT("_Title"))),
			Title, 14, UBFPresentation::GoldColor, true), 6.0f);
		UBFPresentation::AddPageLine(CardBody, CreateText(FName(*(Name.ToString() + TEXT("_Description"))),
			Description, 12, UBFPresentation::MutedTextColor), 8.0f);
		UButton* ActionButton = CreateMenuButton(FName(*(Name.ToString() + TEXT("_Action"))), ActionLabel);
		CardBody->AddChildToVerticalBox(ActionButton);
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			FName(*(Name.ToString() + TEXT("_Size"))));
		CardSize->SetWidthOverride(276.0f);
		CardSize->SetMinDesiredHeight(142.0f);
		CardSize->SetContent(Card);
		ShopOffersGrid->AddChildToWrapBox(CardSize);
		return ActionButton;
	};
	UButton* BasicArmorButton = CreateShopOffer(TEXT("ShopBasicArmor"), TEXT("ARMADURA BÁSICA  ·  1,200 ORO"),
		TEXT("Protección inicial para el combatiente."), TEXT("ADQUIRIR"));
	BasicArmorButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyBasicArmorClicked);
	UButton* BasicRingButton = CreateShopOffer(TEXT("ShopBasicRing"), TEXT("ANILLO BÁSICO  ·  850 ORO"),
		TEXT("Accesorio de prueba para tu inventario."), TEXT("ADQUIRIR"));
	BasicRingButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyBasicRingClicked);
	UButton* BasicNecklaceButton = CreateShopOffer(TEXT("ShopBasicNecklace"), TEXT("COLLAR BÁSICO  ·  1,500 ORO"),
		TEXT("Accesorio de prueba para tu inventario."), TEXT("ADQUIRIR"));
	BasicNecklaceButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyBasicNecklaceClicked);
	UButton* EventRingButton = CreateShopOffer(TEXT("ShopEventRing"), TEXT("ANILLO DE EVENTO  ·  500 PUNTOS"),
		TEXT("Oferta de evento. Se paga con puntos."), TEXT("CANJEAR"));
	EventRingButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyEventRingClicked);
	UButton* EventNecklaceButton = CreateShopOffer(TEXT("ShopEventNecklace"), TEXT("COLLAR DE EVENTO  ·  800 PUNTOS"),
		TEXT("Oferta de evento. Se paga con puntos."), TEXT("CANJEAR"));
	EventNecklaceButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyEventNecklaceClicked);
	UButton* EventFrameButton = CreateShopOffer(TEXT("ShopEventFrame"), TEXT("MARCO DE PERFIL  ·  350 PUNTOS"),
		TEXT("Marco cosmético de prueba."), TEXT("CANJEAR"));
	EventFrameButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnShopBuyEventFrameClicked);
	UVerticalBox* ShopLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopFooter"));
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
	ShopPage->AddChildToVerticalBox(ShopLayout);
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

	UVerticalBox* SettingsPage = CreatePage(TEXT("SettingsPage"), TEXT("AJUSTES"),
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
	AddInputBindingRow(TEXT("UltimateBindingRow"), TEXT("DEFINITIVA"), UltimateBindingText)
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
	WelcomeRoot->SetRenderOpacity(0.0f);
	WelcomeRoot->SetVisibility(ESlateVisibility::Collapsed);
	ClosedBetaRoot->SetRenderOpacity(0.0f);
	ClosedBetaRoot->SetVisibility(ESlateVisibility::Collapsed);
	bShowingClosedBetaNotice = false;

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
		ShowDevelopmentError(TEXT("No se pudo cargar uno de los SoundWaves requeridos: musicintro, musicintrobucle o introdution_ubf_normalized."));
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
	WelcomeRoot->SetRenderOpacity(0.0f);
	WelcomeRoot->SetVisibility(ESlateVisibility::Collapsed);
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
		TEXT("Fase de revelación sincronizada: videobackground.mp4 + introdution_ubf_normalized.wav con offset %.3f s."),
		MediaOffsetSeconds);
	// Reveal the play prompt on the first frame of the post-intro title voice,
	// never over intro.mp4.  The voice continues underneath the welcome screen.
	EnterInteractiveMenu();
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
	WelcomeElapsed = 0.0f;
	MenuEntranceElapsed = 0.0f;
	bShowingWelcomeScreen = true;
	if (UCanvasPanelSlot* LogoSlot = Cast<UCanvasPanelSlot>(LogoImage->Slot))
	{
		LogoSlot->SetAnchors(FAnchors(0.5f, 0.36f));
		LogoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LogoSlot->SetPosition(FVector2D::ZeroVector);
		LogoSlot->SetSize(FVector2D(570.0f, 320.0f));
	}
	LogoImage->SetVisibility(ESlateVisibility::Visible);
	LogoImage->SetRenderOpacity(0.0f);
	LogoImage->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 22.0f), FVector2D(0.94f),
		FVector2D::ZeroVector, 0.0f));
	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	WelcomeRoot->SetRenderOpacity(0.0f);
	WelcomeRoot->SetVisibility(ESlateVisibility::Visible);
	ClosedBetaRoot->SetVisibility(ESlateVisibility::Collapsed);
	bShowingClosedBetaNotice = false;
	SetKeyboardFocus();
	RefreshPlayerData();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Fase de bienvenida del menú iniciada."));
}

void UUBFPresentationWidget::OnContinueFromWelcomeClicked()
{
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Entrada solicitada: fase=%u, bienvenida=%s."),
		static_cast<uint8>(State),
		bShowingWelcomeScreen ? TEXT("sí") : TEXT("no"));
	if (State != EPresentationState::Menu || !bShowingWelcomeScreen || bShowingClosedBetaNotice)
	{
		return;
	}

	ShowClosedBetaNotice();
}

void UUBFPresentationWidget::ShowClosedBetaNotice()
{
	if (bShowingClosedBetaNotice || !ClosedBetaRoot)
	{
		return;
	}

	const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
	const UUBFPlayerSaveGame* Data = PlayerData ? PlayerData->GetPlayerData() : nullptr;
	const FString PlayerName = Data && !Data->PlayerName.IsEmpty() ? Data->PlayerName : TEXT("JUGADOR");
	if (ClosedBetaProfileText)
	{
		ClosedBetaProfileText->SetText(FText::FromString(FString::Printf(TEXT("PERFIL  //  %s"), *PlayerName.ToUpper())));
	}

	bShowingClosedBetaNotice = true;
	ClosedBetaNoticeElapsed = 0.0f;
	ClosedBetaRoot->SetRenderOpacity(0.0f);
	ClosedBetaRoot->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 12.0f), FVector2D(0.985f),
		FVector2D(0.5f), 0.0f));
	ClosedBetaRoot->SetVisibility(ESlateVisibility::Visible);
	SetKeyboardFocus();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Acceso al juego bloqueado: beta cerrada. Perfil de Launcher: %s."), *PlayerName);
}

void UUBFPresentationWidget::OnCloseClosedBetaNoticeClicked()
{
	if (!bShowingClosedBetaNotice || !ClosedBetaRoot)
	{
		return;
	}
	bShowingClosedBetaNotice = false;
	ClosedBetaRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (bShowingWelcomeScreen)
	{
		bShowingWelcomeScreen = false;
		WelcomeRoot->SetVisibility(ESlateVisibility::Collapsed);
		MenuEntranceElapsed = 0.0f;
		PageEntranceElapsed = 0.0f;
		MenuRoot->SetRenderOpacity(0.0f);
		MenuRoot->SetVisibility(ESlateVisibility::Visible);
		if (PageSwitcher)
		{
			PageSwitcher->SetRenderOpacity(0.0f);
		}
	}
	SetKeyboardFocus();
}

void UUBFPresentationWidget::OnContinueCharacterClicked()
{
	if (State == EPresentationState::Menu)
	{
		SetMenuPage(EMenuPage::Play);
	}
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
	if (NewPage == EMenuPage::Character || NewPage == EMenuPage::Inventory)
	{
		EnsureMenuCharacterShowcase();
	}
	else if (IsValid(DraftShowcaseActor))
	{
		DraftShowcaseActor->SetShowcaseActive(false);
	}
	PageEntranceElapsed = 0.0f;
	PageSwitcher->SetRenderOpacity(0.0f);
	PageSwitcher->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 15.0f), FVector2D(0.985f),
		FVector2D::ZeroVector, 0.0f));
	static const TCHAR* PageNames[] = {
		TEXT("INICIO  /  CENTRO DE COMBATE"),
		TEXT("JUGAR  /  CONFIGURAR SALA"),
		TEXT("PERSONAJES  /  SELECCIÓN"),
		TEXT("ARMARIO  /  COLECCIÓN"),
		TEXT("PERFIL  /  PROGRESO"),
		TEXT("TIENDA  /  OBJETOS"),
		TEXT("GACHA  /  RECOMPENSAS"),
		TEXT("CÓDIGOS  /  CANJEAR"),
		TEXT("AJUSTES  /  CONFIGURACIÓN")
	};
	PageTitleText->SetText(FText::FromString(PageNames[static_cast<int32>(NewPage)]));
	if (NewPage == EMenuPage::Character)
	{
		CharacterEntryElapsed = 0.0f;
		if (CharacterNameText)
		{
			CharacterNameText->SetRenderOpacity(0.0f);
			CharacterNameText->SetRenderTransform(FWidgetTransform(FVector2D(-36.0f, 0.0f),
				FVector2D(0.92f), FVector2D::ZeroVector, 0.0f));
		}
		if (CharacterPortraitMarkText)
		{
			CharacterPortraitMarkText->SetRenderOpacity(0.0f);
			CharacterPortraitMarkText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 24.0f),
				FVector2D(0.82f), FVector2D::ZeroVector, 0.0f));
		}
		if (CharacterPreviewImage)
		{
			CharacterPreviewImage->SetRenderOpacity(0.0f);
			CharacterPreviewImage->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 18.0f),
				FVector2D(0.94f), FVector2D::ZeroVector, 0.0f));
		}
		if (CharacterDetailsText)
		{
			CharacterDetailsText->SetRenderOpacity(0.0f);
			CharacterDetailsText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 18.0f),
				FVector2D(0.985f), FVector2D::ZeroVector, 0.0f));
		}
	}
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
	if (WelcomeUserNameText)
	{
		WelcomeUserNameText->SetText(FText::FromString(Data->PlayerName.IsEmpty()
			? TEXT("JUGADOR") : Data->PlayerName.ToUpper()));
	}
	if (ClosedBetaProfileText)
	{
		ClosedBetaProfileText->SetText(FText::FromString(FString::Printf(TEXT("PERFIL  //  %s"),
			Data->PlayerName.IsEmpty() ? TEXT("JUGADOR") : *Data->PlayerName.ToUpper())));
	}
	const int32 DisplayGold = bCurrencyRewardAnimating ? CurrencyDisplayedGold : Data->Gold;
	const int32 DisplayEventPoints = bCurrencyRewardAnimating ? CurrencyDisplayedEventPoints : Data->EventPoints;
	const int32 DisplayGachaTickets = bCurrencyRewardAnimating ? CurrencyDisplayedGachaTickets : Data->GachaTickets;

	const int32 NextLevelExperience = FMath::Max(1000, Data->AccountLevel * 1000);
	if (AccountSummaryText)
	{
		AccountSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  ·  LV.%d\nORO %s  ·  TICKETS %d"),
			*Data->PlayerName.ToUpper(), Data->AccountLevel,
			*FText::AsNumber(DisplayGold).ToString(), DisplayGachaTickets)));
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
		InventorySummaryText->SetText(FText::FromString(TEXT("COLECCIÓN")));
	}
	if (InventoryCategoryText)
	{
		InventoryCategoryText->SetText(FText::FromString(SelectedInventoryCategory == EInventoryCategory::All
			? TEXT("ARMARIO") : TEXT("FILTRAR")));
	}
	if (InventoryCharacterNameText || InventoryCharacterInfoText)
	{
		const UUBFCharacterCatalogSubsystem* Catalog = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UUBFCharacterCatalogSubsystem>() : nullptr;
		const UUBFCharacterDefinition* Character = Catalog && CharacterIds.IsValidIndex(SelectedCharacterIndex)
			? Catalog->FindCharacter(CharacterIds[SelectedCharacterIndex]) : nullptr;
		if (Character)
		{
			if (InventoryCharacterNameText) InventoryCharacterNameText->SetText(Character->DisplayName);
			if (InventoryCharacterInfoText)
			{
				InventoryCharacterInfoText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"),
					*Character->CombatStyle.ToString(), *Character->ShortDescription.ToString())));
			}
		}
	}
	if (InventoryItemsGrid)
	{
		const FString CurrentInventoryKey = FString::Printf(TEXT("%d\n%s"),
			static_cast<int32>(SelectedInventoryCategory), *FString::Join(Data->InventoryItems, TEXT("\n")));
		if (CurrentInventoryKey != LastRenderedInventoryKey)
		{
			LastRenderedInventoryKey = CurrentInventoryKey;
			const FString BuildPrefix = FString::Printf(TEXT("InventoryBuild_%u_"), ++InventoryWidgetBuildSerial);
			InventoryItemsGrid->ClearChildren();
			TArray<int32> VisibleItemIndices;
			auto GetRecognizedCategory = [](const FString& ItemName)
			{
				const FString Lower = ItemName.ToLower();
				if (Lower.Contains(TEXT("armor")) || Lower.Contains(TEXT("armadura")) || Lower.Contains(TEXT("helmet"))
					|| Lower.Contains(TEXT("casco")) || Lower.Contains(TEXT("guante")) || Lower.Contains(TEXT("bota")))
				{
					return EInventoryCategory::Armor;
				}
				if (Lower.Contains(TEXT("weapon")) || Lower.Contains(TEXT("arma")) || Lower.Contains(TEXT("sword"))
					|| Lower.Contains(TEXT("espada")) || Lower.Contains(TEXT("lanza")) || Lower.Contains(TEXT("hacha")))
				{
					return EInventoryCategory::Weapons;
				}
				if (Lower.Contains(TEXT("ring")) || Lower.Contains(TEXT("necklace")) || Lower.Contains(TEXT("accessory"))
					|| Lower.Contains(TEXT("anillo")) || Lower.Contains(TEXT("collar")) || Lower.Contains(TEXT("amuleto")))
				{
					return EInventoryCategory::Accessories;
				}
				return EInventoryCategory::All;
			};
			for (int32 ItemIndex = 0; ItemIndex < Data->InventoryItems.Num(); ++ItemIndex)
			{
				const EInventoryCategory ItemCategory = GetRecognizedCategory(Data->InventoryItems[ItemIndex]);
				if (SelectedInventoryCategory == EInventoryCategory::All || ItemCategory == SelectedInventoryCategory)
				{
					VisibleItemIndices.Add(ItemIndex);
				}
			}
			static const TCHAR* CategoryLabels[] = { TEXT("TODO"), TEXT("ARMADURA"), TEXT("ARMAS"), TEXT("ACCESORIOS") };
			InventorySummaryText->SetText(FText::FromString(FString::Printf(TEXT("%d OBJETOS  ·  %s"),
				VisibleItemIndices.Num(), CategoryLabels[static_cast<int32>(SelectedInventoryCategory)])));
			if (VisibleItemIndices.IsEmpty())
			{
				UBorder* EmptyCard = CreatePanel(FName(*(BuildPrefix + TEXT("EmptyCard"))), UBFPresentation::CardPanelColor);
				UBFPresentation::ApplyUIFrame(EmptyCard, UIFrameGoldTexture);
				EmptyCard->SetPadding(FMargin(18.0f, 17.0f));
				UVerticalBox* EmptyLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
					FName(*(BuildPrefix + TEXT("EmptyLayout"))));
				EmptyCard->SetContent(EmptyLayout);
				UBFPresentation::AddPageLine(EmptyLayout, CreateText(FName(*(BuildPrefix + TEXT("EmptyTitle"))),
					Data->InventoryItems.IsEmpty() ? TEXT("TU ARMARIO ESTÁ LISTO") : TEXT("NO HAY OBJETOS EN ESTA CATEGORÍA"),
					14, UBFPresentation::GoldColor, true), 5.0f);
				UBFPresentation::AddPageLine(EmptyLayout, CreateText(FName(*(BuildPrefix + TEXT("EmptyDescription"))),
					Data->InventoryItems.IsEmpty()
						? TEXT("Los objetos que desbloquees aparecerán aquí.")
						: TEXT("Los nombres guardados no indican con seguridad si son armadura, arma o accesorio; revisa TODO o la tienda."),
					11, UBFPresentation::MutedTextColor), 0.0f);
				InventoryItemsGrid->AddChildToWrapBox(EmptyCard);
			}
			else
			{
				for (const int32 ItemIndex : VisibleItemIndices)
				{
					const FString Suffix = FString::FromInt(ItemIndex);
					USizeBox* ItemSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
						FName(*(BuildPrefix + TEXT("Size_") + Suffix)));
					ItemSize->SetWidthOverride(158.0f);
					ItemSize->SetHeightOverride(138.0f);
					UBorder* ItemCard = CreatePanel(FName(*(BuildPrefix + TEXT("Card_") + Suffix)), UBFPresentation::CardPanelColor);
					UBFPresentation::ApplyUIFrame(ItemCard, UIFrameGoldTexture);
					ItemCard->SetPadding(FMargin(9.0f, 8.0f));
					UVerticalBox* ItemLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
						FName(*(BuildPrefix + TEXT("Layout_") + Suffix)));
					ItemCard->SetContent(ItemLayout);
					const EInventoryCategory ItemCategory = GetRecognizedCategory(Data->InventoryItems[ItemIndex]);
					UTexture2D* ItemIcon = ItemCategory == EInventoryCategory::Armor ? UIArmorIconTexture
						: (ItemCategory == EInventoryCategory::Weapons ? UIWeaponIconTexture
							: (ItemCategory == EInventoryCategory::Accessories ? UIAccessoryIconTexture : UICharacterIconTexture));
					UImage* ItemIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
						FName(*(BuildPrefix + TEXT("Icon_") + Suffix)));
					if (ItemIcon) ItemIconImage->SetBrushFromTexture(ItemIcon, true);
					if (UVerticalBoxSlot* IconSlot = ItemLayout->AddChildToVerticalBox(ItemIconImage))
					{
						IconSlot->SetHorizontalAlignment(HAlign_Center);
						IconSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
					}
					UBFPresentation::AddPageLine(ItemLayout, CreateText(FName(*(BuildPrefix + TEXT("Name_") + Suffix)),
						Data->InventoryItems[ItemIndex], 11, UBFPresentation::TextColor, true), 5.0f);
					UBFPresentation::AddPageLine(ItemLayout, CreateText(FName(*(BuildPrefix + TEXT("Tag_") + Suffix)),
						ItemCategory == EInventoryCategory::All ? TEXT("OBJETO LOCAL")
							: (ItemCategory == EInventoryCategory::Armor ? TEXT("ARMADURA")
								: (ItemCategory == EInventoryCategory::Weapons ? TEXT("ARMA") : TEXT("ACCESORIO"))),
						9, UBFPresentation::MutedTextColor, true), 0.0f);
					ItemSize->SetContent(ItemCard);
					InventoryItemsGrid->AddChildToWrapBox(ItemSize);
				}
			}
		}
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
	SelectedInventoryCategory = EInventoryCategory::All;
	LastRenderedInventoryKey.Reset();
	SetMenuPage(EMenuPage::Inventory);
}

void UUBFPresentationWidget::OnInventoryAllClicked()
{
	SetInventoryCategory(EInventoryCategory::All);
}

void UUBFPresentationWidget::OnInventoryArmorClicked()
{
	SetInventoryCategory(EInventoryCategory::Armor);
}

void UUBFPresentationWidget::OnInventoryWeaponsClicked()
{
	SetInventoryCategory(EInventoryCategory::Weapons);
}

void UUBFPresentationWidget::OnInventoryAccessoriesClicked()
{
	SetInventoryCategory(EInventoryCategory::Accessories);
}

void UUBFPresentationWidget::SetInventoryCategory(EInventoryCategory Category)
{
	if (SelectedInventoryCategory == Category)
	{
		return;
	}
	SelectedInventoryCategory = Category;
	LastRenderedInventoryKey.Reset();
	RefreshPlayerData();
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
	if (State == EPresentationState::Menu && !bShowingWelcomeScreen && ActivePage == EMenuPage::Play)
	{
		ShowClosedBetaNotice();
	}
}

void UUBFPresentationWidget::StartCharacterDraft()
{
	if (State != EPresentationState::Menu || PendingTrainingOptions.IsEmpty() || !DraftRoot)
	{
		return;
	}
	State = EPresentationState::Draft;
	if (!DraftShowcaseActor && GetWorld())
	{
		const FTransform ShowcaseTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 3000.0f));
		DraftShowcaseActor = GetWorld()->SpawnActor<AUBFDraftShowcaseActor>(
			AUBFDraftShowcaseActor::StaticClass(), ShowcaseTransform);
		if (DraftShowcaseActor && DraftPreviewImage)
		{
			DraftPreviewImage->SetBrushResourceObject(DraftShowcaseActor->GetPreviewTexture());
		}
	}
	DraftElapsed = 0.0f;
	DraftCountdownRemaining = 0.0f;
	bDraftReady = false;
	bDraftCountdownActive = false;
	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	WelcomeRoot->SetVisibility(ESlateVisibility::Collapsed);
	DraftRoot->SetRenderOpacity(0.0f);
	DraftRoot->SetVisibility(ESlateVisibility::Visible);
	if (DraftReadyButton) DraftReadyButton->SetIsEnabled(true);
	if (DraftCancelButton) DraftCancelButton->SetIsEnabled(true);
	if (DraftPreviousButton) DraftPreviousButton->SetIsEnabled(true);
	if (DraftNextButton) DraftNextButton->SetIsEnabled(true);
	RefreshCharacterDraft();
	SetKeyboardFocus();
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Fase de draft abierta: 30 segundos para elegir; los bots seleccionan personajes únicos por equipo y quedan listos automáticamente."));
}

void UUBFPresentationWidget::OnDraftReadyClicked()
{
	if (State != EPresentationState::Draft || bDraftReady)
	{
		return;
	}
	bDraftReady = true;
	bDraftCountdownActive = true;
	DraftCountdownRemaining = 5.0f;
	if (DraftReadyButton) DraftReadyButton->SetIsEnabled(false);
	if (DraftCancelButton) DraftCancelButton->SetIsEnabled(false);
	if (DraftPreviousButton) DraftPreviousButton->SetIsEnabled(false);
	if (DraftNextButton) DraftNextButton->SetIsEnabled(false);
	if (DraftStatusText)
	{
		DraftStatusText->SetText(FText::FromString(TEXT("EQUIPOS DE ESTA SALA LISTOS  ·  CARGA DEL MAPA EN 5 SEGUNDOS")));
	}
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Draft confirmado; empieza la cuenta atrás de cinco segundos antes de la segunda pantalla de carga."));
}

void UUBFPresentationWidget::OnDraftCancelClicked()
{
	if (State != EPresentationState::Draft)
	{
		return;
	}
	State = EPresentationState::Menu;
	bDraftReady = false;
	bDraftCountdownActive = false;
	PendingTrainingOptions.Empty();
	DestroyDraftShowcase();
	DraftRoot->SetVisibility(ESlateVisibility::Collapsed);
	MenuRoot->SetVisibility(ESlateVisibility::Visible);
	SetMenuPage(EMenuPage::Play);
}

void UUBFPresentationWidget::RefreshCharacterDraft()
{
	if (!DraftRoot || !DraftCharacterNameText || !CharacterIds.IsValidIndex(SelectedCharacterIndex))
	{
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
	if (DraftShowcaseActor)
	{
		DraftShowcaseActor->SetShowcaseActive(true);
	}

	DraftCharacterNameText->SetText(Definition->DisplayName);
	if (DraftCharacterRoleText)
	{
		DraftCharacterRoleText->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  %s"),
			*Definition->CombatStyle.ToString(), *Definition->ShortDescription.ToString())));
	}
	auto DescribeAbility = [](const TCHAR* InputName, const UUBFAbilityDefinition* Ability)
	{
		if (!Ability)
		{
			return FString::Printf(TEXT("%s  ·  NO DISPONIBLE"), InputName);
		}
		const FString Description = Ability->DetailedDescription.IsEmpty()
			? FString::Printf(TEXT("Inflige %.0f de daño base con un alcance de %.0f unidades. Administra el gauge y úsala cuando su efecto corresponda a la distancia y apertura disponibles."),
				Ability->Damage, Ability->Range)
			: Ability->DetailedDescription.ToString();
		return FString::Printf(TEXT("%s  ·  %s\n%s"), InputName,
			*Ability->DisplayName.ToString(), *Description);
	};
	if (DraftPrimarySkillText)
	{
		DraftPrimarySkillText->SetText(FText::FromString(DescribeAbility(TEXT("Q"), Definition->PrimarySkill)));
	}
	if (DraftSecondarySkillText)
	{
		DraftSecondarySkillText->SetText(FText::FromString(DescribeAbility(TEXT("E"), Definition->SecondarySkill)));
	}
	if (DraftUltimateText)
	{
		DraftUltimateText->SetText(FText::FromString(DescribeAbility(TEXT("ULT"), Definition->Ultimate)));
	}
	if (DraftPassiveText)
	{
		DraftPassiveText->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"),
			*Definition->PassiveName.ToString(), *Definition->PassiveDescription.ToString())));
	}

	const int32 TeamSize = FMath::Clamp(SelectedTeamSize, 1, 5);
	const bool bFillAllOpenSlots = SelectedBotFillModeIndex == 2;
	const bool bFillOpponent = SelectedBotFillModeIndex == 1 || bFillAllOpenSlots;
	if (DraftShowcaseActor)
	{
		DraftShowcaseActor->SetTeamLineupCounts(
			1 + (bFillAllOpenSlots ? TeamSize - 1 : 0),
			(bFillOpponent || bFillAllOpenSlots) ? TeamSize : 0);
		if (DraftPreviewImage)
		{
			DraftPreviewImage->SetBrushResourceObject(DraftShowcaseActor->GetPreviewTexture());
		}
	}
	TArray<FString> BlueRoster;
	TArray<FString> RedRoster;
	TSet<FName> UsedBlueIds;
	TSet<FName> UsedRedIds;
	const FName PlayerCharacterId = CharacterIds[SelectedCharacterIndex];
	UsedBlueIds.Add(PlayerCharacterId);
	BlueRoster.Add(FString::Printf(TEXT("JUGADOR  ·  %s"), *Definition->DisplayName.ToString()));
	auto AddBotSlot = [Catalog](TArray<FString>& Roster, TSet<FName>& UsedIds, int32 SlotIndex)
	{
		const FName BotId = Catalog ? Catalog->ChooseCharacterId(UsedIds, SlotIndex) : NAME_None;
		const UUBFCharacterDefinition* BotDefinition = Catalog ? Catalog->FindCharacter(BotId) : nullptr;
		if (!BotDefinition)
		{
			Roster.Add(TEXT("PLAZA SIN PERSONAJE DISPONIBLE"));
			return;
		}
		UsedIds.Add(BotId);
		Roster.Add(FString::Printf(TEXT("BOT %d  ·  %s"), SlotIndex + 1, *BotDefinition->DisplayName.ToString()));
	};
	if (bFillAllOpenSlots)
	{
		for (int32 SlotIndex = 1; SlotIndex < TeamSize; ++SlotIndex)
		{
			AddBotSlot(BlueRoster, UsedBlueIds, SlotIndex);
		}
	}
	if (bFillOpponent || bFillAllOpenSlots)
	{
		for (int32 SlotIndex = 0; SlotIndex < TeamSize; ++SlotIndex)
		{
			AddBotSlot(RedRoster, UsedRedIds, SlotIndex);
		}
	}
	if (RedRoster.IsEmpty())
	{
		RedRoster.Add(TEXT("SIN IA RIVAL CONFIGURADA"));
	}
	if (DraftRedRosterText)
	{
		DraftRedRosterText->SetText(FText::FromString(FString::Join(BlueRoster, TEXT("\n\n"))));
	}
	if (DraftBlueRosterText)
	{
		DraftBlueRosterText->SetText(FText::FromString(FString::Join(RedRoster, TEXT("\n\n"))));
	}
	if (DraftStatusText && !bDraftCountdownActive)
	{
		DraftStatusText->SetText(FText::FromString(TEXT("Elige tu personaje y revisa sus habilidades. Los bots reservan personajes distintos dentro de cada equipo.")));
	}
}

void UUBFPresentationWidget::BeginMatchLoading()
{
	if ((State != EPresentationState::Menu && State != EPresentationState::Draft) || PendingTrainingOptions.IsEmpty())
	{
		return;
	}

	State = EPresentationState::LoadingLevel;
	DestroyDraftShowcase();
	bLoadingScreenActive = true;
	LoadingElapsed = 0.0f;
	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetPercent(0.08f);
	}
	if (LoadingMapTitleText)
	{
		LoadingMapTitleText->SetText(FText::FromString(
			SelectedGameModeIndex == 1 ? TEXT("ARENA GOLEM") : TEXT("ARENA DE COMBATE")));
	}
	if (LoadingStageText)
	{
		LoadingStageText->SetText(FText::FromString(TEXT("CREANDO SALA LOCAL...")));
	}
	if (LoopAudioComponent && LoopAudioComponent->IsPlaying())
	{
		LoopAudioComponent->FadeOut(0.18f, 0.0f);
	}
	if (TitleVoiceAudioComponent && TitleVoiceAudioComponent->IsPlaying())
	{
		TitleVoiceAudioComponent->Stop();
	}
	if (BackgroundPlayer && BackgroundPlayer->IsPlaying())
	{
		BackgroundPlayer->Pause();
	}
	MenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	WelcomeRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (DraftRoot) DraftRoot->SetVisibility(ESlateVisibility::Collapsed);
	LoadingRoot->SetRenderOpacity(0.0f);
	LoadingRoot->SetVisibility(ESlateVisibility::Visible);
	UE_LOG(UBFPresentation::LogUBFPresentation, Log,
		TEXT("Transición a partida local: selección de personaje y sala confirmadas; carga de mapa iniciada."));
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
	RefreshPlayerData();
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
	RefreshPlayerData();
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
		if (ActivePage == EMenuPage::Character)
		{
			CharacterEntryElapsed = 0.0f;
			CharacterNameText->SetRenderOpacity(0.0f);
			CharacterNameText->SetRenderTransform(FWidgetTransform(FVector2D(-36.0f, 0.0f),
				FVector2D(0.92f), FVector2D::ZeroVector, 0.0f));
		}
	}
	if (CharacterPreviewImage && ActivePage == EMenuPage::Character)
	{
		CharacterEntryElapsed = 0.0f;
		CharacterPreviewImage->SetRenderOpacity(0.0f);
		CharacterPreviewImage->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 18.0f),
			FVector2D(0.94f), FVector2D::ZeroVector, 0.0f));
	}
	if (CharacterPortraitMarkText)
	{
		CharacterPortraitMarkText->SetText(FText::FromString(Definition->CharacterID.ToString().ToUpper().Left(3)));
		if (ActivePage == EMenuPage::Character)
		{
			CharacterPortraitMarkText->SetRenderOpacity(0.0f);
			CharacterPortraitMarkText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 24.0f),
				FVector2D(0.82f), FVector2D::ZeroVector, 0.0f));
		}
	}
	if (CharacterPortraitRoleText)
	{
		CharacterPortraitRoleText->SetText(Definition->CombatStyle);
	}
	if (CharacterDetailsText)
	{
		auto DescribeAbility = [](const TCHAR* AbilitySlotLabel, const UUBFAbilityDefinition* Ability)
		{
			if (!Ability)
			{
				return FString::Printf(TEXT("%s  ·  Habilidad no disponible."), AbilitySlotLabel);
			}
			const FString Detail = Ability->DetailedDescription.IsEmpty()
				? FString::Printf(TEXT("Inflige %.0f de daño base a hasta %.0f unidades. Administra el gauge y busca una apertura segura antes de activarla."),
					Ability->Damage, Ability->Range)
				: Ability->DetailedDescription.ToString();
			return FString::Printf(TEXT("%s  ·  %s\n%s"), AbilitySlotLabel, *Ability->DisplayName.ToString(), *Detail);
		};
		const FString PrimaryDetails = DescribeAbility(TEXT("Q · PRIMARIA"), Definition->PrimarySkill);
		const FString SecondaryDetails = DescribeAbility(TEXT("E · SECUNDARIA"), Definition->SecondarySkill);
		const FString UltimateDetails = DescribeAbility(TEXT("F · DEFINITIVA"), Definition->Ultimate);
		const FString Overview = FString::Printf(TEXT("%s\nVIDA %.0f  ·  MOVIMIENTO %.0f  ·  BÁSICO %.0f"),
			*Definition->ShortDescription.ToString(), Definition->BaseStats.MaximumHealth,
			Definition->BaseStats.MaxWalkSpeed, Definition->BaseStats.BasicAttackDamage);
		const FString Details = FString::Printf(TEXT("%s\n\nPASIVA  ·  %s\n%s\n\n%s\n\n%s\n\n%s"),
			*Overview, *Definition->PassiveName.ToString(),
			*Definition->PassiveDescription.ToString(), *PrimaryDetails, *SecondaryDetails, *UltimateDetails);
		CharacterDetailsText->SetText(FText::FromString(Details));
		if (ActivePage == EMenuPage::Character)
		{
			CharacterDetailsText->SetRenderOpacity(0.0f);
			CharacterDetailsText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, 18.0f),
				FVector2D(0.985f), FVector2D::ZeroVector, 0.0f));
		}
	}
	if (IsValid(DraftShowcaseActor))
	{
		DraftShowcaseActor->SetSoloPreviewMode(true);
		DraftShowcaseActor->SetShowcaseActive(ActivePage == EMenuPage::Character || ActivePage == EMenuPage::Inventory);
		if (CharacterPreviewImage)
		{
			CharacterPreviewImage->SetBrushResourceObject(DraftShowcaseActor->GetPreviewTexture());
		}
		if (InventoryCharacterPreviewImage)
		{
			InventoryCharacterPreviewImage->SetBrushResourceObject(DraftShowcaseActor->GetPreviewTexture());
		}
	}
	RefreshCharacterDraft();
}

void UUBFPresentationWidget::OnPreviousTeamSizeClicked()
{
	SelectedTeamSize = SelectedTeamSize <= 1 ? 5 : SelectedTeamSize - 1;
	RefreshMatchSetupText();
}

void UUBFPresentationWidget::OnNextTeamSizeClicked()
{
	SelectedTeamSize = SelectedTeamSize >= 5 ? 1 : SelectedTeamSize + 1;
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
	static const TCHAR* FormatLabels[] = { TEXT("1v1 · 1 por equipo"), TEXT("2v2 · 2 por equipo"),
		TEXT("3v3 · 3 por equipo"), TEXT("4v4 · 4 por equipo"), TEXT("5v5 · 5 por equipo") };
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
		MatchFormatValueText->SetText(FText::FromString(FormatLabels[FMath::Clamp(SelectedTeamSize - 1, 0, 4)]));
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
			? TEXT("Da el último golpe al Golem Dorado; solo quien obtenga la espada puede dañar al Master rival.")
			: FString();
		MatchSetupHintText->SetText(FText::FromString(ModeHint.IsEmpty()
			? BotHint : FString::Printf(TEXT("%s\n%s"), *ModeHint, *BotHint)));
	}
	if (RoomPreviewText)
	{
		const int32 Team0Bots = SelectedBotFillModeIndex == 2 ? FMath::Max(0, SelectedTeamSize - 1) : 0;
		const int32 Team1Bots = SelectedBotFillModeIndex == 1 || SelectedBotFillModeIndex == 2 ? SelectedTeamSize : 0;
		const FString ModeName = SelectedGameModeIndex == 1 ? TEXT("GOLEM") : TEXT("COMBATE");
		RoomPreviewText->SetText(FText::FromString(FString::Printf(TEXT("SALA LOCAL  //  %s  //  %dv%d"),
			*ModeName, SelectedTeamSize, SelectedTeamSize)));
		const UUBFPlayerDataSubsystem* PlayerData = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UUBFPlayerDataSubsystem>() : nullptr;
		const UUBFPlayerSaveGame* Data = PlayerData ? PlayerData->GetPlayerData() : nullptr;
		const FString PlayerName = Data ? Data->PlayerName : TEXT("JUGADOR LOCAL");
		if (RoomRedRosterText)
		{
			RoomRedRosterText->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  TÚ\n%d IA aliada%s"),
				*PlayerName, Team0Bots, Team0Bots == 1 ? TEXT("") : TEXT("s"))));
		}
		if (RoomBlueRosterText)
		{
			RoomBlueRosterText->SetText(FText::FromString(Team1Bots > 0
				? FString::Printf(TEXT("RIVAL LOCAL\n%d IA enemiga%s"), Team1Bots, Team1Bots == 1 ? TEXT("") : TEXT("s"))
				: TEXT("VACÍO\nSin bots rivales")));
		}
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
	if (bShowingClosedBetaNotice && ClosedBetaRoot)
	{
		ClosedBetaNoticeElapsed += InDeltaTime;
		const float Reveal = FMath::Clamp(ClosedBetaNoticeElapsed / 0.24f, 0.0f, 1.0f);
		const float Ease = 1.0f - FMath::Square(1.0f - Reveal);
		ClosedBetaRoot->SetRenderOpacity(Ease);
		ClosedBetaRoot->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, FMath::Lerp(12.0f, 0.0f, Ease)),
			FVector2D(FMath::Lerp(0.975f, 1.0f, Ease)), FVector2D(0.5f), 0.0f));
	}

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
			const float Reveal = FMath::Clamp(MenuElapsed / 0.82f, 0.0f, 1.0f);
			const float C1 = 1.70158f;
			const float C3 = C1 + 1.0f;
			const float Spring = 1.0f + C3 * FMath::Pow(Reveal - 1.0f, 3.0f) +
				C1 * FMath::Pow(Reveal - 1.0f, 2.0f);
			const float Fade = Reveal * Reveal * (3.0f - 2.0f * Reveal);
			LogoImage->SetRenderOpacity(Fade);
			LogoImage->SetRenderTransform(FWidgetTransform(
				FVector2D(0.0f, FMath::Lerp(54.0f, 0.0f, Spring)),
				FVector2D(FMath::Lerp(0.76f, 1.0f, Spring)), FVector2D(0.5f), FMath::Lerp(-4.0f, 0.0f, Spring)));
		}
	}

	if (State == EPresentationState::Menu)
	{
		MenuElapsed += InDeltaTime;
		if (bShowingWelcomeScreen)
		{
			WelcomeElapsed += InDeltaTime;
			const float Reveal = FMath::Clamp(WelcomeElapsed / 0.7f, 0.0f, 1.0f);
			const float EasedReveal = 1.0f - FMath::Pow(1.0f - Reveal, 3.0f);
			WelcomeRoot->SetRenderOpacity(EasedReveal);
			if (LogoImage)
			{
				const float LogoReveal = FMath::Clamp(WelcomeElapsed / 0.90f, 0.0f, 1.0f);
				const float C1 = 1.70158f;
				const float C3 = C1 + 1.0f;
				const float Spring = 1.0f + C3 * FMath::Pow(LogoReveal - 1.0f, 3.0f) +
					C1 * FMath::Pow(LogoReveal - 1.0f, 2.0f);
				const float Fade = LogoReveal * LogoReveal * (3.0f - 2.0f * LogoReveal);
				const float Breath = LogoReveal >= 1.0f ? FMath::Sin(MenuElapsed * 0.78f) : 0.0f;
				LogoImage->SetRenderOpacity(Fade);
				LogoImage->SetRenderTransform(FWidgetTransform(
					FVector2D(0.0f, FMath::Lerp(46.0f, 0.0f, Spring) + Breath * 1.1f),
					FVector2D(FMath::Lerp(0.77f, 1.0f, Spring) + Breath * 0.004f),
					FVector2D(0.5f), FMath::Lerp(-3.5f, 0.0f, Spring)));
			}
		}
		else
		{
			MenuEntranceElapsed += InDeltaTime;
			const float MenuReveal = FMath::Clamp(MenuEntranceElapsed / 0.34f, 0.0f, 1.0f);
			MenuRoot->SetRenderOpacity(1.0f - FMath::Square(1.0f - MenuReveal));

			PageEntranceElapsed += InDeltaTime;
			const float PageReveal = FMath::Clamp(PageEntranceElapsed / 0.34f, 0.0f, 1.0f);
			const float EasedPageReveal = 1.0f - FMath::Square(1.0f - PageReveal);
			PageSwitcher->SetRenderOpacity(EasedPageReveal);
			PageSwitcher->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, FMath::Lerp(15.0f, 0.0f, EasedPageReveal)),
				FVector2D(FMath::Lerp(0.985f, 1.0f, EasedPageReveal)), FVector2D::ZeroVector, 0.0f));

			if (ActivePage == EMenuPage::Character)
			{
				CharacterEntryElapsed += InDeltaTime;
				const float NameProgress = FMath::Clamp(CharacterEntryElapsed / 0.48f, 0.0f, 1.0f);
				const float NameEase = 1.0f - FMath::Square(1.0f - NameProgress);
				if (CharacterNameText)
				{
					CharacterNameText->SetRenderOpacity(NameEase);
					CharacterNameText->SetRenderTransform(FWidgetTransform(FVector2D(FMath::Lerp(-36.0f, 0.0f, NameEase), 0.0f),
						FVector2D(FMath::Lerp(0.92f, 1.0f, NameEase)), FVector2D::ZeroVector, 0.0f));
				}
				const float ArtProgress = FMath::Clamp((CharacterEntryElapsed - 0.04f) / 0.62f, 0.0f, 1.0f);
				const float ArtEase = 1.0f - FMath::Square(1.0f - ArtProgress);
				if (CharacterPortraitMarkText)
				{
					CharacterPortraitMarkText->SetRenderOpacity(ArtEase);
					CharacterPortraitMarkText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, FMath::Lerp(24.0f, 0.0f, ArtEase)),
						FVector2D(FMath::Lerp(0.82f, 1.0f, ArtEase)), FVector2D::ZeroVector, 0.0f));
				}
				if (CharacterPreviewImage)
				{
					CharacterPreviewImage->SetRenderOpacity(ArtEase);
					CharacterPreviewImage->SetRenderTransform(FWidgetTransform(
						FVector2D(0.0f, FMath::Lerp(18.0f, 0.0f, ArtEase)),
						FVector2D(FMath::Lerp(0.94f, 1.0f, ArtEase)), FVector2D::ZeroVector, 0.0f));
				}
				const float DetailsProgress = FMath::Clamp((CharacterEntryElapsed - 0.10f) / 0.52f, 0.0f, 1.0f);
				const float DetailsEase = 1.0f - FMath::Square(1.0f - DetailsProgress);
				if (CharacterDetailsText)
				{
					CharacterDetailsText->SetRenderOpacity(DetailsEase);
					CharacterDetailsText->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, FMath::Lerp(18.0f, 0.0f, DetailsEase)),
						FVector2D(FMath::Lerp(0.985f, 1.0f, DetailsEase)), FVector2D::ZeroVector, 0.0f));
				}
			}
			if (LogoImage)
			{
				const float Breath = FMath::Sin(MenuElapsed * 0.72f);
				LogoImage->SetRenderTransform(FWidgetTransform(FVector2D(0.0f, Breath * 1.0f),
					FVector2D(1.0f + Breath * 0.003f), FVector2D(0.5f), Breath * 0.12f));
				LogoImage->SetRenderOpacity(1.0f);
			}
		}
	}

	if (State == EPresentationState::Draft)
	{
		DraftElapsed += InDeltaTime;
		if (DraftRoot)
		{
			DraftRoot->SetRenderOpacity(FMath::Clamp(DraftElapsed / 0.24f, 0.0f, 1.0f));
		}
		if (bDraftCountdownActive)
		{
			DraftCountdownRemaining = FMath::Max(0.0f, DraftCountdownRemaining - InDeltaTime);
			const int32 Seconds = FMath::CeilToInt(DraftCountdownRemaining);
			if (DraftTimerText)
			{
				DraftTimerText->SetText(FText::FromString(FString::Printf(TEXT("MAPA EN  ·  00:%02d"), Seconds)));
			}
			if (DraftTimerBar)
			{
				DraftTimerBar->SetPercent(DraftCountdownRemaining / 5.0f);
			}
			if (DraftCountdownRemaining <= 0.0f)
			{
				BeginMatchLoading();
			}
		}
		else
		{
			const float Remaining = FMath::Max(0.0f, 30.0f - DraftElapsed);
			const int32 Seconds = FMath::CeilToInt(Remaining);
			if (DraftTimerText)
			{
				DraftTimerText->SetText(FText::FromString(FString::Printf(TEXT("SELECCIÓN  ·  00:%02d"), Seconds)));
			}
			if (DraftTimerBar)
			{
				DraftTimerBar->SetPercent(Remaining / 30.0f);
			}
			if (Remaining <= 0.0f)
			{
				OnDraftReadyClicked();
			}
		}
	}

	if (State == EPresentationState::LoadingLevel)
	{
		LoadingElapsed += InDeltaTime;
		const float Pulse = 0.08f + (FMath::Sin(LoadingElapsed * 3.8f) + 1.0f) * 0.37f;
		LoadingRoot->SetRenderOpacity(FMath::Clamp(LoadingElapsed / 0.18f, 0.0f, 1.0f));
		if (LoadingProgressBar)
		{
			LoadingProgressBar->SetPercent(Pulse);
		}
		if (LoadingStageText)
		{
			const TCHAR* Stage = LoadingElapsed < 0.42f ? TEXT("CREANDO SALA LOCAL...")
				: LoadingElapsed < 0.92f ? TEXT("PREPARANDO EQUIPOS Y BOTS...")
				: SelectedGameModeIndex == 1 ? TEXT("CARGANDO ARENA GOLEM...") : TEXT("CARGANDO ARENA DE COMBATE...");
			LoadingStageText->SetText(FText::FromString(Stage));
		}
		if (LoadingElapsed >= 1.55f)
		{
			bLoadingScreenActive = false;
			UE_LOG(UBFPresentation::LogUBFPresentation, Log,
				TEXT("Preparación visual terminada; solicitando apertura de UBF_Golem_Arena con las opciones de modo y personaje seleccionadas."));
			UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/UBF_Golem_Arena")), true, PendingTrainingOptions);
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
