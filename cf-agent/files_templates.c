#include "files_templates.h"

#include "files_interfaces.h"
#include "expand.h"
#include "env_context.h"

static const unsigned int MAX_BLOCK_SIZE = 10000;

static bool CheckTemplateSyntax(FILE *input_file)
{
#if 0
    fpos64_t start_pos = 0;
    if (fgetpos64(input_file, &start_pos) == -1)
    {
        return false;
    }

    bool in_block = false;
    int block_size = 0;

    for (ssize_t line_len = CfReadLine(line, CF_BUFSIZE, input_file);
         line_len != 0;
         line_len = CfReadLine(line, CF_BUFSIZE, input_file))
    {
        if (line_len == -1)
        {
            return false;
        }

        if (strncmp("[%CFEngine", line, sizeof("[%CFEngine") - 1) == 0)
        {
            char op[CF_BUFSIZE] = "";
            {
                char bracket[CF_SMALLBUF] = "";
                sscanf(line + strlen("[%CFEngine"), "%1024s %s", op, bracket);
            }

            if (strcmp("BEGIN", op) == 0)
            {
                if (in_block)
                {
                    // TODO: Template cannot contain nested blocks
                    return false;
                }
                assert(block_size == 0);
                in_block = true;
            }
            else if (strcmp(op, "END") == 0)
            {
                if (!in_block)
                {
                    // TODO: Not in a block
                    return false;
                }

                if (block_size > MAX_BLOCK_SIZE)
                {
                    // TODO: Size of block cannot exceed MAX_BLOCK_SIZE
                    return false;
                }
                block_size = 0;
                in_block = false;
            }
            else if (strcmp(op + strlen(op)-2, "::") == 0)
            {
                continue;
            }
            else
            {
                // TODO: Invalid control block
                return false;
            }
        }
        else
        {
            if (in_block)
            {
                block_size++;
            }
        }
    }

    fsetpos64(input_file, start_pos);

    if (in_block)
    {
        // TODO: Unterminated block
        return false;
    }
#else
    return true;
#endif
}

/**
 input_file : the template file
 output : ??
 ns : the namespace
 scope : the scope

*/
TemplateRenderResult TemplateRender(const EvalContext *ctx, FILE *input_file, Writer *output, const char *ns, const char *scope)
{
    if (!CheckTemplateSyntax(input_file))
    {
        return TEMPLATE_RENDER_ERROR_SYNTAX;
    }

    char line[CF_BUFSIZE] = "";

    Seq *block = SeqNew(MAX_BLOCK_SIZE, free);

    bool in_active_context = true;

    /*
       read line by line
       end
    */
    bool in_block = false;
    for (ssize_t line_len = CfReadLine(line, CF_BUFSIZE, input_file);
         line_len != 0;
         line_len = CfReadLine(line, CF_BUFSIZE, input_file))
    {
        if (line_len == -1)
        {
            return TEMPLATE_RENDER_ERROR_IO;
        }

        if (strncmp("[%CFEngine", line, sizeof("[%CFEngine") - 1) == 0)
        {
            char op[CF_BUFSIZE] = "";
            {
                char bracket[CF_SMALLBUF] = "";
                sscanf(line + strlen("[%CFEngine"), "%1024s %s", op, bracket);
            }

            if (strcmp("BEGIN", op) == 0)
            {
                in_block = true;
            }
            else if (strcmp(op, "END") == 0)
            {
                in_block = false;
            }
            else if (strcmp(op + strlen(op)-2, "::") == 0)
            {
                *(op + strlen(op) - 2) = '\0';
                in_active_context = IsDefinedClass(ctx, op, ns);
            }
            else
            {
                printf("PROGRAMMER ERROR : Syntax error in template\n");
            }
        }
        else if (in_active_context)
        {
            if (in_block)
            {
                char expanded_line[CF_EXPANDSIZE] = "";
                ExpandScalar(ctx, scope, line, expanded_line);

                WriterWrite(output, expanded_line);
            }
            else
            {
                WriterWrite(output, line);
            }

            WriterWriteChar(output, '\n');
        }
        else
        {
            continue;
        }
    }

    return TEMPLATE_RENDER_OK;
}
