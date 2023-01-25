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
#include <ndn-cxx/encoding/tlv.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/lp/packet.hpp>

#include "ns3/queue.h"

NS_LOG_COMPONENT_DEFINE("ndn.MulticastNetDeviceTransport");

namespace ns3 {
namespace ndn {

Block stripBlockHeader(BlockHeader header) {
  namespace tlv = ::ndn::tlv;
  namespace lp = ::ndn::lp;
  ::ndn::Buffer::const_iterator first, last;
  lp::Packet p(header.getBlock());
  std::tie(first, last) = p.get<lp::FragmentField>(0);
  try {
    Block fragmentBlock(::ndn::make_span(&*first, std::distance(first, last)));
    return fragmentBlock;
  }
  catch (const tlv::Error& error) {
    std::cout << "Non-TLV bytes (size: " << std::distance(first, last) << ")";
    return header.getBlock();
  }
}
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
    Block block = stripBlockHeader(header);
    uint32_t tlv_type = block.type();
    if (tlv_type == ::ndn::tlv::Interest) {
      Interest i(block);
      // Removing appended sequence number 
      std::string prefix = i.getName().getPrefix(-1).toUri();
      // std::cout << "Interest Prefix: "<< prefix << std::endl;
      // for (auto it = m_next_interest_hop.begin(); it != m_next_interest_hop.end(); it++) {
      //   std::cout << "  From prefix: " << it->first << std::endl;
      // }
      if (m_next_interest_hop.find(prefix) != m_next_interest_hop.end()) {
        netDevice->Send(ns3Packet, m_next_interest_hop[prefix],
                        L3Protocol::ETHERNET_FRAME_TYPE);
      }
    }
    else if (tlv_type == ::ndn::tlv::Data) {
      Data d(block);
      std::string prefix = d.getName().getPrefix(-1).toUri();
      // std::cout << "Data Prefix: "<< prefix << std::endl;
      if (m_next_data_hop.find(prefix) != m_next_data_hop.end()) {
        netDevice->Send(ns3Packet, m_next_data_hop[prefix],
                        L3Protocol::ETHERNET_FRAME_TYPE);
      }
    } else {
      std::cout << "UNKNOWN TLV TYPE: " << tlv_type << std::endl; 
    }
    // for (auto address : m_broadcastAddresses) {
    //   // std::cout << address << std::endl;
    //   Ptr<ns3::Packet> p = ns3Packet->Copy();
    //   netDevice->Send(p, address,
    //                     L3Protocol::ETHERNET_FRAME_TYPE);
    // }
  }
}

void
MulticastNetDeviceTransport::SetNextInterestHop(std::string prefix, Address dest) {
  m_next_interest_hop[prefix] = dest;
}

void
MulticastNetDeviceTransport::SetNextDataHop(std::string prefix, Address dest) {
  m_next_data_hop[prefix] = dest;
}

} // namespace ndn
} // namespace ns3
