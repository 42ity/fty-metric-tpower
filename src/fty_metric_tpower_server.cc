/*  =========================================================================
    fty_metric_tpower_server - Actor generating new metrics

    Copyright (C) 2014 - 2020 Eaton

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

#include "fty_metric_tpower_classes.h"
#include <string>
#include <mutex>
#include <fty_common_mlm_guards.h>
#include <fty_shm.h>

#define ANSI_COLOR_REDTHIN "\x1b[0;31m"
#define ANSI_COLOR_WHITE_ON_BLUE  "\x1b[44;97m"
#define ANSI_COLOR_BOLD  "\x1b[1;39m"
#define ANSI_COLOR_RED     "\x1b[1;31m"
#define ANSI_COLOR_GREEN   "\x1b[1;32m"
#define ANSI_COLOR_YELLOW  "\x1b[1;33m"
#define ANSI_COLOR_BLUE    "\x1b[1;34m"
#define ANSI_COLOR_MAGENTA "\x1b[1;35m"
#define ANSI_COLOR_CYAN    "\x1b[1;36m"
#define ANSI_COLOR_LIGHTMAGENTA    "\x1b[1;95m"
#define ANSI_COLOR_RESET   "\x1b[0m"

std::mutex mtx_tpowerConf;

// agent's name ### DO NOT CHANGE! as other agents can rely on this name
static const char *AGENT_NAME = "agent-tpower";

// ============================================================
//         Functionality for METRIC processing and publishing
// ============================================================

static bool send_metrics (const MetricInfo &M)
{
    log_debug (ANSI_COLOR_YELLOW "SHM metric sent: %s (value: %s [%s], timestamp: %s, ttl: %s)" ANSI_COLOR_RESET,
        M.generateTopic().c_str(),
        std::to_string(M.getValue()).c_str(),
        (M.getUnits().empty() ? "<no_unit>" : M.getUnits().c_str()),
        std::to_string(M.getTimestamp()).c_str(),
        std::to_string(M.getTtl()).c_str()
    );

    int r = fty::shm::write_metric(M.getElementName(), M.getSource(), std::to_string(M.getValue()), M.getUnits(), M.getTtl());
    if ( r == -1 ) {
        log_error(ANSI_COLOR_RED "shm::write_metric() failed (%s, r: %d)" ANSI_COLOR_RESET,
            M.generateTopic().c_str(), r);
        return false;
    }

    return true;
}

static void s_processMetrics (TotalPowerConfiguration &config, fty::shm::shmMetrics& metrics)
{
    for (auto &metric : metrics) {
        const char *asset_name = fty_proto_name (metric);
        const char *type = fty_proto_type(metric);
        const char *value_s = fty_proto_value(metric);
        const char *unit = fty_proto_unit(metric);
        uint64_t timestamp = fty_proto_time (metric);
        uint32_t ttl = fty_proto_ttl(metric); //time-to-live

        std::string topic = type + std::string("@") + asset_name;

        log_trace("process metric %s (value: %s, unit: %s)", topic.c_str(), value_s, unit);

        char *end = NULL;
        errno = 0;
        double value = strtod (value_s, &end);
        if (errno == ERANGE || end == value_s || *end != '\0') {
            log_error("cannot convert %s value '%s' to double, ignored...", topic.c_str(), value_s);
            fty_proto_print (metric);
            continue;
        }

        MetricInfo m (asset_name, type, unit, value, timestamp, ttl);
        mtx_tpowerConf.lock();
        config.processMetric (m, topic);
        mtx_tpowerConf.unlock();

        log_trace("process %s metric done", topic.c_str());
    }

    mtx_tpowerConf.lock();
    config.setPollInterval();
    mtx_tpowerConf.unlock();
}

// simple poller actor
// regularly read and process 'power' metrics coming from shm
static void fty_metric_tpower_metric_pull (zsock_t *pipe, void* args)
{
    assert(pipe);
    assert(args);

    zpoller_t *poller = zpoller_new (pipe, NULL);
    zsock_signal (pipe, 0);

    TotalPowerConfiguration *totalpower_conf = (TotalPowerConfiguration*) args;
    uint64_t timeout = fty_get_polling_interval() * 1000;

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, timeout);
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                break;
            }
            if (zpoller_expired (poller)) {
                const std::string assetFilter(".*");
                const std::string typeFilter("realpower\\.(default|((output|input)\\.L(1|2|3)))");
                fty::shm::shmMetrics result;
                fty::shm::read_metrics(assetFilter.c_str(), typeFilter.c_str(), result);

                log_debug(ANSI_COLOR_BLUE "Polling: read metrics (assets: %s, types: %s, size: %d)" ANSI_COLOR_RESET,
                    assetFilter.c_str(), typeFilter.c_str(), result.size());

                s_processMetrics( *totalpower_conf, result );
            }
        }
        timeout = fty_get_polling_interval() * 1000;
    }
}

// main actor
void fty_metric_tpower_server (zsock_t *pipe, void* args)
{
    assert(pipe);
    assert(args);

    const char *endpoint = static_cast<const char *>(args);

    // Setup the watchdog
    Watchdog watchdog;
    watchdog.start();

    // Signal need to be send as it is required by "actor_new"
    zsock_signal (pipe, 0);

    MlmClientGuard client(mlm_client_new());
    if (!client) {
        log_error("mlm_client_new () failed");
        return;
    }

    if (mlm_client_connect(client, endpoint, 1000, AGENT_NAME) < 0) {
        log_error("%s: can't connect to malamute endpoint '%s'",
                AGENT_NAME, endpoint);
        zstr_send(pipe, "$TERM");
        return;
    }

    if (mlm_client_set_consumer(client, FTY_PROTO_STREAM_ASSETS, ".*") < 0) {
        log_error("%s: can't set consumer on stream '%s', '%s'",
                AGENT_NAME, FTY_PROTO_STREAM_ASSETS, ".*");
        zstr_send(pipe, "$TERM");
        return;
    }

    ZpollerGuard poller(zpoller_new (pipe, mlm_client_msgpipe (client), NULL));

    // Such trick with function is used, because tpower_configuration
    // wants itself to control "advertise time".
    // But We want to separate logic from messaging -> use function as parameter
    std::function<bool(const MetricInfo&)> tpower_conf_callback= [] (const MetricInfo& M) -> bool {
        return send_metrics (M);
    };

    // initial set up
    TotalPowerConfiguration tpower_conf(tpower_conf_callback);
    tpower_conf.configure();

    // run 'power' metrics poller actor
    zactor_t *tpower_metrics_pull = zactor_new (fty_metric_tpower_metric_pull, (void*) &tpower_conf);

    uint64_t last = zclock_mono ();
    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, tpower_conf.getTimeout());

        uint64_t now = zclock_mono();
        if ((now - last) >= static_cast<uint64_t>(tpower_conf.getTimeout())) {
            last = now;
            log_debug("Periodic polling");
            mtx_tpowerConf.lock();
            tpower_conf.onPoll();
            mtx_tpowerConf.unlock();
        }

        if ( zpoller_expired (poller) ) {
            continue;
        }
        if ( zpoller_terminated (poller) ) {
            log_info ("poller was terminated");
            break;
        }

        if (which == pipe) {
            ZmsgGuard msg(zmsg_recv (pipe));
            ZstrGuard cmd(zmsg_popstr(msg));
            log_trace ("Got command '%s'", cmd.get());

            if (streq (cmd, "$TERM")) {
                log_info ("Terminate...");
                break;
            }
            else
            {
                log_info ("unhandled command %s", cmd.get());
            }
            continue;
        }

        // This agent is a reactive agent, it reacts only on messages
        // and doesn't do anything if there is no messages
        zmsg_t *zmessage = mlm_client_recv (client);
        if ( zmessage == NULL ) {
            continue;
        }

        std::string topic = mlm_client_subject(client);
        log_trace("Got message '%s'", topic.c_str());

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
                log_error ("cannot decode fty_proto message, ignore it");
                zmsg_destroy (&zmessage);
                continue;
            }
            // As long as we are receiving metrics from malamute, everything
            // is fine
            watchdog.tick();
            if (fty_proto_id (bmessage) == FTY_PROTO_ASSET)  {
                mtx_tpowerConf.lock();
                tpower_conf.processAsset(bmessage);
                mtx_tpowerConf.unlock();
            }
            else {
                log_error ("it is not an asset message, ignore it");
            }
            fty_proto_destroy (&bmessage);
        }
        else {
            log_error ("not a fty_proto message");
        }

        zmsg_destroy (&zmessage);
    }

    //TODO:  save info to persistence before I die
    zactor_destroy (&tpower_metrics_pull);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_metric_tpower_server_test (bool verbose)
{
    printf (" * fty_metric_tpower_server: ");

    ManageFtyLog::setInstanceFtylog("fty_metric_tpower_server");
    //  @selftest
    static const char* endpoint = "inproc://bios-tpower-server-test";

    zactor_t *server = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (server, "BIND", endpoint, NULL);
    if (verbose)
         ManageFtyLog::getInstanceFtylog()->setVeboseMode();

    assert (fty_shm_set_test_dir("selftest-rw") == 0);

    uint64_t timestamp = ::time(NULL);
    uint32_t ttl = 500;
    MetricInfo M("someUPS", "realpower.default", "W", 456.66, timestamp, ttl);
    assert (send_metrics (M));

    fty_proto_t *bmessage;
    assert(fty::shm::read_metric("someUPS", "realpower.default", &bmessage)==0);
    assert ( bmessage != NULL );
    fty_proto_print(bmessage);
    assert ( fty_proto_id (bmessage) == FTY_PROTO_METRIC );

    fty_proto_destroy (&bmessage);
    zactor_destroy(&server);
    fty_shm_delete_test_dir();
    printf ("OK\n");
}
