#include "RoboRunRecord.h"
#include "RoboTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRecordColumnsMatchTest,
	"RoboDrive.RoboSim.RunRecord.ColumnsMatch", RoboTest::Flags)

bool FRunRecordColumnsMatchTest::RunTest(const FString& Parameters)
{
	// A column added to the row but not the header (or the reverse) shifts
	// every value after it. The file still parses, the numbers are just
	// attached to the wrong names, which is far worse than a crash.
	const int32 HeaderColumns = RoboRunRecord::CountColumns(RoboRunRecord::Header());
	const int32 RowColumns = RoboRunRecord::CountColumns(RoboRunRecord::Format(FRunSample{}));

	TestEqual(TEXT("Header and row have the same number of columns"),
		RowColumns, HeaderColumns);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRecordFormatTest,
	"RoboDrive.RoboSim.RunRecord.Format", RoboTest::Flags)

bool FRunRecordFormatTest::RunTest(const FString& Parameters)
{
	FRunSample Sample;
	Sample.TimeSeconds = 1.25;
	Sample.Step = 150;
	Sample.PositionErrorM = 0.125;

	const FString Row = RoboRunRecord::Format(Sample);

	TestTrue(TEXT("Time is written"), Row.StartsWith(TEXT("1.250000,150,")));
	TestTrue(TEXT("Values keep enough precision"), Row.Contains(TEXT("0.125000")));
	return true;
}

#endif