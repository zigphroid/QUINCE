// The checks QUINCE /SELFTEST and the host test share (quince_core.h). A
// new check goes in its own function, listed in checks[]. No libc here:
// the same bytes must mean the same thing on both sides, so even the
// string compare is our own.
#include "quince_core.h"

static void (*report)(const char *what, const char *got, const char *want);
static int ran, failed;

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void check_str(const char *what, const char *got, const char *want)
{
    ran++;
    if (!same(got, want)) {
        failed++;
        report(what, got, want);
    }
}

static void greeting(void)
{
    char buf[80], small[12];

    check_str("a name", quince_greeting(buf, sizeof buf, "JAM"), "Hello from JAM.");
    check_str("an empty name", quince_greeting(buf, sizeof buf, ""), "Hello from a jar.");
    check_str("no name", quince_greeting(buf, sizeof buf, 0), "Hello from a jar.");
    check_str("a tight buffer cuts cleanly", quince_greeting(small, sizeof small, "JAM"),
              "Hello from ");
}

static void (*const checks[])(void) = { greeting };

int quince_selftest(void (*fail)(const char *what, const char *got, const char *want),
                    int *count)
{
    report = fail;
    ran = failed = 0;
    for (unsigned i = 0; i < sizeof checks / sizeof checks[0]; i++)
        checks[i]();
    *count = ran;
    return failed;
}
