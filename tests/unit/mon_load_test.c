#include "test.h"

#include "generic_agent.h"
#include "mon.h"

void test_load_monitor(void)
{
    double cf_this[100];
    MonLoadGatherData(cf_this);
    double load1[2] = {0,0};
    double load2[2] = {0,0};
    int n1 = getloadavg(load1, 1);
    int n2 = getloadavg(load2, 1);

    double min = (double) (load2[0]<load1[0]?load2[0]:load1[0]);
    double max = (double) (load2[0]<load1[0]?load1[0]:load2[0]); 
    double lower = 50.0 + min - (fabs(load2[0] - load1[0]) * 1.10);
    double upper = 50.0 + max + (fabs(load2[0] - load1[0]) * 1.10);
    printf(">>>oF[%d]=%f low=%f up=%f\n",ob_loadavg, cf_this[ob_loadavg], lower, upper);
    assert_true(cf_this[ob_loadavg]>=lower && cf_this[ob_loadavg]<=upper);
//8 7.2 9
//x should be between min - delta, max + delta
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_load_monitor),
    };

    return run_tests(tests);
}
