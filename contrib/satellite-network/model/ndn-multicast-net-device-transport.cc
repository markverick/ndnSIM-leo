/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-multicast-net-device-transport.h"
#include "ns3/ndnSIM/model/ndn-net-device-transport.hpp"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/model/ndn-block-header.hpp"
#include "ns3/ndnSIM/utils/ndn-ns3-packet-tag.hpp"

#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>

#include "ns3/queue.h"

NS_LOG_COMPONENT_DEFINE("ndn.MulticastNetDeviceTransport");

namespace ns3 {
namespace ndn {

void
MulticastNetDeviceTransport::doSend(const Block& packet)
{
  NS_LOG_FUNCTION(this << "Sending packet from netDevice with URI"
                  << this->getLocalUri());

  // convert NFD packet to NS3 packet
  BlockHeader header(packet);

  Ptr<ns3::Packet> ns3Packet = Create<ns3::Packet>();
  ns3Packet->AddHeader(header);

  // send the NS3 packet
  // Use multicast hack (GSL is not of a multicast type, but we
  // made them do). We cannot determine
  // the net device type since the Hypatia netdevice
  // is compiled after (external ns-3 module)
  auto netDevice = GetNetDevice();
  if (netDevice->IsMulticast()) {
    netDevice->Send(ns3Packet, netDevice->GetBroadcast(),
                      L3Protocol::ETHERNET_FRAME_TYPE);
  } else {
    for (auto address : m_broadcastAddresses) {
      // std::cout << address << std::endl;
      Ptr<ns3::Packet> p = ns3Packet->Copy();
      netDevice->Send(p, address,
                        L3Protocol::ETHERNET_FRAME_TYPE);
    }
  }
}

void
MulticastNetDeviceTransport::AddBroadcastAddress(Address address) {
  m_broadcastAddresses.insert(address);
}

void
MulticastNetDeviceTransport::RemoveBroadcastAddress(Address address) {
  if (m_broadcastAddresses.find(address) != m_broadcastAddresses.end()) {
    m_broadcastAddresses.erase(address);
  }
}

} // namespace ndn
} // namespace ns3
