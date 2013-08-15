#include "test.h"

#include "generic_agent.h"
#include "item_lib.h"
#include "mon.h"

static void tests_setup(void)
{
    snprintf(CFWORKDIR, CF_BUFSIZE, "/tmp/persistent_lock_test.XXXXXX");
    mkdtemp(CFWORKDIR);

    char buf[CF_BUFSIZE];
    snprintf(buf, CF_BUFSIZE, "%s/state", CFWORKDIR);
    mkdir(buf, 0755);
}

static void tests_teardown(void)
{
    char cmd[CF_BUFSIZE];
    snprintf(cmd, CF_BUFSIZE, "rm -rf '%s'", CFWORKDIR);
    //system(cmd);
}

static bool GetSysUsers( int *userListSz, int *numRootProcs, int *numOtherProcs)
{
    FILE *fp;
    char user[CF_BUFSIZE];
    char vbuff[CF_BUFSIZE];
    char cmd[CF_BUFSIZE];

#if defined(__sun)
    snprintf(cmd, CF_BUFSIZE, "/bin/ps -eo user > %s/users.txt", CFWORKDIR);
#elif defined(_AIX)
    snprintf(cmd, CF_BUFSIZE, "/bin/ps -N -eo user > %s/users.txt", CFWORKDIR);
#elif defined(__linux__)
    snprintf(cmd, CF_BUFSIZE, "/bin/ps -eo user > %s/users.txt", CFWORKDIR);
#else
    return false;
#endif

    Item *userList = NULL;
    system(cmd);
    snprintf(cmd, CF_BUFSIZE, "%s/users.txt", CFWORKDIR);
    fp = fopen(cmd, "r");
    while (fgets(vbuff, CF_BUFSIZE, fp) != NULL)
    {
        sscanf(vbuff, "%s", user);
        printf("xBUF=%s\n", user);

        if (strcmp(user, "USER") == 0)
        {
            continue;
        }

        if (!IsItemIn(userList, user))
        {
            PrependItem(&userList, user, NULL);
            (*userListSz)++;
        }

        if (strcmp(user, "root") == 0)
        {
            (*numRootProcs)++;
        }
        else
        {
            (*numOtherProcs)++;
        }
    }
    fclose(fp);
    return true;
}

void test_processes_monitor(void)
{
    double cf_this[100];
    MonProcessesGatherData(cf_this);
    MonProcessesGatherData(cf_this);
    MonProcessesGatherData(cf_this);
    int usr, rusr, ousr;
    usr = rusr = ousr = 0;
    bool res = GetSysUsers(&usr, &rusr, &ousr);
    printf( "(Users,root,other) = (%d,%d,%d)\n", (int) cf_this[0], (int) cf_this[1], (int) cf_this[2]);
    printf( "(Us,rt,ot) = (%d,%d,%d)\n", (int) usr, (int) rusr, (int) ousr);
    usr  = 3*usr;
    rusr = 3*rusr;
    ousr = 3*ousr;
    assert_true(cf_this[0]<=usr*1.10 && cf_this[0]>=usr*0.90);
}

int main()
{
    strcpy(CFWORKDIR, "data");
#if defined(__sun)
    VSYSTEMHARDCLASS = 4;
#elif defined(_AIX)
    VSYSTEMHARDCLASS = 2;
#elif defined(__linux__)
    VSYSTEMHARDCLASS = 3;
#endif

    PRINT_TEST_BANNER();
    tests_setup();
    const UnitTest tests[] =
    {
        unit_test(test_processes_monitor),
    };

    int ret = run_tests(tests);
    tests_teardown();
    return ret;
}
