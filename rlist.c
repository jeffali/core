#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef unsigned int bool;
typedef enum
{ false = 0, true = 1 };

#define CF_MAXVARSIZE  1024

/*Rlist **/ int *
RlistParseStringBounded(const char *str, const char *left, const char *right,
                        int *n)
{
    //Rlist *newlist = NULL;
    char str2[CF_MAXVARSIZE];
    char *s = left;
    char *s2 = str2;

    int precede = false;
    bool ignore = true;         //ignore outside "(s)
    bool skipped = true;
    char *extract = NULL;

    memset(str2, 0, CF_MAXVARSIZE);
    if (str[0] == '\"') {
        return NULL;
    }
    if (str[strlen(str)-1] == '\"') {
        return NULL;
    }

    *n = 0;
    //[  "______"  , "_____" , "______"     ]
    printf("________________________________________________\n");
    while (*s && s < right)
    {
        if (*s != '\\')
        {
            if (precede)
            {
                if (*s != '\\' && *s != '"' /*&& *s!=',' */ )
                {
                    printf
                        ("big problem (presence of %c after separator)\n",
                         *s);
                    goto clean;
                }
                else
                    *s2++ = *s;
                precede = false;
            }
            else
            {
                if (*s == '"')
                {
                    if (ignore)
                    {
                        if (skipped != true)
                            goto clean; //" should come after ,
                        ignore = false;
                        extract = s2;   //+ 1;
                    }
                    else
                    {
                        //add extract (length = s - extract) to list
                        printf("\tExtract :[%s](%d)\n", extract,
                               (size_t) (s2 - extract));
                        //RlistAppendScalar(&newlist, extract);
                        ignore = true;
                        extract = NULL;
                        *n += 1;
                    }
                    skipped = false;
                }
                else if (*s == ',')
                {
                    if (ignore)
                    {
                        *s2++ = 0xa;
                        if (skipped == false)
                        {
                            skipped = true;
                        }
                        else
                        {
                            goto clean; /*duplicate , */
                        }
                    }
                    else
                    {
                        *s2++ = *s;
                    }
                }
                else
                {
                    if (ignore == true && *s != ' ')
                    {
                        printf("Wa3333=%c\n", *s);
                        goto clean;
                    }
                    *s2++ = *s;
                }
            }
        }
        else
        {
            if (precede)
            {
                *s2++ = '\\';
                precede = false;
            }
            else
            {
                precede = true;
            }
        }
        s++;
    }
    //printf("%s\n", str);
    //printf("%s\n", str2);
    //return newlist;
    if (ignore)
    {
        return (int *) 1;
    }
    else
    {
        return NULL;
    }
  clean:
    printf("***cleaned****\n");
    //if(newlist) free(newlist);
    return NULL;
}

char *TrimLeft(const char *str)
{
    char *s = str;

    bool crossed = false;

    if (!s)
        return NULL;
    while (*s)
    {
        if (crossed == false)
        {
            if (*s == ' ')
            {
            }
            else if (*s == '{')
            {
                crossed = true;
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            if (*s == ' ')
            {
            }
            else if (*s == '"')
            {
                return s;
            }
            else
            {
                return NULL;
            }
        }
        s++;
    }
    return NULL;
}

char *TrimRight(const char *str)
{
    bool crossed = false;
    char *s = str + strlen(str) - 1;

    while (*s && s > str)
    {
        if (crossed == false)
        {
            if (*s == ' ')
            {
            }
            else if (*s == '}')
            {
                crossed = true;
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            if (*s == ' ')
            {
            }
            else if (*s == '"')
            {
                return s + 1;
            }
            else
            {
                return NULL;
            }
        }
        s--;
    }
    return NULL;
}

/*Rlist **/ int *
RlistParseString(char *string, int *n)
{
    //Rlist *newlist = NULL;
/* Parse a string representation generated by ShowList and turn back into Rlist */
    char *l = TrimLeft(string);

    printf(">Left.l =[%s]\n", l);
    if (l == NULL)
        return NULL;
    char *r = TrimRight(l);

    printf(">Right.r =[%s]\n", r);
    if (r == NULL)
        return NULL;
    /*newlist = */ return RlistParseStringBounded(string, l, r, n);
    //return newlist;
}

struct ParseRoullete
{
    int nfields;
    char str[4096];
} PR[] =
{
    /*Simple */
    {
    1, "{\"a\"}"},
    {
    2, "{\"a\",\"b\"}"},
    {
    3, "{\"a\",\"b\",\"c\"}"},
        /*Simple empty */
    {
    1, "{\"\"}"},
    {
    2, "{\"\",\"\"}"},
    {
    3, "{\"\",\"\",\"\"}"},
        /*Single escaped */
    {
    1, "{\"\\\"\"}"},
    {
    1, "{\",\"}"},
    {
    1, "{\"\\\\\"}"},
    {
    1, "{\"}\"}"},
    {
    1, "{\"{\"}"},
    {
    1, "{\"'\"}"},
        /*Couple mixed escaped */
    {
    1, "{\"\\\",\"}"},          /*   [",]    */
    {
    1, "{\",\\\"\"}"},          /*   [,"]    */
    {
    1, "{\",,\"}"},             /*   [\\]    */
    {
    1, "{\"\\\\\\\\\"}"},       /*   [\\]    */
    {
    1, "{\"\\\\\\\"\"}"},       /*   [\"]    */
    {
    1, "{\"\\\"\\\\\"}"},       /*   ["\]    */
        /*Very long */
    {
    1, "{\"AaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"}"},
    {
    2, "{\"Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaA\"  ,  \"Bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbb\\\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbB\" }"},
        /*Inner space (inside elements) */
    {
    1, "{\" \"}"},
    {
    1, "{\"  \"}"},
    {
    1, "{\"   \"}"},
    {
    1, "{\"\t\"}"},
        /*Outer space (outside elements) */
    {
    1, "     {\"\"}       "},
    {
    1, "     {\"a\"}       "},
    {
    2, "     {\"a\",\"b\"}       "},
    {
    1, "{    \"a\"      }"},
    {
    2, "{    \"a\",\"b\"      }"},
    {
    2, "{    \"a\"    ,\"b\"      }"},
    {
    2, "{    \"a\",    \"b\"      }"},
    {
    2, "{    \"a\",    \"b\"}       "},
        /*Normal */
    {
    4, "   { \" ab,c,d\\\\ \" ,  \" e,f\\\"g \" ,\"hi\\\\jk\", \"l''m \" }   "},
    {
    21, "   { \"A\\\"\\\\    \", \"    \\\\\",   \"}B\",   \"\\\\\\\\\"  ,   \"   \\\\C\\\"\"  ,   \"\\\",\"  ,   \",\\\"D\"  ,   \"   ,,    \", \"E\\\\\\\\F\", \"\", \"{\",   \"   G    '\"  ,   \"\\\\\\\"\", \" \\\"  H \\\\    \", \",   ,\"  ,   \"I\", \"  \",   \"\\\"    J  \",   \"\\\",\", \",\\\"\", \",\"  }   "}, /**/
    {
    -1, NULL}
};

char *PFR[] = {
    /* trim left failure */
    "",
    " ",
    "a",
    "\"",
    "\"\"",
    /* trim right failure */
    "{",
    "{ ",
    "{a",
    "{\"",
    "{\"\"",
    /* parse failure */
    /* un-even number of quotation marks */
    "{\"\"\"}",
    "{\"\",\"}",
    "{\"\"\"\"}",
    "{\"\"\"\"\"}",
    "{\"\",\"\"\"}",
    "{\"\"\"\",\"}",
    "{\"\",\"\",\"}",
     /**/ "{\"a\",}",
    "{,\"a\"}",
    "{,,\"a\"}",
    "{\"a\",,\"b\"}",
    /*Comma play */
    " {,}",
    " {,,}",
    " {,,,}",
    " {,\"\"}",
    " {\"\",}",
    " {,\"\",}",
    " {\"\",,}",
    " {\"\",,,}",
    " {,,\"\",,}",
    " {\"\",\"\",}",
    " {\"\",\"\",,}",
    " {   \"\"  ,  \"\" ,  , }",
    /*Ignore space's oddities */
    "\" {\"\"}",
    "{ {\"\"}",
    "{\"\"}\"",
    "{\"\"}\\",
    "{\"\"} } ",
    "a{\"\"}",
    " a {\"\"}",
    "{a\"\"}",
    "{ a \"\"}",
    "{\"\"}a",
    "{\"\"}  a ",
    "{\"\"a}",
    "{\"\" a }",
    "a{\"\"}b",
    "{a\"\"b}",
    "a{\"\"b}",
    "{a\"\"}b",
    "{\"\"a\"\"}",
    "{\"\",\"\"a\"\"}",
    /*Incomplete */
    NULL
};


void test_new_parser()
{
    int *list = NULL;
    char str[4096];
    int i = 0;

    while (PR[i].nfields != -1)
    {
        printf
            ("==================  %d ==================================\n",
             i);
        strcpy(str, PR[i].str);

        int n = -1;

        list = RlistParseString(str, &n);
        printf("[%s] = %x\n", list ? "okiz" : "NULL", list);
        if (list && PR[i].nfields == n)
        {
            printf("[OKN]\n");
        }
        else
        {
            printf("[FAIL]\n");
        }
        //assert_int_equal(PR[i].nfields, n);
        //RlistDestroy(list);
        i++;
    }
}

void test_failure()
{
    char str[4096];
    int i = 0;
    int *list = NULL;
    int n = -1;

    while (PFR[i] != NULL)
    {
        printf("=========== %d ===========================\n", i);
        strcpy(str, PFR[i]);
        list = RlistParseString(str, &n);
        printf("[%s]\n", list ? "ok" : "NULL");
        i++;
    }
}

int main()
{
    //char *str = "   {   \"  a \\\" \\,   \"    ,  X      \"  b   \"   }   ";
    //char *str = "   {   \"  a \\\" \\,   \"  X  ,  Y      \"  b   \"   }   ";
    //char *str = "   {   \"  a \\\" \\,   \"  X Y      \"  b   \"   }   ";
    test_failure();
    //test_new_parser();
    return 0;
    char *str = "   {   \"  a \\\" \\,   \"\"  b   \"   }   ";
    int n = -1;

    RlistParseShown2(str, &n);
    return 0;
}
