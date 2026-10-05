#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UBFDraftShowcaseActor.generated.h"

class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

/** Local, inexpensive scene-capture lineup used only on the character-draft screen. */
UCLASS(Transient)
class UBF_API AUBFDraftShowcaseActor : public AActor
{
	GENERATED_BODY()

public:
	AUBFDraftShowcaseActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void SetTeamLineupCounts(int32 RedTeamCount, int32 BlueTeamCount);
	void SetSoloPreviewMode(bool bEnable);
	void SetShowcaseActive(bool bActive);
	UTextureRenderTarget2D* GetPreviewTexture() const { return PreviewTexture; }

private:
	void RebuildLineup();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> Fighters;

	int32 RedTeamCount = 1;
	int32 BlueTeamCount = 0;
	bool bSoloPreviewMode = true;
	bool bShowcaseActive = true;
	float EntryElapsed = 0.0f;
	float CaptureElapsed = 0.0f;
};
