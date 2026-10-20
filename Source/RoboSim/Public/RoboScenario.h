#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoboScenario.generated.h"

class ARoboVehicle;
class FJsonObject;

/**
 * Builds the world a scenario describes, and remembers it so it can be
 * restarted.
 *
 * A world subsystem in RoboSim, like the mission and the weather, so that
 * every client can reach it: the external API, which receives scenarios over
 * the network, and the operator view, which reads them from the Scenarios
 * folder. Either way the simulation is handed a scenario's contents, never a
 * path, so it never reads anyone's filesystem (ADR 0008).
 */
UCLASS()
class ROBOSIM_API URoboScenarioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null outside a game or PIE world. */
	static URoboScenarioSubsystem* Get(const UObject* WorldContext);

	/**
	 * Check a scenario without changing anything. Static and needs no world,
	 * so it can be unit tested directly.
	 */
	static bool Validate(const FJsonObject& Scenario, FString& OutError);

	/**
	 * Build the world a scenario describes: remove the previous scenario's
	 * obstacles, place the new ones, move the robot to the start pose, and set
	 * the goal and the weather. Validates everything first; on failure, fills
	 * OutError and leaves the previous world standing.
	 *
	 * FallbackName is used when the scenario has no name of its own.
	 */
	bool Load(const TSharedRef<FJsonObject>& Scenario, FString& OutError,
		const FString& FallbackName = TEXT("unnamed"));

	/**
	 * Start the current scenario again from the beginning. With no scenario
	 * loaded, return the robot to where it spawned. Simulation time keeps
	 * running: sensors have already scheduled their next sample against it.
	 */
	bool Restart(FString& OutError);

	/**
	 * Remove the current scenario entirely: its obstacles, goal and fog go,
	 * and the robot returns to where it spawned. The world is left as the
	 * level made it. Simulation time keeps running, as with Restart.
	 */
	bool Unload(FString& OutError);

	/** Return the robot to where it spawned, stopped, with fresh dead reckoning. */
	bool ResetRobot(FString& OutError);

	/** Move the robot to a REP-103 pose, stopped, with fresh dead reckoning. */
	bool ResetRobotTo(double X, double Y, double YawRad, FString& OutError);

	bool HasScenario() const { return Current.IsValid(); }

	/** The current scenario's name, or empty with none loaded. */
	FString GetScenarioName() const { return CurrentName; }

	/** How many obstacles the current scenario placed. */
	int32 GetObstacleCount() const { return ScenarioActors.Num(); }

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	/** The robot to place. Found lazily, recording where it spawned the first time. */
	ARoboVehicle* FindVehicle();

	/** Teleport the robot, stop it, and restart its dead reckoning there. */
	void TeleportRobot(ARoboVehicle& Vehicle, const FVector& LocationUU, const FRotator& Rotation);

	/** Remove everything the previous scenario spawned. */
	void ClearObstacles();

	/** Height of the floor at a point, Unreal units. Ignores scenario obstacles. */
	double GroundHeightUU(double XUU, double YUU) const;

	/** The scenario last loaded, kept so it can be restarted. */
	TSharedPtr<FJsonObject> Current;
	FString CurrentName;

	TWeakObjectPtr<ARoboVehicle> CachedVehicle;

	/** Pose the robot had when first seen, used when resetting with nowhere else to go. */
	FVector SpawnLocationUU = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	bool bHasSpawnPose = false;

	/** Actors the current scenario created, so the next one can remove them. */
	TArray<TWeakObjectPtr<AActor>> ScenarioActors;
};
