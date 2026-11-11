// The host test: the shared checks (lib/selftest.c) under the sanitizers.
// Host-only checks (fixtures, files on the PC) go here, after them.
// Fixtures: data with a note on where it came from, never secrets; those
// come from the environment as QUINCE_..., and a test without them skips,
// saying why.
#include <stdio.h>

#include "quince_core.h"

static void fail(const char *what, const char *got, const char *want)
{
    printf("FAIL: %s: got \"%s\", want \"%s\"\n", what, got, want);
}

int main(void)
{
    int ran;
    int failed = quince_selftest(fail, &ran);

    if (failed) {
        printf("test_quince: %d of %d checks failed\n", failed, ran);
        return 1;
    }
    printf("test_quince: %d checks OK\n", ran);
    return 0;
}
