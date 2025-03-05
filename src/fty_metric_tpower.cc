/*  =========================================================================
    fty_metric_tpower - Evaluates some metrics and produces new power metrics

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

/// fty_metric_tpower - Evaluates some metrics and produces new power metrics

#include "fty_metric_tpower_server.h"
#include <fty_common_agents.h>
#include <fty_common_mlm_guards.h>
#include <fty_common_mlm_utils.h>
#include <fty_log.h>

static void usage()
{
    puts(
        "fty-metric-tpower [options]\n"
        "  -c|--config   load a configuration file\n"
        "  -v|--verbose  verbose output\n"
        "  -h|--help     print this information\n"
    );
}

int main(int argc, char* argv[])
{
    const char* config_file = NULL;
    int verbose = 0;

    for (int argn = 1; argn < argc; argn++) {
        const char* arg   = argv[argn];
        const char* param = ((argn + 1) < argc) ? argv[argn + 1] : NULL;

        if (streq(arg, "-h") || streq(arg, "--help")) {
            usage();
            return EXIT_SUCCESS;
        }
        else if (streq(arg, "-v") || streq(arg, "--verbose")) {
            verbose = 1;
        }
        else if (streq(arg, "-c") || streq(arg, "--config")) {
            if (!param) {
                fprintf(stderr, "%s: missing argument\n", arg);
                return EXIT_FAILURE;
            }
            config_file = param;
            ++argn;
        }
        else {
            fprintf(stderr, "Unknown argument (%s)\n", arg);
            return EXIT_FAILURE;
        }
    }

    ManageFtyLog::setInstanceFtylog(AGENT_FTY_METRIC_TPOWER, FTY_COMMON_LOGGING_DEFAULT_CFG);
    if (verbose) {
        ManageFtyLog::getInstanceFtylog()->setVerboseMode();
    }

    // Parse config file
    if (config_file) {
        log_warning("configuration from file is not supported (%s). Ignored...", config_file);
    }

    zactor_t* tpower_server = zactor_new(fty_metric_tpower_server, const_cast<char*>(MLM_ENDPOINT));
    if (!tpower_server) {
        log_error("tpower_server creation failed");
        return EXIT_FAILURE;
    }

    log_info("fty_metric_tpower started");

    // main loop, accept any message back from server
    // copy from src/malamute.c under MPL license
    while (!zsys_interrupted) {
        char* msg = zstr_recv(tpower_server);
        if (!msg) {
            break;
        }

        log_debug("%s recv msg '%s'", "fty_metric_tpower", msg);
        zstr_free(&msg);
    }

    zactor_destroy(&tpower_server);

    log_info("fty_metric_tpower ended");

    return 0;
}
