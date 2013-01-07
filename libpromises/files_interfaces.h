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

#ifndef CFENGINE_FILES_INTERFACES_H
#define CFENGINE_FILES_INTERFACES_H

#include "cf3.defs.h"

void SourceSearchAndCopy(char *from, char *to, int maxrecurse, Attributes attr, Promise *pp, const ReportContext *report_context);
void VerifyCopy(char *source, char *destination, Attributes attr, Promise *pp, const ReportContext *report_context);
void LinkCopy(char *sourcefile, char *destfile, struct stat *sb, Attributes attr, Promise *pp, const ReportContext *report_context);
int cfstat(const char *path, struct stat *buf);
int cf_lstat(char *file, struct stat *buf, Attributes attr, Promise *pp);
int CopyRegularFile(char *source, char *dest, struct stat sstat, struct stat dstat, Attributes attr, Promise *pp, const ReportContext *report_context);

/**
 * @return Number of characters read, or -1 on error.
 */
ssize_t CfReadLine(char *buff, size_t size, FILE *fp);

#endif