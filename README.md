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
The run list is located at `experiments/a_b/run_list.py`. You can change the following:
1) Dynamic states and GSL generation algorithm
2) NDN scenarios (Consumer/Producer nodes, Ping, Window, Multiple Consumers)
3) Set the loss rate, bandwidth and queue size
4) Set the simulation time

## NDN clients

1) Ping - `experiments/scenarios/runs/a_b_ping.cc`: one client sends `Frequency` interest per second with no retransmission timer. Use vanilla `best-route` forwarding strategy and modified `ConsumerPing' application (disable retransmission)
2) PingInstantRetx - `experiments/scenarios/runs/a_b_ping_instant_retx.cc`:  one client sends `Frequency` interest per second with no retransmission timer. The cleint will do instant retransmission after it detects a satellite handover. Use vanilla `best-route` forwarding strategy and modified `ConsumerPing' application (disable retransmission timer)
3) PingNackRetx - `experiments/scenarios/runs/a_b_ping_nack_retx.cc`:  one client sends `Frequency` interest per second with no retransmission timer. The cleint will do instant retransmission after it detects a satellite handover. Use custom `nack-retx` forwarding strategy and modified `ConsumerPing' application (disable retransmission timer). The satellites in the network will react to NACK to find the alternative path.
4) FixedWindow - `experiments/scenarios/runs/a_b_fixed_window.cc`: one client sends interests, allowing fixed-window pending interests. Use default rtt estimator for retransmission timer. Use custom `ConsumerFixedWindow` client
5) FixedWindowRetx - `experiments/scenarios/runs/a_b_fixed_window_retx.cc`: just like Fixed window but with `nack-retx` strategy.

