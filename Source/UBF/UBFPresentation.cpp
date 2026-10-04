#include "UBFPresentation.h"

#include "UBFGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
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
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundWave.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"

namespace UBFPresentation
{
	DEFINE_LOG_CATEGORY_STATIC(LogUBFPresentation, Log, All);
	const FLinearColor AccentColor(0.92f, 0.04f, 0.10f, 1.0f);
	const FLinearColor BackdropColor(0.012f, 0.018f, 0.028f, 1.0f);

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
	UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Árbol visual de presentación construido antes de Slate."));
}

void UUBFPresentationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PrepareResources();
}

void UUBFPresentationWidget::NativeDestruct()
{
	for (UMediaPlayer* Player : { IntroVideoPlayer.Get(), BackgroundPlayer.Get() })
	{
		if (Player)
		{
			Player->Close();
		}
	}
	if (IntroAudioComponent)
	{
		IntroAudioComponent->Stop();
	}
	if (LoopAudioComponent)
	{
		LoopAudioComponent->Stop();
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
		FVector2D::ZeroVector, FVector2D::ZeroVector, 0);
	Backdrop->SetBrushColor(UBFPresentation::BackdropColor);

	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Video"));
	PositionWidget(VideoImage, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FVector2D::ZeroVector,
		FVector2D::ZeroVector, FVector2D::ZeroVector, 1);
	VideoImage->SetVisibility(ESlateVisibility::Collapsed);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingStatus"));
	PositionWidget(StatusText, FAnchors(0.5f, 0.61f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(1000.0f, 72.0f), 2);
	StatusText->SetText(FText::FromString(TEXT("INICIANDO PRESENTACIÓN UBF")));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.90f, 0.94f, 1.0f)));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));

	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Logo"));
	PositionWidget(LogoImage, FAnchors(0.5f, 0.34f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(460.0f, 240.0f), 3);

	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	PositionWidget(PlayButton, FAnchors(0.5f, 0.72f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(270.0f, 68.0f), 4);
	PlayButton->OnClicked.AddDynamic(this, &UUBFPresentationWidget::OnPlayClicked);
	UTextBlock* PlayLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayLabel"));
	PlayLabel->SetText(FText::FromString(TEXT("JUGAR")));
	PlayLabel->SetJustification(ETextJustify::Center);
	PlayLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PlayLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
	PlayButton->SetContent(PlayLabel);
	PlayButton->SetVisibility(ESlateVisibility::Collapsed);

	SessionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SessionUser"));
	PositionWidget(SessionText, FAnchors(0.5f, 0.94f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(1000.0f, 42.0f), 5);
	SessionText->SetJustification(ETextJustify::Center);
	SessionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.84f, 0.86f, 1.0f)));
	SessionText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 15));
	SessionText->SetVisibility(ESlateVisibility::Collapsed);

	BetaErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BetaError"));
	PositionWidget(BetaErrorText, FAnchors(0.5f, 0.84f), FVector2D(0.5f, 0.5f),
		FVector2D::ZeroVector, FVector2D(1000.0f, 82.0f), 6);
	BetaErrorText->SetText(FText::FromString(TEXT("no se ha podido conectar a RPmods-Services\nERROR #BETACLOSE")));
	BetaErrorText->SetJustification(ETextJustify::Center);
	BetaErrorText->SetColorAndOpacity(FSlateColor(UBFPresentation::AccentColor));
	BetaErrorText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
	BetaErrorText->SetVisibility(ESlateVisibility::Collapsed);
}

void UUBFPresentationWidget::PositionWidget(UWidget* Widget, const FAnchors& Anchors,
	const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
{
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(Widget)))
	{
		PanelSlot->SetAnchors(Anchors);
		PanelSlot->SetAlignment(Alignment);
		PanelSlot->SetPosition(Position);
		PanelSlot->SetSize(Size);
		PanelSlot->SetZOrder(ZOrder);
	}
}

bool UUBFPresentationWidget::LoadLogo()
{
	const FString LogoPath = FPaths::ProjectContentDir() / TEXT("Presentation/Images/logo.png");
	TArray<uint8> Compressed;
	if (!FFileHelper::LoadFileToArray(Compressed, *LogoPath))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("No se pudo leer logo.png: %s"), *LogoPath);
		return false;
	}

	IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Png = ImageWrapper.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Pixels;
	if (!Png.IsValid() || !Png->SetCompressed(Compressed.GetData(), Compressed.Num()) ||
		!Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("logo.png existe, pero no se pudo decodificar."));
		return false;
	}

	LogoTexture = UTexture2D::CreateTransient(Png->GetWidth(), Png->GetHeight(), PF_B8G8R8A8);
	if (!LogoTexture)
	{
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
	LoadLogo();
	State = EPresentationState::Intro;
	IntroElapsed = 0.0f;
	StatusText->SetVisibility(ESlateVisibility::Visible);
	LogoImage->SetVisibility(ESlateVisibility::Visible);

	IntroSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Presentation/Audio/musicintro.musicintro"));
	LoopSound = LoadObject<USoundWave>(nullptr, TEXT("/Game/Presentation/Audio/musicintrobucle.musicintrobucle"));
	if (LoopSound)
	{
		LoopSound->bLooping = true;
	}
	if (IntroSound)
	{
		IntroAudioComponent = UGameplayStatics::SpawnSound2D(this, IntroSound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (IntroAudioComponent)
		{
			IntroAudioComponent->OnAudioFinished.AddDynamic(this, &UUBFPresentationWidget::OnIntroAudioEnded);
			UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Iniciando SoundWave musicintro."));
		}
		else
		{
			UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("No se pudo crear el audio 2D de la intro."));
		}
	}
	else
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("No se encontró el SoundWave musicintro."));
		StartLoopMusic();
	}

	const FString IntroVideoPath = FPaths::ProjectContentDir() / TEXT("Presentation/Videos/intro.mp4");
	if (!IFileManager::Get().FileExists(*IntroVideoPath))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("No se encontró intro.mp4: %s"), *IntroVideoPath);
		BeginMenu();
		return;
	}

	IntroVideoSource = UBFPresentation::MakeFileSource(this, IntroVideoPath);
	IntroVideoPlayer = UBFPresentation::MakePlayer(this);
	IntroTexture = UBFPresentation::MakeTexture(this, IntroVideoPlayer);
	IntroVideoPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnIntroVideoOpened);
	IntroVideoPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnIntroVideoOpenFailed);

	if (!IntroVideoPlayer->OpenSource(IntroVideoSource))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("Unreal rechazó intro.mp4 al abrirlo."));
		BeginMenu();
	}
}

void UUBFPresentationWidget::OnIntroVideoOpened(FString OpenedUrl)
{
	if (State != EPresentationState::Intro || !IntroVideoPlayer)
	{
		return;
	}
	if (!IntroVideoPlayer->IsReady() || IntroVideoPlayer->GetNumTracks(EMediaPlayerTrack::Video) < 1)
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("intro.mp4 no contiene una pista de video legible."));
		BeginMenu();
		return;
	}

	const int32 VideoFormat = IntroVideoPlayer->GetTrackFormat(EMediaPlayerTrack::Video, 0);
	const float FrameRate = IntroVideoPlayer->GetVideoTrackFrameRate(0, VideoFormat);
	IntroCutTime = FTimespan::FromSeconds(13.0 + (FrameRate > 0.0f ? 6.0 / FrameRate : 0.2));
	if (IntroVideoPlayer->GetDuration() > FTimespan::Zero() &&
		IntroVideoPlayer->GetDuration() < IntroCutTime)
	{
		IntroCutTime = IntroVideoPlayer->GetDuration();
	}

	VideoImage->SetBrushResourceObject(IntroTexture);
	VideoImage->SetVisibility(ESlateVisibility::Visible);
	LogoImage->SetVisibility(ESlateVisibility::Visible);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	bIntroVideoReady = true;
	bIntroVideoHasAdvanced = false;
	if (!IntroVideoPlayer->Play())
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("intro.mp4 no pudo iniciar la reproducción."));
		BeginMenu();
	}
	else
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Reproduciendo intro.mp4 hasta %s."),
			*IntroCutTime.ToString());
	}
}

void UUBFPresentationWidget::OnIntroVideoOpenFailed(FString FailedUrl)
{
	UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("Falló intro.mp4 (%s); se mostrará el menú estático."),
		*FailedUrl);
	if (State == EPresentationState::Intro)
	{
		BeginMenu();
	}
}

void UUBFPresentationWidget::OnIntroAudioEnded()
{
	if (State != EPresentationState::BetaError)
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Terminó musicintro; iniciando musicintrobucle."));
		StartLoopMusic();
	}
}

void UUBFPresentationWidget::StartLoopMusic()
{
	if (LoopAudioComponent || !LoopSound)
	{
		return;
	}
	LoopAudioComponent = UGameplayStatics::SpawnSound2D(this, LoopSound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	if (!LoopAudioComponent)
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Error, TEXT("No se pudo iniciar el audio en bucle."));
	}
}

void UUBFPresentationWidget::BeginMenu()
{
	if (State != EPresentationState::Intro)
	{
		return;
	}
	State = EPresentationState::Menu;
	if (IntroVideoPlayer)
	{
		IntroVideoPlayer->Close();
	}
	VideoImage->SetVisibility(ESlateVisibility::Collapsed);
	LogoImage->SetVisibility(ESlateVisibility::Visible);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	PlayButton->SetVisibility(ESlateVisibility::Visible);
	SessionText->SetVisibility(ESlateVisibility::Visible);

	const UUBFGameInstance* UBFInstance = GetGameInstance<UUBFGameInstance>();
	const FString UserName = UBFInstance ? UBFInstance->GetSessionUserName() : TEXT("Usuario de desarrollo");
	SessionText->SetText(FText::FromString(FString::Printf(TEXT("Sesión iniciada: %s"), *UserName)));
	StartBackgroundVideo();
}

void UUBFPresentationWidget::StartBackgroundVideo()
{
	if (bBackgroundRequested)
	{
		return;
	}
	bBackgroundRequested = true;
	const FString BackgroundPath = FPaths::ProjectContentDir() / TEXT("Presentation/Videos/videobackground.mp4");
	if (!IFileManager::Get().FileExists(*BackgroundPath))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("No se encontró videobackground.mp4; se conserva el fondo estático."));
		return;
	}

	BackgroundSource = UBFPresentation::MakeFileSource(this, BackgroundPath);
	BackgroundPlayer = UBFPresentation::MakePlayer(this);
	BackgroundTexture = UBFPresentation::MakeTexture(this, BackgroundPlayer);
	BackgroundPlayer->OnMediaOpened.AddDynamic(this, &UUBFPresentationWidget::OnBackgroundOpened);
	BackgroundPlayer->OnMediaOpenFailed.AddDynamic(this, &UUBFPresentationWidget::OnBackgroundOpenFailed);
	if (!BackgroundPlayer->OpenSource(BackgroundSource))
	{
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("Unreal no pudo abrir videobackground.mp4."));
	}
}

void UUBFPresentationWidget::OnBackgroundOpened(FString OpenedUrl)
{
	if (State != EPresentationState::Menu || !BackgroundPlayer)
	{
		return;
	}
	BackgroundPlayer->SetLooping(true);
	VideoImage->SetBrushResourceObject(BackgroundTexture);
	VideoImage->SetVisibility(ESlateVisibility::Visible);
	if (!BackgroundPlayer->Play())
	{
		VideoImage->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("No se pudo iniciar el video de fondo."));
	}
}

void UUBFPresentationWidget::OnBackgroundOpenFailed(FString FailedUrl)
{
	UE_LOG(UBFPresentation::LogUBFPresentation, Warning, TEXT("Falló el video de fondo (%s); se conserva el menú estático."),
		*FailedUrl);
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
	if (State == EPresentationState::Intro)
	{
		IntroElapsed += InDeltaTime;
		if (!bIntroVideoHasAdvanced && bIntroVideoReady && IntroVideoPlayer &&
			IntroVideoPlayer->GetTime() >= FTimespan::FromMilliseconds(250))
		{
			bIntroVideoHasAdvanced = true;
			LogoImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (IntroElapsed >= static_cast<float>(IntroCutTime.GetTotalSeconds()))
		{
			UE_LOG(UBFPresentation::LogUBFPresentation, Log, TEXT("Terminó el segmento de intro; se abre el menú."));
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
