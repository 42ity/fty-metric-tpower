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
#include "termColors.h"

#include <fty_log.h>
#include <fty_common_db_asset.h>
#include <fty_common_db_dbpath.h>
#include <fty_common_str_defs.h>

#include <algorithm>
#include <errno.h>
#include <exception>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

// handle multi-cards power devices (Hercule UPS)
// NOTICE: devices can be changed
static void sanitizeDevices(tntdb::Connection& conn, std::vector<std::string>& devices)
{
    if (devices.size() < 2) { return; } // nothing to do

    // set devices ids (used for db select)
    std::set<uint32_t> devicesIds;
    for (const auto& iname : devices) {
        int64_t id = DBAssets::name_to_asset_id(iname);
        if (id > 0) { devicesIds.insert(uint32_t(id)); }
    }

    // list (comm. cards) assets related to real devices (set realDevices)
    std::map<std::string, std::set<std::string>> realDevices; // <serial_no, <iname,...>>
    {
        // select serial_no ext. attribute of devices
        std::function<void(const tntdb::Row&)> cb = [&realDevices] (const tntdb::Row& row) {
            uint32_t asset_id = 0;
            row["id_asset_element"].get(asset_id);
            std::string iname = DBAssets::id_to_name_ext_name(asset_id).first;
            if (iname.empty()) { log_debug("iname empty (asset_id: %d)", asset_id); return; }

            std::string serial_no;
            row["value"].get(serial_no);
            if (serial_no.empty()) { log_debug("serial_no empty (iname: %s)", iname.c_str()); return; }

            if (realDevices.count(serial_no) == 0) { realDevices[serial_no] = {}; }
            realDevices[serial_no].insert(iname);
        };

        // walk on all "serial_no" ext. attributes of devices
        int r = DBAssets::select_asset_ext_attribute_by_keytag(conn, "serial_no", devicesIds, cb);
        if (r != 0) { log_error("select_asset_ext_attribute_by_keytag serial_no failed"); return; }
    }

    for (const auto& realDevice : realDevices) {
        if (realDevice.second.size() < 2) { continue; } // mono-card device ignored

        // real device as Hercule UPS with at least 2 comm. cards/assets
        // get the asset (xname) representative of the ups in devices
        std::string xname; //empty
        for (const auto& iname : realDevice.second) {
            if (std::find(devices.begin(), devices.end(), iname) != devices.end())
                { xname = iname; break; } // first found
        }
        if (xname.empty()) { continue; } // not contributing

        // remove from devices any asset representative of the ups that is not xname
        for (const auto& iname : realDevice.second) {
            if (iname == xname) { continue; }
            if (std::find(devices.begin(), devices.end(), iname) != devices.end()) {
                // remove iname from devices (devices changed)
                devices.erase(std::remove(devices.begin(), devices.end(), iname), devices.end());

                log_debug(TC_MAGENTA "Removes %s which duplicates %s (serial: %s)" TC0,
                    iname.c_str(), xname.c_str(), realDevice.first.c_str());
            }
        }
    }
}

bool TotalPowerConfiguration::configure()
{
    log_info("loading power topology");

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

            for (const auto& rack : ret.item) {
                auto devices = rack.second;
                sanitizeDevices(connection, devices);

                std::string aux;
                for (const auto& device : devices) {
                    addDeviceToMap(_racks, _affectedRacks, rack.first, device);
                    aux += (aux.empty() ? "" : ", ") + device;
                }

                log_info(TC_BOLD "rack '%s' powerdevices: %s" TC0,
                    rack.first.c_str(), aux.empty() ? "<empty>" : aux.c_str());
            }
        }

        // reading DCs
        ret = select_devices_total_power_dcs(connection); // calc_power.cc
        if (ret.status) {
            log_info("reading DCs (count: %lu)...", ret.item.size());

            for (const auto& dc : ret.item) {
                auto devices = dc.second;
                sanitizeDevices(connection, devices);

                std::string aux;
                for (const auto& device : devices) {
                    addDeviceToMap(_DCs, _affectedDCs, dc.first, device);
                    aux += (aux.empty() ? "" : ", ") + device;
                }

                log_info(TC_BOLD "DC '%s' powerdevices: %s" TC0,
                    dc.first.c_str(), aux.empty() ? "<empty>" : aux.c_str());
            }
        }

        connection.close();

        // no reconfiguration should be scheduled
        _reconfigPending = 0;

        log_info("topology loaded with success");
        return true;
    }
    catch (const std::exception& e) {
        log_error("Failed to read configuration from database. Excepton caught: '%s'.", e.what());
    }
    catch (...) {
        log_error("Failed to read configuration from database. Unknown exception caught.");
    }

    _reconfigPending = ::time(NULL) + 60; // retry later
    return false;
}

void TotalPowerConfiguration::addDeviceToMap(
    std::map<std::string, TPUnit>& elements, // owners map
    std::map<std::string, std::string>& reverseMap, // device -> owner
    const std::string& owner,  // datacenter-3, rack-5, ... (asset name)
    const std::string& device // ups-xx, epdu-yy, ... (asset name)
) {
    auto element = elements.find(owner);
    if (element == elements.end()) {
        TPUnit box;
        box.name(owner);
        box.addPowerDevice(device);
        elements[owner] = box;
    }
    else {
        element->second.addPowerDevice(device);
    }

    reverseMap[device] = owner;
}

void TotalPowerConfiguration::processAsset(fty_proto_t* message)
{
    if (!message) { return; }

    const std::string operation(fty_proto_operation(message));

    if (operation != FTY_PROTO_ASSET_OP_CREATE
        && operation != FTY_PROTO_ASSET_OP_UPDATE
        && operation != FTY_PROTO_ASSET_OP_DELETE
        && operation != FTY_PROTO_ASSET_OP_RETIRE
    ) {
        return;
    }

    // something is being reconfigured, let things to settle down
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

void TotalPowerConfiguration::processMetric(const MetricInfo& metricInfo, const std::string& topic)
{
    log_trace("processMetric %s", topic.c_str());

    // topic: <quantity>@<asset_name>
    // ex.: 'realpower.input.L3@epdu-42'
    std::string quantity = topic.substr(0, topic.find('@'));

    bool used = false;
    bool rackMeasure = false;
    bool dcMeasure = false;

    // ASSUMPTION: one device can affect only one ASSET of each type (Datacenter or Rack)

    if (isRackQuantity(quantity)) {
        auto affected_it = _affectedRacks.find(metricInfo.getElementName());
        if (affected_it != _affectedRacks.end()) {
            // the metric affects some total rack power
            log_trace("%s is interesting for rack %s", topic.c_str(), affected_it->second.c_str());

            auto rack = _racks.find(affected_it->second); // < std::string, TPUnit > &rack;
            if (rack != _racks.end()) {
                // affected rack found, handle the new metric
                rack->second.updateMeasurement(metricInfo); // register the measure
                rackMeasure = exportMeasurement(*rack, quantity); // compute + export conditionally
                used = true;
            }
        }
    }

    if (isDCQuantity(quantity)) {
        auto affected_it = _affectedDCs.find(metricInfo.getElementName());
        if (affected_it != _affectedDCs.end()) {
            // the metric affects some total DC power
            log_trace("%s is interesting for DC %s", topic.c_str(), affected_it->second.c_str());

            auto dc = _DCs.find(affected_it->second); // < std::string, TPUnit > &dc;
            if (dc != _DCs.end()) {
                // affected dc found, handle the new metric
                dc->second.updateMeasurement(metricInfo); // register the measure
                dcMeasure = exportMeasurement(*dc, quantity); // compute + export conditionally
                used = true;
            }
        }
    }

    log_trace("processMetric %s was %s (rack measure: %s, DC measure: %s)",
        topic.c_str(),
        (used ? "used" : "ignored"),
        (rackMeasure ? "Yes" : "No"),
        (dcMeasure ? "Yes" : "No")
    );
}

bool TotalPowerConfiguration::exportMeasurement(std::pair<const std::string, TPUnit>& element, const std::string& quantity)
{
    bool isSent = false;

    // renaming for better reading
    auto& powerUnit = element.second; // TPUnit

    // calculate quantity for element.first (rack or dc)
    powerUnit.calculate(quantity);

    if (powerUnit.advertise(quantity)) {
        try {
            MetricInfo metricInfo = powerUnit.getMetricInfo(quantity);
            isSent = _exportMetric(metricInfo);
            if (isSent) {
                powerUnit.advertised(quantity);
            }
        }
        catch (...) {
            log_error(TC_RED "Some unexpected error sending new measurement" TC0);
        };
    }
    else {
        // log something from time to time if device calculation is unknown
        auto devices = powerUnit.devicesInUnknownState(quantity);
        if (!devices.empty()) {
            std::string aux;
            for (const auto& it : devices) {
                aux += (aux.empty() ? "" : ", ") + it;
            }

            log_info(TC_BOLD "%zd device(s) preventing total %s calculation for %s: %s" TC0,
                devices.size(), quantity.c_str(), element.first.c_str(), aux.c_str());
        }
    }

    return isSent;
}

void TotalPowerConfiguration::exportMeasurement(std::map<std::string, TPUnit>& elements, const std::vector<std::string>& quantities)
{
    for (auto& element : elements) {
        // XXX: This overload is called by onPoll() periodically, hence the purging
        element.second.removeDeprecatedMetrics();

        for (const auto& quantity : quantities) {
            exportMeasurement(element, quantity);
        }
    }
}

int64_t TotalPowerConfiguration::getPollInterval() const
{
    int64_t T = TPOWER_MEASUREMENT_REPEAT_AFTER; // default, seconds

    for (const auto& rack : _racks) {
        for (auto& q : _rackQuantities) {
            int64_t Tx = rack.second.timeToAdvertisement(q);
            if ((Tx > 0) && (Tx < T)) {
                T = Tx;
            }
        }
    }

    for (const auto& dc : _DCs) {
        for (auto& q : _dcQuantities) {
            int64_t Tx = dc.second.timeToAdvertisement(q);
            if ((Tx > 0) && (Tx < T)) {
                T = Tx;
            }
        }
    }

    if (_reconfigPending != 0) {
        int64_t Tx = _reconfigPending - ::time(NULL) + 1;
        if (Tx <= 0) {
            Tx = 1;
        }
        if (Tx < T) {
            T = Tx;
        }
    }

    return T * 1000; // ms
}

void TotalPowerConfiguration::onPoll()
{
    exportMeasurement(_racks, _rackQuantities);
    exportMeasurement(_DCs, _dcQuantities);

    if ((_reconfigPending != 0) && (_reconfigPending <= ::time(NULL))) {
        configure();
    }

    _timeout = getPollInterval();
}

void TotalPowerConfiguration::setPollInterval()
{
    _timeout = getPollInterval();
}
