/*
 * sigUnixStubs.h --
 *
 *     Declarations for sig module Unix compatibility stubs.
 *
 * Copyright (C) 1991 Regents of the University of California
 * All rights reserved.
 *
 *
 * $Header: /sprite/src/kernel/sig/RCS/sig.h,v 9.6 90/10/05 16:15:58 mendel Exp $ SPRITE (Berkeley)
 */

#ifndef _SIG_UNIX_STUBS
#define _SIG_UNIX_STUBS

#include <sprite.h>
#include <user/sys/signal.h>

extern int Sig_KillStub _ARGS_((int pid, int sig));
extern int Sig_KillpgStub _ARGS_((int pgrp, int sig));
extern int Sig_SigvecStub _ARGS_((int sig, struct sigvec *newVectorPtr, struct sigvec *oldVectorPtr));
extern int Sig_SigblockStub _ARGS_((int mask));
extern int Sig_SigsetmaskStub _ARGS_((int mask));
extern int Sig_SigpauseStub _ARGS_((int mask));
extern int Sig_SigstackStub _ARGS_((struct sigstack *ss, struct sigstack *oss));

#endif /* _SIG_UNIX_STUBS */
