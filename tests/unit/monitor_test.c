#include "test.h"

#include "generic_agent.h"
#include "env_context.h"
#include "sysinfo.h"

#include "mon.h"

void test_load_masterfiles(void)
{
    EvalContext *ctx = EvalContextNew();
    DiscoverVersion(ctx);

    double cf_this[100];
    int i;
    for(i=0; i<10; i++) printf("iF[%d]=%f\n",i, cf_this[i]);
    MonCPUGatherData(cf_this);
    for(i=0; i<10; i++) printf("oF[%d]=%f\n",i, cf_this[i]);
    for(i=0; i<4; i++) printf("oLq[%d]=%f,oLt[%d]=%f\n",i, LAST_CPU_Q[i],i, LAST_CPU_T[i]);

    GenericAgentConfig *config = GenericAgentConfigNewDefault(AGENT_TYPE_COMMON);

    GenericAgentConfigSetInputFile(config, NULL,
                                   ABS_TOP_SRCDIR "/masterfiles/promises.cf");

    Policy *masterfiles = GenericAgentLoadPolicy(ctx, config);
    assert_true(masterfiles);

    PolicyDestroy(masterfiles);
    GenericAgentConfigDestroy(config);
    EvalContextDestroy(ctx);
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
