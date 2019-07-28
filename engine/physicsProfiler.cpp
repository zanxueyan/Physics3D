#include "physicsProfiler.h"

const char * physicsLabels[]{
	"Transforms",
	"Externals",
	"GJK Col",
	"GJK No Col",
	"EPA",
	"Collision",
	"Col. Handling",
	"Update Tree Bounds",
	"Update Tree Structure",
	"Wait for lock",
	"Updates",
	"Other"
};

const char * intersectionLabels[]{
	"Colission",
	"GJK reject",
	"Phys Dist reject",
	"Phys Bound reject",
	"Part Dist reject",
	"Part Bound reject"
};

const char* iterationLabels[]{
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"15+",
	"MAX",
};

BreakdownAverageProfiler<100, PhysicsProcess> physicsMeasure(physicsLabels);
HistoricTally<1, long long, IntersectionResult> intersectionStatistics(intersectionLabels);
CircularBuffer<int, 1> gjkCollideIterStats;
CircularBuffer<int, 1> gjkNoCollideIterStats;

HistoricTally<1, long long, IterationTime> GJKCollidesIterationStatistics(iterationLabels);
HistoricTally<1, long long, IterationTime> GJKNoCollidesIterationStatistics(iterationLabels);
HistoricTally<1, long long, IterationTime> EPAIterationStatistics(iterationLabels);
