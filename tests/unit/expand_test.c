#include "test.h"

#include "expand.h"
#include "rlist.h"
#include "scope.h"
#include "env_context.h"

void test_map_iterators_from_rval_empty(void **state)
{
    EvalContext *ctx = EvalContextNew();

    Rlist *lists = NULL;
    MapIteratorsFromRval(ctx, "none", &lists, (Rval) { "", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(lists));

    EvalContextDestroy(ctx);
}

void test_map_iterators_from_rval_literal(void **state)
{
    EvalContext *ctx = EvalContextNew();

    Rlist *lists = NULL;
    MapIteratorsFromRval(ctx, "none", &lists, (Rval) { "snookie", RVAL_TYPE_SCALAR });

    assert_int_equal(0, RlistLen(lists));

    EvalContextDestroy(ctx);
}

void test_map_iterators_from_rval_naked_list_var(void **state)
{
    EvalContext *ctx = EvalContextNew();
    ScopeDeleteAll();
    ScopeSetCurrent("scope");

    Rlist *list = NULL;
    RlistAppend(&list, "jersey", RVAL_TYPE_SCALAR);
    RlistAppend(&list, "kafette", RVAL_TYPE_SCALAR);

    printf("LENY = [%d]\n", RlistLen(list));
    VarRef lval = VarRefParse("scope.jwow");

    EvalContextVariablePut(ctx, lval, (Rval) { list, RVAL_TYPE_LIST }, DATA_TYPE_STRING_LIST);

    Rlist *lists = NULL;
    MapIteratorsFromRval(ctx, "scope", &lists, (Rval) { "${jwow}", RVAL_TYPE_SCALAR });

    assert_int_equal(1, RlistLen(lists));
    printf("LENYLAST=[%s]\n", RlistScalarValue(RlistLast(lists)));
    assert_string_equal("jwow", lists->item);
#if 1
 Rval rval = (Rval) { "${jwow}", RVAL_TYPE_SCALAR };
 Rlist *rp;
        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            MapIteratorsFromRval(ctx, "scope", &lists, (Rval) {rp->item, rp->type});
            printf("Grrrrr=[%s]\n", lists->item);
        }
#endif

    VarRefDestroy(lval);
    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_map_iterators_from_rval_empty),
        unit_test(test_map_iterators_from_rval_literal),
        unit_test(test_map_iterators_from_rval_naked_list_var),
    };

    return run_tests(tests);
}
