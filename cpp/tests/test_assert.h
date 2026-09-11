#pragma once

#include <cstdio>
#include <cstdlib>

inline int g_testFailures = 0;

#define TEST(name) static void name()
#define RUN_TEST(name)                        \
    do {                                      \
        const int before = g_testFailures;    \
        name();                               \
        if (g_testFailures == before) {       \
            std::printf("  OK  %s\n", #name); \
        }                                     \
    } while (0)

#define EXPECT_TRUE(expr)                                              \
    do {                                                               \
        if (!(expr)) {                                                 \
            std::printf("  FAIL %s:%d: expected true: %s\n", __FILE__, \
                __LINE__, #expr);                                      \
            ++g_testFailures;                                          \
        }                                                              \
    } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(a, b)                                                       \
    do {                                                                      \
        const auto _a = (a);                                                  \
        const auto _b = (b);                                                  \
        if (_a != _b) {                                                       \
            std::printf("  FAIL %s:%d: expected equal: %s vs %s\n", __FILE__, \
                __LINE__, #a, #b);                                            \
            ++g_testFailures;                                                 \
        }                                                                     \
    } while (0)
