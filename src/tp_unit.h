/*
 * Copyright (C) 2014 Eaton
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
 */

/*! \file   tp_unit.h
    \brief  Object representing one rack or DC
    \author Tomas Halman <TomasHalman@Eaton.com>
*/

#ifndef TP_UNIT_H_INCLUDED
#define TP_UNIT_H_INCLUDED

#include <map>
#include <string>
#include <vector>
#include <ctime>
#include <functional>

#include "metriclist.h"

//! \brief class representing total power calculation unit (rack or DC)
class TPUnit {
 public:

    //\! \brief calculate total value for all interesting quantities
    void calculate(const std::vector<std::string> &quantities);

    //\! \brief get value of particular quantity. Method throws an exception if quantity is unknown.
    double get( const std::string &quantity) const;

    //\! \brief set value of particular quantity.
    // TODO const &
    void set(const std::string &quantity, MetricInfo &measurement);

    //\! \brief Metric Info per articular quantity.
    MetricInfo getMetricInfo(const std::string &quantity) const;


    //\! \brief get set unit name
    std::string name() const { return _name; };
    void name(const std::string &name) { _name = name; };
    void name(const char *name) { _name =  name ? name : ""; };

    //\! \brief returns true if at least one measurement of all included powerdevices is unknown
    bool quantityIsUnknown( const std::string &quantity ) const;
    //\! \brief returns true if totalpower can be calculated.
    bool quantityIsKnown( const std::string &quantity ) const {
        return ! quantityIsUnknown(quantity);
    }

    //\! \brief returns list of devices in unknown state
    std::vector<std::string> devicesInUnknownState(const std::string &quantity) const;

    //\! \brief add powerdevice to unit
    void addPowerDevice(const std::string &device);

    //\! \brief save new received measurement
    void setMeasurement(const MetricInfo &M);

    //! \brief returns true if measurement is changend and we should advertised
    bool changed(const std::string &quantity) const;

    //! \brief set/clear changed status
    void changed(const std::string &quantity, bool newStatus);

    //! \brief returns true if measurement should be send (changed is true or we did not send it for long time)
    bool advertise( const std::string &quantity ) const;

    //! \brief set timestamp of the last publishing moment
    void advertised( const std::string &quantity );

    //! \brief time to next advertisement [s]
    int64_t timeToAdvertisement( const std::string &quantity ) const;

    //! \brief return timestamp for quantity change
    uint64_t timestamp( const std::string &quantity ) const;
 protected:
    //! \brief A list of the last measurement values:  topic -> MetricInfo
    MetricList _lastValue;

    //! \brief measurement status
    std::map < std::string, bool > _changed;

    //! \brief measurement change timestamp
    std::map < std::string, uint64_t> _changetimestamp;

    //! \brief measurement advertisement timestamp
    std::map < std::string, uint64_t> _advertisedtimestamp;

    /*! \brief list of measurements for included devices
     *
     *     map---device1---map---realpower.default---MetricInfo
     *      |               +----realpower.input.L1--MetricInfo
     *      |               +----realpower.input.L2--MetricInfo
     *      |               +----realpower.input.L3--MetricInfo
     *      +----device2-...
     */
    std::map< std::string, MetricList> _powerdevices;

    //! \brief unit name
    std::string _name;

    //! \brief replace not present measurement with another
    static const std::map<std::string,std::string> _emergencyReplacements;

    //! \brief replace not present measurement with some algorithm
    static const std::map<std::string,int> _calculations;

    double getMetricValue(
        const MetricList  &measurements,
        const std::string &quantity,
        const std::string &deviceName
    ) const;

    //\! \brief calculate total value for one quantity
    void calculate(const std::string &quantity);
    //\! \brief discard obsolete measurements
    void dropOldMetricInfos();
    //\! \brief calculate simple sum over devices considering the replacement table
    MetricInfo simpleSummarize(const std::string &quantity) const;
    //\! \brief calculate realpower sum over devices
    MetricInfo realpowerDefault(const std::string &quantity) const;
    //\! send realpower output or null in case of phase incompatibilities
    MetricInfo realpowerOutput(const std::string &quantity) const;
private:
    std::string generateTopic (const std::string &quantity) const;

    // time to live of the generated metrics [s]
    static const uint64_t TTL = 6*60;
};
void tp_unit_test(bool verbose);

#endif // TP_UNIT_H_INCLUDED
