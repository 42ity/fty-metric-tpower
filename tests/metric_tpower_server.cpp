#include <catch2/catch.hpp>
#include <malamute.h>
#include "src/metricinfo.h"
#include "src/fty_metric_tpower_server.h"
#include <fty_proto.h>
#include <fty_shm.h>

TEST_CASE("fty metric tpower server test")
{
    static const char* endpoint = "inproc://bios-tpower-server-test";

    zactor_t* server = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    zstr_sendx(server, "BIND", endpoint, nullptr);

    REQUIRE(fty_shm_set_test_dir("selftest-rw") == 0);

    uint64_t   timestamp = uint64_t(::time(nullptr));
    uint32_t   ttl       = 500;
    MetricInfo M("someUPS", "realpower.default", "W", 456.66, timestamp, ttl);
    REQUIRE(send_metrics(M));

    fty_proto_t* bmessage;
    REQUIRE(fty::shm::read_metric("someUPS", "realpower.default", &bmessage) == 0);
    REQUIRE(bmessage != nullptr);
    fty_proto_print(bmessage);
    CHECK(fty_proto_id(bmessage) == FTY_PROTO_METRIC);

    fty_proto_destroy(&bmessage);
    zactor_destroy(&server);
    fty_shm_delete_test_dir();
    printf("OK\n");
}
