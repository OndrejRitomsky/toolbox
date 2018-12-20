/*
	@author Ondrej Ritomsky
	@version 0.5, 20.12.2018

	No warranty implied

	USAGE:
		#define TEST_IMPLEMENTATION 
		before test.h include in one .cpp file
*/

#ifndef TEST_INCLUDE_TEST_H
#define TEST_INCLUDE_TEST_H


enum TestConsoleStyle {
	TEST_CONSOLE_INVALID = 0,
	TEST_CONSOLE_TEXT_DEFAULT,
	TEST_CONSOLE_TEXT_SUCCESS,
	TEST_CONSOLE_TEXT_FAIL
};

typedef void (*TestSetConsoleStyleFn)(void* styleContext, TestConsoleStyle style);

typedef double (*TestTimeFn)(void* timeContext);


struct TestConfig {
	bool stopOnError;
	bool stopOnEnd;
	bool perfStopOnEnd;

	float floatEpsilon;
	
	void* styleContext;
	void* timeContext;
	TestSetConsoleStyleFn SetConsoleStyle;
	TestTimeFn Time;
	double doubleEpsilon;
};

void TestSetConfig(const TestConfig* config);
bool TestRun();
void TestRunPerf();

void TestDeinit();

//
// TEST(function);
//
// TEST_TRUE(a, failText);
// TEST_FALSE(a, failText);
// TEST_EQ(a, b, failText);
// TEST_L(a, b, failText);
// TEST_LE(a, b, failText);
// TEST_NE(a, b, failText);
// 
// TEST_FLOAT_E(a, b, failText);
// TEST_DOUBLE_E(a, b, failText);
//
// TEST_ARRAY_EQ(a, b, count, failText);
//
//
//
// TEST_PERF(function)
//
// TEST_PERF_START(name, iterator, count)
// TEST_PERF_STOP()
//




typedef void(*Test_TestFn)();

void Test_PrintError(const char* left, const char* operation, const char* right, const char* userText);
bool Test_ContainerRegisterTest(Test_TestFn func);
bool Test_ContainerRegisterPerfTest(Test_TestFn func);
bool Test_ContainerTest(bool ok, const char* testName);
void Test_ContainerPerfTestStart();
void Test_ContainerPerfTestStop();
void Test_ContainerPerfTestStartScope(const char* functionName, const char* scopeName);
void Test_ContainerPerfTestStopScope();
bool Test_FloatEq(float a, float b);
bool Test_DoubleEq(double a, double b);


#define TEST(function) \
void function(); \
static bool g_test_register##function = Test_ContainerRegisterTest(function); \
void function() 

#define TEST_INTERNAL_TEST(expr, text, a, op, b) if (!Test_ContainerTest(expr, __FUNCTION__)) Test_PrintError(#a, op, #b, text);

#define TEST_TRUE(a, text)  TEST_INTERNAL_TEST(       (a), text, a, "==",  "true");
#define TEST_FALSE(a, text) TEST_INTERNAL_TEST(      !(a), text, a, "==", "false");
#define TEST_EQ(a, b, text) TEST_INTERNAL_TEST((a) == (b), text, a, "==",       b);
#define TEST_L(a, b, text)  TEST_INTERNAL_TEST( (a) < (b), text, a,  "<",       b);
#define TEST_LE(a, b, text) TEST_INTERNAL_TEST((a) <= (b), text, a, "<=",       b);
#define TEST_NE(a, b, text) TEST_INTERNAL_TEST((a) != (b), text, a, "!=",       b);

#define TEST_FLOAT_E(a, b, text) TEST_INTERNAL_TEST(Test_FloatEq((a), (b)), text, a, "==", b);

#define TEST_DOUBLE_E(a, b, text) TEST_INTERNAL_TEST(Test_DoubleEq((a), (b)), text, a, "==", b);


#define TEST_ARRAY_EQ(a, b, count, text) {\
	bool correct = true; \
	for (unsigned int i = 0; correct && i < count; ++i) { \
		correct &= (a)[i] == (b)[i]; \
	} \
	TEST_INTERNAL_TEST(correct, text, a, "==", b); \
}



#define TEST_PERF(function) \
void function(); \
static bool g_perf_register##function = Test_ContainerRegisterPerfTest(function); \
void function() 



#define TEST_PERF_START(name, iterator, count) { \
	Test_ContainerPerfTestStartScope(__FUNCTION__, #name);\
	for (unsigned int iterator = 0; iterator < count; ++iterator) { \
		Test_ContainerPerfTestStart();

#define TEST_PERF_STOP() \
		Test_ContainerPerfTestStop(); \
	} \
	Test_ContainerPerfTestStopScope(); \
}

#endif // TEST_INCLUDE_TEST_H


#if defined(TEST_IMPLEMENTATION)

#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include <stdio.h>


struct Test_Container {
	bool failed;

	unsigned int assertsCount;
	unsigned int assertsOk;

	unsigned int testsCapacity;
	unsigned int testsCount;

	unsigned int perfTestsCapacity;
	unsigned int perfTestsCount;

	Test_TestFn* tests;
	Test_TestFn* perfTests;

	const char* currentTestName;
	const char* prevTestName;

	const char* currentScopeName;
	const char* prevScopeName;

	unsigned int perfScopeCount;
	unsigned int prevPerfScopeCount;

	double timeStamp;
	double timeStampBest;
	double timeStampWorst;
	double timeStampSum;

	double prevTimeStampBest;
	double prevTimeStampWorst;
	double prevTimeStampSum;

	TestConfig config;
};


Test_Container* Test_ContainerInstance();
void Test_PrintDots(const char* testName);




void TestSetConfig(const TestConfig* config) {
	if (config)
		Test_ContainerInstance()->config = *config;
}

void TestDeinit() {
	Test_Container* container = Test_ContainerInstance();
	if (container->tests)
		free(container->tests);
	if (container->perfTests)
		free(container->perfTests);
}

bool TestRun() {
	Test_Container* container = Test_ContainerInstance();

	unsigned int nonEmptyCount = 0;
	for (unsigned int i = 0; i < container->testsCount; ++i) {
		container->failed = false;
		unsigned int currentCount = container->assertsCount;
		container->tests[i]();

		if (currentCount == container->assertsCount) {
			continue;
		}
		nonEmptyCount++;

		if (!container->failed) {
			Test_PrintDots(container->currentTestName);
			container->config.SetConsoleStyle(container->config.styleContext, TEST_CONSOLE_TEXT_SUCCESS);
			printf("SUCCESS");
		}
		else {
			printf("\n");
		}

		container->config.SetConsoleStyle(container->config.styleContext, TEST_CONSOLE_TEXT_DEFAULT);
	}

	printf("\n\nUnit tests count %d \n", nonEmptyCount);

	float ratio = container->assertsCount == 0 ? 0 : (float) container->assertsOk / container->assertsCount;
	ratio *= 100;

	TestConsoleStyle style = container->assertsCount == container->assertsOk ? TEST_CONSOLE_TEXT_SUCCESS : TEST_CONSOLE_TEXT_FAIL;
	container->config.SetConsoleStyle(container->config.styleContext, style);
	printf("Tests result %d / %d, %f%% \n\n", container->assertsOk, container->assertsCount, ratio);

	container->config.SetConsoleStyle(container->config.styleContext, TEST_CONSOLE_TEXT_DEFAULT);

	if (container->config.stopOnEnd) {
		char c;
		scanf_s("%c", &c, 1);
	}

	return container->assertsOk == container->assertsCount;
}


void TestRunPerf() {
	Test_Container* container = Test_ContainerInstance();

	container->currentTestName = nullptr;
	container->prevTestName = nullptr;

	for (unsigned int i = 0; i < container->perfTestsCount; ++i) {
		container->currentScopeName = nullptr;
		container->prevScopeName = nullptr;
		container->perfTests[i]();
		container->prevTestName = container->currentTestName;
	}

	if (container->config.perfStopOnEnd) {
		char c;
		scanf_s("%c", &c, 1);
	}
}

bool Test_ContainerRegisterTest(Test_TestFn func) {
	Test_Container* container = Test_ContainerInstance();

	if (container->testsCount == container->testsCapacity) {
		unsigned int capacity = container->testsCapacity * 2;
		Test_TestFn* newTests = (Test_TestFn*)malloc(capacity * sizeof(Test_TestFn));
		if (!newTests)
			return false;

		memcpy(newTests, container->tests, sizeof(Test_TestFn) * container->testsCapacity);
		container->testsCapacity = capacity;
		free(container->tests);
		container->tests = newTests;
	}

	container->tests[container->testsCount++] = func;
	return true;
}

bool Test_ContainerRegisterPerfTest(Test_TestFn func) {
	Test_Container* container = Test_ContainerInstance();

	if (container->perfTestsCount == container->perfTestsCapacity) {
		unsigned int capacity = container->perfTestsCapacity * 2;
		Test_TestFn* newTests = (Test_TestFn*)malloc(capacity * sizeof(Test_TestFn));
		if (!newTests)
			return false;

		memcpy(newTests, container->perfTests, sizeof(Test_TestFn) * container->perfTestsCapacity);
		container->perfTestsCapacity = capacity;
		free(container->perfTests);
		container->perfTests = newTests;
	}

	container->perfTests[container->perfTestsCount++] = func;
	return true;
}

bool Test_ContainerTest(bool ok, const char* testName) {
	Test_Container* container = Test_ContainerInstance();

	if (container->currentTestName != testName) {
		container->currentTestName = testName;
		printf("\n%s", testName);
	}

	container->assertsCount++;
	if (ok) {
		container->assertsOk++;
	}
	else {
		if (!container->failed) {
			Test_PrintDots(testName);
			container->config.SetConsoleStyle(container->config.styleContext, TEST_CONSOLE_TEXT_FAIL);
			printf("FAILED");
		}

		container->failed = true;
		container->config.SetConsoleStyle(container->config.styleContext, TEST_CONSOLE_TEXT_DEFAULT);
		if (container->config.stopOnError) {
			char c;
			scanf_s("%c", &c, 1);
		}
	}

	return ok;
}

void Test_ContainerPerfTestStartScope(const char* functionName, const char* scopeName) {
	Test_Container* container = Test_ContainerInstance();

	if (container->currentTestName != functionName) {
		printf("%s\n", functionName);
	}

	if (!container->prevScopeName && container->currentScopeName) {
		container->prevTimeStampBest = container->timeStampBest;
		container->prevTimeStampWorst = container->timeStampWorst;
		container->prevScopeName = container->currentScopeName;
		container->prevTestName = container->currentTestName;
		container->prevPerfScopeCount = container->perfScopeCount;
		container->prevTimeStampSum = container->timeStampSum;
	}

	if (container->currentTestName != functionName || container->currentScopeName != scopeName) {
		container->currentTestName = functionName;
		container->perfScopeCount = 0;
		container->currentScopeName = scopeName;
		container->timeStampBest = 1e+100;
		container->timeStampWorst = 0;
		container->timeStampSum = 0;
	}
}

void Test_ContainerPerfTestStart() {
	Test_Container* container = Test_ContainerInstance();
	double time = container->config.Time(container->config.timeContext);
	container->timeStamp = time;
}

void Test_ContainerPerfTestStopScope() {
	Test_Container* container = Test_ContainerInstance();

	double avg = container->perfScopeCount == 0 ? 0 : container->timeStampSum / container->perfScopeCount;
	printf("  %s: B %g, W %g, A %g in %d", container->currentScopeName, container->timeStampBest, container->timeStampWorst, avg, container->perfScopeCount);

	if (container->prevScopeName) {
		{
			TestConsoleStyle style = container->timeStampBest < container->prevTimeStampBest ? TEST_CONSOLE_TEXT_SUCCESS : TEST_CONSOLE_TEXT_FAIL;
			container->config.SetConsoleStyle(container->config.styleContext, style);
			printf(" Ratio to %s: %g,", container->prevScopeName, container->timeStampBest / container->prevTimeStampBest);
		}
		{
			TestConsoleStyle style = container->timeStampWorst < container->prevTimeStampWorst ? TEST_CONSOLE_TEXT_SUCCESS : TEST_CONSOLE_TEXT_FAIL;
			container->config.SetConsoleStyle(container->config.styleContext, style);
			printf(" %g,", container->timeStampWorst / container->prevTimeStampWorst);
		}
		{
			double prevAvg = container->prevPerfScopeCount == 0 ? 0 : container->prevTimeStampSum / container->prevPerfScopeCount;
			TestConsoleStyle style = container->timeStampBest < container->prevTimeStampBest ? TEST_CONSOLE_TEXT_SUCCESS : TEST_CONSOLE_TEXT_FAIL;
			container->config.SetConsoleStyle(container->config.styleContext, style);
			printf(" %g", avg / prevAvg);
		}
	}
	printf("\n");

	container->config.SetConsoleStyle(container->config.styleContext, TEST_CONSOLE_TEXT_DEFAULT);
}

void Test_ContainerPerfTestStop() {
	Test_Container* container = Test_ContainerInstance();
	double t = container->config.Time(container->config.timeContext) - container->timeStamp;

	if (t < container->timeStampBest)
		container->timeStampBest = t;

	if (t > container->timeStampWorst)
		container->timeStampWorst = t;

	container->perfScopeCount++;
	container->timeStampSum += t;
}

void Test_PrintError(const char* left, const char* operation, const char* right, const char* userText) {
	printf("\n  %s %s %s, %s", left, operation, right, userText);
}

bool Test_FloatEq(float a, float b) {
	float eps = Test_ContainerInstance()->config.floatEpsilon;
	return (a - b < eps) && (b - a < eps);
}

bool Test_DoubleEq(double a, double b) {
	double eps = Test_ContainerInstance()->config.doubleEpsilon;
	return (a - b < eps) && (b - a < eps);
}

static Test_Container* Test_ContainerInstance() {
	static bool initialized = false;
	static Test_Container instance;

	if (!initialized) {
		initialized = true;

		instance.perfTests = (Test_TestFn*) malloc(16 * sizeof(Test_TestFn));
		instance.perfTestsCapacity = 16;
		instance.perfTestsCount = 0;
		instance.tests = (Test_TestFn*) malloc(16 * sizeof(Test_TestFn));
		instance.testsCapacity = 16;
		instance.testsCount = 0;
		instance.currentTestName = nullptr;
		instance.assertsCount = 0;
		instance.assertsOk = 0;
		instance.failed = false;
	}

	return &instance;
}

static void Test_PrintDots(const char* testName) {
	static const char dots[] = "................................................................................";
	// -1 last spaces, -7 for SUCCESS, -2 for offset, +1 for zero char in fn name
	int n = 80 - 1 - 7 - 2 - (int)strlen(testName) + 1;
	printf(" %.*s ", n, dots);
}


#endif // TEST_IMPLEMENTATION