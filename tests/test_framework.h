#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int _tests_run    = 0;
static int _tests_failed = 0;
#define TEST(name)   void test_##name(void)
#define RUN(name)    do { _tests_run++; test_##name(); } while(0)
#define ASSERT_EQ(a, b) do { \
    unsigned _a = (unsigned)(a), _b = (unsigned)(b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL %s:%d  expected 0x%04X  got 0x%04X\n", \
                __FILE__, __LINE__, _b, _a); \
        _tests_failed++; \
    } \
} while(0)
#define ASSERT_TRUE(x)   ASSERT_EQ(!!(x), 1)
#define ASSERT_FALSE(x)  ASSERT_EQ(!!(x), 0)
#define ASSERT_STR_CONTAINS(hay, needle) do { \
    if (!strstr((hay), (needle))) { \
        fprintf(stderr, "  FAIL %s:%d  missing: %s\n", __FILE__, __LINE__, (needle)); \
        _tests_failed++; \
    } \
} while(0)
#define PRINT_RESULTS() do { \
    printf("  %d test(s) run, %d failed\n", _tests_run, _tests_failed); \
    return _tests_failed ? 1 : 0; \
} while(0)
#endif
