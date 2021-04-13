# fty-metric-tpower

Agent fty-metric-tpower computes power metrics for racks and DCs.

## How to build

To build fty-metric-tpower project run:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=usr -DBUILD_TESTING=On ..
make
make check # to run self-test
sudo make install
```

## How to run

To run fty-metric-tpower project:

* from within the source tree, run:

```bash
./src/fty-metric-tpower
```

For the other options available, refer to the manual page of fty-metric-store

* from an installed base, using systemd, run:

```bash
systemctl start fty-metric-tpower
```

### Configuration file

Configuration file - fty-metric-tpower.cfg - is currently ignored.

Agent reads environment variable BIOS\_LOG\_LEVEL to set verbosity level.

## Architecture

### Overview

fty-metric-tpower has 1 actor:

* fty-metric-tpower-server: main actor

It also has one built-in timer, which runs each minute, sends metrics for racks/DCS
which request it and reloads the topology if reconfig was pending.

## Protocols

### Published metrics

Agent publishes power metrics for racks and DCs:

```bash
stream=METRICS
sender=agent-tpower
subject=realpower.default@rack-51
D: 18-01-19 13:30:18 FTY_PROTO_METRIC:
D: 18-01-19 13:30:18     aux=
D: 18-01-19 13:30:18     time=1516368615
D: 18-01-19 13:30:18     ttl=360
D: 18-01-19 13:30:18     type='realpower.default'
D: 18-01-19 13:30:18     name='rack-51'
D: 18-01-19 13:30:18     value='885.000000'
D: 18-01-19 13:30:18     unit='W'
```

```bash
stream=METRICS
sender=agent-tpower
subject=realpower.output.L1@datacenter-3
D: 18-01-19 13:30:18 FTY_PROTO_METRIC:
D: 18-01-19 13:30:18     aux=
D: 18-01-19 13:30:18     time=1516368617
D: 18-01-19 13:30:18     ttl=360
D: 18-01-19 13:30:18     type='realpower.output.L1'
D: 18-01-19 13:30:18     name='datacenter-3'
D: 18-01-19 13:30:18     value='1700.000000'
D: 18-01-19 13:30:18     unit='W'
```

### Published alerts

Agent doesn't publish any alerts.

### Mailbox requests

Agent doesn't receive any mailbox requests.

### Stream subscriptions

# METRICS stream

If the metric is not realpower metric, ignore it.

Otherwise, check whether the metric is relevant for any known rack/DC,
recompute its power metrics and publish them if asked to do so.

# ASSETS stream

On any ASSET message, postpone the reconfig if we are currently in the middle of one
and set the timer to the next metric publish request.
