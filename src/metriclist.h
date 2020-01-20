/*
Copyright (C) 2014 - 2017 Eaton

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

/*! \file   metriclist.h
 *  \author Alena Chernikava <AlenaChernikava@Eaton.com>
 *  \brief  This class is intended to handle set of current known metrics
 */
#ifndef SRC_METRICLIST_H
#define SRC_METRICLIST_H

#include <string>
#include <map>

#include "metricinfo.h"

/*
 * \brief This class is intended to handle set of current known metrics.
 *
 * You can create it, add new metrics, find known metrics by topic,
 * and remove metrics that are not valid.
 */

class MetricList {
public:

    /*
     * \brief Constructs the empty list
     */
    MetricList() {};

    /*
     * \brief Destroys the list
     */
    ~MetricList() {};

    /*
     * \brief Adds new metric
     *
     * This will add new metric if it isn't known to the list
     * and update the value if it is known already.
     *
     * Also it will update value of last added Metric.
     *
     * \param[in] metricInfo - metric to add
     */
    void addMetricInfo (const MetricInfo &metricInfo);

    /*
     * \brief Gets metric by the topic
     *
     * \param[in] topic - topic we are looking for
     *
     * \return MetricInfo       - if metric was found or
     *         MetricInfo empty - if metric isn't found
     *                            ( isUnknown() is true)
     */
    MetricInfo getMetricInfo (
        const std::string &topic) const;

    /*
     * \brief Finds a value of the metric in the list
     *
     * To check if value is NAN or not use isnan() function from math.h
     *
     * \param[in] topic - topic we are looking for
     *
     * \return NAN   - if metric is not present in the list
     *         value - otherwise
     */
    double find (const std::string &topic) const;

    /*
     * \brief Removes old metrics from the list (related to ttl of metrics)
     */
    void removeOldMetrics (void);

private:

    // Metric list <topic, MetricInfo>
    std::map <std::string, MetricInfo> _knownMetrics;
};

void
metriclist_test (bool verbose);

#endif // SRC_METRICLIST_H_
