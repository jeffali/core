#include "test.h"

#include "generic_agent.h"
#include "mon.h"

void test_load_masterfiles(void)
{
    double cf_this[100];
    int i;
    for(i=0; i<10; i++) printf("iF[%d]=%f\n",i, cf_this[i]);
    MonLoadGatherData(cf_this);
    double load1[2] = {0,0};
    double load2[2] = {0,0};
    int n1 = getloadavg(load1, 1);
    printf(">>>NNN=%d, load1=%f\n",n1,load1[0]);
    int n2 = getloadavg(load2, 1);
    load2[0] +=0.1777;
    printf(">>>NNN=%d, load2=%f\n",n2,load2[0]);

    printf(">>>oF[%d]=%f\n",4, cf_this[4 /*ob_load*/]);
    double lower = 50.0 + (double) (load2[0]<load1[0]?load2[0]:load1[0]) - (fabs(load2[0] - load1[0]) * 1.10);
    double upper = 50.0 + (double) (load2[0]<load1[0]?load1[0]:load2[0]) + (fabs(load2[0] - load1[0]) * 1.10);
    printf(">>>oF[%d]=%f low=%f up=%f\n",4, cf_this[4 /*ob_load*/], lower, upper);
    assert_true(cf_this[4]>=lower && cf_this[4]<=upper);
//8 7.2 9
//x should be between min - delte, max + delta
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
