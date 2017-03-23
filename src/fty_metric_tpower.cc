/*  =========================================================================
    fty_metric_tpower - Evaluates some metrics and produces new power metrics

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
    fty_metric_tpower - Evaluates some metrics and produces new power metrics
@discuss
@end
*/

#include "fty_metric_tpower_classes.h"

#include <getopt.h>

// agent's name
// DO NOT CHANGE! as other agents can rely on this name
static const char *AGENT_NAME = "agent-tpower";

// malamute's endpoint
// TODO make endpoint configurable on the start of the agent
static const char *ENDPOINT = "ipc://@/malamute";


void usage ()
{
    puts ("fty-metric-tpower [options]\n"
          "  -v|--verbose          verbose test output\n"
          "  -h|--help             print this information\n"
          "Environment variables for parameters are FTY_LOG_LEVEL.\n"
          "Command line option takes precedence over variable.");
}

int main (int argc, char *argv [])
{
    int verbose = 0;
    int help = 0;

    // set defaults
    char* fty_log_level = getenv ("FTY_LOG_LEVEL");
    if ( fty_log_level && streq (fty_log_level, "LOG_DEBUG") ) {
        verbose = 1;
    }

    // get options
    int c;
// Some systems define struct option with non-"const" "char *"
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif
    static const char *short_options = "hv";
    static struct option long_options[] =
    {
        {"help",       no_argument,       &help,    1},
        {"verbose",    no_argument,       &verbose, 1},
        {NULL, 0, 0, 0}
    };
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

    while(true) {
        int option_index = 0;
        c = getopt_long (argc, argv, short_options, long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 0:
                // just now walking trough some long opt
                break;
            case 'h':
            default:
                help = 1;
                break;
        }
    }
    if ( help ) {
        usage();
        exit(1);
    }

    zsys_info ("fty_metric_tpower STARTED");

    zactor_t *tpower_server = zactor_new (fty_metric_tpower_server, (void*) AGENT_NAME);
    if ( !tpower_server ) {
        zsys_error ("cannot start the daemon");
        exit(1);
    }

    if (verbose) {
        zstr_sendx (tpower_server, "VERBOSE", NULL);
    }
    // Connect to malamute
    zstr_sendx (tpower_server, "CONNECT", ENDPOINT, NULL);
    zsock_wait (tpower_server);
    // Producing new metrics
    zstr_sendx (tpower_server, "PRODUCER", "METRICS", NULL);
    zsock_wait (tpower_server);
    // Consuming some metrics
    zstr_sendx (tpower_server, "CONSUMER", "METRICS", ".*", NULL);
    zsock_wait (tpower_server);
    // Consuming some metrics
    zstr_sendx (tpower_server, "CONSUMER", "ASSETS", ".*", NULL);
    zsock_wait (tpower_server);
    //  Accept and print any message back from server
    //  copy from src/malamute.c under MPL license
    while (true) {
        char *message = zstr_recv (tpower_server);
        if (message) {
            puts (message);
            free (message);
        }
        else {
            puts ("interrupted");
            break;
        }
    }

    zactor_destroy (&tpower_server);
    zsys_info ("fty_metric_tpower ENDED");
    return 0;
}
