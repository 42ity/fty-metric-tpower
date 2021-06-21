/*  =========================================================================
    tpowerconfiguration - Configuration

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

#include "tpowerconfiguration.h"
#include "calc_power.h"
#include <algorithm>
#include <errno.h>
#include <exception>
#include <fty_common.h>
#include <fty_common_db_asset.h>
#include <fty_common_db_dbpath.h>
#include <fty_common_str_defs.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#define ANSI_COLOR_BOLD  "\x1b[1;39m"
#define ANSI_COLOR_RED   "\x1b[1;31m"
#define ANSI_COLOR_RESET "\x1b[0m"

bool TotalPowerConfiguration::configure(void)
{
    log_info("loading power topology");

    // TODO should be rewritten, for usinf messages
    try {
        // remove old topology
        _racks.clear();
        _affectedRacks.clear();
        _DCs.clear();
        _affectedDCs.clear();

        // connect to the database
        tntdb::Connection connection = tntdb::connectCached(DBConn::url);

        // reading racks
        auto ret = select_devices_total_power_racks(connection); // calc_power.cc
        if (ret.status) {
            log_info("reading racks (count: %lu)...", ret.item.size());
            for (auto& rack : ret.item) {
                std::string aux;
                auto&       devices = rack.second;
                for (auto& device : devices) {
                    addDeviceToMap(_racks, _affectedRacks, rack.first, device);
                    aux += (aux.empty() ? "" : ", ") + device;
                }
                log_info(ANSI_COLOR_BOLD "rack '%s' powerdevices: %s" ANSI_COLOR_RESET, rack.first.c_str(),
                    aux.empty() ? "<empty>" : aux.c_str());
            }
        }

        // reading DCs
        ret = select_devices_total_power_dcs(connection); // calc_power.cc
        if (ret.status) {
            log_info("reading DCs (count: %lu)...", ret.item.size());
            for (auto& dc : ret.item) {
                std::string aux;
                auto&       devices = dc.second;
                for (auto& device : devices) {
                    addDeviceToMap(_DCs, _affectedDCs, dc.first, device);
                    aux += ((!aux.empty()) ? ", " : "") + device;
                }
                log_info(ANSI_COLOR_BOLD "DC '%s' powerdevices: %s" ANSI_COLOR_RESET, dc.first.c_str(),
                    aux.empty() ? "<empty>" : aux.c_str());
            }
        }

        connection.close();

        // no reconfiguration should be scheduled
        _reconfigPending = 0;

        log_info("topology loaded with success");
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to read configuration from database. Excepton caught: '%s'.", e.what());
    } catch (...) {
        log_error("Failed to read configuration from database. Unknown exception caught.");
    }

    _reconfigPending = ::time(NULL) + 60; // retry later
    return false;
}

void TotalPowerConfiguration::addDeviceToMap(std::map<std::string, TPUnit>& elements,   // owners map
    std::map<std::string, std::string>&                                     reverseMap, // device -> owner
    const std::string& owner,  // datacenter-3, rack-5, ... (asset name)
    const std::string& device) // ups-xx, epdu-yy, ... (asset name)
{
    auto element = elements.find(owner);
    if (element == elements.end()) {
        auto box = TPUnit();
        box.name(owner);
        box.addPowerDevice(device);
        elements[owner] = box;
    } else {
        element->second.addPowerDevice(device);
    }
    reverseMap[device] = owner;
}

void TotalPowerConfiguration::processAsset(fty_proto_t* message)
{
    std::string operation(fty_proto_operation(message));
    if (operation != FTY_PROTO_ASSET_OP_CREATE && operation != FTY_PROTO_ASSET_OP_UPDATE &&
        operation != FTY_PROTO_ASSET_OP_DELETE && operation != FTY_PROTO_ASSET_OP_RETIRE) {
        return;
    }

    // something is beeing reconfigured, let things to settle down
    if (_reconfigPending == 0) {
        log_info("Reconfiguration scheduled");
        _reconfigPending = ::time(NULL) + 60; // in 60[s]
    }
    _timeout = getPollInterval();
    log_info("ASSET %s, %s operation processed", fty_proto_name(message), operation.c_str());
}

bool TotalPowerConfiguration::isRackQuantity(const std::string& quantity) const
{
    return std::find(_rackQuantities.begin(), _rackQuantities.end(), quantity) != _rackQuantities.end();
}

bool TotalPowerConfiguration::isDCQuantity(const std::string& quantity) const
{
    return std::find(_dcQuantities.begin(), _dcQuantities.end(), quantity) != _dcQuantities.end();
}

void TotalPowerConfiguration::processMetric(const MetricInfo& M, const std::string& topic)
{
    // topic: <quantity>@<asset_name>
    // ex.: 'realpower.input.L3@epdu-42'
    std::string quantity = topic.substr(0, topic.find('@'));

    log_trace("processMetric %s", topic.c_str());
    bool used = false, rackMeasureSent = false, dcMeasureSent = false;

    // ASSUMPTION: one device can affect only one ASSET of each type ( Datacenter or Rack )

    if (isRackQuantity(quantity)) {
        auto affected_it = _affectedRacks.find(M.getElementName());
        if (affected_it != _affectedRacks.end()) {
            // the metric affects some total rack power
            log_trace("%s is interesting for rack %s", topic.c_str(), affected_it->second.c_str());

            auto rack = _racks.find(affected_it->second); // < std::string, TPUnit > &rack;
            if (rack != _racks.end()) {
                // affected rack found, handle the new metric
                rack->second.setMeasurement(M);                     // register the measure
                rackMeasureSent = sendMeasurement(*rack, quantity); // compute + send conditionally
                used            = true;
            }
        }
    }

    if (isDCQuantity(quantity)) {
        auto affected_it = _affectedDCs.find(M.getElementName());
        if (affected_it != _affectedDCs.end()) {
            // the metric affects some total DC power
            log_trace("%s is interesting for DC %s", topic.c_str(), affected_it->second.c_str());

            auto dc = _DCs.find(affected_it->second); // < std::string, TPUnit > &dc;
            if (dc != _DCs.end()) {
                // affected dc found, handle the new metric
                dc->second.setMeasurement(M);                   // register the measure
                dcMeasureSent = sendMeasurement(*dc, quantity); // compute + send conditionally
                used          = true;
            }
        }
    }

    log_trace("processMetric %s was %s (rack measure %s, DC measure %s)", topic.c_str(), (used ? "used" : "ignored"),
        (rackMeasureSent ? "sent" : "not sent"), (dcMeasureSent ? "sent" : "not sent"));
}

bool TotalPowerConfiguration::sendMeasurement(
    std::pair<const std::string, TPUnit>& element, const std::string& quantity)
{
    bool isSent = false;

    // renaming for better reading
    auto& powerUnit = element.second; // TPUnit

    // calculate quantity for element.first (rack or dc)
    powerUnit.calculate(quantity);

    if (powerUnit.advertise(quantity)) {
        try {
            MetricInfo M = powerUnit.getMetricInfo(quantity);
            isSent       = _sendingFunction(M);
            if (isSent) {
                powerUnit.advertised(quantity);
            }
        } catch (...) {
            log_error(ANSI_COLOR_RED "Some unexpected error during sending new measurement" ANSI_COLOR_RESET);
        };
    } else {
        // log something from time to time if device calculation is unknown
        auto devices = powerUnit.devicesInUnknownState(quantity);
        if (!devices.empty()) {
            std::string aux;
            for (auto& it : devices) {
                aux += (aux.empty() ? "" : ", ") + it;
            }

            log_info(ANSI_COLOR_BOLD "%zd devices preventing total %s calculation for %s: %s" ANSI_COLOR_RESET,
                devices.size(), quantity.c_str(), element.first.c_str(), aux.c_str());
        }
    }

    return isSent;
}

void TotalPowerConfiguration::sendMeasurement(
    std::map<std::string, TPUnit>& elements, const std::vector<std::string>& quantities)
{
    for (auto& element : elements) {
        // XXX: This overload is called by onPoll() periodically, hence the purging
        element.second.dropOldMetricInfos();
        for (auto& quantity : quantities) {
            sendMeasurement(element, quantity);
        }
    }
}

int64_t TotalPowerConfiguration::getPollInterval()
{
    int64_t T = TPOWER_MEASUREMENT_REPEAT_AFTER; // default, seconds
    int64_t Tx;

    for (auto& rack : _racks) {
        for (auto& q : _rackQuantities) {
            Tx = rack.second.timeToAdvertisement(q);
            if ((Tx > 0) && (Tx < T))
                T = Tx;
        }
    }

    for (auto& dc : _DCs) {
        for (auto& q : _dcQuantities) {
            Tx = dc.second.timeToAdvertisement(q);
            if ((Tx > 0) && (Tx < T))
                T = Tx;
        }
    }

    if (_reconfigPending != 0) {
        Tx = _reconfigPending - ::time(NULL) + 1;
        if (Tx <= 0)
            Tx = 1;
        if (Tx < T)
            T = Tx;
    }

    return T * 1000; // ms
}

void TotalPowerConfiguration::onPoll()
{
    sendMeasurement(_racks, _rackQuantities);
    sendMeasurement(_DCs, _dcQuantities);

    if ((_reconfigPending != 0) && (_reconfigPending <= ::time(NULL))) {
        configure();
    }

    _timeout = getPollInterval();
}

void TotalPowerConfiguration::setPollInterval()
{
    _timeout = getPollInterval();
}
