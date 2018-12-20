# TEST

Single header for unit and performance tests in c++

Tests are staticaly registered.
No support for grouping or any pre / post test code.
Fail doesnt write the values of variables. 
	(If you know a way without templates or calling specific functions for specific types, please let me know :))
	
Performance tests support repetition and comparison to first timed scope.
	
Console coloring and timer functions are set by user
	

## Usage
	#define TEST_IMPLEMENTATION 
	before test.h include in one .cpp file

## Examples
	
	
```
// Unit Tests
TEST(BarTest) {
	TEST_EQ(4, 4, "Test should pass");
	// -> BarTest ................................................................. SUCCESS
}
	

TEST(FooTest) {
	int size = 4;
	TEST_TRUE(size == 8, "size should be 4");
	
	int capacity = 3;
	TEST_L(size, capacity, "size should be less than capacity");
	
	// -> Test3 .................................................................. FAILED
	// ->   size == 8, size should be 4
	// ->   size < capacity, size should be less than capacity
}
```


```
// Performance Test
TEST_PERF(FooFooTest) {
	TEST_PERF_START(MyFoo, i, 10);
	Sleep(10);
	TEST_PERF_STOP();

	TEST_PERF_START(OtherFoo, i, 10);
	Sleep(20);
	TEST_PERF_STOP();
	
	// -> FooFooTest
	// ->   MyFoo: B 10588.6, W 11344.6, A 10956.5 in 10
	// ->   OtherFoo: B 20178.1, W 21043.1, A 20865.9 in 10 Ratio to MyFoo: 1.90563, 1.8549, 1.90444
}
```

```
// Setup MSVC

#define TEST_IMPLEMENTATION
#include "test.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void ConsoleSetStyle(void*, TestConsoleStyle style) {
	const unsigned int GREEN_TEXT = 0x0A;
	const unsigned int RED_TEXT = 0x0C;
	const unsigned int GREY_TEXT = 0x07;
	
	switch (style)
	{
	case TEST_CONSOLE_TEXT_DEFAULT: SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), GREY_TEXT); break;
	case TEST_CONSOLE_TEXT_SUCCESS: SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), GREEN_TEXT); break;
	case TEST_CONSOLE_TEXT_FAIL:SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), RED_TEXT); break;
	case TEST_CONSOLE_INVALID:
	default:
		break;
	}
}

double Timer(void* timeContext) {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart / *(double*) timeContext;
}

double Frequency() {
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return li.QuadPart / 1000000.0;
}

int main(int argc, char* args[]) {
	double freq = Frequency();

	TestConfig config;
	config.SetConsoleStyle = ConsoleSetStyle;
	config.Time = Timer;
	config.stopOnEnd = false;
	config.stopOnError = false;
	config.perfStopOnEnd = true;
	config.floatEpsilon = 1e-10f;
	config.doubleEpsilon = 1e-20;
	config.styleContext = nullptr;
	config.timeContext = &freq;
	TestSetConfig(&config);
	bool ok = TestRun();
	TestRunPerf();

	//TestDeinit();
	return ok;
}

```
