#include "test.h"

#include "generic_agent.h"
#include "env_context.h"
#include "sysinfo.h"

#include "mon.h"

void test_disk_masterfiles(void)
{
    double cf_this[100];
    int i;
    for(i=0; i<10; i++) printf("iF[%d]=%f\n",i, cf_this[i]);
    MonDiskGatherData(cf_this);
    for(i=0; i<10; i++) printf("oF[%d]=%f\n",i, cf_this[i]);
    assert_true(1);
}

int main()
{
    strcpy(CFWORKDIR, "/workdir");

    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_disk_masterfiles),
    };

    return run_tests(tests);
}
