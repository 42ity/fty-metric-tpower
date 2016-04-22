/*  =========================================================================
    bios_agent_tpower_server - Actor generating new metrics

    Copyright (C) 2014 - 2015 Eaton                                        
                                                                           
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
    bios_agent_tpower_server - Actor generating new metrics
@discuss
@end
*/

#include "agent_tpower_classes.h"

// ============================================================
//         Functionality for METRIC processing and publishing
// ============================================================
bool send_metrics (mlm_client_t* client, const MetricInfo &M){
    zsys_info ("Metric is sent: %s", M.generateTopic().c_str());
    zmsg_t *msg = bios_proto_encode_metric (
            NULL,
            M.getSource().c_str(),
            M.getElementName().c_str(),
            std::to_string(M.getValue()).c_str(),
            M.getUnits().c_str(),
            M.getTimestamp());
    int r = mlm_client_send (client, M.generateTopic().c_str(), &msg);
    if ( r == -1 ) {
        return false;
    }
    else {
        return true;
    }
}

static void
    s_processMetric(
        TotalPowerConfiguration &config,
        const std::string &topic,
        bios_proto_t **bmessage_p)
{
    bios_proto_t *bmessage = *bmessage_p;

    const char *value = bios_proto_value(bmessage);
    char *end;
    double dvalue = strtod (value, &end);
    if (errno == ERANGE) {
        errno = 0;
        zsys_debug ("cannot convert value to double, ignore message\n");
        return;
    }
    else if (end == value || *end != '\0') {
        zsys_debug ("cannot convert value to double, ignore message\n");
        return;
    }

    const char *type = bios_proto_type(bmessage);
    const char *element_src = bios_proto_element_src(bmessage);
    const char *unit = bios_proto_unit(bmessage);
    // in the protocol in the field time now "ttl" is sent
    uint64_t ttl = bios_proto_time(bmessage);
    // time is a time, when message is delivered
    uint64_t timestamp = ::time(NULL);

    zsys_debug("Got message '%s' with value %s\n", topic.c_str(), value);

    MetricInfo m (element_src, type, unit, dvalue, timestamp, "", ttl);
    config.processMetric (m, topic);
}

// ============================================================
//         Functionality for ASSET processing
// ============================================================
static void
    s_processAsset(
        TotalPowerConfiguration &config,
        const std::string &topic,
        bios_proto_t **bmessage_p)
{
    //bios_proto_t *bmessage = *bmessage_p;
    config.processAsset (topic);
    zsys_info ("ASSET PROCESSED");
}

void
bios_agent_tpower_server (zsock_t *pipe, void* args)
{
    bool verbose = false;

    char *name = strdup ((char*) args);

    mlm_client_t *client = mlm_client_new ();

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe(client), NULL);

    // Signal need to be send as it is required by "actor_new"
    zsock_signal (pipe, 0);

    // Such trick with function is used, because tpower_configuration
    // wants itself to control "advertise time".
    // But We want to separate logic from messaging -> use function as parameter
    std::function<bool(const MetricInfo&)> fff= [&client] (const MetricInfo& M) -> bool {
        return send_metrics (client, M);
    };
    // initial set up
    TotalPowerConfiguration tpower_conf(fff);
    tpower_conf.configure();
    while (!zsys_interrupted) {

        zsys_info ("timeout = %d", tpower_conf.getTimeout());
        void *which = zpoller_wait (poller, tpower_conf.getTimeout());
        if ( zpoller_expired (poller) ) {
            tpower_conf.onPoll();
            continue;
        }
        if ( zpoller_terminated (poller) ) {
            zsys_info ("poller was terminated");
            break;
        }

        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);
            if ( verbose ) {
                zsys_debug ("actor command=%s", cmd);
            }

            if (streq (cmd, "$TERM")) {
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                goto exit;
            }
            else
            if (streq (cmd, "VERBOSE")) {
                verbose = true;
            }
            else
            if (streq (cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr (msg);
                int rv = mlm_client_connect (client, endpoint, 1000, name);
                if (rv == -1) {
                    zsys_error ("%s: can't connect to malamute endpoint '%s'", name, endpoint);
                }
                zstr_free (&endpoint);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "PRODUCER")) {
                char* stream = zmsg_popstr (msg);
                int rv = mlm_client_set_producer (client, stream);
                if (rv == -1) {
                    zsys_error ("%s: can't set producer on stream '%s'", name, stream);
                }
                zstr_free (&stream);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "CONSUMER")) {
                char* stream = zmsg_popstr (msg);
                char* pattern = zmsg_popstr (msg);
                int rv = mlm_client_set_consumer (client, stream, pattern);
                if (rv == -1) {
                    zsys_error ("%s: can't set consumer on stream '%s', '%s'", name, stream, pattern);
                }
                zstr_free (&pattern);
                zstr_free (&stream);
                zsock_signal (pipe, 0);
            }
            else
            {
                zsys_info ("unhandled command %s", cmd);
            }
            zstr_free (&cmd);
            zmsg_destroy (&msg);
            continue;
        }

        // This agent is a reactive agent, it reacts only on messages
        // and doesn't do anything if there is no messages
        zmsg_t *zmessage = mlm_client_recv (client);
        if ( zmessage == NULL ) {
            continue;
        }
        std::string topic = mlm_client_subject(client);
        if ( verbose ) {
            zsys_debug("Got message '%s'", topic.c_str());
        }
        // What is going on???
        //
        // Listen on metrics +
        // Listen on assets
        //
        // Produce metrics +
        //
        // Current iplementation: read topology from DB
        // TODO: move it to asset agent and receive this info
        // as message


        if (is_bios_proto (zmessage)) {
            bios_proto_t *bmessage = bios_proto_decode (&zmessage);
            if (!bmessage) {
                zsys_error ("cannot decode bios_proto message, ignore it");
                continue;
            }
            if (bios_proto_id (bmessage) == BIOS_PROTO_METRIC)  {
                s_processMetric (tpower_conf, topic, &bmessage);
            }
            else if (bios_proto_id (bmessage) == BIOS_PROTO_ASSET)  {
                s_processAsset (tpower_conf, topic, &bmessage);
            }
            else {
                zsys_error ("it is not an alert message, ignore it");
            }
            bios_proto_destroy (&bmessage);
        }
        else {
            zsys_error ("not bios proto");
        }

        // listen 
        zmsg_destroy (&zmessage);
    }
exit:
    //TODO:  save info to persistence before I die
    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
    zstr_free (&name);
}



//  --------------------------------------------------------------------------
//  Self test of this class

void
bios_agent_tpower_server_test (bool verbose)
{
    printf (" * bios_agent_tpower_server: ");
    printf ("OK\n");
}
