The Network Simulator of the LEO satellite with NDN stack
================================
## Overview
The goal is to simulate the NDN traffic on LEO satellites with accurate delays and mobility. Core simulation is based on a modified ndnSIM version of ns-3 (https://github.com/named-data-ndnSIM/ns-3-dev & https://github.com/named-data-ndnSIM/ndnSIM). Satellite topologies, net devices, and channels are taken from Hypatia (https://github.com/snkas/hypatia). However, putting ndnSIM's and Hypatia's components together needs a bit more work because:

1) Both act as modules of different versions of ndnSIM. Several patches are needed for compatibility.
2) The ndnSIM implementation of NFD and ndn-cxx do not support the nature of satellite topology.
Our approach is to identify a common ground, extract reusable pieces of the codes, and implement a bridge between the two.

## Brief Specification of the Topology
1) There are two types of nodes: Satellite and Ground Station.
2) There are two types of links: GSL (Ground Station <-> Satellite) and ISL (Satellite <-> Satellite).
3) GSL link is assumed to be broadcast due to poor performance of lasers from the LEO orbit to the ground.
4) ISL link is assumed to be a point-to-point laser.
5) Each Satellite node has five net devices: one GSL and four ISLs. This is a default value that can be changed.
6) Each Ground Station has one net device: one GSL
7) Each ISL channel connects two Satellite's ISL net devices.
8) Only one GSL channel connects all Satellite's GSL net devices and Ground Station's GSL net devices.
9) The mobility model calculates delays from a distance between the positions of two nodes and the speed of light, regardless of channel types.

## NDN Stack and Faces
1) Instead of IP Interface, NDN has Faces.
2) Each Face has precisely two components: upper-level Link Service and lower-level Transport.
3) Link Service (in this case, generic-link-service) handles congestion control, packet encoding, managing transmission queue, etc.
4) Transport (in this case, ndn-multicast-net-device-transport) brings a packet to another end of the net device through a channel.
5) The forwarding component (e.g., FIB table) forwards the name to the Face. The Transport layer of that Face decides where the packet goes.
6) Implementing a custom Transport is unnecessary for ISL channels since ndnSIM's FibHelper helps connect two nodes on a point-to-point channel. For the GSL channel, the implementation can be tricky because, unlike IP, Transport only knows the "name." Sending Interest forward and receiving Data back complicates the decision of the Transport. Our solution is using Ad-hoc mode with a data structure that stores the addresses of the multicast members. Ad-Hoc mode allows the packet to be sent to the same face, which may take it to another destination.

## Modifications/Adoptions of Hypatia's code:
1) Remove the IP-related components (e.g., Arbiter, ARP Cache, and other IP congestion controls). Basic-sim module is still used for utility functions (e.g., exp-util.cc).
2) Fully adopt GSL and ISL channels.
3) Require a small patch for GSL and ISL net devices, such as adding the NDN protocol number.
4) Adopt most of the import functions, including forwarding state and topology data.

## Changes to the base ns-3 code
1) Install and fix missing dependencies caused by different versions of external modules and ns-3.
2) Implement a custom Transport to support GSL multicast.
3) Implement a custom NDN StackHelper class to support different types of net devices and channels.
4) Apply Hypatia's mobility model to the NDN Stack for calculating delays.
5) Extends some of the NFD and ndnSIM components to add functionalities for satellite topology.
6) Internal modules are only modified for compatibility issues.

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

