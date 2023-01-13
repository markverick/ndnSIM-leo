The Network Simulator of LEO satellite with NDN stack
================================

## Overview
The goal of is to simulate of the NDN traffic on LEO satelliete with accurate delays and mobility. The core simulation is based on the modified ndnSIM version of ns-3 (https://github.com/named-data-ndnSIM/ns-3-dev & https://github.com/named-data-ndnSIM/ndnSIM), and the satellite topology, net devices, and channels are taken from Hypatia (https://github.com/snkas/hypatia). However, putting ndnSIM's and Hypatia's component together does not simply work because:
1) Both act as modules of different version of ndnSIM. A number of patches are needed for the incompatability.
2) Default NDN Stack (NFD, ndn-cxx, and additional ns-3 functionality) of ndnSIM do not support the nature of satellite topology.

Our approach is to identify the common ground, extract the reusable pieces of the codes, and implement the bridge between the two.

## Breif Specification of the Topology
1) There are two types of nodes: Satellite and Ground Station.
2) There are two types of links: GSL (Ground Station <-> Satellite) and ISL (Satellite <-> Satellite).
3) GSL link is assumed to be broadcast due to the poor performance of laser from LEO orbit to the ground.
4) ISL link is assumed to be point to point laser.
5) Each Satellite node has five net devices: one GSL and four ISLs. This is a default value that can be changed.
6) Each Ground Station has one net device: one GSL
7) Each ISL channel connects between two Satellite's ISL net devices
8) There is only one GSL channel that connects all Satellite's GSL net devices and Ground Station's GSL net devices.
9) Mobility model is calculated by the distance of the current position and the speed of light for both GSL and ISL.

## NDN Stack and Faces
1) Instead of IP Interface, NDN has Faces
2) Each Face is composed of two components: upper-level Link Service and lower-level Transport
3) Link Service (in this case, generic-link-service) takes care of congestion control, packet encoding, managing transmission queue, etc.
4) Transport (in this case, ndn-multicast-net-device-transport) brings the packet to the other end of the net device. Transport is created from a net device
5) The forwarding component (e.g. FIB table) forwards the name to the Face. The Transport layer of that Face will send the packet to the assigned net device.
6) For point-to-point ISL channel, implementation of a custom Transport is not needed since ndnSIM's FibHelper already supports that. For GSL channel, the implementation can be tricky because unlike IP, Transport only knows the "name". Sending Interest forward and receiving Data back complicates the decision of the Transport. 

## Modifications/Adoptions of Hypatia's code:
1) Remove the IP-related components (e.g. Arbiter, ARP Cache, and other IP congestion controls). `basic-sim` module is still used for utility functions (e.g. exp-util.cc).
2) Fully adopt GSL and ISL channels.
3) Need a small patch for GSL and ISL net devices such as adding NDN protocol number.
4) Adopt most of the import functions, including forwarding state and topology data.

## Changes to the base ns-3 code
1) Install and fix missing dependencies caused by different version of external modules and ns-3.
2) Implement a custom Transport to support GSL multicast.
3) Implement a custom NDN StackHelper class to support different types of net devices and channels.
4) Apply the Hypatia's mobility model to the NDN Stack for calculating delays.
5) Extends some of the NFD and ndnSIM components to add functionalities for satellite topology.
6) Internal modules are never touched except for compatability issues.




