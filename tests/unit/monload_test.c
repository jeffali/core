#include "test.h"

#include "generic_agent.h"
#include "env_context.h"
#include "sysinfo.h"

#include "mon.h"

void test_load_masterfiles(void)
{
    double cf_this[100];
    int i;
    for(i=0; i<10; i++) printf("iF[%d]=%f\n",i, cf_this[i]);
    MonLoadGatherData(cf_this);
    double load[2] = {0,0};
    int n = getloadavg(load, 1);
    printf(">>>NNN=%d, load=%f\n",n,load[0]);
        n = getloadavg(load, 1);
    printf(">>>NNN=%d, load=%f\n",n,load[0]);
    printf(">>>oF[%d]=%f\n",4, cf_this[4 /*ob_load*/]);
    assert_true(1);
}

int main()
{
    strcpy(CFWORKDIR, "/workdir");

    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_load_masterfiles),
    };

    return run_tests(tests);
}
