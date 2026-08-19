#include "unity.h"

static int s_tests;
static int s_failures;
static int s_ignored;
static const char *s_file;
static int s_current_failed;

void setUp(void);
void tearDown(void);

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

void UnityBegin(const char *filename)
{
  s_file = filename ? filename : "";
  s_tests = 0;
  s_failures = 0;
  s_ignored = 0;
}

int UnityEnd(void)
{
  printf("\n-----------------------\n");
  printf("%d Tests %d Failures %d Ignored\n", s_tests, s_failures, s_ignored);
  printf(s_failures ? "FAIL\n" : "OK\n");
  return s_failures;
}

void UnityDefaultTestRun(void (*func)(void), const char *name, UNITY_LINE_TYPE line)
{
  (void)line;
  s_tests++;
  s_current_failed = 0;
  printf("TEST(%s)", name ? name : "?");
  setUp();
  func();
  tearDown();
  if (!s_current_failed) {
    printf(" PASS\n");
  }
}

void UnityFail(const char *msg, UNITY_LINE_TYPE line)
{
  s_current_failed = 1;
  s_failures++;
  printf(" FAIL\n  %s:%u: FAIL", s_file ? s_file : "", (unsigned)line);
  if (msg && msg[0]) {
    printf(": %s", msg);
  }
  printf("\n");
}

void UnityAssertTrue(int condition, const char *msg, UNITY_LINE_TYPE line)
{
  if (!condition) {
    UnityFail(msg ? msg : "Expected TRUE", line);
  }
}

void UnityAssertEqualNumber(UNITY_INT expected, UNITY_INT actual, const char *msg, UNITY_LINE_TYPE line)
{
  char buf[160];
  if (expected == actual) {
    return;
  }
  snprintf(buf, sizeof(buf), "%sExpected %d Was %d", msg ? msg : "", (int)expected, (int)actual);
  UnityFail(buf, line);
}

void UnityAssertEqualString(const char *expected, const char *actual, const char *msg, UNITY_LINE_TYPE line)
{
  char buf[320];
  expected = expected ? expected : "";
  actual = actual ? actual : "";
  if (strcmp(expected, actual) == 0) {
    return;
  }
  snprintf(buf, sizeof(buf), "%sExpected <%s> Was <%s>", msg ? msg : "", expected, actual);
  UnityFail(buf, line);
}

void UnityAssertEqualMemory(const void *expected, const void *actual, UNITY_UINT32 len, const char *msg, UNITY_LINE_TYPE line)
{
  if (!expected || !actual) {
    UnityFail(msg ? msg : "NULL memory", line);
    return;
  }
  if (memcmp(expected, actual, (size_t)len) != 0) {
    UnityFail(msg ? msg : "Memory mismatch", line);
  }
}
