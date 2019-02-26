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
// agent's name
// DO NOT CHANGE! as other agents can rely on this name
static const char *AGENT_NAME = "agent-tpower";

#include "fty_metric_tpower_classes.h"
#include <string>
#include <fty_common_mlm_guards.h>
#include <mutex>
#include <fty_shm.h>

std::mutex mtx_tpowerConf;
// ============================================================
//         Functionality for METRIC processing and publishing
// ============================================================
bool send_metrics (mlm_client_t* client, const MetricInfo &M){
    log_trace ("Metric is sent: topic = %s, time = %s, value = %s",
        M.generateTopic().c_str(), std::to_string(M.getTimestamp()).c_str(),
        std::to_string(M.getValue()).c_str());
    
    int r = fty::shm::write_metric(M.getElementName(), M.getSource(), std::to_string(M.getValue()), M.getUnits(), M.getTtl());
    if ( r == -1 ) {
        return false;
    }
    
    zmsg_t *msg = fty_proto_encode_metric (
            NULL,
            ::time (NULL),
            M.getTtl (),
            M.getSource().c_str(),
            M.getElementName().c_str(),
            std::to_string(M.getValue()).c_str(),
            M.getUnits().c_str());
    r = mlm_client_send (client, M.generateTopic().c_str(), &msg);
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
    errno = 0;
    double dvalue = strtod (value, &end);

    if (errno == ERANGE || end == value || *end != '\0') {

        if (errno == ERANGE)
            errno = 0;

        log_info ("cannot convert value '%s' to double, ignore message\n", value);
        fty_proto_print (bmessage);
        return;
    }

    const char *type = fty_proto_type(bmessage);
    const char *element_src = fty_proto_name (bmessage);
    const char *unit = fty_proto_unit(bmessage);
    uint32_t ttl = fty_proto_ttl(bmessage);
    uint64_t timestamp = fty_proto_time (bmessage);

    log_trace("Got message '%s' with value %s\n", topic.c_str(), value);

    MetricInfo m (element_src, type, unit, dvalue, timestamp, "", ttl);
    config.processMetric (m, topic);
}

static void
    s_processMetrics(
        TotalPowerConfiguration &config,
        fty::shm::shmMetrics& metrics)
{
    for (auto &element : metrics) {
      const char *value = fty_proto_value(element);
      char *end;
      errno = 0;
      double dvalue = strtod (value, &end);

      if (errno == ERANGE || end == value || *end != '\0') {
          log_info ("cannot convert value '%s' to double, ignore message\n", value);
          fty_proto_print (element);
          continue;
      }

      const char *type = fty_proto_type(element);
      const char *element_src = fty_proto_name (element);
      const char *unit = fty_proto_unit(element);
      uint32_t ttl = fty_proto_ttl(element);
      uint64_t timestamp = fty_proto_time (element);
      std::string topic(type);
      topic.append("@");
      topic.append(element_src);

      log_trace("Got message '%s' with value %s\n", topic.c_str(), value);

      MetricInfo m (element_src, type, unit, dvalue, timestamp, "", ttl);
      mtx_tpowerConf.lock();
      config.processMetric (m, topic);
      mtx_tpowerConf.unlock();
    }
    mtx_tpowerConf.lock();
    config.setPollInterval();
    mtx_tpowerConf.unlock();
}

void
fty_metric_tpower_metric_pull (zsock_t *pipe, void* args)
{
   zpoller_t *poller = zpoller_new (pipe, NULL);
  zsock_signal (pipe, 0);

  TotalPowerConfiguration *tpower_conf = (TotalPowerConfiguration*) args;
  uint64_t timeout = fty_get_polling_interval() * 1000;
  //int64_t timeCash = zclock_mono();
  while (!zsys_interrupted) {
      void *which = zpoller_wait (poller, timeout);
      if (which == NULL) {
        if (zpoller_terminated (poller) || zsys_interrupted) {
            break;
        }
        if (zpoller_expired (poller)) {
          fty::shm::shmMetrics result;
          log_debug("read metrics !");
          fty::shm::read_metrics(".*", "realpower\\.(default|((output|input)\\.L(1|2|3)))",  result);
          log_debug("metric reads : %d", result.size());
          s_processMetrics((*tpower_conf), result);
        }
      }
      timeout = fty_get_polling_interval() * 1000;
  }
}

void
fty_metric_tpower_server (zsock_t *pipe, void* args)
{
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
    if (mlm_client_set_producer(client, FTY_PROTO_STREAM_METRICS) < 0) {
        log_error("%s: can't set producer on stream '%s'",
                AGENT_NAME, FTY_PROTO_STREAM_METRICS);
        zstr_send(pipe, "$TERM");
        return;
    }
//    if (mlm_client_set_consumer(client, FTY_PROTO_STREAM_METRICS, "^realpower.*") < 0) {
//        log_error("%s: can't set consumer on stream '%s', '%s'",
//                AGENT_NAME, FTY_PROTO_STREAM_METRICS, "^realpower.*");
//        zstr_send(pipe, "$TERM");
//        return;
//    }
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
    std::function<bool(const MetricInfo&)> fff= [&client] (const MetricInfo& M) -> bool {
        return send_metrics (client, M);
    };
    // initial set up
    TotalPowerConfiguration tpower_conf(fff);
    tpower_conf.configure();
    zactor_t *tpower_metrics_pull = zactor_new (fty_metric_tpower_metric_pull, (void*) &tpower_conf);
    uint64_t last = zclock_mono ();
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, tpower_conf.getTimeout());
        uint64_t now = zclock_mono();
        if (now - last >= static_cast<uint64_t>(tpower_conf.getTimeout())) {
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
            log_trace ("actor command=%s", cmd.get());

            if (streq (cmd, "$TERM")) {
                log_info ("Got $TERM");
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
                continue;
            }
            // As long as we are receiving metrics from malamute, everything
            // is fine
            watchdog.tick();
            if (fty_proto_id (bmessage) == FTY_PROTO_METRIC)  {
                s_processMetric (tpower_conf, topic, &bmessage);
            }
            else if (fty_proto_id (bmessage) == FTY_PROTO_ASSET)  {
                mtx_tpowerConf.lock();
                tpower_conf.processAsset(bmessage);
                mtx_tpowerConf.unlock();
            }
            else {
                log_error ("it is not an alert message, ignore it");
            }
            fty_proto_destroy (&bmessage);
        }
        else {
            log_error ("not fty proto");
        }

        // listen
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
    
    fty_shm_set_test_dir("src/selftest-rw");

    mlm_client_t *producer = mlm_client_new ();
    mlm_client_connect (producer, endpoint, 1000, "producer");
    mlm_client_set_producer (producer, "METRICS");

    mlm_client_t *consumer = mlm_client_new ();
    mlm_client_connect (consumer, endpoint, 1000, "consumer");
    mlm_client_set_consumer (consumer, "METRICS", ".*");

    uint64_t timestamp = ::time(NULL);
    MetricInfo M("someUPS", "realpower.default", "W", 456.66, timestamp, "", 500);
    assert (send_metrics (producer, M));

    zmsg_t *msg = mlm_client_recv (consumer);
    assert ( msg != NULL);
    assert ( M.generateTopic() == std::string(mlm_client_subject(consumer)));
    assert ( is_fty_proto (msg));
    fty_proto_t *bmessage = fty_proto_decode (&msg);
    fty_proto_print(bmessage);
    assert ( bmessage != NULL );
    assert ( fty_proto_id (bmessage) == FTY_PROTO_METRIC );

    fty_proto_destroy (&bmessage);
    zmsg_destroy (&msg);
    mlm_client_destroy (&consumer);
    mlm_client_destroy (&producer);
    zactor_destroy(&server);
    fty_shm_delete_test_dir();
    printf ("OK\n");
}
