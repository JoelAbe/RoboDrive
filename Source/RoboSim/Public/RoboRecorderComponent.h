#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoboRunRecord.h"
#include "RoboRecorderComponent.generated.h"

class ARoboVehicle;
class URoboImuComponent;
class URoboOdometryComponent;

/**
 * Records a run to CSV for offline analysis.
 *
 * Samples are buffered and written once at the end: a file write per sample
 * would make the recorder the most expensive thing in the simulation and
 * would itself distort what is being measured.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboRecorderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoboRecorderComponent();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recording")
	void StartRecording();

	/** Stop and write the file. Where it was written, or why it could not be, goes to the log. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recording")
	void StopRecording();

	UFUNCTION(BlueprintPure, Category = "Recording")
	bool IsRecording() const { return bRecording; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording")
	bool bRecordFromStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording",
		meta = (ClampMin = "1.0", ClampMax = "500.0"))
	double RecordRateHz = 50.0;

	/** Prefix for the file name; a timestamp is appended. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recording")
	FString RunName = TEXT("run");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	FRunSample TakeSample(double NowSeconds, int64 Step) const;

	/** Settings that produced this run, written as comments above the data. */
	TArray<FString> BuildMetadataLines() const;

	UPROPERTY(Transient)
	TObjectPtr<ARoboVehicle> Vehicle;

	UPROPERTY(Transient)
	TObjectPtr<URoboOdometryComponent> Odometry;

	UPROPERTY(Transient)
	TObjectPtr<URoboImuComponent> Imu;

	TArray<FString> Rows;
	bool bRecording = false;
	double NextSampleTimeSeconds = 0.0;
};