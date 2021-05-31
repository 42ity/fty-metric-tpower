cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

project(fty-metric-tpower
    VERSION 1.0.0
    DESCRIPTION "42ity component for power metrics computation"
)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

etn_target(static ${PROJECT_NAME}-lib
    SOURCES
        src/calc_power.cc
        src/calc_power.h
        src/fty_metric_tpower_server.cc
        src/fty_metric_tpower_server.h
        src/metricinfo.h
        src/metriclist.cc
        src/metriclist.h
        src/tpowerconfiguration.cc
        src/tpowerconfiguration.h
        src/tp_unit.cc
        src/tp_unit.h
        src/watchdog.cc
        src/watchdog.h
    USES
        czmq
        mlm
        cxxtools
        fty_common
        fty_common_logging
        fty_proto
        fty_common_mlm
        fty_common_db
        fty_shm
        tntdb
    PRIVATE
)

##############################################################################################################

etn_target(exe ${PROJECT_NAME}
    SOURCES
        src/fty_metric_tpower.cc
    USES_PRIVATE
        ${PROJECT_NAME}-lib
)

##############################################################################################################

etn_test_target(${PROJECT_NAME}-lib
    SOURCES
        tests/main.cpp
        tests/metric_tpower_server.cpp
    PREPROCESSOR
        -DCATCH_CONFIG_FAST_COMPILE
    SUBDIR
        tests
)

##############################################################################################################

#install resources files
set(AGENT_SETTINGS_DIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/fty/${PROJECT_NAME}")
set(AGENT_CONF_FILE "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME}/fty-metric-store.cfg")
set(AGENT_USER "bios")

configure_file("${PROJECT_SOURCE_DIR}/resources/${PROJECT_NAME}.cfg.in" "${PROJECT_BINARY_DIR}/resources/${PROJECT_NAME}.cfg" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/resources/${PROJECT_NAME}.cfg" DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME})

configure_file("${PROJECT_SOURCE_DIR}/resources/${PROJECT_NAME}.service.in" "${PROJECT_BINARY_DIR}/resources/${PROJECT_NAME}.service" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/resources/${PROJECT_NAME}.service" DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/)
