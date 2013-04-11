#include "test.h"

#include "cf3.defs.h"
#include "files_templates.h"
#include "string_lib.h"
#include "env_context.h"
#include "rlist.h"

void test_template(const EvalContext *ctx, const char *filename)
{
    char *input_path = StringFormat("%s/%s", TESTDATADIR, filename);
    FILE *input = fopen(input_path, "r");
    free(input_path);

    FILE *output = tmpfile();
    Writer *w = FileWriter(output);

    assert_int_equal(TEMPLATE_RENDER_OK, TemplateRender(ctx, input, w, NamespaceDefault(), "foo"));

    fclose(input);

    char *expected_path = StringFormat("%s/%s.expected", TESTDATADIR, filename);
    FILE *expected = fopen(expected_path, "r");
    free(expected_path);

    rewind(output);
    assert_file_equal(expected, output);

    WriterClose(w);
    fclose(expected);
}

void test_template1(void **state)
{
    EvalContext *ctx = EvalContextNew();
    EvalContextHeapAddHard(ctx, "linux");

    test_template(ctx, "template1");

    EvalContextDestroy(ctx);
}

void test_template2(void **state)
{
    test_template(NULL, "template2");
}

void test_template3(void **state)
{
    EvalContext *ctx = EvalContextNew();

    test_template(ctx, "template3");

    EvalContextDestroy(ctx);
}

void test_template4(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:foo.one");
        EvalContextVariablePut(ctx, lval, (Rval) { "one", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }
    {
        VarRef lval = VarRefParse("default:foo.two");
        EvalContextVariablePut(ctx, lval, (Rval) { "two", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    test_template(ctx, "template4");

    EvalContextDestroy(ctx);
}

void test_template_block_iteration(void **state)
{
    EvalContext *ctx = EvalContextNew();
    {
        VarRef lval = VarRefParse("default:foo.list");

        Rlist *list = NULL;
        RlistAppendScalar(&list, "1");
        RlistAppendScalar(&list, "2");

        EvalContextVariablePut(ctx, lval, (Rval) { list, RVAL_TYPE_LIST }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
        RlistDestroy(list);
    }
    {
        VarRef lval = VarRefParse("default:foo.two");
        EvalContextVariablePut(ctx, lval, (Rval) { "two", RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
        VarRefDestroy(lval);
    }

    test_template(ctx, "template_block_iteration");

    EvalContextDestroy(ctx);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_template1),
        unit_test(test_template2),
        unit_test(test_template3),
        unit_test(test_template4),
        unit_test(test_template_block_iteration),
    };

    return run_tests(tests);
}
