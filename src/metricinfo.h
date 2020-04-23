/*  =========================================================================
    metricinfo - Measurement

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
/*! \file   metricinfo.h
 *  \author Alena Chernikava <AlenaChernikava@Eaton.com>
 *  \brief  This class is intended to handle one metric
 */

#ifndef SRC_METRICINFO_H_
#define SRC_METRICINFO_H_

#include <string>
#include <ctime>

class MetricInfo {

public:
    MetricInfo()
    :
        _value{0},
        _timestamp{0},
        _ttl{5 * 60}
    {};

    MetricInfo (
        const std::string &element_name,
        const std::string &source,
        const std::string &units,
        double value,
        uint64_t timestamp,
        uint64_t ttl
        ):
        _element_name (element_name),
        _source (source),
        _units (units),
        _value (value),
        _timestamp (timestamp),
        _ttl (ttl)
    {};

    std::string getElementName (void) const{
        return _element_name;
    };

    std::string getSource (void) const {
        return _source;
    };

    std::string getUnits (void) const {
        return _units;
    };

    double getValue (void) const{
        return _value;
    };

    uint64_t getTimestamp (void) const {
        return _timestamp;
    };

    uint64_t getTtl(void) const {
        return _ttl;
    };

    std::string generateTopic (void) const {
        return _source + "@" + _element_name;
    };

    bool isUnknown (void) const {
        if ( _element_name.empty() ||
             _source.empty() ||
             _units.empty() ) {
            return true;
        }
        return false;
    };

    void setUnits (const std::string &units) { _units = units; };

    void setTime (void) { _timestamp = std::time(NULL); }; // timetamp = now

    friend inline bool operator==( const MetricInfo &lhs, const MetricInfo &rhs );
    friend inline bool operator!=( const MetricInfo &lhs, const MetricInfo &rhs );

    // This class is very close to metric info, so let it use fields directly
    friend class MetricList;

private:
    std::string _element_name; // 'epdu-42'
    std::string _source; // 'realpower.input.L3'  (as fty_proto_t METRIC type, or quantity)
    std::string _units;
    double      _value;
    uint64_t    _timestamp; // [s]
    uint64_t    _ttl; // time to live [s]

    std::string _element_destination_name;
};

inline bool operator==( const MetricInfo &lhs, const MetricInfo &rhs ) {
    return ( lhs._units == rhs._units &&
             lhs._value == rhs._value
           );
}

inline bool operator!=( const MetricInfo &lhs, const MetricInfo &rhs ) {
    return ! ( lhs == rhs );
}

void
metricinfo_test (bool verbose);

#endif // SRC_METRICINFO_H_
