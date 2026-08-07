/* test_util.h - micro framework per gli unit test nativi (gcc di sistema). */
#ifndef PP_TEST_UTIL_H
#define PP_TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>

static int t_run, t_failed;
static const char *t_section = "";

#define SECTION(name) do { t_section = (name); printf("\n-- %s\n", (name)); } while (0)

#define CHECK(cond)                                                        \
    do {                                                                   \
        t_run++;                                                           \
        if (cond) {                                                        \
            printf("  PASS  %s\n", #cond);                                 \
        } else {                                                           \
            t_failed++;                                                    \
            printf("  FAIL  %s   (%s:%d, sezione \"%s\")\n",               \
                   #cond, __FILE__, __LINE__, t_section);                  \
        }                                                                  \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        long _a = (long)(actual), _e = (long)(expected);                   \
        t_run++;                                                           \
        if (_a == _e) {                                                    \
            printf("  PASS  %s == %ld\n", #actual, _e);                    \
        } else {                                                           \
            t_failed++;                                                    \
            printf("  FAIL  %s: atteso %ld, ottenuto %ld   (%s:%d, sezione \"%s\")\n", \
                   #actual, _e, _a, __FILE__, __LINE__, t_section);        \
        }                                                                  \
    } while (0)

static int t_summary(const char *suite)
{
    printf("\n===== %s: %d controlli, %d falliti -> %s =====\n",
           suite, t_run, t_failed, t_failed ? "FAIL" : "PASS");
    return t_failed ? 1 : 0;
}

#endif /* PP_TEST_UTIL_H */
