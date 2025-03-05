#include <catch2/catch.hpp>
#include <malamute.h>
#include "src/metricinfo.h"
#include "src/fty_metric_tpower_server.h"
#include <fty_proto.h>
#include <fty_shm.h>

TEST_CASE("fty metric tpower server test")
{
    const char* ENDPOINT = "inproc://bios-tpower-server-test";

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
