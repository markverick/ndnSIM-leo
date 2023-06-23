/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
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

#include "ndn-consumer-ping-instant-retx.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerPingInstantRetx");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerPingInstantRetx);

TypeId
ConsumerPingInstantRetx::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerPingInstantRetx")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerPingInstantRetx>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&ConsumerPingInstantRetx::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"),
                    MakeStringAccessor(&ConsumerPingInstantRetx::SetRandomize, &ConsumerPingInstantRetx::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&ConsumerPingInstantRetx::m_seqMax), MakeIntegerChecker<uint32_t>())

    ;

  return tid;
}

ConsumerPingInstantRetx::ConsumerPingInstantRetx()
  : m_frequency(1.0)
  , m_firstTime(true)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

ConsumerPingInstantRetx::~ConsumerPingInstantRetx()
{
}

void
ConsumerPingInstantRetx::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Consumer::SendPacket, this);
    m_firstTime = false;
  }
  else if (!m_sendEvent.IsRunning())
    m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
                                                      : Seconds(m_random->GetValue()),
                                      &Consumer::SendPacket, this);
}

// void
// ConsumerPingInstantRetx::ScheduleNextPacket()
// {
//   // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
//   // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

//   if (m_firstTime) {
//     m_sendEvent = Simulator::Schedule(Seconds(0.0), &Consumer::SendPacket, this);
//     m_firstTime = false;
//   } else if (m_retxSeqs.size()) {
//     // if (!m_sendEvent.IsRunning()) {
//     //   Simulator::Remove(m_sendEvent);
//     // }
//     // std::cout << "Found retx event!" << std::endl;
//     m_sendEvent = Simulator::ScheduleNow(&Consumer::SendPacket, this);
//   }
//   else if (!m_sendEvent.IsRunning())
//     m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
//                                                       : Seconds(m_random->GetValue()),
//                                       &Consumer::SendPacket, this);
// }

void
ConsumerPingInstantRetx::SetRandomize(const std::string& value)
{
  if (value == "uniform") {
    m_random = CreateObject<UniformRandomVariable>();
    m_random->SetAttribute("Min", DoubleValue(0.0));
    m_random->SetAttribute("Max", DoubleValue(2 * 1.0 / m_frequency));
  }
  else if (value == "exponential") {
    m_random = CreateObject<ExponentialRandomVariable>();
    m_random->SetAttribute("Mean", DoubleValue(1.0 / m_frequency));
    m_random->SetAttribute("Bound", DoubleValue(50 * 1.0 / m_frequency));
  }
  else
    m_random = 0;

  m_randomType = value;
}

// void
// Consumer::OnTimeout(uint32_t sequenceNumber)
// {
//   NS_LOG_FUNCTION(sequenceNumber);
//   // std::cout << Simulator::Now () << ", TO: " << sequenceNumber << ", current RTO: " <<
//   // m_rtt->RetransmitTimeout ().ToDouble (Time::S) << "s\n";

//   m_rtt->IncreaseMultiplier(); // Double the next RTO
//   m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
//                  1); // make sure to disable RTT calculation for this sample
//   m_retxSeqs.insert(sequenceNumber);
//   ScheduleNextPacket();
// }

void
ConsumerPingInstantRetx::CheckRetxTimeout()
{
  Time now = Simulator::Now();

  Time rto = Seconds(1000000);
}

// void
// ConsumerPingInstantRetx::ForceTimeout()
// {
//   while (!m_seqTimeouts.empty()) {
//   SeqTimeoutsContainer::index<i_timestamp>::type::iterator entry =
//     m_seqTimeouts.get<i_timestamp>().begin();
//   if (entry->time + rto <= now) // timeout expired?
//   {
//     uint32_t seqNo = entry->seq;
//     m_seqTimeouts.get<i_timestamp>().erase(entry);
//     OnTimeout(seqNo);
//   }
//   else
//     break; // nothing else to do. All later packets need not be retransmitted
// }
// }

std::string
ConsumerPingInstantRetx::GetRandomize() const
{
  return m_randomType;
}

} // namespace ndn
} // namespace ns3
