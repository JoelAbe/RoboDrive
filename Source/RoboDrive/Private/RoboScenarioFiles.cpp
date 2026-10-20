#include "RoboScenarioFiles.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString RoboScenarioFiles::Directory()
{
	// The same folder the Python harness and CI read, next to the .uproject.
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Scenarios"));
}

TArray<FString> RoboScenarioFiles::List()
{
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *FPaths::Combine(Directory(), TEXT("*.json")),
		/*Files*/ true, /*Directories*/ false);

	// Sorted, so the list keeps the same order however the disk returns it.
	FileNames.Sort();
	return FileNames;
}

TSharedPtr<FJsonObject> RoboScenarioFiles::Read(const FString& FileName, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(Directory(), FileName)))
	{
		OutError = TEXT("could not be read");
		return nullptr;
	}

	TSharedPtr<FJsonObject> Scenario;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Scenario) || !Scenario.IsValid())
	{
		OutError = TEXT("is not valid JSON");
		return nullptr;
	}

	return Scenario;
}