// QUINCE's core (quince_core.h says what belongs here).
#include "quince_core.h"

char *quince_greeting(char *buf, unsigned size, const char *who)
{
    const char *from = "Hello from ";
    unsigned n = 0;

    if (size == 0)
        return buf;
    if (!who || !*who)
        who = "a jar";
    for (const char *p = from; *p && n + 1 < size; p++)
        buf[n++] = *p;
    for (const char *p = who; *p && n + 1 < size; p++)
        buf[n++] = *p;
    if (n + 1 < size)
        buf[n++] = '.';
    buf[n] = '\0';
    return buf;
}
