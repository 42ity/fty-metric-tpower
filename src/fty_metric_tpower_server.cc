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

/// fty_metric_tpower_server - Actor generating new metrics

#include "fty_metric_tpower_server.h"
#include "metricinfo.h"
#include "tpowerconfiguration.h"
#include "termColors.h"

#include <fty_log.h>
#include <fty_shm.h>
#include <czmq.h>
#include <malamute.h>
#include <mutex>

static std::mutex mtx_tpowerConf;

// agent's name ### DO NOT CHANGE! as other agents can rely on this name
static const char* AGENT_NAME = "agent-tpower";

// ============================================================
//         Functionality for METRIC processing and publishing
// ============================================================

// write metric in shared memory
bool write_metric(const MetricInfo& metricInfo)
{
    log_debug(TC_YELLOW "SHM write: %s (value: %s [%s], timestamp: %s, ttl: %s)" TC0,
        metricInfo.generateTopic().c_str(),
        std::to_string(metricInfo.getValue()).c_str(),
        (metricInfo.getUnits().empty() ? "<no_unit>" : metricInfo.getUnits().c_str()),
        std::to_string(metricInfo.getTimestamp()).c_str(),
        std::to_string(metricInfo.getTtl()).c_str()
    );

    int r = fty::shm::write_metric(
        metricInfo.getElementName(),
        metricInfo.getSource(),
        std::to_string(metricInfo.getValue()),
        metricInfo.getUnits(),
        int(metricInfo.getTtl())
    );
    if (r != 0) {
        log_error(TC_RED "shm::write_metric() failed (%s, r: %d)" TC0,
            metricInfo.generateTopic().c_str(), r);
        return false;
    }

    return true;
}

static void s_processMetrics(TotalPowerConfiguration& config, fty::shm::shmMetrics& metrics)
{
    for (const auto& metric : metrics) {
        const char* asset_name = fty_proto_name(metric);
        const char* type       = fty_proto_type(metric);
        const char* value_s    = fty_proto_value(metric);
        const char* unit       = fty_proto_unit(metric);
        uint64_t    timestamp  = fty_proto_time(metric);
        uint32_t    ttl        = fty_proto_ttl(metric); // time-to-live

        const std::string topic = type + std::string("@") + asset_name;

        log_trace("process metric %s (value: %s, unit: %s)", topic.c_str(), value_s, unit);

        char* end = NULL;
        double value = strtod(value_s, &end);
        if (errno == ERANGE || end == value_s || (end && (*end  != 0))) {
            log_error("cannot convert %s value '%s' to double, ignored...", topic.c_str(), value_s);
            fty_proto_print(metric);
            continue;
        }

        MetricInfo metricInfo(asset_name, type, unit, value, timestamp, ttl);
        mtx_tpowerConf.lock();
        config.processMetric(metricInfo, topic);
        mtx_tpowerConf.unlock();

        log_trace("process %s metric done", topic.c_str());
    }

    mtx_tpowerConf.lock();
    config.setPollInterval();
    mtx_tpowerConf.unlock();
}

// simple poller actor
// regularly read and process 'power' metrics coming from shm
static void fty_metric_tpower_metric_pull(zsock_t* pipe, void* args)
{
    TotalPowerConfiguration* totalpower_conf = reinterpret_cast<TotalPowerConfiguration*>(args);
    if (!totalpower_conf) {
        log_error("totalpower_conf is NULL");
        return;
    }

    zpoller_t* poller = zpoller_new(pipe, NULL);
    if (!poller) {
        log_error("poller is NULL");
        return;
    }

    log_info("fty_metric_tpower_metric_pull started");

    zsock_signal(pipe, 0);

    const std::string assetFilter(".*");

    // No current, voltage and VA for location
    const std::string typeFilter("realpower\\.(default|((output|input)\\.L(1|2|3)))"
                                    "|current\\.(output|input)\\.L(1|2|3)"
                                    "|voltage\\.(output|input)\\.L(1|2|3)-N");

    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, fty_get_polling_interval() * 1000);
        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
            if (zpoller_expired(poller)) {
                fty::shm::shmMetrics metrics;
                fty::shm::read_metrics(assetFilter.c_str(), typeFilter.c_str(), metrics);

                log_debug(TC_BLUE "Polling: read metrics (assets: %s, types: %s, size: %d)" TC0,
                    assetFilter.c_str(), typeFilter.c_str(), metrics.size());

                s_processMetrics(*totalpower_conf, metrics);
            }
        }
    }

    zpoller_destroy(&poller);

    log_info("fty_metric_tpower_metric_pull ended");
}

// main actor
void fty_metric_tpower_server(zsock_t* pipe, void* args)
{
    const char* endpoint = static_cast<const char*>(args);
    if (!endpoint) {
        log_error("endpoint is NULL");
        return;
    }

    mlm_client_t* client = mlm_client_new();
    if (!client) {
        log_error("mlm_client_new () failed");
        return;
    }

    int r = mlm_client_connect(client, endpoint, 1000, AGENT_NAME);
    if (r != 0) {
        log_error("%s: can't connect to endpoint '%s'", AGENT_NAME, endpoint);
        mlm_client_destroy(&client);
        return;
    }

    r = mlm_client_set_consumer(client, FTY_PROTO_STREAM_ASSETS, ".*");
    if (r != 0) {
        log_error("%s: can't set consumer on stream '%s', '%s'", AGENT_NAME, FTY_PROTO_STREAM_ASSETS, ".*");
        mlm_client_destroy(&client);
        return;
    }

    zpoller_t* poller = zpoller_new(pipe, mlm_client_msgpipe(client), NULL);
    if (!poller) {
        log_error("poller is NULL");
        mlm_client_destroy(&client);
        return;
    }

    // Such trick with function is used, because tpower_configuration
    // wants itself to control "advertise time".
    // But We want to separate logic from messaging -> use function as parameter
    std::function<bool(const MetricInfo&)> tpower_conf_callback = [](const MetricInfo& metricInfo) -> bool {
        return write_metric(metricInfo);
    };

    // initial set up
    TotalPowerConfiguration tpower_conf(tpower_conf_callback);
    tpower_conf.configure();

    // run 'power' metrics poller actor
    zactor_t* tpower_metrics_pull = zactor_new(fty_metric_tpower_metric_pull, &tpower_conf);
    if (!tpower_metrics_pull) {
        log_error("tpower_metrics_pull creation failed");
        zpoller_destroy(&poller);
        mlm_client_destroy(&client);
        return;
    }

    log_info("fty_metric_tpower_server started");

    zsock_signal(pipe, 0);

    uint64_t last = uint64_t(zclock_mono());

    while (!zsys_interrupted) {

        void* which = zpoller_wait(poller, int(tpower_conf.getTimeout()));

        if (!(zpoller_terminated(poller) || zsys_interrupted)) {
            uint64_t now = uint64_t(zclock_mono());
            if ((now - last) >= static_cast<uint64_t>(tpower_conf.getTimeout())) {
                last = now;
                log_debug("Periodic polling");
                mtx_tpowerConf.lock();
                tpower_conf.onPoll();
                mtx_tpowerConf.unlock();
            }
        }

        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
        }
        else if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            char* cmd = msg ? zmsg_popstr(msg) : NULL;
            log_debug("Cmd: %s", cmd);
            bool term{cmd && streq(cmd, "$TERM")};
            zstr_free(&cmd);
            zmsg_destroy(&msg);
            if (term) {
                break;
            }
        }
        else if (which == mlm_client_msgpipe(client)) {
            zmsg_t* msg = mlm_client_recv(client);
            const char* cmd = mlm_client_command(client);
            const char* subject = mlm_client_subject(client);
            const char* sender = mlm_client_sender(client);
            log_debug("Got message %s/%s from %s", cmd, subject, sender);

            if (streq(cmd, "STREAM DELIVER")) {
                if (msg && fty_proto_is(msg)) {
                    fty_proto_t* proto = fty_proto_decode(&msg);
                    if (proto) {
                        if (fty_proto_id(proto) == FTY_PROTO_ASSET) {
                            mtx_tpowerConf.lock();
                            tpower_conf.processAsset(proto);
                            mtx_tpowerConf.unlock();
                        }
                        else {
                            log_debug("it is not an asset message, ignore it");
                        }
                    }
                    else {
                        log_debug("cannot decode fty_proto message");
                    }
                    fty_proto_destroy(&proto);
                }
                else {
                    log_debug("it is not a proto message");
                }
            }
            else if (streq(cmd, "MAILBOX DELIVER")) {
                log_debug("Unexpected mailbox message");
                //if (msg) { zmsg_print(msg); }
            }

            zmsg_destroy(&msg);
        }
    }

    zactor_destroy(&tpower_metrics_pull);
    zpoller_destroy(&poller);
    mlm_client_destroy(&client);

    log_info("fty_metric_tpower_server ended");
}
