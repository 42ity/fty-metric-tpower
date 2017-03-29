/*  =========================================================================
    fty_metric_tpower_server - Actor generating new metrics

    Copyright (C) 2014 - 2017 Eaton

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
    fty_metric_tpower_server - Actor generating new metrics
@discuss
@end
*/
int agent_tpower_verbose = 0;

#define zsys_debug1(...) \
    do { if (agent_tpower_verbose) zsys_debug (__VA_ARGS__); } while (0);

#include "fty_metric_tpower_classes.h"
#include <string>
// ============================================================
//         Functionality for METRIC processing and publishing
// ============================================================
bool send_metrics (mlm_client_t* client, const MetricInfo &M){
    zsys_debug1 ("Metric is sent: topic = %s , time = %s", M.generateTopic().c_str(), std::to_string(M.getTimestamp()).c_str());
    zmsg_t *msg = fty_proto_encode_metric (
            NULL,
            ::time (NULL),
            M.getTtl (),
            M.getSource().c_str(),
            M.getElementName().c_str(),
            std::to_string(M.getValue()).c_str(),
            M.getUnits().c_str());
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
        fty_proto_t **bmessage_p)
{
    fty_proto_t *bmessage = *bmessage_p;

    const char *value = fty_proto_value(bmessage);
    char *end;
    double dvalue = strtod (value, &end);

    if (errno == ERANGE || end == value || *end != '\0') {

        if (errno == ERANGE)
            errno = 0;

        zsys_info ("cannot convert value '%s' to double, ignore message\n", value);
        fty_proto_print (bmessage);
        return;
    }

    const char *type = fty_proto_type(bmessage);
    const char *element_src = fty_proto_name (bmessage);
    const char *unit = fty_proto_unit(bmessage);
    uint32_t ttl = fty_proto_ttl(bmessage);
    uint64_t timestamp = fty_proto_time (bmessage);

    zsys_debug1("Got message '%s' with value %s\n", topic.c_str(), value);

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
        fty_proto_t **bmessage_p)
{
    //fty_proto_t *bmessage = *bmessage_p;
    config.processAsset (topic);
    zsys_info ("ASSET PROCESSED");
}

void
fty_metric_tpower_server (zsock_t *pipe, void* args)
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
                zsys_debug1 ("actor command=%s", cmd);
            }

            if (streq (cmd, "$TERM")) {
                zsys_info ("Got $TERM");
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                goto exit;
            }
            else
            if (streq (cmd, "VERBOSE")) {
                verbose = true;
                agent_tpower_verbose = true;
                zsys_debug1 ("VERBOSE received");
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
            zsys_debug1("Got message '%s'", topic.c_str());
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


        if (is_fty_proto (zmessage)) {
            fty_proto_t *bmessage = fty_proto_decode (&zmessage);
            if (!bmessage) {
                zsys_error ("cannot decode fty_proto message, ignore it");
                continue;
            }
            if (fty_proto_id (bmessage) == FTY_PROTO_METRIC)  {
                s_processMetric (tpower_conf, topic, &bmessage);
            }
            else if (fty_proto_id (bmessage) == FTY_PROTO_ASSET)  {
                s_processAsset (tpower_conf, topic, &bmessage);
            }
            else {
                zsys_error ("it is not an alert message, ignore it");
            }
            fty_proto_destroy (&bmessage);
        }
        else {
            zsys_error ("not fty proto");
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
fty_metric_tpower_server_test (bool verbose)
{
    printf (" * fty_metric_tpower_server: ");

    //  @selftest
    static const char* endpoint = "inproc://bios-tpower-server-test";

    zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (server, "BIND", endpoint, NULL);
    if (verbose)
        zstr_send (server, "VERBOSE");

    mlm_client_t *producer = mlm_client_new ();
    mlm_client_connect (producer, endpoint, 1000, "producer");
    mlm_client_set_producer (producer, "METRICS");

    mlm_client_t *consumer = mlm_client_new ();
    mlm_client_connect (consumer, endpoint, 1000, "consumer");
    mlm_client_set_consumer (consumer, "METRICS", ".*");

    uint64_t timestamp = ::time(NULL);
    MetricInfo M("someUPS", "realpower.default", "W", 456.66, timestamp, "", 500);
    send_metrics (producer, M);

    zmsg_t *msg = mlm_client_recv (consumer);
    assert ( msg != NULL);
    assert ( M.generateTopic() == std::string(mlm_client_subject(consumer)));
    assert ( is_fty_proto (msg));
    fty_proto_t *bmessage = fty_proto_decode (&msg);
    fty_proto_print(bmessage);
    assert ( bmessage != NULL );
    assert ( fty_proto_id (bmessage) == FTY_PROTO_METRIC );
    uint64_t timestamp_new = fty_proto_time (bmessage);
    assert ( timestamp == timestamp_new);

    fty_proto_destroy (&bmessage);
    zmsg_destroy (&msg);
    mlm_client_destroy (&consumer);
    mlm_client_destroy (&producer);
    zactor_destroy(&server);
    printf ("OK\n");
}
