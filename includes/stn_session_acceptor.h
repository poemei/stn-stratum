/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_SESSION_ACCEPTOR_H
#define STN_SESSION_ACCEPTOR_H

#include "stn_client_session.h"
#include "stn_mining_listener.h"

/*
 * Accept one mining connection and bind its immutable service mode from the
 * listener that accepted it.
 *
 * Returns:
 *   1  session accepted and returned in *session_out
 *   0  no connection waiting
 *  -1  listener/accept/allocation failure
 */
int stn_session_acceptor_accept(
    const stn_mining_listener *listener,
    stn_client_session **session_out);

void stn_session_acceptor_release(stn_client_session *session);

#endif
