/*
 *
 * Copyright (C) 2015 Eaton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "tpower_classes.h"
#include <ctime>
#include <exception>

/**
 * \brief mapping table for missing values replacements
 *
 * This table says simply "if we don't have measurement for realpower.input.L1, we can
 * use realpower.output.L1 instead".
 */
const std::map<std::string,std::string> TPUnit::_emergencyReplacements = {
    { "realpower.input.L1", "realpower.output.L1" },
    { "realpower.input.L2", "realpower.output.L2" },
    { "realpower.input.L3", "realpower.output.L3" },
};

enum TPowerMethod {
    TPOWER_REALPOWER_DEFAULT = 1,
};

const std::map<std::string,int> TPUnit::_calculations = {
    { "realpower.default", TPOWER_REALPOWER_DEFAULT },
};

double TPUnit::
    get( const std::string &quantity) const
{
    double result = _lastValue.find( quantity );
    if ( isnan(result) ) {
        throw std::runtime_error("Unknown quantity");
    }
    return result;
}


MetricInfo TPUnit::
    getMetricInfo(const std::string &quantity) const
{
    auto result = _lastValue.getMetricInfo( generateTopic(quantity) );
    if ( result.isUnknown() ) {
        throw std::runtime_error("Unknown quantity");
    }
    return result;
}

void TPUnit::
    set(const std::string &quantity, MetricInfo &measurement)
{
    double itSums = _lastValue.find(quantity);
    if( isnan(itSums) || (itSums != measurement.getValue()) ) {
        _lastValue.addMetric(measurement);
        _changed[quantity] = true;
        _changetimestamp[quantity] = measurement.getTimestamp();
    }
}

MetricInfo TPUnit::
    simpleSummarize(const std::string &quantity) const
{
    double sum = 0;
    for( const auto &it : _powerdevices ) {
        auto itMetricInfo = getMetricInfoIter( it.second, quantity, it.first );
        if( isnan(itMetricInfo) ) {
            throw std::runtime_error("value can't be calculated");
        } else {
            sum += itMetricInfo;
        }
    }
    MetricInfo result ( _name, quantity, "W", sum, -1, "");
    return result;
}

MetricInfo TPUnit::
    realpowerDefault(const std::string &quantity) const
{
    double sum = 0;
    for( const auto it : _powerdevices ) {
        auto itMetricInfos = getMetricInfoIter( it.second, quantity, it.first );
        if( isnan (itMetricInfos) ) {
            // realpower.default not present, try to sum the phases
            for( int phase = 1 ; phase <= 3 ; ++phase ) {
                auto itItem = getMetricInfoIter( it.second, "realpower.input.L" + std::to_string( phase ), it.first );
                if( isnan(itItem) ) {
                    throw std::runtime_error("value can't be calculated");
                }
                sum += itItem;
            }
        } else {
            sum += itMetricInfos;
        }
    }
    MetricInfo result ( _name, quantity, "W", sum, -1, "");
    return result;
}

void TPUnit::
    calculate(const std::vector<std::string> &quantities)
{
    dropOldMetricInfos();
    for( const auto it : quantities ) {
        calculate( it );
    }
}

void TPUnit::
    calculate(const std::string &quantity)
{
    try {
        MetricInfo result;
        int calc = 0;
        const auto how = _calculations.find(quantity);
        if( how != _calculations.cend() ) calc = how->second;
        switch( calc ) {
        case TPOWER_REALPOWER_DEFAULT:
            result = realpowerDefault( quantity );
            break;
        default:
            result = simpleSummarize( quantity );
            break;
        }
        set( quantity, result );
    } catch (...) { }
}

double TPUnit::
    getMetricInfoIter(
        const MetricList  &measurements,
        const std::string &quantity,
        const std::string &deviceName
    ) const
{
    std::string topic = quantity + "@" + deviceName;
    double result = measurements.find(topic);
    if( !isnan(result) ) {
        // we have it
        return result;
    }
    const auto &replacement = _emergencyReplacements.find(quantity);
    if( replacement == _emergencyReplacements.cend() ) {
        // there is no replacement for this value
        return NAN;
    }
    // find the replacement value if any
    double result_replace = measurements.find( replacement->second );
    if( isnan(result_replace) ) {
        zsys_info("device %s, value of %s is unknown",
                 deviceName.c_str(),
                 replacement->second.c_str() );
    } else {
        zsys_debug("device %s, using replacement value %s instead of %s",
                  deviceName.c_str(),
                  replacement->second.c_str(),
                  quantity.c_str() );
    }
    return result_replace;
}
// TODO setup max life time metric
void TPUnit::
    dropOldMetricInfos(void)
{
//    int64_t now = std::time(NULL);
    for( auto & device : _powerdevices ) {
        auto &measurements = device.second;
        measurements.removeOldMetrics();
    }
    _lastValue.removeOldMetrics();
}

std::string TPUnit::
    generateTopic (const std::string &quantity) const
{
    return quantity + "@" + _name;
}

bool TPUnit::
    quantityIsUnknown(const std::string &topic) const
{
    return  isnan(_lastValue.find(topic));
}

std::vector<std::string> TPUnit::
    devicesInUnknownState(const std::string &quantity) const
{
    std::vector<std::string> result;

    if ( quantityIsKnown( generateTopic(quantity) ) ) {
        return result;
    }

    int64_t now = std::time(NULL);
    for( const auto &device : _powerdevices ) {
        const auto &deviceMetrics = device.second;
        std::string topic = quantity + "@" + device.first;
        auto measurement = deviceMetrics.getMetricInfo(topic);
        if ( ( isnan (measurement.getValue()) ) ||
             ( now - measurement.getTimestamp() > measurement.getLtl() * 2 )
           )
        {
            result.push_back( device.first );
        }
    }
    return result;
}

void TPUnit::
    addPowerDevice(const std::string &device)
{
    _powerdevices[device] = {};
}

void TPUnit::
    setMeasurement(const MetricInfo &M)
{
    auto device = _powerdevices.find( M.getElementName() );
    if( device != _powerdevices.end() ) {
        device->second.addMetric (M);
    }
}

bool TPUnit::
    changed(const std::string &quantity) const
{
    auto it = _changed.find(quantity);
    if( it == _changed.end() ) return false;
    return it->second;
}

void TPUnit::
    changed(const std::string &quantity, bool newStatus)
{
    if( changed( quantity ) != newStatus ) {
        _changed[quantity] = newStatus;
        _changetimestamp[quantity] = time(NULL);
        if( _advertisedtimestamp.find(quantity) == _advertisedtimestamp.end() ) {
            _advertisedtimestamp[quantity] = 0;
        }
    }
}

void TPUnit::
    advertised(const std::string &quantity)
{
    changed( quantity, false );
    int64_t now_timestamp = ::time(NULL);
    _changetimestamp[quantity] = now_timestamp;
    _advertisedtimestamp[quantity] = now_timestamp;
}

int64_t TPUnit::
    timestamp( const std::string &quantity ) const
{
    auto it = _changetimestamp.find(quantity);
    if( it == _changetimestamp.end() ) return 0;
    return it->second;
}

int64_t TPUnit::
    timeToAdvertisement ( const std::string &quantity ) const
{
    auto quantityTimestamp = timestamp (quantity);
    if ( ( quantityTimestamp == 0 ) ||
           quantityIsUnknown(generateTopic(quantity))
       )
    {
        // if quantity didn't change and it is still unknown
        return TPOWER_MEASUREMENT_REPEAT_AFTER;
    }
    int64_t dt = ::time(NULL) - quantityTimestamp;
    if ( dt > TPOWER_MEASUREMENT_REPEAT_AFTER ) {
        // no time left for waiting -> Need to advertise
        return 0;
    }
    // we should wait a little bit, before advertising
    return TPOWER_MEASUREMENT_REPEAT_AFTER - dt;
}

bool TPUnit::
    advertise( const std::string &quantity ) const
{
    if ( quantityIsUnknown(generateTopic(quantity)) ) {
        // if do not know the quantity -> nothing to advertise
        return false;
    }
    int64_t now_timestamp = ::time(NULL);
    // find the time, when quantity was advertised last time
    const auto it = _advertisedtimestamp.find(quantity);
    if ( ( it != _advertisedtimestamp.end() ) &&
         ( it->second == now_timestamp ) )
    {
        // if time is known and
        //    time is just now was advertised -> nothing to advertise
        return false;
    }
    // advertise if 
    // * value changed or
    // * we should advertise according schedule
    return ( changed(quantity) || ( now_timestamp - timestamp(quantity) > TPOWER_MEASUREMENT_REPEAT_AFTER ) );
}

void tp_unit_test(bool verbose)
{
    //empty
}
