The Network Simulator of the LEO satellite with NDN stack
================================
## Overview
The goal is to simulate the NDN traffic on LEO satellites with accurate delays and mobility. Core simulation is based on a modified ndnSIM version of ns-3 (https://github.com/named-data-ndnSIM/ns-3-dev & https://github.com/named-data-ndnSIM/ndnSIM). Satellite topologies, net devices, and channels are currently taken from Hypatia (https://github.com/snkas/hypatia). However, putting ndnSIM's and Hypatia's components together needs a bit more work because:

1) Both act as modules of different versions of ndnSIM. Several patches are needed for compatibility.
2) Current ndnSIM implementation on top of NFD and ndn-cxx do not support the multicast nature of satellite topology.

## Modifications/Adoptions of Hypatia's code:
1) Replace routing coomponents (e.g., Arbiter, ARP Cache, and other IP congestion controls) with custom NDN forwarding strategy. Basic-sim module is still used for utility functions (e.g., exp-util.cc).
2) Fully adopt GSL and ISL channels.
3) Require a small patch for GSL and ISL net devices, such as adding the NDN protocol number.
4) Adopt most of the import functions, including forwarding state and topology data.

## Changes to the base ns-3 code
1) Install and fix missing dependencies caused by different versions of external modules and ns-3.
2) Implement a custom ns-3's Transport to support GSL multicast.
3) Implement a custom NDN StackHelper class to support different types of net devices and channels.
4) Apply Hypatia's mobility model to the NDN Stack for calculating delays.
5) Extends some of the NFD and ndnSIM components to add functionalities for satellite topology.
6) Internal modules are modified for compatibility issues.

## Installation
```
git submodule update --recursive --init
./waf configure --enable-mpi
```

## Running
```
cd ns3-dev-leo/experiments/a_b
python step_1_generate_runs.py 
python step_2_run.py
```
## Modifying run list
The run list is located at `experiments/a_b/run_list.py`. Due to the recent major change to the code base, only a_b scenario is currently provided, meaning that the multiple consumers scenarios are temporary disabled.
You can change the following:
1) Dynamic states and GSL generation algorithm
2) NDN scenarios (Consumer/Producer nodes, Ping, Window, Multiple Consumers)
3) Set the loss rate, link bandwidth and packet queue size
4) Set the simulation time

## NDN clients
Additional parameters such as frequency, window size, and consumers/producer pairs can be adjusted in the corresponding file

1) Ping - `experiments/scenarios/runs/a_b_ping.cc`: one client sends `Frequency` interest per second with no retransmission timer. Segment number is unique, and is incrementing by one per interest transmission. Use vanilla `best-route` forwarding strategy and modified `ConsumerPing' application (disabled retransmission)
2) PingInstantRetx - `experiments/scenarios/runs/a_b_ping_instant_retx.cc`:  one client sends `Frequency` interest per second with no retransmission timer. The client will do instant retransmission on every pending interest when it detects a satellite handover. Use vanilla `best-route` forwarding strategy and modified `ConsumerPing' application (disable retransmission timer)
3) PingNackRetx - `experiments/scenarios/runs/a_b_ping_nack_retx.cc`:  one client sends `Frequency` interest per second with no retransmission timer. The cleint will do instant retransmission after it detects a satellite handover. Use custom `nack-retx` forwarding strategy and modified `ConsumerPing' application (disable retransmission timer). The satellites in the network will react to NACK to find the alternative path.
4) FixedWindow - `experiments/scenarios/runs/a_b_fixed_window.cc`: one client sends interests, allowing fixed-window pending interests. Use default rtt estimator for retransmission timer. Use custom `ConsumerFixedWindow` client
5) FixedWindowRetx - `experiments/scenarios/runs/a_b_fixed_window_retx.cc`: just like Fixed window but with `nack-retx` strategy.

## TODO:
- Reimplementing the forwarding hint mechanism
- Add additional `ConsumerPingNoRetx` client instead of modifying the existing `ConsumerPing` client
- Improve the forwarding strategy to allow more cases of synchronous multi-path forwarding
- Implement and evaluate proposed hop-by-hop congestion control schemes

## Tools
### [ndnSIM](https://ndnsim.net/current/)
ndnSIM is an NDN simulation framework that serves as a foundation to implement satellite network specifics. A few patches are needed for compability.
Copyright © 2011-2023 University of California, Los Angeles
ndnSIM is licensed under conditions of GNU General Public License version 3.0 or later with permission to be linked with NS-3 codebase (GPL 2.0).
ndnSIM uses a number of third-party software and libraries, licensed under the following licenses:
- The Boost libraries are licensed under the Boost Software License 1.0
- any-lite by Martin Moene is licensed under the Boost Software License 1.0
- optional-lite by Martin Moene is licensed under the Boost Software License 1.0
- variant-lite by Martin Moene is licensed under the Boost Software License 1.0
- SQLite is in the public domain
- The waf build system is licensed under the terms of the BSD license
- NDN Forwarding Daemon (NFD) licensed under conditions of GNU GPL 3.0+
ndnSIM also relies on several other third-party libraries with non-GPL compatible license. These library fall into category of “System Libraries” under GPL license definitions and are used in accordance with GPL license exception for “System Libraries”:
The GPL license is provided below in this file. For more information about these licenses, see https://www.gnu.org/licenses/
### [Hypatia](https://github.com/snkas/hypatia)
Hypatia provides useful libraries and ns-3's network models to contribute to our simulator.

Hypatia uses a number of third-party software and libraries, licensed under the following licenses:
- ns3-sat-sim/ is licensed under the GNU GPL version 2 license.
- satgenpy/ is licensed under the MIT license.
- satviz/ is license under the MIT license.
- paper/ is licensed under the MIT license.

## Acknowledgements
<img src="cisco-logo-transparent.png" height="150">

This material is based upon work supported by Cisco and the National Science Foundation. Any opinions, findings, and conclusions or recommendations expressed in this material are those of the author(s) and do not necessarily reflect the views of the sponsors.
