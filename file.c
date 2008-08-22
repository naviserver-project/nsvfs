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
 * file.c --
 *
 *      Implements Tcl VFS friendly versions of the fastpath
 *      static file serving routines.
 *
 *      To simplify the code a cache is not used. We assume that the
 *      application is not speed critical if the Tcl VFS is used.
 */

#include "vfs.h"

NS_RCSID("@(#) $Header$");


/*
 * Static functions defined in this file.
 */

static int Stat(CONST char *path, Tcl_StatBuf *stPtr);
static int StatObj(Tcl_Obj *pathObj, Tcl_StatBuf *stPtr);

static int Return(Ns_Conn *conn, int status,
                  CONST char *type, CONST char *path,
                  Tcl_StatBuf *stPtr);



/*
 *----------------------------------------------------------------------
 *
 * VFSRegisterFastpathObjCmd --
 *
 *      Implements vfs_register_fastpath.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      Overrides any previously registered op-proc for the same URL.
 *
 *----------------------------------------------------------------------
 */

int
VFSRegisterFastpathObjCmd(ClientData arg, Tcl_Interp *interp, int objc,
                           Tcl_Obj *CONST objv[])
{
    VFSConfig  *cfg = arg;
    char       *method, *url;
    int         flags = 0;

    Ns_ObjvSpec opts[] = {
        {"-noinherit", Ns_ObjvBool,  &flags, (void *) NS_OP_NOINHERIT},
        {"--",         Ns_ObjvBreak, NULL,   NULL},
        {NULL, NULL, NULL, NULL}
    };
    Ns_ObjvSpec args[] = {
        {"method",     Ns_ObjvString, &method, NULL},
        {"url",        Ns_ObjvString, &url,    NULL},
        {NULL, NULL, NULL, NULL}
    };
    if (Ns_ParseObjv(opts, args, interp, 1, objc, objv) != NS_OK) {
        return TCL_ERROR;
    }

    Ns_RegisterRequest(cfg->server, method, url,
                       VFSFastget, NULL, cfg, 0);

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * VFSRegisterFastpathObjCmd --
 *
 *      Implements vfs_returnfile.  Send complete response to client
 *      using contents of filename if exists and is readable, otherwise
 *      send error response.
 *
 * Results:
 *      0 - failure, 1 - success
 *      Tcl error on wrong syntax/arguments
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VFSReturnFileObjCmd(ClientData arg, Tcl_Interp *interp, int objc,
                    Tcl_Obj *CONST objv[])
{
    Ns_Conn     *conn = Ns_GetConn();
    Tcl_StatBuf  st;
    char        *type, *path;
    int          status, result;

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 1, objv, "status type filename");
        return TCL_ERROR;
    }
    if (conn == NULL) {
        Tcl_SetResult(interp, "no connection", TCL_STATIC);
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &status) != TCL_OK) {
        return TCL_ERROR;
    }
    type = Tcl_GetString(objv[2]);
    path = Tcl_GetString(objv[3]);

    result = StatObj(objv[3], &st);
    if (result == NS_OK) {
        result = Return(conn, status, type, path, &st);
    }

    Tcl_SetObjResult(interp,
                     Tcl_NewBooleanObj(result == NS_OK ? 1 : 0));

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * VFSRespondObjCmd --
 *
 *      Implements vfs_respond.  Send complete response to client using
 *      a variety of options.
 *
 * Results:
 *      Standard Tcl result.
 *      Interpreter result set to 0 on success or 1 on failure.
 *
 * Side effects:
 *      String data may be transcoded. Connection will be closed.
 *
 *----------------------------------------------------------------------
 */

int
VFSRespondObjCmd(ClientData arg, Tcl_Interp *interp, int objc,
                 Tcl_Obj *CONST objv[])
{
    Ns_Conn     *conn;
    int          status = 200, length = -1;
    char        *type = "*/*", *setid = NULL, *binary = NULL;
    char        *string = NULL, *chanid = NULL;
    Tcl_Obj     *pathObj = NULL;
    Ns_Set      *set = NULL;
    Tcl_Channel  chan;
    Tcl_StatBuf  st;
    int          result;

    Ns_ObjvSpec opts[] = {
        {"-status",   Ns_ObjvInt,       &status,   NULL},
        {"-type",     Ns_ObjvString,    &type,     NULL},
        {"-length",   Ns_ObjvInt,       &length,   NULL},
        {"-headers",  Ns_ObjvString,    &setid,    NULL},
        {"-string",   Ns_ObjvString,    &string,   NULL},
        {"-file",     Ns_ObjvObj,       &pathObj,  NULL},
        {"-fileid",   Ns_ObjvString,    &chanid,   NULL},
        {"-binary",   Ns_ObjvByteArray, &binary,   &length},
        {NULL, NULL, NULL, NULL}
    };
    if (Ns_ParseObjv(opts, NULL, interp, 1, objc, objv) != NS_OK) {
        return TCL_ERROR;
    }

    if (chanid != NULL && length < 0) {
        Tcl_SetResult(interp, "length required when -fileid is used",
                      TCL_STATIC);
        return TCL_ERROR;
    }
    if ((binary != NULL) + (string != NULL) + (pathObj != NULL)
        + (chanid != NULL) != 1) {
        Tcl_SetResult(interp, "must specify only one of -string, "
                      "-file, -binary or -fileid", TCL_STATIC);
        return TCL_ERROR;
    }
    if (setid != NULL) {
        set = Ns_TclGetSet(interp, setid);
        if (set == NULL) {
            Ns_TclPrintfResult(interp, "illegal ns_set id: \"%s\"", setid);
            return TCL_ERROR;
        }
    }
    conn = Ns_GetConn();
    if (conn == NULL) {
        Tcl_SetResult(interp, "no connection", TCL_STATIC);
        return TCL_ERROR;
    }
    if (set != NULL) {
        Ns_ConnReplaceHeaders(conn, set);
    }

    if (chanid != NULL) {
        /*
         * We'll be returning an open channel
         */

        if (Ns_TclGetOpenChannel(interp, chanid, 0, 1, &chan) != TCL_OK) {
            return TCL_ERROR;
        }
        result = Ns_ConnReturnOpenChannel(conn, status, type, chan, length);

    } else if (pathObj != NULL) {
        /*
         * We'll be returning a file by name using the Tcl VFS.
         */

        result = StatObj(pathObj, &st);
        if (result == NS_OK) {
            result = Return(conn, status, type, Tcl_GetString(pathObj), &st);
        }

    } else if (binary != NULL) {
        /*
         * We'll be returning a binary data
         */

        result = Ns_ConnReturnData(conn, status, binary, length, type);

    } else {
        /*
         * We'll be returning a string now.
         */

        result = Ns_ConnReturnCharData(conn, status, string, length, type);
    }

    Tcl_SetObjResult(interp,
                     Tcl_NewBooleanObj(result == NS_OK ? 1 : 0));

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * VFSFastGet --
 *
 *      Return the contents of a URL using the Tcl VFS.
 *
 * Results:
 *      NS_OK for success or NS_ERROR for failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VFSFastget(void *arg, Ns_Conn *conn)
{
    VFSConfig   *cfg = arg;
    char        *url = conn->request->url;
    char        *type;
    Ns_DString   ds;
    int          status, result, i;
    Tcl_StatBuf  st;

    Ns_DStringInit(&ds);

    if (Ns_UrlToFile(&ds, cfg->server, url) != NS_OK
            || Stat(ds.string, &st) != NS_OK) {
        goto notfound;
    }
    if (S_ISREG(st.st_mode)) {

        type = Ns_GetMimeType(ds.string);
        result = Return(conn, 200, type, ds.string, &st);

    } else if (S_ISDIR(st.st_mode)) {

        /*
         * For directories, search for a matching directory file and
         * restart the connection if found.
         */

        for (i = 0; i < cfg->dirc; ++i) {
            Ns_DStringSetLength(&ds, 0);
            if (Ns_UrlToFile(&ds, cfg->server, url) != NS_OK) {
                goto notfound;
            }
            Ns_DStringVarAppend(&ds, "/", cfg->dirv[i], NULL);
            status = Stat(ds.string, &st);
            if (status == NS_OK && S_ISREG(st.st_mode)) {
                if (url[strlen(url) - 1] != '/') {
                    Ns_DStringSetLength(&ds, 0);
                    Ns_DStringVarAppend(&ds, url, "/", NULL);
                    result = Ns_ConnReturnRedirect(conn, ds.string);
                } else {
                    Ns_DStringSetLength(&ds, 0);
                    Ns_MakePath(&ds, url, cfg->dirv[i], NULL);
                    result = Ns_ConnRedirect(conn, ds.string);
                }
                goto done;
            }
        }

        /*
         * If no index file was found, invoke a directory listing
         * ADP or Tcl proc if configured.
         */

        if (cfg->diradp != NULL) {
            result = Ns_AdpRequest(conn, cfg->diradp);
        } else if (cfg->dirproc != NULL) {
            result = Ns_TclRequest(conn, cfg->dirproc);
        } else {
            goto notfound;
        }
    } else {
    notfound:
        result = Ns_ConnReturnNotFound(conn);
    }

 done:
    Ns_DStringFree(&ds);

    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Return --
 *
 *      Return contents of file to connection.
 *
 * Results:
 *      Standard Ns_Request result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Return(Ns_Conn *conn, int status, CONST char *type, CONST char *path,
       Tcl_StatBuf *stPtr)
{
    Tcl_Channel  channel;
    int          result;

    /*
     * Set the last modified header if not set yet.
     * If not modified since last request, return now.
     */

    Ns_ConnSetLastModifiedHeader(conn, &stPtr->st_mtime);
    if (Ns_ConnModifiedSince(conn, stPtr->st_mtime) == NS_FALSE) {
        return Ns_ConnReturnNotModified(conn);
    }

    /*
     * Open, send, close the file.
     */

    channel = Tcl_OpenFileChannel(NULL, path, "r", 0644);
    if (channel == NULL) {
        Ns_Log(Warning, "nsvfs: failed to open '%s': '%s'",
               path, Tcl_ErrnoMsg(Tcl_GetErrno()));
        return Ns_ConnReturnNotFound(conn);
    }
    result = Ns_ConnReturnOpenChannel(conn, 200, type, channel, stPtr->st_size);
    Tcl_Close(NULL, channel);

    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Stat, StatObj --
 *
 *      Stat a file, logging an error on unexpected results.
 *
 * Results:
 *      NS_OK if stat ok, NS_ERROR otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Stat(CONST char *path, Tcl_StatBuf *stPtr)
{
    Tcl_Obj *pathObj;
    int      status;

    pathObj = Tcl_NewStringObj(path, -1);
    Tcl_IncrRefCount(pathObj);
    status = StatObj(pathObj, stPtr);
    Tcl_DecrRefCount(pathObj);

    return status;
}

static int
StatObj(Tcl_Obj *pathObj, Tcl_StatBuf *stPtr)
{
    int status, err;

    status = Tcl_FSStat(pathObj, stPtr);
    err = Tcl_GetErrno();

    if (status != 0) {
        if (err != ENOENT && err != EACCES) {
            Ns_Log(Error, "nsvfs: stat(%s) failed: %s",
                   Tcl_GetString(pathObj), Tcl_ErrnoMsg(err));
        }
        return NS_ERROR;
    }
    return NS_OK;
}
