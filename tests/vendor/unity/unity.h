#ifndef UNITY_H
#define UNITY_H

#include "unity_internals.h"

#ifdef __cplusplus
extern "C" {
#endif

void UnityBegin(const char *filename);
int UnityEnd(void);
void UnityFail(const char *msg, UNITY_LINE_TYPE line);
void UnityAssertEqualNumber(UNITY_INT expected, UNITY_INT actual, const char *msg, UNITY_LINE_TYPE line);
void UnityAssertEqualString(const char *expected, const char *actual, const char *msg, UNITY_LINE_TYPE line);
void UnityAssertEqualMemory(const void *expected, const void *actual, UNITY_UINT32 len, const char *msg, UNITY_LINE_TYPE line);
void UnityAssertTrue(int condition, const char *msg, UNITY_LINE_TYPE line);
void UnityDefaultTestRun(void (*func)(void), const char *name, UNITY_LINE_TYPE line);
void setUp(void);
void tearDown(void);

#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END() UnityEnd()
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)

#define TEST_ASSERT(cond) UnityAssertTrue((cond) ? 1 : 0, NULL, __LINE__)
#define TEST_ASSERT_TRUE(cond) TEST_ASSERT(cond)
#define TEST_ASSERT_FALSE(cond) UnityAssertTrue((cond) ? 0 : 1, NULL, __LINE__)
#define TEST_ASSERT_NULL(ptr) UnityAssertTrue((ptr) == NULL, "Expected NULL", __LINE__)
#define TEST_ASSERT_NOT_NULL(ptr) UnityAssertTrue((ptr) != NULL, "Expected non-NULL", __LINE__)
#define TEST_ASSERT_EQUAL(expected, actual) UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__)
#define TEST_ASSERT_EQUAL_INT TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_UINT TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_INT8 TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_UINT8 TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_INT32 TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_UINT32 TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_size_t TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL_STRING(expected, actual) UnityAssertEqualString((expected), (actual), NULL, __LINE__)
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) UnityAssertEqualMemory((expected), (actual), (UNITY_UINT32)(len), NULL, __LINE__)
#define TEST_FAIL_MESSAGE(msg) UnityFail((msg), __LINE__)
#define TEST_FAIL() UnityFail(NULL, __LINE__)
#define TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, msg) UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (msg), __LINE__)
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, msg) UnityAssertEqualString((expected), (actual), (msg), __LINE__)
#define TEST_ASSERT_NOT_EQUAL(expected, actual) UnityAssertTrue(((expected) != (actual)) ? 1 : 0, "Expected inequality", __LINE__)

#ifdef __cplusplus
}
#endif

#endif
