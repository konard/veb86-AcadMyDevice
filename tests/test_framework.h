// test_framework.h — минимальный каркас модульных тестов без внешних зависимостей.
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing
{
    struct TestCase
    {
        const char* name;
        std::function<void()> body;
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    inline int& failures()
    {
        static int count = 0;
        return count;
    }

    struct Registrar
    {
        Registrar(const char* name, std::function<void()> body)
        {
            registry().push_back(TestCase{ name, body });
        }
    };

    inline std::string narrow(const std::wstring& s)
    {
        std::string out;
        for (wchar_t c : s)
        {
            if (c < 0x80)
                out += static_cast<char>(c);
            else
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            }
        }
        return out;
    }

    inline void fail(const char* file, int line, const std::string& message)
    {
        std::printf("    FAILED %s:%d: %s\n", file, line, message.c_str());
        failures()++;
    }

    inline int runAll()
    {
        int failedTests = 0;
        for (const TestCase& test : registry())
        {
            const int before = failures();
            test.body();
            const bool ok = failures() == before;
            std::printf("[%s] %s\n", ok ? " OK " : "FAIL", test.name);
            if (!ok)
                failedTests++;
        }
        std::printf("\n%zu tests, %d failed\n", registry().size(), failedTests);
        return failedTests == 0 ? 0 : 1;
    }
}

#define TEST_CONCAT_(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_(a, b)

#define TEST(name)                                                              \
    static void name();                                                         \
    static testing::Registrar TEST_CONCAT(registrar_, name)(#name, &name);      \
    static void name()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond))                                                            \
            testing::fail(__FILE__, __LINE__, "CHECK(" #cond ")");              \
    } while (0)

#define CHECK_EQ(actual, expected)                                              \
    do {                                                                        \
        const auto& a_ = (actual);                                              \
        const auto& e_ = (expected);                                            \
        if (!(a_ == e_))                                                        \
            testing::fail(__FILE__, __LINE__,                                   \
                          "CHECK_EQ(" #actual ", " #expected ")");              \
    } while (0)

#define CHECK_WSTR(actual, expected)                                            \
    do {                                                                        \
        const std::wstring a_ = (actual);                                       \
        const std::wstring e_ = (expected);                                     \
        if (a_ != e_)                                                           \
            testing::fail(__FILE__, __LINE__,                                   \
                          "CHECK_WSTR(" #actual "): \"" + testing::narrow(a_) + \
                          "\" != \"" + testing::narrow(e_) + "\"");             \
    } while (0)

#define CHECK_NEAR(actual, expected, tol)                                       \
    do {                                                                        \
        const double a_ = (actual);                                             \
        const double e_ = (expected);                                           \
        if (!(std::fabs(a_ - e_) <= (tol)))                                     \
            testing::fail(__FILE__, __LINE__,                                   \
                          "CHECK_NEAR(" #actual ", " #expected "): " +          \
                          std::to_string(a_) + " != " + std::to_string(e_));    \
    } while (0)
