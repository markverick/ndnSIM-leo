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

#include "ndn-net-device-transport.hpp"

#include "../helper/ndn-stack-helper.hpp"
#include "ndn-block-header.hpp"
#include "../utils/ndn-ns3-packet-tag.hpp"

#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/encoding/tlv.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/lp/packet.hpp>

#include "ns3/queue.h"

NS_LOG_COMPONENT_DEFINE("ndn.NetDeviceTransport");

namespace ns3 {
namespace ndn {

NetDeviceTransport::NetDeviceTransport(Ptr<Node> node,
                                       const Ptr<NetDevice>& netDevice,
                                       const std::string& localUri,
                                       const std::string& remoteUri,
                                       ::ndn::nfd::FaceScope scope,
                                       ::ndn::nfd::FacePersistency persistency,
                                       ::ndn::nfd::LinkType linkType)
  : m_netDevice(netDevice)
  , m_node(node)
{
  this->setLocalUri(FaceUri(localUri));
  this->setRemoteUri(FaceUri(remoteUri));
  this->setScope(scope);
  this->setPersistency(persistency);
  this->setLinkType(linkType);
  this->setMtu(m_netDevice->GetMtu()); // Use the MTU of the netDevice

  // Get send queue capacity for congestion marking
  PointerValue txQueueAttribute;
  if (m_netDevice->GetAttributeFailSafe("TxQueue", txQueueAttribute)) {
    Ptr<ns3::QueueBase> txQueue = txQueueAttribute.Get<ns3::QueueBase>();
    // must be put into bytes mode queue

    auto size = txQueue->GetMaxSize();
    if (size.GetUnit() == BYTES) {
      this->setSendQueueCapacity(size.GetValue());
    }
    else {
      // don't know the exact size in bytes, guessing based on "standard" packet size
      this->setSendQueueCapacity(size.GetValue() * 1500);
    }
  }

  NS_LOG_FUNCTION(this << "Creating an ndnSIM transport instance for netDevice with URI"
                  << this->getLocalUri());

  NS_ASSERT_MSG(m_netDevice != 0, "NetDeviceFace needs to be assigned a valid NetDevice");

  m_node->RegisterProtocolHandler(MakeCallback(&NetDeviceTransport::receiveFromNetDevice, this),
                                  L3Protocol::ETHERNET_FRAME_TYPE, m_netDevice,
                                  true /*promiscuous mode*/);
}

NetDeviceTransport::~NetDeviceTransport()
{
  NS_LOG_FUNCTION_NOARGS();
}

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

ssize_t
NetDeviceTransport::getSendQueueLength()
{
  PointerValue txQueueAttribute;
  if (m_netDevice->GetAttributeFailSafe("TxQueue", txQueueAttribute)) {
    Ptr<ns3::QueueBase> txQueue = txQueueAttribute.Get<ns3::QueueBase>();
    return txQueue->GetNBytes();
  }
  else {
    return nfd::face::QUEUE_UNSUPPORTED;
  }
}

void
NetDeviceTransport::doClose()
{
  NS_LOG_FUNCTION(this << "Closing transport for netDevice with URI"
                  << this->getLocalUri());

  // set the state of the transport to "CLOSED"
  this->setState(nfd::face::TransportState::CLOSED);
}

void
NetDeviceTransport::doSend(const Block& packet)
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
      if (m_next_interest_hop.find(prefix) != m_next_interest_hop.end()) {
        if (m_next_interest_hop[prefix] != netDevice->GetAddress()) {
          netDevice->Send(ns3Packet, m_next_interest_hop[prefix],
                          L3Protocol::ETHERNET_FRAME_TYPE);
        }
      }
    }
    else if (tlv_type == ::ndn::tlv::Data) {
      Data d(block);
      std::string prefix = d.getName().getPrefix(-1).toUri();
      if (m_next_data_hops.find(prefix) != m_next_data_hops.end()) {
        for (Address addr : m_next_data_hops[prefix]) {
          netDevice->Send(ns3Packet->Copy(), addr,
                        L3Protocol::ETHERNET_FRAME_TYPE);
        }
      }
    } else {
      std::cout << "UNKNOWN TLV TYPE: " << tlv_type << std::endl; 
    }
  }
}

// callback
void
NetDeviceTransport::receiveFromNetDevice(Ptr<NetDevice> device,
                                         Ptr<const ns3::Packet> p,
                                         uint16_t protocol,
                                         const Address& from, const Address& to,
                                         NetDevice::PacketType packetType)
{
  NS_LOG_FUNCTION(device << p << protocol << from << to << packetType);

  // Convert NS3 packet to NFD packet
  Ptr<ns3::Packet> packet = p->Copy();

  BlockHeader header;
  packet->RemoveHeader(header);

  this->receive(std::move(header.getBlock()));
}

void
NetDeviceTransport::SetNextInterestHop(std::string prefix, Address dest) {
  m_next_interest_hop[prefix] = dest;
}

void
NetDeviceTransport::SetNextDataHop(std::string prefix, Address dest) {
  std::set<Address> v;
  v.insert(dest);
  if (m_next_data_hops.find(prefix) != m_next_data_hops.end()) {
    m_next_data_hops[prefix].clear();
  }
  m_next_data_hops[prefix] = v;
}

void
NetDeviceTransport::AddNextDataHop(std::string prefix, Address dest) {
  if (m_next_data_hops.find(prefix) == m_next_data_hops.end()) {
    std::set<Address> v;
    v.insert(dest);
    m_next_data_hops[prefix] = v;
  } else {
    m_next_data_hops[prefix].insert(dest);
  }
}

void
NetDeviceTransport::RemoveNextDataHop(std::string prefix, Address dest) {
  if (m_next_data_hops.find(prefix) != m_next_data_hops.end()) {
    auto it = m_next_data_hops[prefix].find(dest);
    if (it != m_next_data_hops[prefix].end()) {
      m_next_data_hops[prefix].erase(it);
    }
  }
}

void
NetDeviceTransport::ClearNextDataHop(std::string prefix, Address dest) {
  if (m_next_data_hops.find(prefix) != m_next_data_hops.end()) {
      m_next_data_hops[prefix].clear();
  }
}

Ptr<NetDevice>
NetDeviceTransport::GetNetDevice() const
{
  return m_netDevice;
}

} // namespace ndn
} // namespace ns3
