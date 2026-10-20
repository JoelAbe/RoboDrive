#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * The scenario files in the project's Scenarios folder, for the operator to
 * choose from.
 *
 * Reading files is the client's job, not the simulation's: the operator view
 * reads a file and hands the simulation its contents, exactly as the Python
 * harness does over the API (ADR 0008).
 */
namespace RoboScenarioFiles
{
	/** The folder scenarios are read from. */
	FString Directory();

	/** The file name of every .json file in it, sorted, without the folder. */
	TArray<FString> List();

	/** Read and parse one file by name. Returns null and fills OutError if it cannot. */
	TSharedPtr<FJsonObject> Read(const FString& FileName, FString& OutError);
}