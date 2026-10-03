#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "RoboCameraComponent.generated.h"

class UTextureRenderTarget2D;

/**
 * A simulated camera.
 *
 * Renders the scene into a texture at a fixed rate on the simulation clock.
 * Unlike the other sensors its cost falls on the GPU, and it scales with
 * pixels and scene complexity rather than with sample count.
 *
 * Pixels are not read back to the CPU. Nothing consumes images yet, and a
 * readback is the expensive part: Phase 3 adds it when the external API needs
 * to send frames to a client.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboCameraComponent : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	URoboCameraComponent();

	/** Simulation time of the most recent frame, s. */
	UFUNCTION(BlueprintPure, Category = "Robo Camera")
	double GetLastFrameTimeSeconds() const { return LastFrameTimeSeconds; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robo Camera")
	bool bEnabled = true;

	/** Frames per second. Real machine-vision cameras run 10 to 60 Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robo Camera",
		meta = (ClampMin = "0.1", ClampMax = "120.0"))
	double FrameRateHz = 10.0;

	/** Used only when no render target is assigned in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robo Camera", meta = (ClampMin = "16"))
	int32 ResolutionX = 640;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robo Camera", meta = (ClampMin = "16"))
	int32 ResolutionY = 480;

	/** Keep the robot's own body out of its camera's view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robo Camera")
	bool bHideOwner = true;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> OwnedRenderTarget;

	double NextCaptureTimeSeconds = 0.0;
	double LastFrameTimeSeconds = 0.0;
};