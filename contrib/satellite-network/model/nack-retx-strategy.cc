/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2021,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nack-retx-strategy.h"
#include "common/global.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/algorithm.hpp"
#include "ns3/ndnSIM/NFD/daemon/common/logger.hpp"

namespace nfd {
namespace fw {

NFD_LOG_INIT(NackRetxStrategy);
NFD_REGISTER_STRATEGY(NackRetxStrategy);

const time::milliseconds NackRetxStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds NackRetxStrategy::RETX_SUPPRESSION_MAX(250);

NackRetxStrategy::NackRetxStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    NDN_THROW(std::invalid_argument("NackRetxStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "NackRetxStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
NackRetxStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/nack-retx").appendVersion(0);
  return strategyName;
}

void NackRetxStrategy::sendNackOrForward(const Interest& interest, const FaceEndpoint& faceEndpoint, const shared_ptr<pit::Entry>& pitEntry) {
  // Don't use forwarding hint when handling NACK
  Interest interestNoHint(interest);
  const_cast<Interest&>(interestNoHint).setForwardingHint({});
  const pit::Entry pitEntryNoHint(interestNoHint);
  const fib::Entry& fibEntry = this->lookupFib(pitEntryNoHint);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  // Check if there's any eligible next-hop
  it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
    return isNextHopEligible(faceEndpoint.face, interest, nexthop, pitEntry);
  });
  // The only hop is the incoming face, send nack
  if (it == nexthops.end()) {
    NFD_LOG_DEBUG(interest << " from=" << faceEndpoint << " noNextHop");
    std::cout << "nack," << interest.getName() << "," << ns3::Simulator::Now().GetMicroSeconds() << std::endl;
    // std::cout << "OG NACK SENT: " << interest << " from=" << ingress << " noNextHop" << std::endl;
    lp::NackHeader nackHeader;
    nackHeader.setReason(lp::NackReason::NO_ROUTE);
    this->sendNack(nackHeader, faceEndpoint.face, pitEntry);
    // pitEntry->insertOrUpdateInRecord(ingress.face, interest);
    // this->rejectPendingInterest(pitEntry);
    return;
  }

  // Found eligible face, simply forward
  Face& outFace = it->getFace();
  NFD_LOG_DEBUG(interest << " from=" << faceEndpoint << " newPitEntry-to=" << outFace.getId());
  this->sendInterest(interest, outFace, pitEntry);
  return;
}

void NackRetxStrategy::sendNackOrForward(const Interest& interest, const shared_ptr<pit::Entry>& pitEntry) {
  // Don't use forwarding hint when handling NACK
  Interest interestNoHint(interest);
  const_cast<Interest&>(interestNoHint).setForwardingHint({});
  const pit::Entry pitEntryNoHint(interestNoHint);
  const fib::Entry& fibEntry = this->lookupFib(pitEntryNoHint);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  // Check if there's any eligible next-hop
  // We don't know the ingress face (from the interest) that triggers nack,
  // so we look up the pit in records
  it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
    const Face& outFace = nexthop.getFace();
    // Return true if the new next hop does not match any in records of the PIT
    for (auto& inFace : pitEntry->getInRecords()) {
      if (outFace.getId() == inFace.getFace().getId())
        return false;
    }
    return true;
  });
  // If the next-hop is not found or is the ingress face, propagate the nack
  if (it == nexthops.end()) {
    // std::cout << "PROP NACK SENT: " << nack.getInterest().getName() << std::endl;
    NFD_LOG_DEBUG(interest << " noNextHop");
    while (pitEntry->hasInRecords()) {
      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(nackHeader, pitEntry->getInRecords().begin()->getFace(), pitEntry);
    }
    // this->rejectPendingInterest(pitEntry);
  } else {
    // Attempt to retransmit
    Face& outFace = it->getFace();
    // Re-insert the out record
    pitEntry->insertOrUpdateOutRecord(outFace, interest);
    std::cout << "retrans," << interest.getName().getSubName(-1) << "," << ns3::Simulator::Now().GetMicroSeconds() << std::endl;
    // std::cout << "RETRANS SENT: " << ingress.face.getId() << " -> " << outFace.getId() << " of " << interest.getInterestLifetime() << std::endl;
    NFD_LOG_DEBUG(interest << " retrans, newPitEntry-to=" << outFace.getId());
    // Reset the expiry timer
    this->setExpiryTimer(pitEntry, time::milliseconds(2000));
    this->sendInterest(interest, outFace, pitEntry);
  }
}

void
NackRetxStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " suppressed");
    return;
  }

  if (suppression == RetxSuppressionResult::NEW) {
    // forward to nexthop with lowest cost except downstream, or send nack if no eligible path
    // std::cout << "SENT" << std::endl;
    sendNackOrForward(interest, ingress, pitEntry);
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  // find an unused upstream with lowest cost except downstream
  it = std::find_if(nexthops.begin(), nexthops.end(),
                    [&, now = time::steady_clock::now()] (const auto& nexthop) {
                      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry, true, now);
                    });

  if (it != nexthops.end()) {
    Face& outFace = it->getFace();
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-unused-to=" << outFace.getId());
    return;
  }

  // find an eligible upstream that is used earliest
  it = findEligibleNextHopWithEarliestOutRecord(ingress.face, interest, nexthops, pitEntry);
  if (it == nexthops.end()) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmitNoNextHop");
  }
  else {
    Face& outFace = it->getFace();
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-retry-to=" << outFace.getId());
  }
}

void
NackRetxStrategy::afterReceiveNack(const lp::Nack& nack, const FaceEndpoint& ingress,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  std::cout << "NACK RECEIVED: " << nack.getInterest().getName() << std::endl;
  // Try to retransmit or backtrack to retransmit
  if (nack.getReason() == lp::NackReason::NO_ROUTE) {
    // We don't know which inFace triggers this nack because they are all aggregated
    sendNackOrForward(nack.getInterest(), pitEntry);
  }
}

} // namespace fw
} // namespace nfd
