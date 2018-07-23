/*  =========================================================================
    watchdog - Watchdog

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
    watchdog - Watchdog
@discuss
@end
*/

#include "watchdog.h"
#include <fty_log.h>

#define WATCHDOG_INTERVAL 60
#define WATCHDOG_LIMIT 600

Watchdog::Watchdog()
    : thread_(0)
{
}

Watchdog::~Watchdog()
{
    zactor_destroy(&thread_);
}

static void watchdog_thread(zsock_t *pipe, void* args)
{
    Watchdog *wd = static_cast<Watchdog*>(args);

    zsock_signal(pipe, 0);
    zpoller_t *poller = zpoller_new(pipe, NULL);
    while (!zsys_interrupted) {
        void *which = zpoller_wait(poller, WATCHDOG_INTERVAL);
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv(pipe);
            char *cmd = zmsg_popstr(msg);
            if (streq(cmd, "$TERM")) {
                zstr_free(&cmd);
                zmsg_destroy(&msg);
                break;
            }
            zstr_free(&cmd);
            zmsg_destroy(&msg);
            wd->tick();
        } else if (wd->check()) {
            continue;
        }
        log_error("watchdog expired");
        exit(2);
    }
    zpoller_destroy(&poller);
}

void Watchdog::start()
{
    tick();
    thread_ = zactor_new(watchdog_thread, this);
    assert(thread_);
}

bool Watchdog::check()
{
    return last_tick_.load() + WATCHDOG_LIMIT > zclock_mono() / 1000;
}

