#include "test.h"

#include "cf3.defs.h"
#include "matching.h"

static void test_full_text_match(void)
{
    assert_int_equal(FullTextMatch("[a-z]*", "1234abcd6789"), 0);
}

static void test_full_text_match2(void)
{
    assert_int_not_equal(FullTextMatch("[1-4]*[a-z]*.*", "1234abcd6789"), 0);
}

static void test_block_text_match(void)
{
    int start, end;

    assert_int_not_equal(BlockTextMatch("#[^\n]*", "line 1:\nline2: # comment to end\nline 3: blablab", &start, &end),
                         0);

    assert_int_equal(start, 15);
    assert_int_equal(end, 31);
}

static void test_block_text_match2(void)
{
    int start, end;

    assert_int_not_equal(BlockTextMatch("[a-z]+", "1234abcd6789", &start, &end), 0);
    assert_int_equal(start, 4);
    assert_int_equal(end, 8);
}

static void test_extract(void)
{
    char *str;
    str = ExtractFirstReference("[0-9]([0-9]+)([a-z]+)([0-9]+)", "123456aazdfe18999");
    printf("S = %x %s\n", (unsigned int *)str, str);

    char *str2;
    str2 = ExtractNthReference("[0-9]([0-9]+)([a-z]+)([0-9]+)", 2, "123456aazdfe18999");
    printf("S = %x %s\n", (unsigned int *)str2, str2);
}

void test_validate_regex(void)
{
    int ret;
    ret =  ValidateRegEx("[a-z");
    printf("%d\n", ret);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_full_text_match),
        unit_test(test_full_text_match2),
        unit_test(test_block_text_match),
        unit_test(test_block_text_match2),
        unit_test(test_extract),
        unit_test(test_validate_regex),
    };

    return run_tests(tests);
}
