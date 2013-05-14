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

#include "cf3.defs.h"

#include "files_names.h"
#include "files_interfaces.h"
#include "files_operators.h"
#include "files_lib.h"
#include "item_lib.h"
#include "conversion.h"
#include "matching.h"
#include "logging_old.h"
#include "string_lib.h"
#include "pipes.h"
#include "nfs.h"
#include "misc_lib.h"
#include "rlist.h"
#include "env_context.h"

/* seconds */
#define RPCTIMEOUT 60

static int FSTAB_EDITS;
static Item *FSTABLIST = NULL;

static void AugmentMountInfo(Rlist **list, char *host, char *source, char *mounton, char *options, char *type);
static int MatchFSInFstab(char *mountpt, char *host, char *rmountpt, char *opts, char *where);
//static int MatchFSInFstab(char *match);
static void DeleteThisItem(Item **liststart, Item *entry);

static const char *VMOUNTCOMM[PLATFORM_CONTEXT_MAX] =
{
    "",
    "/sbin/mount -ea",          /* hpux */
    "/usr/sbin/mount -t nfs",   /* aix */
    "/bin/mount -va",           /* linux */
    "/usr/sbin/mount -a",       /* solaris */
    "/sbin/mount -va",          /* freebsd */
    "/sbin/mount -a",           /* netbsd */
    "/etc/mount -va",           /* cray */
    "/bin/sh /etc/fstab",       /* NT - possible security issue */
    "/sbin/mountall",           /* Unixware */
    "/sbin/mount",              /* openbsd */
    "/etc/mountall",            /* sco */
    "/sbin/mount -va",          /* darwin */
    "/bin/mount -v",            /* qnx */
    "/sbin/mount -va",          /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/bin/mount -a",            /* vmware */
};

static const char *VUNMOUNTCOMM[PLATFORM_CONTEXT_MAX] =
{
    "",
    "/sbin/umount",             /* hpux */
    "/usr/sbin/umount",         /* aix */
    "/bin/umount",              /* linux */
    "/etc/umount",              /* solaris */
    "/sbin/umount",             /* freebsd */
    "/sbin/umount",             /* netbsd */
    "/etc/umount",              /* cray */
    "/bin/umount",              /* NT */
    "/sbin/umount",             /* Unixware */
    "/sbin/umount",             /* openbsd */
    "/etc/umount",              /* sco */
    "/sbin/umount",             /* darwin */
    "/bin/umount",              /* qnx */
    "/sbin/umount",             /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/bin/umount",              /* vmware */
};

static const char *VMOUNTOPTS[PLATFORM_CONTEXT_MAX] =
{
    "",
    "bg,hard,intr",             /* hpux */
    "bg,hard,intr",             /* aix */
    "defaults",                 /* linux */
    "bg,hard,intr",             /* solaris */
    "bg,intr",                  /* freebsd */
    "-i,-b",                    /* netbsd */
    "bg,hard,intr",             /* cray */
    "",                         /* NT */
    "bg,hard,intr",             /* Unixware */
    "-i,-b",                    /* openbsd */
    "bg,hard,intr",             /* sco */
    "-i,-b",                    /* darwin */
    "bg,hard,intr",             /* qnx */
    "bg,intr",                  /* dragonfly */
    "mingw-invalid",            /* mingw */
    "defaults",                 /* vmstate */
};

static void ParseMountCommandOutput(char *iline, char *mountpt, char *remote, char *options, char *type)
{
  /*TODO: 
   - filter blank
   - if new regex/scanf DNW, revert to older %s%s%s
  */
    sscanf(iline, "%s on %s type %s (%s)", remote, mountpt, type, options);
    printf("X1=[%s],X2=[%s],X3=[%s],X4=[%s]\n",iline,mountpt,remote,options,type);
    //char *str2;
    //str2 = ExtractNthReference("[0-9]([0-9]+)([a-z]+)([0-9]+)", 2, "123456aazdfe18999");

#if defined(__QNX__) || defined(__QNXNTO__)
#elif defined(_CRAY)
#elif defined(__hpux)
#elif defined(_AIX)
#elif defined(__linux__)
#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
#elif defined(__sun) || defined(sco) || defined(__SCO_DS)
#elif defined(__CYGWIN__)
#endif

}

bool LoadMountInfo(Rlist **list)
/* This is, in fact, the most portable way to read the mount info! */
/* Depressing, isn't it? */
{
    FILE *pp;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE], buf3[CF_BUFSIZE];
    char buf4[CF_BUFSIZE]; /*fs type*/
    char host[CF_MAXVARSIZE], source[CF_BUFSIZE], mounton[CF_BUFSIZE], vbuff[CF_BUFSIZE];
    int i, nfs = false;

    for (i = 0; VMOUNTCOMM[VSYSTEMHARDCLASS][i] != ' '; i++)
    {
        buf1[i] = VMOUNTCOMM[VSYSTEMHARDCLASS][i];
    }

    buf1[i] = '\0';

    SetTimeOut(RPCTIMEOUT);

    if ((pp = cf_popen(buf1, "r", true)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Can't open %s\n", buf1);
        return false;
    }

    for (;;)
    {
        vbuff[0] = buf1[0] = buf2[0] = buf3[0] = source[0] = '\0';
        nfs = false;
        ssize_t res = CfReadLine(vbuff, CF_BUFSIZE, pp);

        if (res == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fread", "Unable to read list of mounted filesystems");
            cf_pclose(pp);
            return false;
        }

        if (res == 0)
        {
            break;
        }

        ParseMountCommandOutput(vbuff, buf3/*mountp*/, buf1/*srv/src*/, buf2/*opts*/, buf4/*type*/);
 printf("VBUF=[%s]\n",vbuff);

        if (strstr(/*vbuff*/buf4, "nfs"))  //TODO : is this really safe ?
        {
            nfs = true;
        }

        //sscanf(vbuff, "%s%s%s", buf1, buf2, buf3);
 printf("B1=[%s],B2=[%s],B3=[%s]\n",buf1,buf2,buf3);

        if ((vbuff[0] == '\0') || (vbuff[0] == '\n'))
        {
            break;
        }

        if (strstr(vbuff, "not responding"))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "%s\n", vbuff);
        }

        if (strstr(vbuff, "be root"))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Mount access is denied. You must be root.\n");
            CfOut(OUTPUT_LEVEL_ERROR, "", "Use the -n option to run safely.");
        }

        if ((strstr(vbuff, "retrying")) || (strstr(vbuff, "denied")) || (strstr(vbuff, "backgrounding")))
        {
            continue;
        }

        if ((strstr(vbuff, "exceeded")) || (strstr(vbuff, "busy")))
        {
            continue;
        }

        if (strstr(vbuff, "RPC"))
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "There was an RPC timeout. Aborting mount operations.\n");
            CfOut(OUTPUT_LEVEL_INFORM, "", "Session failed while trying to talk to remote host\n");
            CfOut(OUTPUT_LEVEL_INFORM, "", "%s\n", vbuff);
            cf_pclose(pp);
            return false;
        }

#if defined(__sun) || defined(__hpux)
        if (IsAbsoluteFileName(buf3))
        {
            strcpy(host, "localhost");
            strcpy(mounton, buf1);
        }
        else
        {
            sscanf(buf1, "%[^:]:%s", host, source);
            strcpy(mounton, buf1);
        }
#elif defined(_AIX)
        /* skip header */

        if (IsAbsoluteFileName(buf1))
        {
            strcpy(host, "localhost");
            strcpy(mounton, buf2);
        }
        else
        {
            strcpy(host, buf1);
            strcpy(source, buf1);
            strcpy(mounton, buf3);
        }
#elif defined(__CYGWIN__)
        strcpy(mounton, buf2);
        strcpy(host, buf1);
#elif defined(sco) || defined(__SCO_DS)
        CfOut(OUTPUT_LEVEL_ERROR, "", "Don't understand SCO mount format, no data");
#else
        if (IsAbsoluteFileName(buf1))
        {
            strcpy(host, "localhost");
            strcpy(mounton, buf3);
        }
        else
        {
            sscanf(buf1, "%[^:]:%s", host, source);
            strcpy(mounton, buf3);
        }
#endif

        CfDebug("GOT: host=%s, source=%s, mounton=%s\n", host, source, mounton);

        if (nfs)
        {
            AugmentMountInfo(list, host, source, mounton, buf2, "nfs");
        }
        else
        {
            AugmentMountInfo(list, host, source, mounton, buf2, NULL);
        }
    }

    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cf_pclose(pp);
    return true;
}

/*******************************************************************/

static void AugmentMountInfo(Rlist **list, char *host, char *source, char *mounton, char *options, char *type)
{
    Mount *entry = xcalloc(1, sizeof(Mount));

    if (host)
    {
        entry->host = xstrdup(host);
    }

    if (source)
    {
        entry->source = xstrdup(source);
    }

    if (mounton)
    {
        entry->mounton = xstrdup(mounton);
    }

    if (options)
    {
        entry->options = xstrdup(options);
    }

    if (type)
    {
        entry->type = xstrdup(type);
    }

    RlistAppendAlien(list, (void *) entry);
}

/*******************************************************************/

void DeleteMountInfo(Rlist *list)
{
    Rlist *rp, *sp;
    Mount *entry;

    for (rp = list; rp != NULL; rp = sp)
    {
        sp = rp->next;
        entry = (Mount *) rp->item;

        if (entry->host)
        {
            free(entry->host);
        }

        if (entry->source)
        {
            free(entry->source);
        }

        if (entry->mounton)
        {
            free(entry->mounton);
        }

        if (entry->options)
        {
            free(entry->options);
        }

        if (entry->type)
        {
            free(entry->type);
        }

        free((char *) entry);
    }
}

/*******************************************************************/

int VerifyInFstab(EvalContext *ctx, char *name, Attributes a, Promise *pp)
/* Ensure filesystem IS in fstab, and return no of changes */
{
    char fstab[CF_BUFSIZE];
    char *host, *rmountpt, *mountpt, *fstype, *opts;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a.edits))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't open %s!\n", VFSTAB[VSYSTEMHARDCLASS]);
            return false;
        }
        else
        {
            FSTAB_EDITS = 0;
        }
    }

    if (a.mount.mount_options)
    {
        opts = Rlist2String(a.mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    host = a.mount.mount_server;
    rmountpt = a.mount.mount_source;
    mountpt = name;
    fstype = a.mount.mount_type;

    fstab = ConstructFstabLine(mountpt, host, rmountpt, opts, fstype);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Verifying %s in %s\n", mountpt, VFSTAB[VSYSTEMHARDCLASS]);

    char where[CF_BUFSIZE];
    where[0]='\0';
    int ret = MatchFSInFstab(mountpt, host, rmountpt, opts, where);

    if (ret == PRESENT_NONE)
    {
        AppendItem(&FSTABLIST, fstab, NULL);
        FSTAB_EDITS++;
        cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, "Adding file system %s:%s seems to %s.\n", host, rmountpt,
             VFSTAB[VSYSTEMHARDCLASS]);
    }
    else if (ret == PRESENT_MOUNTP || ret == PRESENT_REMOTE)
    {
        //TODO: - delete from FSTABLIST wrong entry (extracted   fstab)
        DeleteItemGeneral(&FSTABLIST, where, ITEM_MATCH_TYPE_LITERAL_COMPLETE);
        //      - add    to   FSTABLIST right entry (constructed fstab)
        AppendItem(&FSTABLIST, fstab, NULL);
        FSTAB_EDITS++;
        cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, "Replacing file system %s:%s entry in %s.\n", host, rmountpt,
             VFSTAB[VSYSTEMHARDCLASS]);
    }

    free(opts);
    return 0;
}

/*******************************************************************/

int VerifyNotInFstab(EvalContext *ctx, char *name, Attributes a, Promise *pp)
/* Ensure filesystem is NOT in fstab, and return no of changes */
{
    char regex[CF_BUFSIZE];
    char *host, *mountpt, *opts;
    Item *ip;

    if (!FSTABLIST)
    {
        if (!LoadFileAsItemList(&FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a.edits))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Couldn't open %s!\n", VFSTAB[VSYSTEMHARDCLASS]);
            return false;
        }
        else
        {
            FSTAB_EDITS = 0;
        }
    }

    if (a.mount.mount_options)
    {
        opts = Rlist2String(a.mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    host = a.mount.mount_server;
    mountpt = name;

    char where[CF_BUFSIZE];
    where[0]='\0';
    int ret = MatchFSInFstab(mountpt, host, a.mount.mount_source, opts, where);
    if (ret == PRESENT_EXACT || ret == PRESENT_MOUNTP || ret == PRESENT_REMOTE)
    {
        if (a.mount.editfstab)
        {
#if defined(_AIX)
            FILE *pfp;
            char line[CF_BUFSIZE], aixcomm[CF_BUFSIZE];

            snprintf(aixcomm, CF_BUFSIZE, "/usr/sbin/rmnfsmnt -f %s", mountpt);

            if ((pfp = cf_popen(aixcomm, "r", true)) == NULL)
            {
                cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "", pp, a, "Failed to invoke /usr/sbin/rmnfsmnt to edit fstab");
                return 0;
            }

            for (;;)
            {
                ssize_t res = CfReadLine(line, CF_BUFSIZE, pfp);

                if (res == -1)
                {
                    cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "fread", pp, a, "Unable to read output of /bin/rmnfsmnt");
                    cf_pclose(pfp);
                    return 0;
                }

                if (res == 0)
                {
                    break;
                }

                if (line[0] == '#')
                {
                    continue;
                }

                if (strstr(line, "busy"))
                {
                    cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_INTERRUPTED, "", pp, a, "The device under %s cannot be removed from %s\n",
                         mountpt, VFSTAB[VSYSTEMHARDCLASS]);
                    return 0;
                }
            }

            cf_pclose(pfp);

            return 0;       /* ignore internal editing for aix , always returns 0 changes */
#else
            DeleteItemGeneral(&FSTABLIST, where, ITEM_MATCH_TYPE_LITERAL_COMPLETE);
            FSTAB_EDITS++;
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, "Deleting file system mounted on %s.\n", host);

#endif
        }
    }

    if (a.mount.mount_options)
    {
        free(opts);
    }

    return 0;
}

/*******************************************************************/

int VerifyMount(EvalContext *ctx, char *name, Attributes a, Promise *pp)
{
    char comm[CF_BUFSIZE], line[CF_BUFSIZE];
    FILE *pfp;
    char *host, *rmountpt, *mountpt, *opts=NULL;

    host = a.mount.mount_server;
    rmountpt = a.mount.mount_source;
    mountpt = name;

    /* Check for options required for this mount - i.e., -o ro,rsize, etc. */
    if (a.mount.mount_options)
    {
        opts = Rlist2String(a.mount.mount_options, ",");
    }
    else
    {
        opts = xstrdup(VMOUNTOPTS[VSYSTEMHARDCLASS]);
    }

    if (!DONTDO)
    {
        snprintf(comm, CF_BUFSIZE, "%s -o %s %s:%s %s", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]), opts, host, rmountpt, mountpt);

        if ((pfp = cf_popen(comm, "r", true)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to open pipe from %s\n", CommandArg0(VMOUNTCOMM[VSYSTEMHARDCLASS]));
            return 0;
        }

        ssize_t res = CfReadLine(line, CF_BUFSIZE, pfp);

        if (res == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fread", "Unable to read output of mount command");
            cf_pclose(pfp);
            return 0;
        }

        if (res != 0 && ((strstr(line, "busy")) || (strstr(line, "Busy"))))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The device under %s cannot be mounted\n", mountpt);
            cf_pclose(pfp);
            return 1;
        }

        cf_pclose(pfp);
    }

    /* Since opts is either Rlist2String or xstrdup'd, we need to always free it */
    free(opts);

    cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, " -> Mounting %s to keep promise\n", mountpt);
    return 0;
}

/*******************************************************************/

int VerifyUnmount(EvalContext *ctx, char *name, Attributes a, Promise *pp)
{
    char comm[CF_BUFSIZE], line[CF_BUFSIZE];
    FILE *pfp;
    char *mountpt;

    mountpt = name;

    if (!DONTDO)
    {
        snprintf(comm, CF_BUFSIZE, "%s %s", VUNMOUNTCOMM[VSYSTEMHARDCLASS], mountpt);

        if ((pfp = cf_popen(comm, "r", true)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to open pipe from %s\n", VUNMOUNTCOMM[VSYSTEMHARDCLASS]);
            return 0;
        }

        ssize_t res = CfReadLine(line, CF_BUFSIZE, pfp);

        if (res == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "fread", "Unable to read output of unmount command");
            cf_pclose(pfp);
            return 0;
        }

        if (res != 0 && ((strstr(line, "busy")) || (strstr(line, "Busy"))))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_INTERRUPTED, "", pp, a, " !! The device under %s cannot be unmounted\n", mountpt);
            cf_pclose(pfp);
            return 1;
        }

        cf_pclose(pfp);
    }

    cfPS(ctx, OUTPUT_LEVEL_INFORM, PROMISE_RESULT_CHANGE, "", pp, a, " -> Unmounting %s to keep promise\n", mountpt);
    return 0;
}

/*******************************************************************/
static char *ConstructFstabLine(char *mountpt, char *host, char *rmountpt, char *opts, char *fstype)
{
    char fstab[CF_BUFSIZE];
#if defined(__QNX__) || defined(__QNXNTO__)
    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s %s\t%s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(_CRAY)
    char fstype_upper[CF_BUFSIZE];
    strlcpy(fstype_upper, fstype, CF_BUFSIZE);
    ToUpperStrInplace(fstype_upper);

    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s %s\t%s", host, rmountpt, mountpt, fstype_upper, opts);
    break;
#elif defined(__hpux)
    snprintf(fstab, CF_BUFSIZE, "%s:%s %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(_AIX)
    snprintf(fstab, CF_BUFSIZE,
             "%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s\n\taccount\t= false\n",
             mountpt, rmountpt, fstype, fstype, host, opts);
#elif defined(__linux__)
    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s \t %s \t %s", host, rmountpt, mountpt, fstype, opts);
#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
    snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(__sun) || defined(sco) || defined(__SCO_DS)
    snprintf(fstab, CF_BUFSIZE, "%s:%s - %s %s - yes %s", host, rmountpt, mountpt, fstype, opts);
#elif defined(__CYGWIN__)
    snprintf(fstab, CF_BUFSIZE, "/bin/mount %s:%s %s", host, rmountpt, mountpt);
#endif
    return fstab;
}

static int DeconstructFstabLine(char *ifstab, char *mountpt, char *host, char *rmountpt, char *opts)
{
    char *out;
    char *fstype;  //should ignore ???
    char *fstype_upper;
    int ret;
#if defined(__QNX__) || defined(__QNXNTO__)
    ret = sscanf(ifstab, "%s:%s \t %s %s\t%s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(_CRAY)
    char fstype_upper[CF_BUFSIZE];
    strlcpy(fstype_upper, fstype, CF_BUFSIZE);
    ToUpperStrInplace(fstype_upper);

    ret = sscanf(ifstab, "%s:%s \t %s %s\t%s", host, rmountpt, mountpt, fstype_upper, opts);
    break;
#elif defined(__hpux)
    ret = sscanf(ifstab, "%s:%s %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(_AIX)
    ret = sscanf(ifstab,
             "%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s\n\taccount\t= false\n",
             mountpt, rmountpt, fstype, fstype, host, opts);
#elif defined(__linux__)
    ret = sscanf(ifstab, "%s:%s \t %s \t %s \t %s", host, rmountpt, mountpt, fstype, opts);
#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
    ret = sscanf(ifstab, "%s:%s \t %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts);
#elif defined(__sun) || defined(sco) || defined(__SCO_DS)
    ret = sscanf(ifstab, "%s:%s - %s %s - yes %s", host, rmountpt, mountpt, fstype, opts);
#elif defined(__CYGWIN__)
    ret = sscanf(ifstab, "/bin/mount %s:%s %s", host, rmountpt, mountpt);
#endif
    //TODO: trim from spaces
    return ret;
}

bool CompareNFSOptions(char *opts, char *opts2) {
   Rlist *o1 = RlistFromSplitString(opts, ",");
   Rlist *o2 = RlistFromSplitString(opts, ",");
   bool ret = RlistCompareExact(o1, o2);
   if(o1) RlistDestroy(o1);
   if(o2) RlistDestroy(o2);
   return ret;
}


static int MatchFSInFstab(char *imountpt, char *ihost, char *irmountpt, char *iopts, char *where)
//@returns : storage state (not changes)
{
    Item *ip;
    char mountpt2 [CF_BUFSIZE];
    char host2    [CF_BUFSIZE];
    char rmountpt2[CF_BUFSIZE];
    char opts2    [CF_BUFSIZE];
    int  ret;
    int  presence = PRESENT_NONE;

    int n = 0;
    for (ip = FSTABLIST; ip != NULL; ip = ip->next)
    {
        if (strstr(ip->name, imountpt))
        {
            ret = DeconstructFstabLine(ip->name, mountpt2, host2, rmountpt2, opts2);
            if(ret)
            {
                 printf("Warning : maybe we are missing something very important\n");
            }
            if(!strncmp(imountpt, mountpt2, CF_BUFSIZE)) {

                strlcpy(where, ip->name, CF_BUFSIZE);

                if(!strncmp(irmountpt, rmountpt2, CF_BUFSIZE) &&
                   !strncmp(ihost,     host2,     CF_BUFSIZE))
                {
                    presence = PRESENT_REMOTE;
                    if(!CompareNFSOptions(iopts, opts2)) {
                        presence = PRESENT_EXACT;
                        n++;
                    }
                }
                else
                {
                    presence = PRESENT_MOUNTP;
                    n++;
                }
            }
        }
    }

    return presence;
}

/*******************************************************************/

void MountAll()
{
    char line[CF_BUFSIZE];
    FILE *pp;

    if (DONTDO)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Promised to mount filesystem, but not on this trial run\n");
        return;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Attempting to mount all filesystems.\n");
    }

#if defined(__CYGWIN__)
    /* This is a shell script. Make sure it hasn't been compromised. */

    struct stat sb;

    if (stat("/etc/fstab", &sb) == -1)
    {
        int fd;
        if ((fd = creat("/etc/fstab", 0755)) > 0)
        {
            if (write(fd, "#!/bin/sh\n\n", 10) != 10)
            {
                UnexpectedError("Failed to write to file '/etc/fstab'");
            }
            close(fd);
        }
        else
        {
            if (sb.st_mode & (S_IWOTH | S_IWGRP))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "File /etc/fstab was insecure. Cannot mount filesystems.\n");
                return;
            }
        }
    }
#endif

    SetTimeOut(RPCTIMEOUT);

    if ((pp = cf_popen(VMOUNTCOMM[VSYSTEMHARDCLASS], "r", true)) == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "cf_popen", "Failed to open pipe from %s\n", VMOUNTCOMM[VSYSTEMHARDCLASS]);
        return;
    }

    for (;;)
    {
        ssize_t res = CfReadLine(line, CF_BUFSIZE, pp);

        if (res == 0)
        {
            break;
        }

        if (res == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "ferror", "Error reading list of mounted filesystems");
            break;
        }

        if ((strstr(line, "already mounted")) || (strstr(line, "exceeded")) || (strstr(line, "determined")))
        {
            continue;
        }

        if (strstr(line, "not supported"))
        {
            continue;
        }

        if ((strstr(line, "denied")) || (strstr(line, "RPC")))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "There was a mount error, trying to mount one of the filesystems on this host.\n");
            break;
        }

        if ((strstr(line, "trying")) && (!strstr(line, "NFS version 2")) && (!strstr(line, "vers 3")))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Attempting abort because mount went into a retry loop.\n");
            break;
        }
    }

    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cf_pclose(pp);
}

/*******************************************************************/
/* Addendum                                                        */
/*******************************************************************/

static void DeleteThisItem(Item **liststart, Item *entry)
{
    Item *ip, *sp;

    if (entry != NULL)
    {
        if (entry->name != NULL)
        {
            free(entry->name);
        }

        sp = entry->next;

        if (entry == *liststart)
        {
            *liststart = sp;
        }
        else
        {
            for (ip = *liststart; ip->next != entry; ip = ip->next)
            {
            }

            ip->next = sp;
        }

        free((char *) entry);
    }
}

void CleanupNFS(void)
{
    Attributes a = { {0} };
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Number of changes observed in %s is %d\n", VFSTAB[VSYSTEMHARDCLASS], FSTAB_EDITS);

    if (FSTAB_EDITS && FSTABLIST && !DONTDO)
    {
        if (FSTABLIST)
        {
            SaveItemListAsFile(FSTABLIST, VFSTAB[VSYSTEMHARDCLASS], a);
            DeleteItemList(FSTABLIST);
            FSTABLIST = NULL;
        }
        FSTAB_EDITS = 0;
    }
}
#if 0
Delete
#if defined(__QNX__) || defined(__QNXNTO__)
([^ \t:]+):([^ \t:]+) \t ([^ \t:]+) ([^ \t:]+)\t([^ \t:]+)[ ]+0[ ]+0
  "%s:%s \t %s %s\t%s 0 0", host, rmountpt, mountpt, fstype, opts

#elif defined(_CRAY)
  "%s:%s \t %s %s\t%s", host, rmountpt, mountpt, fstype_upper, opts

#elif defined(__hpux)
  "%s:%s %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts

#elif defined(_AIX)
  "%s:\n\tdev\t= %s\n\ttype\t= %s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n\toptions\t= %s\n\taccount\t= false\n",
             mountpt, rmountpt, fstype, fstype, host, opts);

#elif defined(__linux__)
  "%s:%s \t %s \t %s \t %s", host, rmountpt, mountpt, fstype, opts

#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__)
  "%s:%s \t %s \t %s \t %s 0 0", host, rmountpt, mountpt, fstype, opts

#elif defined(__sun) || defined(sco) || defined(__SCO_DS)
  "%s:%s - %s %s - yes %s", host, rmountpt, mountpt, fstype, opts

#elif defined(__CYGWIN__)
  "/bin/mount %s:%s %s", host, rmountpt, mountpt
#endif

#endif
