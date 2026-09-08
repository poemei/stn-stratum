#include <stdio.h>
#include "stn_stratum.h"

int main(void)
{
    stn_stratum_session session;

    stn_stratum_session_init(&session, 1u);

    if (session.session_id != 1u) {
        fprintf(stderr, "session init failed\n");
        return 1;
    }

    if (stn_stratum_subscribe(
            &session,
            STN_STRATUM_PROTOCOL_VERSION) != STN_STRATUM_OK) {
        fprintf(stderr, "subscribe failed\n");
        return 1;
    }

    puts("stn-stratum tests passed");
    return 0;
}