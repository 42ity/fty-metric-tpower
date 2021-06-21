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

/// @file   tpowerconfiguration.h
/// @brief  Configuration of the power computation
/// @author Tomas Halman <TomasHalman@Eaton.com>

#pragma once

#include "tp_unit.h"
#include <fty_proto.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

// TODO: read this from configuration (once in 5 minutes now (300s)) in [s]
#define TPOWER_MEASUREMENT_REPEAT_AFTER 300
// TODO: read this from configuration (check with upsd ever 5s) in [ms]
#define TPOWER_POLLING_INTERVAL 5000


class TotalPowerConfiguration
{
public:
    TotalPowerConfiguration(std::function<bool(const MetricInfo&)> f)
        : _timeout{TPOWER_POLLING_INTERVAL}
    {
        _sendingFunction = f;
    };

    void processMetric(const MetricInfo& M, const std::string& topic);
    void processAsset(fty_proto_t* message);
    void onPoll();
    void setPollInterval();
    /// read configuration from database
    bool configure();

    /// in[ms]
    int64_t getTimeout(void)
    {
        return _timeout;
    };

private:
    /// Function that is responsible for sending the message
    /// @param M - MetricInfo represents a metric to be sent
    /// @return true is metric was sent successfully
    std::function<bool(const MetricInfo&)> _sendingFunction;

    /// in [ms]
    int64_t _timeout;
    /// list of racks
    std::map<std::string, TPUnit> _racks;
    /// list of interested units
    const std::vector<std::string> _rackQuantities = {
        "realpower.default",
    };
    bool isRackQuantity(const std::string& quantity) const;
    /// list of racks, affected by powerdevice
    std::map<std::string, std::string> _affectedRacks;

    /// list of datacenters
    std::map<std::string, TPUnit> _DCs;
    /// list of interested units
    const std::vector<std::string> _dcQuantities = {
        "realpower.default",
        "realpower.input.L1",
        "realpower.input.L2",
        "realpower.input.L3",
        "realpower.output.L1",
        "realpower.output.L2",
        "realpower.output.L3",
    };
    bool isDCQuantity(const std::string& quantity) const;
    /// list of DCs, affected by powerdevice
    std::map<std::string, std::string> _affectedDCs;

    /// timestamp, when we should re-read configuration
    int64_t _reconfigPending = 0;


    /// send measurement message if needed
    void sendMeasurement(std::map<std::string, TPUnit>& elements, const std::vector<std::string>& quantities);
    /// send measurement message for a single unit if needed
    bool sendMeasurement(std::pair<const std::string, TPUnit>& element, const std::string& quantity);

    /// powerdevice to DC or rack and put it also in _affected* map
    void addDeviceToMap(std::map<std::string, TPUnit>& elements, std::map<std::string, std::string>& reverseMap,
        const std::string& owner, const std::string& device);

    /// calculete polling interval (not to wake up every 5s)
    int64_t getPollInterval();
};
