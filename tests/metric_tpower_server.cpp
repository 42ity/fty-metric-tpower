/*  ========================================================================
    Copyright (C) 2020 Eaton
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
    ========================================================================
*/

#include <catch2/catch.hpp>
#include <malamute.h>
#include "src/metricinfo.h"
#include "src/fty_metric_tpower_server.h"
#include <fty_proto.h>
#include <fty_shm.h>

// !?
TEST_CASE("fty metric tpower server test")
{
    const char* ENDPOINT = "inproc://fty-tpower-server-test";

    zactor_t* server = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    REQUIRE(server);
    zstr_sendx(server, "BIND", ENDPOINT, nullptr);

    REQUIRE(fty_shm_set_test_dir("selftest-rw") == 0);

    uint64_t timestamp = uint64_t(::time(nullptr));
    uint32_t ttl = 500;
    MetricInfo mi("someUPS", "realpower.default", "W", 456.66, timestamp, ttl);
    REQUIRE(write_metric(mi));

    fty_proto_t* proto = NULL;
    REQUIRE(fty::shm::read_metric("someUPS", "realpower.default", &proto) == 0);
    REQUIRE(proto != nullptr);
    fty_proto_print(proto);
    fty_proto_destroy(&proto);

    zactor_destroy(&server);
    fty_shm_delete_test_dir();
}
