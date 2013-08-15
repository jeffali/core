#include "test.h"

#include "generic_agent.h"
#include "mon.h"

static double GetCpuStat()
{
    double q, dq = -1.0;
    long total_time = 1;
    FILE *fp;
    long userticks = 0, niceticks = 0, systemticks = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
    char cpuname[CF_MAXVARSIZE], buf[CF_BUFSIZE];


    if ((fp = fopen("/proc/stat", "r")) == NULL)
    {
        printf( "Didn't find proc data");
        return -1.0;
    }

    printf( "Reading /proc/stat utilization data -------");

    while (!feof(fp))
    {
        if (fgets(buf, sizeof(buf), fp) == NULL)
        {
            break;
        }

        if (sscanf(buf, "%s%ld%ld%ld%ld%ld%ld%ld", cpuname, &userticks, &niceticks, &systemticks, &idle, &iowait, &irq,
               &softirq) != 8)
        {
            printf( "Could not scan /proc/stat line: %60s", buf);
            continue;
        }

        total_time = (userticks + niceticks + systemticks + idle);
printf("total=%ld\n", total_time);

        q = 100.0 * (double) (total_time - idle);

        if (strcmp(cpuname, "cpu") == 0)
        {
            printf( "Found aggregate CPU");

            dq = q / (double) total_time;
            if ((dq > 100) || (dq < 0))
            {
                dq = 50;
            }
        }
    }

    fclose(fp);
    return dq;
}


void test_load_masterfiles(void)
{
    double cf_this[100];
    int i;
    double dq1 = GetCpuStat();
    MonCPUGatherData(cf_this);
    printf("oF[%d]=%f\n",ob_cpuall, cf_this[ob_cpuall]);
    printf("oF[%d]=%f\n",ob_cpu0, cf_this[ob_cpu0]);
    double dq2 = GetCpuStat();
    printf("dq1=%f dq2=%f\n", dq1, dq2);
    //for(i=0; i<4; i++) printf("oLq[%d]=%f,oLt[%d]=%f\n",i, LAST_CPU_Q[i],i, LAST_CPU_T[i]);

    assert_true(cf_this[ob_cpuall]>=dq1 && cf_this[ob_cpuall]<=dq2);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_load_masterfiles),
    };

    return run_tests(tests);
}
