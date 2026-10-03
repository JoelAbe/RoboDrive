#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoboReplayComponent.generated.h"

class ARoboVehicle;

/** One recorded moment: what was commanded, and where the robot actually was. */
struct FReplayEntry
{
	double TimeSeconds = 0.0;
	double LinearMS = 0.0;
	double AngularRadS = 0.0;
	double TrueX = 0.0;
	double TrueY = 0.0;
	double TrueYawRad = 0.0;
};

/**
 * Replays a recorded run's commands and measures how far the outcome diverges
 * from the original.
 *
 * Commands are submitted as External, which outranks the autonomy, so a replay
 * takes over without anything needing to be switched off. Divergence is the
 * distance between where the robot is now and where it was at the same point
 * in the original run: zero means the simulation is reproducible.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboReplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoboReplayComponent();

	/** File name inside Saved/RoboDrive/Runs, or an absolute path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	FString ReplayFile = TEXT("Run_Baseline.csv");

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Replay")
	void StartReplay();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Replay")
	void StopReplay();

	UFUNCTION(BlueprintPure, Category = "Replay")
	double GetDivergenceM() const { return DivergenceM; }

	UFUNCTION(BlueprintPure, Category = "Replay")
	double GetMaxDivergenceM() const { return MaxDivergenceM; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawReadout = true;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool LoadFile(const FString& Path);

	UPROPERTY(Transient)
	TObjectPtr<ARoboVehicle> Vehicle;

	TArray<FReplayEntry> Entries;
	int32 NextIndex = 0;
	double StartOffsetSeconds = 0.0;
	double DivergenceM = 0.0;
	double MaxDivergenceM = 0.0;
	bool bReplaying = false;
};