#include "RoboCameraComponent.h"

#include "RoboSim.h"
#include "RoboSimSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"

DECLARE_CYCLE_STAT(TEXT("Camera Capture"), STAT_RoboCameraCapture, STATGROUP_RoboSim);

URoboCameraComponent::URoboCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Capture on our schedule rather than every frame: a sensor has a frame
	// rate of its own, and it is rarely the renderer's.
	bCaptureEveryFrame = false;
	bCaptureOnMovement = false;

	// Final colour as a display-ready image, which is what a real camera
	// delivers. Depth and object masks are other capture sources, useful later
	// for generating labelled training data.
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	FOVAngle = 90.0f;
}

void URoboCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// An asset assigned in the editor wins, because it can be opened during
	// play to watch the feed live.
	if (TextureTarget == nullptr)
	{
		OwnedRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		OwnedRenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
		OwnedRenderTarget->ClearColor = FLinearColor::Black;
		OwnedRenderTarget->bAutoGenerateMips = false;
		OwnedRenderTarget->InitAutoFormat(ResolutionX, ResolutionY);
		OwnedRenderTarget->UpdateResourceImmediate(true);

		TextureTarget = OwnedRenderTarget;
	}

	if (bHideOwner && GetOwner() != nullptr)
	{
		HiddenActors.Add(GetOwner());
	}

	UE_LOG(LogRoboSim, Log, TEXT("Camera running at %dx%d, %.1f Hz"),
		TextureTarget->SizeX, TextureTarget->SizeY, FrameRateHz);
}

void URoboCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (!bEnabled || Sim == nullptr || TextureTarget == nullptr)
	{
		return;
	}

	const double NowSeconds = Sim->GetSimTimeSeconds();
	if (NowSeconds < NextCaptureTimeSeconds)
	{
		return;
	}

	NextCaptureTimeSeconds = NowSeconds + 1.0 / FrameRateHz;

	{
		// Measures queuing the capture, not the rendering. The GPU work happens
		// afterwards, which is why this counter will look suspiciously cheap
		// and the real cost shows up in "GPU Time" from stat unit.
		SCOPE_CYCLE_COUNTER(STAT_RoboCameraCapture);
		CaptureScene();
	}

	LastFrameTimeSeconds = NowSeconds;
}