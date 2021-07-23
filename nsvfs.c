/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://mozilla.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is AOLserver Code and related documentation
 * distributed by AOL.
 * 
 * The Initial Developer of the Original Code is America Online,
 * Inc. Portions created by AOL are Copyright (C) 1999 America Online,
 * Inc. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under either the License or the GPL.
 */

/* 
 * nsvfs.c --
 *
 *      Implements Tcl VFS friendly versions of some
 *      NaviServer routines.
 */

#include "vfs.h"

NS_EXPORT int Ns_ModuleVersion = 1;
NS_EXPORT Ns_ModuleInitProc Ns_ModuleInit;


/*
 * Static functions defined in this file.
 */


static Ns_TclTraceProc InitInterp;


/*
 * Static variables defined in this file.
 */

static struct {
    const char                 *name;
    Tcl_ObjCmdProc             *proc;
} cmds[] = {
    {"vfs_register_fastpath",  VFSRegisterFastpathObjCmd},
    {"vfs_returnfile",         VFSReturnFileObjCmd},
    {"vfs_respond",            VFSRespondObjCmd}
};


/*
 *----------------------------------------------------------------------
 *
 * Ns_ModuleInit --
 *
 *      Register the vfs commands.
 *
 * Results:
 *      NS_OK.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

NS_EXPORT Ns_ReturnCode
Ns_ModuleInit(const char *server, const char *module)
{
    VFSConfig  *cfg;
    const char *path, *p;

    cfg = ns_malloc(sizeof(VFSConfig));
    cfg->server = server;

    Ns_Log(Notice, "nslvfs: module %s version %s loaded", module, NSVFS_VERSION);
    /*
     * Inspect the fastpath configuration.
     */

    path = Ns_ConfigGetPath(server, NULL, "fastpath", NULL);
    p = Ns_ConfigString(path, "directoryfile",
            "index.adp index.tcl index.html index.htm");
    if (p != NULL && Tcl_SplitList(NULL, p, &cfg->dirc, &cfg->dirv) != TCL_OK) {
        Ns_Log(Error, "nsvfs: directoryfile is not a list: \"%s\"", p);
    }

    p = Ns_ConfigString(path, "directorylisting", "simple");
    if (p != NULL && (STREQ(p, "simple") || STREQ(p, "fancy"))) {
        p = "_ns_dirlist";
    }
    cfg->dirproc = Ns_ConfigString(path, "directoryproc", p);
    cfg->diradp  = Ns_ConfigGetValue(path, "directoryadp");

    /*
     * Add commands to an initialsing interp.
     */

    Ns_TclRegisterTrace(server, InitInterp, cfg, NS_TCL_TRACE_CREATE);

    /*
     * Register introspection callbacks.
     */

    Ns_RegisterProcInfo((Ns_Callback *)InitInterp, "vfs:initinterp", NULL);
    Ns_RegisterProcInfo((Ns_Callback *)VFSFastget, "vfs:fastget", NULL);
    /* Ns_RegisterProcInfo(VFSAdpPage, "vfs:adppage", NULL); */
    /* Ns_RegisterProcInfo(VFSTclPage, "vfs:tclpage", NULL); */
    /* Ns_RegisterProcInfo(VFSPageMap, "vfs:onepage", VFSPageMapArgProc); */

    return NS_OK;
}

static Ns_ReturnCode
InitInterp(Tcl_Interp *interp, const void *arg)
{
    VFSConfig *cfg = (VFSConfig *)arg;
    size_t     i;

    for (i = 0u; i < (sizeof(cmds) / sizeof(cmds[0])); i++) {
        Tcl_CreateObjCommand(interp, cmds[i].name, cmds[i].proc, cfg, NULL);
    }

    return NS_OK;
}
