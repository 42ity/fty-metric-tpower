/*
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
*/

#include "metriclist.h"
#include <cassert>
#include <cmath> // NAN is here

void MetricList::addMetricInfo(const MetricInfo& metricInfo)
{
    std::string topic = metricInfo.generateTopic();

    // find topic; if found -> replace metric, else add it
    auto it = _knownMetrics.find(topic);
    if (it != _knownMetrics.cend())
        it->second = metricInfo;
    else
        _knownMetrics.emplace(topic, metricInfo);
}

MetricInfo MetricList::getMetricInfo(const std::string& topic) const
{
    auto it = _knownMetrics.find(topic);
    return (it != _knownMetrics.cend()) ? it->second : MetricInfo();
}

double MetricList::find(const std::string& topic) const
{
    auto it = _knownMetrics.find(topic);
    return (it != _knownMetrics.cend()) ? it->second._value : std::nan("");
}

void MetricList::removeOldMetrics()
{
    uint64_t now = uint64_t(::time(NULL));

    std::map<std::string, MetricInfo>::iterator iter = _knownMetrics.begin();
    while (iter != _knownMetrics.end()) {
        if ((now - iter->second._timestamp) > iter->second.getTtl()) {
            _knownMetrics.erase(iter++);
        } else {
            ++iter;
        }
    }
}
