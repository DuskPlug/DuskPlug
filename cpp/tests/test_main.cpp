#include "test_assert.h"

void RunScheduleTests();
void RunJsonTests();
void RunCryptoTests();
void RunSolarTests();
void RunConfigTests();

int main() {
    RunScheduleTests();
    RunJsonTests();
    RunCryptoTests();
    RunSolarTests();
    RunConfigTests();

    if (g_testFailures == 0) {
        std::printf("\nAll tests passed.\n");
        return 0;
    }

    std::printf("\n%d test(s) failed.\n", g_testFailures);
    return 1;
}
