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

#include "fty_metric_tpower_classes.h"
#include <ctime>
#include <exception>

#define ANSI_COLOR_BOLD  "\x1b[1;39m"
#define ANSI_COLOR_RED     "\x1b[1;31m"
#define ANSI_COLOR_LIGHTMAGENTA    "\x1b[1;95m"
#define ANSI_COLOR_RESET   "\x1b[0m"

enum TPowerMethod {
    TPOWER_REALPOWER_UNDEFINED = 0,
    TPOWER_REALPOWER_DEFAULT   = 1,
    TPOWER_REALPOWER_OUTPUT_L1 = 2,
    TPOWER_REALPOWER_OUTPUT_L2 = 3,
    TPOWER_REALPOWER_OUTPUT_L3 = 4,
};

const std::map<std::string,int> TPUnit::_calculations = {
    { "realpower.default",   TPOWER_REALPOWER_DEFAULT   },
    { "realpower.output.L1", TPOWER_REALPOWER_OUTPUT_L1 },
    { "realpower.output.L2", TPOWER_REALPOWER_OUTPUT_L2 },
    { "realpower.output.L3", TPOWER_REALPOWER_OUTPUT_L3 },
};

double TPUnit::
    get( const std::string &quantity) const
{
    double result = _lastValue.find( quantity );
    if ( std::isnan(result) ) {
        throw std::runtime_error("Unknown quantity (" + generateTopic(quantity) + ")");
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
    double currentValue = _lastValue.find(generateTopic(quantity));

    if( std::isnan(currentValue) || ( abs(currentValue - measurement.getValue()) > 0.00001) )
    {
        _lastValue.addMetricInfo(measurement);
        _changed[quantity] = true;
        _changetimestamp[quantity] = measurement.getTimestamp();
    }
}

MetricInfo TPUnit::
    simpleSummarize(const std::string &quantity) const
{
    log_trace("simpleSummarize %s", generateTopic(quantity).c_str());

    double sum = 0;
    for( const auto &it : _powerdevices ) //std::map< std::string, MetricList> it
    {
        double value = getMetricValue( it.second, quantity, it.first );
        if( std::isnan(value) ) {
            log_debug(ANSI_COLOR_LIGHTMAGENTA "%s@%s is NAN" ANSI_COLOR_RESET,
                quantity.c_str(), it.first.c_str());

            throw std::runtime_error(quantity + "@" + it.first + " is missing");
        }
        else {
            sum += value;
        }
    }

    MetricInfo result ( _name, quantity, "W", sum, ::time (NULL), TTL);
    return result;
}

MetricInfo TPUnit::
    realpowerDefault(const std::string &quantity) const
{
    std::string topic = generateTopic(quantity);
    log_trace("realpowerDefault %s", topic.c_str());

    double sum = 0;
    for( const auto it : _powerdevices ) //std::map< std::string, MetricList> it
    {
        double value = getMetricValue( it.second, quantity, it.first );
        if( std::isnan (value) ) {
            //ZZZ continue; // ignore NAN values

            // realpower.default not present, try to sum the phases
            log_debug(ANSI_COLOR_LIGHTMAGENTA "%s calculation: %s@%s is NAN" ANSI_COLOR_RESET,
                topic.c_str(), quantity.c_str(), it.first.c_str());

            for( int phase = 1 ; phase <= 3 ; ++phase )
            {
                const std::string quant = "realpower.output.L" + std::to_string( phase );
                value = getMetricValue( it.second, quant, it.first );
                if( std::isnan (value) ) {
                    log_debug(ANSI_COLOR_LIGHTMAGENTA "%s calculation: %s@%s is NAN" ANSI_COLOR_RESET,
                        topic.c_str(), quant.c_str(), it.first.c_str());

                    throw std::runtime_error(quant + "@" + it.first + " is missing");
                }
                sum += value;
            }
        }
        else {
            sum += value;
        }
    }

    MetricInfo result ( _name, quantity, "W", sum, ::time (NULL), TTL);
    return result;
}

MetricInfo TPUnit::
    realpowerOutput(const std::string &quantity) const
{
    std::string topic = generateTopic(quantity);
    log_trace("realpowerOutput %s", topic.c_str());

    double value = NAN;
    int phases = 0;

    for( const auto it : _powerdevices ) //std::map< std::string, MetricList> it
    {
        value = getMetricValue( it.second, quantity, it.first );

        double roL2 = getMetricValue (it.second, "realpower.output.L2", it.first);

        // detect a mix of single and three phase devices - return NAN for this case
        if (phases == 0) { // first time
            if (std::isnan (roL2))
                phases = 1; // 1-phase
            else
                phases = 3; // 3-phase

            log_debug("%s calculation: choose %d phases (%s@%s)",
                topic.c_str(), phases, quantity.c_str(), it.first.c_str());
        }
        else {
            if ((std::isnan (roL2) && (phases == 3)) ||
                (!std::isnan (roL2) && (phases == 1)))
            {
                log_debug(ANSI_COLOR_LIGHTMAGENTA "%s calculation: avoid mixed phases (%s@%s, phases: %d)" ANSI_COLOR_RESET,
                    topic.c_str(), quantity.c_str(), it.first.c_str(), phases);

                // return NAN instead of throw an exception?
                return MetricInfo ( _name, quantity, "W", NAN, ::time (NULL), TTL);
            }
        }
    }

    // Here value is NAN, or the latest getMetricValue()
    // ZZZ Is there a bug ?!

    MetricInfo result ( _name, quantity, "W", value, ::time (NULL), TTL);
    return result;
}

void TPUnit::
    calculate(const std::string &quantity)
{
    const std::string topic = generateTopic(quantity);
    log_trace(ANSI_COLOR_BOLD "%s calculate" ANSI_COLOR_RESET,
        topic.c_str());

    try {
        const auto it = _calculations.find(quantity);
        int calcMethod = (it != _calculations.cend()) ? it->second : TPOWER_REALPOWER_UNDEFINED;

        MetricInfo result;
        switch( calcMethod ) {
            case TPOWER_REALPOWER_DEFAULT:
                result = realpowerDefault( quantity );
                break;
            case TPOWER_REALPOWER_OUTPUT_L1:
            case TPOWER_REALPOWER_OUTPUT_L2:
            case TPOWER_REALPOWER_OUTPUT_L3:
                result = realpowerOutput ( quantity );
                break;
            case TPOWER_REALPOWER_UNDEFINED:
            default:
                result = simpleSummarize( quantity );
                break;
        }

        set( quantity, result );

        log_trace("%s calculate " ANSI_COLOR_BOLD "succeeded" ANSI_COLOR_RESET,
            topic.c_str());
    }
    catch (std::exception &e) {
        log_debug(ANSI_COLOR_RED "%s calculate failed on exception (%s)" ANSI_COLOR_RESET,
            topic.c_str(), e.what());
    }
    catch (...) {
        log_debug(ANSI_COLOR_RED "%s calculate failed" ANSI_COLOR_RESET,
            topic.c_str());
    }
}

void TPUnit::
    calculate(const std::vector<std::string> &quantities)
{
    dropOldMetricInfos();
    for( const auto it : quantities ) {
        calculate( it );
    }
}

double TPUnit::
    getMetricValue(
        const MetricList  &measurements,
        const std::string &quantity,
        const std::string &deviceName
    ) const
{
    std::string topic = quantity + "@" + deviceName;
    return measurements.find(topic);
}

// TODO setup max life time metric
void TPUnit::
    dropOldMetricInfos()
{
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
    return std::isnan(_lastValue.find(topic));
}

std::vector<std::string> TPUnit::
    devicesInUnknownState(const std::string &quantity) const
{
    std::vector<std::string> result;

    if ( quantityIsKnown( generateTopic(quantity) ) ) {
        return result; // empty vector
    }

    uint64_t now = ::time(NULL);
    for( const auto &device : _powerdevices ) {
        const auto &deviceMetrics = device.second; // TPUnit
        std::string topic = quantity + "@" + device.first;
        auto measurement = deviceMetrics.getMetricInfo(topic);
        if ( ( std::isnan (measurement.getValue()) ) ||
             ( (now - measurement.getTimestamp()) > (measurement.getTtl() * 2) )
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
    if( device != _powerdevices.end() ) //std::map< std::string, MetricList> device
        device->second.addMetricInfo (M);
}

bool TPUnit::
    changed(const std::string &quantity) const
{
    auto it = _changed.find(quantity);
    if( it == _changed.end() )
        return false;
    return it->second;
}

void TPUnit::
    changed(const std::string &quantity, bool newStatus)
{
    if( changed( quantity ) != newStatus )
    {
        _changed[quantity] = newStatus;
        _changetimestamp[quantity] = ::time(NULL);
        if( _advertisedtimestamp.find(quantity) == _advertisedtimestamp.end() ) {
            _advertisedtimestamp[quantity] = 0;
        }
    }
}

uint64_t TPUnit::
    timestamp( const std::string &quantity ) const
{
    auto it = _changetimestamp.find(quantity);
    if( it == _changetimestamp.end() )
        return 0;
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
    uint64_t dt = ::time(NULL) - quantityTimestamp;
    if ( dt > TPOWER_MEASUREMENT_REPEAT_AFTER ) {
        // no time left for waiting -> Need to advertise
        return 0;
    }
    // we should wait a little bit, before advertising
    return TPOWER_MEASUREMENT_REPEAT_AFTER - dt;
}

bool TPUnit::advertise( const std::string &quantity ) const
{
    if ( quantityIsUnknown(generateTopic(quantity)) ) {
        // if we don't know the quantity -> nothing to advertise
        return false;
    }

    uint64_t now_timestamp = ::time(NULL);
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
    return ( changed(quantity) || ( (now_timestamp - timestamp(quantity)) > TPOWER_MEASUREMENT_REPEAT_AFTER ) );
}

void TPUnit::
    advertised(const std::string &quantity)
{
    changed( quantity, false );
    int64_t now_timestamp = ::time(NULL);
    _changetimestamp[quantity] = now_timestamp;
    _advertisedtimestamp[quantity] = now_timestamp;
}

void tp_unit_test(bool verbose)
{
    printf (" * tp_unit: ");
    printf ("OK\n");
}
