/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "mod_services.h"

#include "syntax.h"

static const ConstraintSyntax CF_SERVMETHOD_BODY[] =
{
    ConstraintSyntaxNewString("service_args", "", "Parameters for starting the service as command", NULL),
    ConstraintSyntaxNewOption("service_autostart_policy", "none,boot_time,on_demand", "Should the service be started automatically by the OS", NULL),
    ConstraintSyntaxNewBundle("service_bundle", "A bundle reference with two arguments (service_name,args) used if the service type is generic"),
    ConstraintSyntaxNewOption("service_dependence_chain", "ignore,start_parent_services,stop_child_services,all_related", "How to handle dependencies and dependent services", NULL),
    ConstraintSyntaxNewOption("service_type", "windows,generic", "Service abstraction type", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_SERVICES_BODIES[] =
{
    ConstraintSyntaxNewOption("service_policy", "start,stop,disable,restart,reload", "Policy for cfengine service status", NULL),
    ConstraintSyntaxNewStringList("service_dependencies", CF_IDRANGE, "A list of services on which the named service abstraction depends"),
    ConstraintSyntaxNewBody("service_method", CF_SERVMETHOD_BODY, "Details of promise body for the service abtraction feature"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_SERVICES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "services", ConstraintSetSyntaxNew(CF_SERVICES_BODIES, NULL)),
    PromiseTypeSyntaxNewNull()
};
