/*  =========================================================================
    fty_mlm - Malamute connection helpers

    Copyright (C) 2014 - 2018 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_mlm - Malamute connection helpers
@discuss
@end
*/

#include "fty_metric_tpower_classes.h"
#include <sys/types.h>

#define MAX_UNCONNECT_DURATION_MINUTE 5 //minutes

void
MlmClientGuard::check_connection_alive_or_die()
{
    uint64_t     now          = zclock_mono();
    mlm_client_t *client      = get();
    if(mlm_client_connected(client)){
        ts_conn_OK=now;
        //check if a previous connection was lost
        if(ts_conn_KO!=0)zsys_info("reconnected to broker");
        //reset KO counter
        ts_conn_KO=0;
    }else{
        //if we not yet connected, we don't care
        if(ts_conn_OK==0)return;
        //detect beginning of disconnection
        if(ts_conn_KO==0){
            zsys_warning("malamute broker is unreachable");
            ts_conn_KO=now;
        }else
            //test if coonnection lost for 5 minutes
            if((now-ts_conn_KO)>MAX_UNCONNECT_DURATION_MINUTE*60*1000){
            // so decide to die 
            zsys_error("disconnected from broker for %d minutes, so ABORT !",
                    MAX_UNCONNECT_DURATION_MINUTE);
            kill(getpid(),SIGTERM);
        }
    }
}
