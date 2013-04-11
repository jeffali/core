#ifndef CFENGINE_FILES_TEMPLATES_H
#define CFENGINE_FILES_TEMPLATES_H

#include "cf3.defs.h"
#include "writer.h"

typedef enum
{
    TEMPLATE_RENDER_OK,

    TEMPLATE_RENDER_ERROR_SYNTAX,
    TEMPLATE_RENDER_ERROR_IO,

    TEMPLATE_RENDER_ERROR_MAX
} TemplateRenderResult;

TemplateRenderResult TemplateRender(const EvalContext *ctx, FILE *input_file, Writer *output, const char *ns, const char *scope);

#endif
