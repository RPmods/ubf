#include "UBFPresentation.h"

#include "UBFGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TextureResource.h"
#include "Styling/CoreStyle.h"

namespace UBFPresentation
{
	DEFINE_LOG_CATEGORY_STATIC(LogUBFPresentation, Log, All);
	constexpr float PreparationTimeoutSeconds = 90.0f;
	const FLinearColor AccentColor(0.92f, 0.04f, 0.10f, 1.0f);

	UFileMediaSource* MakeFileSource(UObject* Outer, const FString& Path)
	{
		UFileMediaSource* Source = NewObject<UFileMediaSource>(Outer);
		Source->SetFilePath(Path);
		return Source;
	}

	UMediaPlayer* MakePlayer(UObject* Outer, bool bNativeAudio)
	{
		UMediaPlayer* Player = NewObject<UMediaPlayer>(Outer);
		Player->PlayOnOpen = false;
		Player->NativeAudioOut = bNativeAudio;
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
	UE_LOG(LogUBFPresentation, Log, TEXT("PresentationController iniciado; creando widget de presentación."));
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));

	PresentationWidget = CreateWidget<UUBFPresentationWidget>(this);
	if (PresentationWidget)
	{
		PresentationWidget->AddToViewport(1000);
	}
	else
	{
		UE_LOG(LogUBFPresentation, Error, TEXT("No se pudo crear UUBFPresentationWidget."));
	}
}

void UUBFPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateCanTick();
	BuildInterface();
	PrepareResources();
}

void UUBFPresentationWidget::NativeDestruct()
{
	for (UMediaPlayer* Player : { IntroVideoPlayer.Get(), IntroAudioPlayer.Get(), LoopAudioPlayer.Get(), BackgroundPlayer.Get() })
	{
		if (Player)
		{
			Player->Close();
		}
	}
	Super::NativeDestruct();
}

void UUBFPresentationWidget::BuildInterface()
{
	if (!WidgetTree)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PresentationRoot"));
	WidgetTree->RootWidget = RootCanvas;
	Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LoadingBackdrop"));
	PositionWidget(Backdrop, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector);
	Backdrop->SetBrushColor(FLinearColor::Black);

	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Video"));
	PositionWidget(VideoImage, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingStatus"));
	PositionWidget(StatusText, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.0f, 0.0f), FVector2D(1000.0f, 110.0f));
	StatusText->SetText(FText::FromString(TEXT("PREPARANDO PRESENTACIÓN UBF...")));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));

	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Logo"));
	PositionWidget(LogoImage, FAnchors(0.5f, 0.26f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(460.0f, 240.0f));
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);

	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	PositionWidget(PlayButton, FAnchors(0.5f, 0.72f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(270.0f, 68.0f));
	PlayButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPlayClicked);
	UTextBlock* PlayLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayLabel"));
	PlayLabel->SetText(FText::FromString(TEXT("JUGAR")));
	PlayLabel->SetJustification(ETextJustify::Center);
	PlayLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PlayLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
	PlayButton->SetContent(PlayLabel);
	PlayButton->SetVisibility(ESlateVisibility::Collapsed);

	SessionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SessionUser"));
	PositionWidget(SessionText, FAnchors(0.5f, 0.94f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(1000.0f, 42.0f));
	SessionText->SetJustification(ETextJustify::Center);
	SessionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.84f, 0.86f, 1.0f)));
	SessionText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 15));
	SessionText->SetVisibility(ESlateVisibility::Collapsed);

	BetaErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BetaError"));
	PositionWidget(BetaErrorText, FAnchors(0.5f, 0.84f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(1000.0f, 82.0f));
	BetaErrorText->SetText(FText::FromString(TEXT("no se ha podido conectar a RPmods-Services\nERROR #BETACLOSE")));
	BetaErrorText->SetJustification(ETextJustify::Center);
	BetaErrorText->SetColorAndOpacity(FSlateColor(UBFPresentation::AccentColor));
	BetaErrorText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	BetaErrorText->SetVisibility(ESlateVisibility::Collapsed);
}

void UUBFPresentationWidget::PositionWidget(UWidget* Widget, const FAnchors& Anchors,
	const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size)
{
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(Widget)))
	{
		PanelSlot->SetAnchors(Anchors);
		PanelSlot->SetAlignment(Alignment);
		PanelSlot->SetPosition(Position);
		PanelSlot->SetSize(Size);
	}
}

bool UUBFPresentationWidget::LoadLogo()
{
	const FString LogoPath = FPaths::ProjectContentDir() / TEXT("Presentation/Images/logo.png");
	TArray<uint8> Compressed;
	if (!FFileHelper::LoadFileToArray(Compressed, *LogoPath))
	{
		ShowDevelopmentError(FText::FromString(FString::Printf(TEXT("No se pudo leer logo.png: %s"), *LogoPath)));
		return false;
	}

	IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Png = ImageWrapper.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Pixels;
	if (!Png.IsValid() || !Png->SetCompressed(Compressed.GetData(), Compressed.Num()) ||
		!Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
	{
		ShowDevelopmentError(FText::FromString(TEXT("logo.png existe, pero Unreal no pudo decodificarlo.")));
		return false;
	}

	LogoTexture = UTexture2D::CreateTransient(Png->GetWidth(), Png->GetHeight(), PF_B8G8R8A8);
	if (!LogoTexture)
	{
		ShowDevelopmentError(FText::FromString(TEXT("No se pudo preparar la textura del logo.")));
		return false;
	}
	LogoTexture->SRGB = true;
	void* TextureData = LogoTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num());
	LogoTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	LogoTexture->UpdateResource();
	LogoImage->SetBrushFromTexture(LogoTexture, false);
	return true;
}

void UUBFPresentationWidget::PrepareResources()
{
	const FString ContentDirectory = FPaths::ProjectContentDir();
	const FString IntroVideoPath = ContentDirectory / TEXT("Presentation/Videos/intro.mp4");
	const FString BackgroundPath = ContentDirectory / TEXT("Presentation/Videos/videobackground.mp4");
	const FString IntroAudioPath = ContentDirectory / TEXT("Presentation/Audio/musicintro.mp3");
	const FString LoopAudioPath = ContentDirectory / TEXT("Presentation/Audio/musicintrobucle.mp3");
	const FString LogoPath = ContentDirectory / TEXT("Presentation/Images/logo.png");

	const TArray<TPair<FString, FString>> RequiredFiles = {
		{ TEXT("intro.mp4"), IntroVideoPath },
		{ TEXT("videobackground.mp4"), BackgroundPath },
		{ TEXT("musicintro.mp3"), IntroAudioPath },
		{ TEXT("musicintrobucle.mp3"), LoopAudioPath },
		{ TEXT("logo.png"), LogoPath }
	};
	for (const TPair<FString, FString>& File : RequiredFiles)
	{
		if (!IFileManager::Get().FileExists(*File.Value))
		{
			ShowDevelopmentError(FText::FromString(FString::Printf(
				TEXT("Falta el recurso requerido %s (%s)."), *File.Key, *File.Value)));
			return;
		}
	}
	if (!LoadLogo())
	{
		return;
	}

	IntroVideoSource = UBFPresentation::MakeFileSource(this, IntroVideoPath);
	BackgroundSource = UBFPresentation::MakeFileSource(this, BackgroundPath);
	IntroAudioSource = UBFPresentation::MakeFileSource(this, IntroAudioPath);
	LoopAudioSource = UBFPresentation::MakeFileSource(this, LoopAudioPath);
	IntroVideoPlayer = UBFPresentation::MakePlayer(this, false);
	BackgroundPlayer = UBFPresentation::MakePlayer(this, false);
	IntroAudioPlayer = UBFPresentation::MakePlayer(this, true);
	LoopAudioPlayer = UBFPresentation::MakePlayer(this, true);
	IntroTexture = UBFPresentation::MakeTexture(this, IntroVideoPlayer);
	BackgroundTexture = UBFPresentation::MakeTexture(this, BackgroundPlayer);

	IntroVideoPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnIntroVideoOpened);
	IntroAudioPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnIntroAudioOpened);
	LoopAudioPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnLoopAudioOpened);
	IntroAudioPlayer->OnEndReached.AddDynamic(this, &UUBFPresentationWidget::OnIntroAudioEnded);
	BackgroundPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnBackgroundOpened);
	IntroVideoPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnMediaOpenFailed);
	IntroAudioPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnMediaOpenFailed);
	LoopAudioPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnMediaOpenFailed);
	BackgroundPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnMediaOpenFailed);

	UpdateStatus(FText::FromString(TEXT("CARGANDO Y PREPARANDO MEDIOS...")));
	const bool bIntroVideoOpened = IntroVideoPlayer->OpenSource(IntroVideoSource);
	const bool bIntroAudioOpened = IntroAudioPlayer->OpenSource(IntroAudioSource);
	const bool bLoopAudioOpened = LoopAudioPlayer->OpenSource(LoopAudioSource);
	const bool bBackgroundOpened = BackgroundPlayer->OpenSource(BackgroundSource);
	UE_LOG(LogUBFPresentation, Log, TEXT("OpenSource: intro_video=%d intro_audio=%d loop_audio=%d background=%d"),
		bIntroVideoOpened, bIntroAudioOpened, bLoopAudioOpened, bBackgroundOpened);
	if (!bIntroVideoOpened || !bIntroAudioOpened || !bLoopAudioOpened || !bBackgroundOpened)
	{
		ShowDevelopmentError(FText::FromString(TEXT("Un MediaPlayer rechazó un recurso al abrirlo.")));
	}
}

void UUBFPresentationWidget::OnIntroVideoOpened(FString OpenedUrl)
{
	bIntroVideoReady = true;
	TryStartIntro();
}

void UUBFPresentationWidget::OnIntroAudioOpened(FString OpenedUrl)
{
	bIntroAudioReady = true;
	TryStartIntro();
}

void UUBFPresentationWidget::OnLoopAudioOpened(FString OpenedUrl)
{
	bLoopAudioReady = true;
	TryStartIntro();
}

void UUBFPresentationWidget::OnBackgroundOpened(FString OpenedUrl)
{
	bBackgroundReady = true;
	TryStartIntro();
}

void UUBFPresentationWidget::OnMediaOpenFailed(FString FailedUrl)
{
	ShowDevelopmentError(FText::FromString(FString::Printf(TEXT("No se pudo preparar el medio: %s"), *FailedUrl)));
}

void UUBFPresentationWidget::TryStartIntro()
{
	if (State != EPresentationState::Preparing || !bIntroVideoReady || !bIntroAudioReady || !bLoopAudioReady || !bBackgroundReady)
	{
		return;
	}
	if (!IntroVideoPlayer->IsReady() || !IntroAudioPlayer->IsReady() || !LoopAudioPlayer->IsReady() || !BackgroundPlayer->IsReady() ||
		IntroVideoPlayer->GetNumTracks(EMediaPlayerTrack::Video) < 1 ||
		BackgroundPlayer->GetNumTracks(EMediaPlayerTrack::Video) < 1 ||
		IntroAudioPlayer->GetNumTracks(EMediaPlayerTrack::Audio) < 1 ||
		LoopAudioPlayer->GetNumTracks(EMediaPlayerTrack::Audio) < 1)
	{
		ShowDevelopmentError(FText::FromString(TEXT("Los recursos se abrieron, pero falta una pista de vídeo o audio requerida.")));
		return;
	}

	const int32 VideoFormat = IntroVideoPlayer->GetTrackFormat(EMediaPlayerTrack::Video, 0);
	IntroFrameRate = IntroVideoPlayer->GetVideoTrackFrameRate(0, VideoFormat);
	if (IntroFrameRate <= 0.0f)
	{
		ShowDevelopmentError(FText::FromString(TEXT("No se pudo leer la frecuencia de cuadros de intro.mp4.")));
		return;
	}
	IntroCutTime = FTimespan::FromSeconds(13.0 + 6.0 / static_cast<double>(IntroFrameRate));
	if (IntroVideoPlayer->GetDuration() < IntroCutTime)
	{
		ShowDevelopmentError(FText::FromString(TEXT("intro.mp4 termina antes del punto 00:00:13:06.")));
		return;
	}

	if (IntroVideoPlayer->SupportsPlaybackTimeRange())
	{
		const TRange<FTimespan> PlaybackRange(
			TRangeBound<FTimespan>::Inclusive(FTimespan::Zero()),
			TRangeBound<FTimespan>::Exclusive(IntroCutTime));
		IntroVideoPlayer->SetPlaybackTimeRange(PlaybackRange);
	}
	bResourcesValidated = true;
	UE_LOG(LogUBFPresentation, Log, TEXT("Medios validados; iniciando intro sin esperar un Seek opcional."));
	StartIntroPlayback();
}

void UUBFPresentationWidget::StartIntroPlayback()
{
	if (State != EPresentationState::Preparing || !bResourcesValidated)
	{
		return;
	}

	State = EPresentationState::Intro;
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	VideoImage->SetBrushResourceObject(IntroTexture);
	if (!IntroVideoPlayer->Play() || !IntroAudioPlayer->Play())
	{
		ShowDevelopmentError(FText::FromString(TEXT("No se pudo iniciar la introducción de vídeo y audio.")));
	}
}

void UUBFPresentationWidget::OnIntroAudioEnded()
{
	if (State != EPresentationState::DevelopmentError)
	{
		StartLoopMusic();
	}
}

void UUBFPresentationWidget::StartLoopMusic()
{
	if (bLoopAudioStarted || !bLoopAudioReady || State == EPresentationState::DevelopmentError)
	{
		return;
	}

	bLoopAudioStarted = true;
	if (IntroAudioPlayer)
	{
		IntroAudioPlayer->Close();
	}
	LoopAudioPlayer->SetLooping(true);
	if (!LoopAudioPlayer->Play())
	{
		bLoopAudioStarted = false;
		ShowDevelopmentError(FText::FromString(TEXT("No se pudo iniciar musicintrobucle.mp3.")));
	}
}
void UUBFPresentationWidget::BeginMenu()
{
	if (State != EPresentationState::Intro)
	{
		return;
	}
	State = EPresentationState::Menu;
	IntroVideoPlayer->Pause();
	BackgroundPlayer->SetLooping(true);
	VideoImage->SetBrushResourceObject(BackgroundTexture);
	LogoImage->SetVisibility(ESlateVisibility::Visible);
	PlayButton->SetVisibility(ESlateVisibility::Visible);
	SessionText->SetVisibility(ESlateVisibility::Visible);
	const UUBFGameInstance* UBFInstance = GetGameInstance<UUBFGameInstance>();
	const FString UserName = UBFInstance ? UBFInstance->GetSessionUserName() : TEXT("Usuario de desarrollo");
	SessionText->SetText(FText::FromString(FString::Printf(TEXT("Sesión iniciada: %s"), *UserName)));
	if (!BackgroundPlayer->Play())
	{
		ShowDevelopmentError(FText::FromString(TEXT("No se pudo iniciar el vídeo de menú.")));
	}
}

void UUBFPresentationWidget::ShowDevelopmentError(const FText& Reason)
{
	if (State == EPresentationState::DevelopmentError)
	{
		return;
	}
	State = EPresentationState::DevelopmentError;
	for (UMediaPlayer* Player : { IntroVideoPlayer.Get(), IntroAudioPlayer.Get(), LoopAudioPlayer.Get(), BackgroundPlayer.Get() })
	{
		if (Player)
		{
			Player->Close();
		}
	}
	if (LogoImage) LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	if (PlayButton) PlayButton->SetVisibility(ESlateVisibility::Collapsed);
	if (SessionText) SessionText->SetVisibility(ESlateVisibility::Collapsed);
	if (BetaErrorText) BetaErrorText->SetVisibility(ESlateVisibility::Collapsed);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("ERROR DE DESARROLLO\n%s"), *Reason.ToString())));
		StatusText->SetColorAndOpacity(FSlateColor(UBFPresentation::AccentColor));
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 19));
		StatusText->SetVisibility(ESlateVisibility::Visible);
	}
}

void UUBFPresentationWidget::UpdateStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetVisibility(ESlateVisibility::Visible);
	}
}

void UUBFPresentationWidget::OnPlayClicked()
{
	if (State != EPresentationState::Menu && State != EPresentationState::BetaError)
	{
		return;
	}
	State = EPresentationState::BetaError;
	BetaErrorText->SetVisibility(ESlateVisibility::Visible);
}

void UUBFPresentationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (State == EPresentationState::Preparing)
	{
		PreparationElapsed += InDeltaTime;
		if (PreparationElapsed >= UBFPresentation::PreparationTimeoutSeconds)
		{
			ShowDevelopmentError(FText::FromString(TEXT("La preparación de medios excedió 90 s. Revisa los archivos y el decodificador.")));
		}
		return;
	}
	if (State == EPresentationState::Intro)
	{
		if (IntroVideoPlayer && IntroVideoPlayer->IsReady() && IntroVideoPlayer->GetTime() >= IntroCutTime)
		{
			BeginMenu();
		}
		return;
	}
	if (State == EPresentationState::Menu || State == EPresentationState::BetaError)
	{
		MenuElapsed += InDeltaTime;
		if (LogoImage)
		{
			FVector2D Translation(0.0f, FMath::Sin(MenuElapsed * 0.65f) * 3.0f);
			float Opacity = 1.0f;
			if (GlitchRemaining <= 0.0f && MenuElapsed >= NextGlitchTime)
			{
				GlitchRemaining = FMath::FRandRange(0.035f, 0.085f);
				NextGlitchTime = MenuElapsed + FMath::FRandRange(3.5f, 7.0f);
			}
			if (GlitchRemaining > 0.0f)
			{
				GlitchRemaining -= InDeltaTime;
				Translation.X += FMath::FRandRange(-7.0f, 7.0f);
				Opacity = FMath::FRandRange(0.82f, 1.0f);
			}
			LogoImage->SetRenderTransform(FWidgetTransform(Translation, FVector2D(1.0f), FVector2D::ZeroVector, 0.0f));
			LogoImage->SetRenderOpacity(Opacity);
		}
	}
}
