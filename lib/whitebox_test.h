#ifndef __WHITEBOX_TEST_H__
#define __WHITEBOX_TEST_H__

#include <assert.h>
#include <stdio.h>

typedef int (*whitebox_test_func_t)(void *data);

typedef struct whitebox_test {
    char *name;
    whitebox_test_func_t func;
    void *data;
} whitebox_test_t;

#define WHITEBOX_TEST(FUNC) { #FUNC, FUNC, 0 }
#define WHITEBOX_TEST_LONG(NAME, FUNC, DATA) { NAME, FUNC, (void*)(DATA) }

int whitebox_test_main(whitebox_test_t *tests, void *data, int argc, char **argv) {
    int i;
    int final_result = 0;
    int result;
    for(i = 0; tests[i].func; ++i) {
        printf("%s... ", tests[i].name);
        fflush(stdout);
        result = tests[i].func(tests[i].data);
        final_result |= result;
        if (result)
            printf("FAILED\n");
        else
            printf("PASSED\n");
        fflush(stdout);
    }
    return final_result;
}

#endif /* __WHITEBOX_TEST_H__ */
