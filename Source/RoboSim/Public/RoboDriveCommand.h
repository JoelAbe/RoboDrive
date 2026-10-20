#pragma once

#include "CoreMinimal.h"
#include "RoboDriveCommand.generated.h"

/**
 * Who produced a drive command.
 *
 * The numeric order is the priority order: a higher value wins arbitration.
 * Manual is highest so a person can always take over immediately.
 */
UENUM(BlueprintType)
enum class EDriveSource : uint8
{
	/** No valid command. Arbitrates to a stop. */
	None = 0 UMETA(DisplayName = "None"),

	/** The in-engine autonomy. */
	Autonomy = 1 UMETA(DisplayName = "Autonomy"),

	/** An external client, such as a test script or a ROS bridge. */
	External = 2 UMETA(DisplayName = "External"),

	/** A human at the controls. Always wins. */
	Manual = 3 UMETA(DisplayName = "Manual")
};

/**
 * Which sources the operator allows to drive.
 *
 * A gate, not a priority: in Manual, autonomy and external clients are
 * ignored entirely rather than merely outranked, so letting go of the keys
 * stops the robot instead of handing it back to the brain.
 */
UENUM(BlueprintType)
enum class EDriveMode : uint8
{
	/** Any source may drive, highest priority first. A person at the keys still overrides. */
	Auto = 0	UMETA(DisplayName = "Auto"),

	/** Only a person at the controls may drive. */
	Manual = 1	UMETA(DisplayName = "Manual")
};

/**
 * A request for the robot to move, in REP-103 SI units.
 *
 * Commands expire: an arbiter ignores one whose timestamp is too old, so a
 * source that stops sending stops driving rather than latching its last order.
 */
USTRUCT(BlueprintType)
struct FDriveCommand
{
	GENERATED_BODY()

	/** Forward speed, m/s. Positive is forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive Command")
	double Linear = 0.0;

	/** Yaw rate, rad/s. Positive turns left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive Command")
	double Angular = 0.0;

	/** Simulation time this command was issued, in seconds. Not wall-clock time. */
	UPROPERTY(BlueprintReadOnly, Category = "Drive Command")
	double TimeSeconds = 0.0;

	/** Which source issued it. */
	UPROPERTY(BlueprintReadOnly, Category = "Drive Command")
	EDriveSource Source = EDriveSource::None;
};