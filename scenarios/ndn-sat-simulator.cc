// ndn-sat-simulator.cc
#include <utility>
#include <filesystem>
#include <boost/algorithm/string.hpp>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/core-module.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/topology.h"
#include "ns3/exp-util.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/command-line.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/ground-station.h"
#include "ns3/satellite-position-helper.h"
#include "ns3/satellite-position-mobility-model.h"
#include "ns3/mobility-helper.h"
#include "ns3/string.h"
#include "ns3/type-id.h"
#include "ns3/vector.h"
#include "ns3/satellite-position-helper.h"
#include "ns3/point-to-point-laser-helper.h"
#include "ns3/gsl-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/wifi-net-device.h"
#include "ns3/point-to-point-laser-net-device.h"
#include "ns3/ipv4.h"
#include "ns3/ndnSIM/model/ndn-net-device-transport.hpp"
#include "ndn-cxx/name.hpp"
#include "ns3/ndn-leo-stack-helper.h"
#include "ns3/ndnSIM/apps/ndn-consumer.hpp"
#include "ndn-sat-simulator.h"
#include "ns3/nack-retx-strategy.h"

namespace ns3 {

double HANDOVER_DURATION = 0.000000001; // in seconds
double DELAYED_REMOVAL = 0.000000002; // in seconds
double MAX_GSL_LENGTH_M = 1089686.4181956202;

void printFibTable(Ptr<Node> node) {
  cout << Simulator::Now().GetSeconds() << " -- Node: " << node->GetId() << endl;
  Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  auto fw = ndn->getForwarder();
  auto &fib = fw->getFib();
  for (auto it = fib.begin(); it != fib.end(); it++) {
    for (auto i = it->getNextHops().begin(); i != it->getNextHops().end(); i++) {
      cout << "  " << it->getPrefix() << "," << i->getFace().getId() << endl;
    }
  }
}

void retransmitPitTable(Ptr<Node> node, string prefix) {
  Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  // std::shared_ptr<nfd::Forwarder> fw = ndn->getForwarder();
  // ndn::nfd::pit::Pit pit = fw->getPit();
  // ndn::nfd::fib::Fib fib = fw->getFib();

  auto fw = ndn->getForwarder();
  auto &pit = fw->getPit();
  auto &fib = fw->getFib();
  ndn::Name pf(prefix);
  if (pit.size() <= 1) return;
  cout << Simulator::Now().GetSeconds() << " -- Retx Node: " << node->GetId() << endl;
  for (auto it = pit.begin(); it != pit.end(); it++) {
    // Don't do anything if we're not interested in that entry
    ndn::Name fullName(it->getName());
    if (!pf.isPrefixOf(fullName)) {
      continue;
    }
    // Default life time is 2s
    ndn::time::milliseconds interestLifeTime(2000);
    // Remove all out records
    it->clearOutRecords();
    // for (auto i = it->getOutRecords().begin(); i != it->getOutRecords().end(); i++) {
    //   cout << "  PIT REMOVED - " << fullName << "," << i->getFace().getId() << endl;
    //   cout << "REMOVED?" << endl;
    //   it->deleteOutRecord(i->getFace());
    //   cout << "REMOVED!" << endl;
    // }
    // cout << "  PIT CLEARED" << endl;
    // Find the match in the fib, return empty if no match so don't have to check
    auto &fi = fib.findLongestPrefixMatch(fullName);
    // Re-add out records from FIB
    for (auto i : fi.getNextHops()) {
      cout << "  PIT ADDED - " << fullName << "," << i.getFace().getId() << endl;
      ndn::Interest interest = ndn::Interest(fullName, interestLifeTime);
      it->insertOrUpdateOutRecord(i.getFace(), interest);
      // Retransmit interest based on what's left in the pit with new face from fib.
      i.getFace().sendInterest(interest);
    }
  }
}

void sendNackOrRetransmit(Ptr<Node> node, const ndn::nfd::FaceEndpoint& faceEndPoint, string prefix) {
  Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  auto fw = ndn->getForwarder();
  auto &pit = fw->getPit();
  auto &fib = fw->getFib();
  ndn::Name pf(prefix);
  if (pit.size() <= 1) return;
  cout << Simulator::Now().GetSeconds() << " -- Retx Node: " << node->GetId() << endl;
  for (ndn::nfd::Pit::const_iterator it = pit.begin(); it != pit.end(); it++) {
    // Don't do anything if we're not interested in that entry
    ndn::Name fullName(it->getName());
    if (!pf.isPrefixOf(fullName)) {
      continue;
    }
    nfd::fw::NackRetxStrategy strat(*fw, "/localhost/nfd/strategy/nack-retx");
    // shared_ptr<ndn::nfd::pit::Entry> entry = (*it);
    strat.sendNackOrForward(it->getInterest(), faceEndPoint, pit.find(it->getInterest()));
  }

}

void
InstallRegionTable(NodeContainer allNodes) {
  for (Ptr<Node> node : allNodes) {
    string prefix = "/leo/" + to_string(node->GetId());
    ndn::NetworkRegionTableHelper::AddRegionName(node, prefix);
  }
}

NDNSatSimulator::NDNSatSimulator(string config) {
  ReadConfig(config);
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("20p"));

  // Configuration
  // string ns3_config = "scenarios/config/run.properties";

  // Reading nodes
  
  ReadSatellites();

  ReadGroundStations();

  // Only ground stations are valid endpoints
  for (uint32_t i = 0; i < m_groundStations.size(); i++) {
      m_endpoints.insert(m_satelliteNodes.GetN() + i);
  } 

  m_allNodes.Add(m_satelliteNodes);
  m_allNodes.Add(m_groundStationNodes);
  std::cout << "  > Number of nodes............. " << m_allNodes.GetN() << std::endl;

  // Link settings
  m_ipv4_helper.SetBase ("10.0.0.0", "255.255.255.0");
  m_isl_data_rate_megabit_per_s = parse_positive_double(getConfigParamOrDefault("isl_data_rate_megabit_per_s", "10000"));
  m_gsl_data_rate_megabit_per_s = parse_positive_double(getConfigParamOrDefault("gsl_data_rate_megabit_per_s", "10000"));
  m_isl_max_queue_size_pkts = parse_positive_int64(getConfigParamOrDefault("isl_max_queue_size_pkts", "10000"));
  m_gsl_max_queue_size_pkts = parse_positive_int64(getConfigParamOrDefault("gsl_max_queue_size_pkts", "10000"));
  m_isl_error_rate = parse_positive_double(getConfigParamOrDefault("isl_error_rate", "0"));
  m_gsl_error_rate = parse_positive_double(getConfigParamOrDefault("gsl_error_rate", "0"));
  // Default to 100ms

  ReadISLs();

  AddGSLs();
  // Install NDN stack on all nodes
  ndn::LeoStackHelper ndnHelper;

  // Set content store size
  ndnHelper.setCsSize(10000);

  // ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(m_allNodes);

  std::cout << "  > Installed NDN stacks" << std::endl;

  InstallRegionTable(m_allNodes);

  std::cout << "  > Installed region table" << std::endl;
}

std::string NDNSatSimulator::getConfigParamOrDefault(std::string key, std::string default_value) {
  auto it = m_config.find(key);
  if (it != m_config.end())
    return it->second;
  return default_value;
}

void NDNSatSimulator::ReadConfig(std::string conf) {
  // Read the config
  m_config = read_config(conf);
  m_satellite_network_dir = getConfigParamOrDefault("satellite_network_dir", "network_dir");
  m_satellite_network_routes_dir =  getConfigParamOrDefault("satellite_network_routes_dir", "network_dir/routes_dir");
  m_satellite_network_force_static = parse_boolean(getConfigParamOrDefault("satellite_network_force_static", "false"));
  m_node1_id = stoi(getConfigParamOrDefault("from_id", "0"));
  m_node2_id = stoi(getConfigParamOrDefault("to_id", "0"));
  m_name = getConfigParamOrDefault("name", "run");

  // Print full config
  printf("CONFIGURATION\n-----\nKEY                                       VALUE\n");
  std::map<std::string, std::string>::iterator it;
  for ( it = m_config.begin(); it != m_config.end(); it++ ) {
      printf("%-40s  %s\n", it->first.c_str(), it->second.c_str());
  }
  printf("\n");
}

void NDNSatSimulator::ReadSatellites()
{
  // Open file
  std::ifstream fs;
  fs.open(m_satellite_network_dir + "/tles.txt");
  NS_ABORT_MSG_UNLESS(fs.is_open(), "File tles.txt could not be opened");

  // First line:
  // <orbits> <satellites per orbit>
  std::string orbits_and_n_sats_per_orbit;
  std::getline(fs, orbits_and_n_sats_per_orbit);
  std::vector<std::string> res = split_string(orbits_and_n_sats_per_orbit, " ", 2);
  int64_t num_orbits = parse_positive_int64(res[0]);
  int64_t satellites_per_orbit = parse_positive_int64(res[1]);
  // Create the nodes
  m_satelliteNodes.Create(num_orbits * satellites_per_orbit);

  // Associate satellite mobility model with each node
  int64_t counter = 0;
  std::string name, tle1, tle2;
  while (std::getline(fs, name)) {
    std::getline(fs, tle1);
    std::getline(fs, tle2);

    // Format:
    // <name>
    // <TLE line 1>
    // <TLE line 2>

    // Create satellite
    Ptr<Satellite> satellite = CreateObject<Satellite>();
    satellite->SetName(name);
    satellite->SetTleInfo(tle1, tle2);

    // Decide the mobility model of the satellite
    MobilityHelper mobility;
    if (m_satellite_network_force_static) {
      // Static at the start of the epoch
      mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
      mobility.Install(m_satelliteNodes.Get(counter));
      Ptr<MobilityModel> mobModel = m_satelliteNodes.Get(counter)->GetObject<MobilityModel>();
      mobModel->SetPosition(satellite->GetPosition(satellite->GetTleEpoch()));
    } else {
      // Dynamic 
      mobility.SetMobilityModel(
              "ns3::SatellitePositionMobilityModel",
              "SatellitePositionHelper",
              SatellitePositionHelperValue(SatellitePositionHelper(satellite))
      );
      mobility.Install(m_satelliteNodes.Get(counter));
    }

    // Add to all satellites present
    m_satellites.push_back(satellite);

    counter++;
  }

  // Check that exactly that number of satellites has been read in
  if (counter != num_orbits * satellites_per_orbit) {
      throw std::runtime_error("Number of satellites defined in the TLEs does not match");
  }
  fs.close();
}

void NDNSatSimulator::ReadGroundStations()
{
  // Create a new file stream to open the file
  std::ifstream fs;
  fs.open(m_satellite_network_dir + "/ground_stations.txt");
  NS_ABORT_MSG_UNLESS(fs.is_open(), "File ground_stations.txt could not be opened");
  // Read ground station from each line

  std::string line;
  while (std::getline(fs, line)) {

    std::vector<std::string> res = split_string(line, ",", 8);

    // All eight values
    uint32_t gid = parse_positive_int64(res[0]);
    std::string name = res[1];
    double latitude = parse_double(res[2]);
    double longitude = parse_double(res[3]);
    double elevation = parse_double(res[4]);
    double cartesian_x = parse_double(res[5]);
    double cartesian_y = parse_double(res[6]);
    double cartesian_z = parse_double(res[7]);
    Vector cartesian_position(cartesian_x, cartesian_y, cartesian_z);

    // Create ground station data holder
    Ptr<GroundStation> gs = CreateObject<GroundStation>(
      gid, name, latitude, longitude, elevation, cartesian_position
    );
    m_groundStations.push_back(gs);

    // Create the node
    m_groundStationNodes.Create(1);
    if (m_groundStationNodes.GetN() != gid + 1) {
      throw std::runtime_error("GID is not incremented each line");
    }

    // Install the constant mobility model on the node
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(m_groundStationNodes.Get(gid));
    Ptr<MobilityModel> mobilityModel = m_groundStationNodes.Get(gid)->GetObject<MobilityModel>();
    mobilityModel->SetPosition(cartesian_position);
  }

  fs.close();
}

void NDNSatSimulator::ReadISLs()
{

    // Link helper
    PointToPointLaserHelper p2p_laser_helper;
    std::string max_queue_size_str = format_string("%" PRId64 "p", m_isl_max_queue_size_pkts);
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    em->SetAttribute ("ErrorRate", DoubleValue (m_isl_error_rate));
    p2p_laser_helper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(max_queue_size_str)));
    p2p_laser_helper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string(m_isl_data_rate_megabit_per_s) + "Mbps")));
    p2p_laser_helper.SetDeviceAttribute ("ReceiveErrorModel", PointerValue(em));
    std::cout << "    >> ISL data rate........ " << m_isl_data_rate_megabit_per_s << " Mbit/s" << std::endl;
    std::cout << "    >> ISL max queue size... " << m_isl_max_queue_size_pkts << " packets" << std::endl;
    std::cout << "    >> ISL loss rate... " << m_isl_error_rate << std::endl;

    // Open file
    std::ifstream fs;
    fs.open(m_satellite_network_dir + "/isls.txt");
    NS_ABORT_MSG_UNLESS(fs.is_open(), "File isls.txt could not be opened");

    // Read ISL pair from each line
    std::string line;
    int counter = 0;
    while (std::getline(fs, line)) {
        std::vector<std::string> res = split_string(line, " ", 2);

        // Retrieve satellite identifiers
        int32_t sat0_id = parse_positive_int64(res.at(0));
        int32_t sat1_id = parse_positive_int64(res.at(1));
        Ptr<Satellite> sat0 = m_satellites.at(sat0_id);
        Ptr<Satellite> sat1 = m_satellites.at(sat1_id);

        // Install a p2p laser link between these two satellites
        NodeContainer c;
        c.Add(m_satelliteNodes.Get(sat0_id));
        c.Add(m_satelliteNodes.Get(sat1_id));
        NetDeviceContainer netDevices = p2p_laser_helper.Install(c);

        // Utilization tracking
        if (m_enable_isl_utilization_tracking) {
            netDevices.Get(0)->GetObject<PointToPointLaserNetDevice>()->EnableUtilizationTracking(m_isl_utilization_tracking_interval_ns);
            netDevices.Get(1)->GetObject<PointToPointLaserNetDevice>()->EnableUtilizationTracking(m_isl_utilization_tracking_interval_ns);

            m_islNetDevices.Add(netDevices.Get(0));
            m_islFromTo.push_back(std::make_pair(sat0_id, sat1_id));
            m_islNetDevices.Add(netDevices.Get(1));
            m_islFromTo.push_back(std::make_pair(sat1_id, sat0_id));
        }

        counter += 1;
    }
    fs.close();

    // Completed
    std::cout << "    >> Created " << std::to_string(counter) << " ISL(s)" << std::endl;
}

void RemoveNextHop(ns3::ndn::NetDeviceTransport* ts, Address dest) {
  ts->RemoveNextHop(dest);
}

void AddRouteCustom(nfd::fib::Fib &fib, ns3::Ptr<ns3::Node> node, string prefix, shared_ptr<ns3::ndn::Face> face, int32_t metric) {
  const ns3::ndn::Name pn(prefix);

  // Potentially unsafe pointer hack
  nfd::fib::Entry *entry = fib.findExactMatch(pn);
  bool ret = true;
  // std::cout << node->GetId() << ", " << prefix << ", " << face->getId() << std::endl;
  
  if (entry != nullptr) {
    fib.erase(pn);
  }
  tie(entry, ret) = fib.insert(pn);
  NS_ASSERT_MSG(ret, "FIB insert failed");
  entry->addOrUpdateNextHop(*face, metric);
}


void RemoveExistingLink(Ptr<Node> node, string prefix, shared_ptr<ns3::ndn::Face> nextHop, shared_ptr<ns3::ndn::Face> prevCurFace, shared_ptr<ns3::ndn::Face> prevNextFace, Address dest) {
  // Remove route when we strictly want one FIB entry
  if (prevCurFace->getId() != nextHop->getId()) {
    ns3::ndn::FibHelper::RemoveRoute(node, prefix, prevCurFace);
  }
  // Remove additional GSl hardware routes
  // if (prevCurFace->getLinkType() == ::ndn::nfd::LINK_TYPE_AD_HOC) {
  //   ns3::ndn::NetDeviceTransport* ts = dynamic_cast<ns3::ndn::NetDeviceTransport*>(prevNextFace->getTransport());
  //   ts->RemoveNextDataHop(dest);
  //   ns3::Simulator::Schedule(ns3::MilliSeconds(1), &sendNackOrRetransmit, node, ndn::nfd::FaceEndpoint(*prevCurFace, prevCurFace->getId()), prefix);
  //   // ns3::Simulator::Schedule(ns3::MilliSeconds(1), &retransmitPitTable, node, prefix);
  //   // ns3::Simulator::Schedule(ns3::Seconds(0), &RemoveNextDataHop, ts, dest);
  // }
}

void AddRouteISL(ns3::Ptr<ns3::Node> node, int deviceId,
                string prefix, ns3::Ptr<ns3::Node> otherNode, int otherDeviceId, shared_ptr<map<pair<uint32_t, string>, tuple<shared_ptr<ns3::ndn::Face>, shared_ptr<ns3::ndn::Face>, Address> >> curNextHop)
{
  NS_ASSERT_MSG(deviceId < node->GetNDevices(), "Source device ID must be valid");
  NS_ASSERT_MSG(otherDeviceId < otherNode->GetNDevices(), "Next hop device ID must be valid");

  Ptr<PointToPointLaserNetDevice> netDevice = DynamicCast<PointToPointLaserNetDevice>(node->GetDevice(deviceId));
  Ptr<PointToPointLaserNetDevice> remoteNetDevice = DynamicCast<PointToPointLaserNetDevice>(otherNode->GetDevice(otherDeviceId));
  Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  Ptr<ns3::ndn::L3Protocol> remoteNdn = otherNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT_MSG(ndn != 0, "Ndn stack should be installed on the node");

  shared_ptr<ns3::ndn::Face> face = ndn->getFaceByNetDevice(netDevice);
  shared_ptr<ns3::ndn::Face> remoteFace = remoteNdn->getFaceByNetDevice(remoteNetDevice);
  NS_ASSERT_MSG(face != 0, "There is no face associated with the p2p link");
  // Removing existing route
  // Remove route -> Add route -> Remove backward link
  auto p = make_pair(node->GetId(), prefix);
  if (curNextHop->find(p) != curNextHop->end()) {
    shared_ptr<ns3::ndn::Face> prevCurFace, prevNextFace;
    Address prevDest;
    tie(prevCurFace, prevNextFace, prevDest) = (*curNextHop)[p];
    ns3::Simulator::Schedule(ns3::Seconds(DELAYED_REMOVAL), &RemoveExistingLink, node, prefix, face, prevCurFace, prevNextFace, prevDest);
  }
  // Get delay value
  Ptr<MobilityModel> senderMobility = node->GetObject<MobilityModel>();
  Ptr<MobilityModel> receiverMobility = otherNode->GetObject<MobilityModel>();
  double distance = senderMobility->GetDistanceFrom(receiverMobility);
  AddRouteCustom(ndn->getForwarder()->getFib(), node, prefix, face, distance);
  // ns3::Simulator::Schedule(ns3::Seconds(HANDOVER_DURATION), &AddRouteCustom, node, prefix, face, distance);
  // ns3::ndn::FibHelper::AddRoute(node, prefix, face, 1);
  // Add the current route for future removal
  (*curNextHop)[p] = make_tuple(face, remoteFace, netDevice->GetAddress());
}

void SetRouteISL(ns3::Ptr<ns3::Node> node, int deviceId,
                string prefix, ns3::Ptr<ns3::Node> otherNode, int otherDeviceId)
{
  NS_ASSERT_MSG(deviceId < node->GetNDevices(), "Sorce device ID must be valid");
  NS_ASSERT_MSG(otherDeviceId < otherNode->GetNDevices(), "Next hop device ID must be valid");

  // std::cout << "Setting route ISL at " << Simulator::Now().GetNanoSeconds() << " : " << node->GetId() << ", " << prefix << ", " << otherNode->GetId() << std::endl;
  
  Ptr<PointToPointLaserNetDevice> netDevice = DynamicCast<PointToPointLaserNetDevice>(node->GetDevice(deviceId));
  Ptr<PointToPointLaserNetDevice> remoteNetDevice = DynamicCast<PointToPointLaserNetDevice>(otherNode->GetDevice(otherDeviceId));
  Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  Ptr<ns3::ndn::L3Protocol> remoteNdn = otherNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT_MSG(ndn != 0, "Ndn stack should be installed on the node");

  shared_ptr<ns3::ndn::Face> face = ndn->getFaceByNetDevice(netDevice);
  shared_ptr<ns3::ndn::Face> remoteFace = remoteNdn->getFaceByNetDevice(remoteNetDevice);
  NS_ASSERT_MSG(face != 0, "There is no face associated with the p2p link");
  // Removing existing route
  // Remove route -> Add route -> Remove backward link
  auto p = make_pair(node->GetId(), prefix);
  // Get delay value
  Ptr<MobilityModel> senderMobility = node->GetObject<MobilityModel>();
  Ptr<MobilityModel> receiverMobility = otherNode->GetObject<MobilityModel>();
  double distance = senderMobility->GetDistanceFrom(receiverMobility);
  AddRouteCustom(ndn->getForwarder()->getFib(), node, prefix, face, distance);
  // ns3::ndn::FibHelper::AddRoute(node, prefix, face, distance);
  // ns3::Simulator::Schedule(ns3::Seconds(HANDOVER_DURATION), &AddRouteCustom, node, prefix, face, distance);
}

void AddRouteGSL(ns3::Ptr<ns3::Node> node, int deviceId,
                string prefix, ns3::Ptr<ns3::Node> otherNode, int otherDeviceId, shared_ptr<map<pair<uint32_t, string>, tuple<shared_ptr<ns3::ndn::Face>, shared_ptr<ns3::ndn::Face>, Address> >> curNextHop)
{
  // Prevent legacy dynamic state to create more GSLs than needed
  if (deviceId >= node->GetNDevices()) {
    deviceId = node->GetNDevices() - 1;
  }
  if (otherDeviceId >= otherNode->GetNDevices()) {
    otherDeviceId = otherNode->GetNDevices() - 1;
  }
  NS_ASSERT_MSG(deviceId < node->GetNDevices(), "Source device ID must be valid");
  NS_ASSERT_MSG(otherDeviceId < otherNode->GetNDevices(), "Next hop device ID must be valid");
  // Removing existing route
  ns3::Ptr<ns3::Node> gsNode;
  ns3::Ptr<ns3::Node> satNode;
  Ptr<GSLNetDevice> gsNetDevice;
  Ptr<GSLNetDevice> satNetDevice;
  // src node is satellite
  if (node->GetId() < otherNode->GetId()) {
    satNode = node;
    gsNode = otherNode;
    satNetDevice = DynamicCast<GSLNetDevice>(node->GetDevice(deviceId));
    gsNetDevice = DynamicCast<GSLNetDevice>(otherNode->GetDevice(otherDeviceId));
  } else {
    satNode = otherNode;
    gsNode = node;
    satNetDevice = DynamicCast<GSLNetDevice>(otherNode->GetDevice(otherDeviceId));
    gsNetDevice = DynamicCast<GSLNetDevice>(node->GetDevice(deviceId));
  }

  // Get GS Information
  
  Ptr<ns3::ndn::L3Protocol> gsNdn = gsNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT_MSG(gsNdn != 0, "Ndn stack should be installed on the ground station node");
  shared_ptr<ns3::ndn::Face> gsFace = gsNdn->getFaceByNetDevice(gsNetDevice);
  NS_ASSERT_MSG(gsFace != 0, "There is no face associated with the gsl link");
  ns3::ndn::NetDeviceTransport* gsTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(gsFace->getTransport());
  NS_ASSERT_MSG(gsTransport != 0, "There is no valid transport associated with the ground station face");

  // Get Sat Information
  Ptr<ns3::ndn::L3Protocol> satNdn = satNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT_MSG(satNdn != 0, "Ndn stack should be installed on the satellite node");
  shared_ptr<ns3::ndn::Face> satFace = satNdn->getFaceByNetDevice(satNetDevice);
  NS_ASSERT_MSG(satFace != 0, "There is no face associated with the gsl link");
  ns3::ndn::NetDeviceTransport* satTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(satFace->getTransport());
  NS_ASSERT_MSG(satTransport != 0, "There is no valid transport associated with the ground station face");
  
  // Remove route -> Add route -> Remove backward link
  auto p = make_pair(node->GetId(), prefix);
  shared_ptr<ns3::ndn::Face> prevCurFace, prevNextFace;
  Address prevDest;
  // Get delay value
  Ptr<MobilityModel> senderMobility = node->GetObject<MobilityModel>();
  Ptr<MobilityModel> receiverMobility = otherNode->GetObject<MobilityModel>();
  double distance = senderMobility->GetDistanceFrom(receiverMobility);

  if (node == gsNode) {
    // gs -> sat
    // Remove existing route
    if (curNextHop->find(p) != curNextHop->end()) {
      tie(prevCurFace, prevNextFace, prevDest) = (*curNextHop)[p];
      ns3::Simulator::Schedule(ns3::Seconds(DELAYED_REMOVAL), &RemoveExistingLink, node, prefix, gsFace, prevCurFace, prevNextFace, prevDest);
    }
    AddRouteCustom(gsNdn->getForwarder()->getFib(), node, prefix, gsFace, distance);
    // ns3::Simulator::Schedule(ns3::Seconds(HANDOVER_DURATION), &AddRouteCustom, node, prefix, gsFace, distance);
    // Add the current route for future removal
    (*curNextHop)[p] = make_tuple(gsFace, satFace, gsNetDevice->GetAddress());
  } else {
    // sat -> gs
    if (curNextHop->find(p) != curNextHop->end()) {
      tie(prevCurFace, prevNextFace, prevDest) = (*curNextHop)[p];
      // ns3::ndn::FibHelper::RemoveRoute(node, prefix, prevCurFace);
      ns3::Simulator::Schedule(ns3::Seconds(DELAYED_REMOVAL), &RemoveExistingLink, node, prefix, satFace, prevCurFace, prevNextFace, prevDest);
    }
    AddRouteCustom(satNdn->getForwarder()->getFib(), node, prefix, satFace, distance);
    // ns3::Simulator::Schedule(ns3::Seconds(HANDOVER_DURATION), &AddRouteCustom, node, prefix, satFace, distance);
    // Add the current route for future removal
    (*curNextHop)[p] = make_tuple(satFace, gsFace, satNetDevice->GetAddress());
  }
}

void SetRouteGSL(ns3::Ptr<ns3::Node> node, int deviceId,
                string prefix, ns3::Ptr<ns3::Node> otherNode, int otherDeviceId)
{
  NS_ASSERT_MSG(deviceId < node->GetNDevices(), "Sorce device ID must be valid");
  NS_ASSERT_MSG(otherDeviceId < otherNode->GetNDevices(), "Next hop device ID must be valid");
  // std::cout << "Setting route GSL at " << Simulator::Now().GetMilliSeconds() << " : " << node->GetId() << ", " << otherNode->GetId() << std::endl;
  ns3::Ptr<ns3::Node> gsNode;
  ns3::Ptr<ns3::Node> satNode;
  Ptr<GSLNetDevice> gsNetDevice;
  Ptr<GSLNetDevice> satNetDevice;
  // src node is satellite
  if (node->GetId() < otherNode->GetId()) {
    satNode = node;
    gsNode = otherNode;
    satNetDevice = DynamicCast<GSLNetDevice>(node->GetDevice(deviceId));
    gsNetDevice = DynamicCast<GSLNetDevice>(otherNode->GetDevice(otherDeviceId));
  } else {
    satNode = otherNode;
    gsNode = node;
    satNetDevice = DynamicCast<GSLNetDevice>(otherNode->GetDevice(otherDeviceId));
    gsNetDevice = DynamicCast<GSLNetDevice>(node->GetDevice(deviceId));
  }

  // Get GS Information
  
  Ptr<ns3::ndn::L3Protocol> gsNdn = gsNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT_MSG(gsNdn != 0, "Ndn stack should be installed on the ground station node");
  shared_ptr<ns3::ndn::Face> gsFace = gsNdn->getFaceByNetDevice(gsNetDevice);
  NS_ASSERT_MSG(gsFace != 0, "There is no face associated with the gsl link");
  ns3::ndn::NetDeviceTransport* gsTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(gsFace->getTransport());
  NS_ASSERT_MSG(gsTransport != 0, "There is no valid transport associated with the ground station face");

  // Get Sat Information
  Ptr<ns3::ndn::L3Protocol> satNdn = satNode->GetObject<ns3::ndn::L3Protocol>();
  NS_ASSERT_MSG(satNdn != 0, "Ndn stack should be installed on the satellite node");
  shared_ptr<ns3::ndn::Face> satFace = satNdn->getFaceByNetDevice(satNetDevice);
  NS_ASSERT_MSG(satFace != 0, "There is no face associated with the gsl link");
  ns3::ndn::NetDeviceTransport* satTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(satFace->getTransport());
  NS_ASSERT_MSG(satTransport != 0, "There is no valid transport associated with the ground station face");
  
  // Remove route -> Add route -> Remove backward link
  auto p = make_pair(node->GetId(), prefix);
  shared_ptr<ns3::ndn::Face> prevCurFace, prevNextFace;
  Address prevDest;
  // Get delay value
  Ptr<MobilityModel> senderMobility = node->GetObject<MobilityModel>();
  Ptr<MobilityModel> receiverMobility = otherNode->GetObject<MobilityModel>();
  double distance = senderMobility->GetDistanceFrom(receiverMobility);

  if (node == gsNode) {
    // gs -> sat
    AddRouteCustom(gsNdn->getForwarder()->getFib(), node, prefix, gsFace, distance);
    // ns3::ndn::FibHelper::AddRoute(node, prefix, gsFace, distance);
    // ns3::Simulator::Schedule(ns3::Seconds(HANDOVER_DURATION), &AddRouteCustom, node, prefix, gsFace, distance);
  } else {
    // sat -> gs
    AddRouteCustom(satNdn->getForwarder()->getFib(), node, prefix, satFace, distance);
    // ns3::ndn::FibHelper::AddRoute(node, prefix, satFace, distance);
    // ns3::Simulator::Schedule(ns3::Seconds(HANDOVER_DURATION), &AddRouteCustom, node, prefix, satFace, distance);
  }
}

void NDNSatSimulator::AddGSLs() {

  // Link helper
  GSLHelper gsl_helper;
  std::string max_queue_size_str = format_string("%" PRId64 "p", m_gsl_max_queue_size_pkts);
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(m_gsl_error_rate));
  gsl_helper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(max_queue_size_str)));
  gsl_helper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string(m_gsl_data_rate_megabit_per_s) + "Mbps")));
  gsl_helper.SetDeviceAttribute ("ReceiveErrorModel", PointerValue(em));
  std::cout << "    >> GSL data rate........ " << m_gsl_data_rate_megabit_per_s << " Mbit/s" << std::endl;
  std::cout << "    >> GSL max queue size... " << m_gsl_max_queue_size_pkts << " packets" << std::endl;
  std::cout << "    >> GSL loss rate... " << m_gsl_error_rate << std::endl;

  // Traffic control helper
  // TrafficControlHelper tch_gsl;
  // tch_gsl.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", QueueSizeValue(QueueSize("1p")));  // Will be removed later any case

  // Check that the file exists
  std::string filename = m_satellite_network_dir + "/gsl_interfaces_info.txt";
  if (!file_exists(filename)) {
      throw std::runtime_error(format_string("File %s does not exist.", filename.c_str()));
  }

  // Read file contents
  std::string line;
  std::ifstream fstate_file(filename);
  std::vector<std::tuple<int32_t, double>> node_gsl_if_info;
  uint32_t total_num_gsl_ifs = 0;
  if (fstate_file) {
    size_t line_counter = 0;
    while (getline(fstate_file, line)) {
      std::vector<std::string> comma_split = split_string(line, ",", 3);
      int64_t node_id = parse_positive_int64(comma_split[0]);
      int64_t num_ifs = parse_positive_int64(comma_split[1]);
      double agg_bandwidth = parse_positive_double(comma_split[2]);
      if ((size_t) node_id != line_counter) {
          throw std::runtime_error("Node id must be incremented each line in GSL interfaces info");
      }
      node_gsl_if_info.push_back(std::make_tuple((int32_t) num_ifs, agg_bandwidth));
      total_num_gsl_ifs += num_ifs;
      line_counter++;
    }
    fstate_file.close();
  } else {
    throw std::runtime_error(format_string("File %s could not be read.", filename.c_str()));
  }
  std::cout << "    >> Read all GSL interfaces information for the " << node_gsl_if_info.size() << " nodes" << std::endl;
  std::cout << "    >> Number of GSL interfaces to create... " << total_num_gsl_ifs << std::endl;

  // Create and install GSL network devices
  NetDeviceContainer devices = gsl_helper.Install(m_satelliteNodes, m_groundStationNodes, node_gsl_if_info);
  std::cout << "    >> Finished install GSL interfaces (interfaces, network devices, one shared channel)" << std::endl;

  // Install queueing disciplines
  // tch_gsl.Install(devices);
  std::cout << "    >> Finished installing traffic control layer qdisc which will be removed later" << std::endl;

  // Check that all interfaces were created
  NS_ABORT_MSG_IF(total_num_gsl_ifs != devices.GetN(), "Not the expected amount of interfaces has been created.");

  std::cout << "    >> GSL interfaces are setup" << std::endl;

}

void ReinstallGSL(ns3::NodeContainer gsNodes, ns3::NodeContainer satNodes) {
  map<int, pair<double, Address> > nearestSat;
  for (Ptr<Node> satNode : satNodes) {
    Ptr<ns3::ndn::L3Protocol> satNdn = satNode->GetObject<ns3::ndn::L3Protocol>();
    NS_ASSERT_MSG(satNdn != 0, "Ndn stack should be installed on the satellite node");
    shared_ptr<ns3::ndn::Face> satFace;
    // Find the GSL face among all, starting from the last index to optimize
    int i = satNode->GetNDevices() - 1;
    while (i >= 0) {
      satFace = satNdn->getFaceByNetDevice(satNode->GetDevice(i));
      if (satFace->getLinkType() == ::ndn::nfd::LINK_TYPE_AD_HOC) {
        break;
      }
      i--;
    }
    // std::cout << "sat net id: " << i << " / " << satNode->GetNDevices() << std::endl;
    NS_ASSERT_MSG(satFace != 0, "There is no face associated with the gsl link");
    ns3::ndn::NetDeviceTransport* satTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(satFace->getTransport());
    NS_ASSERT_MSG(satTransport != 0, "There is no valid transport associated with the satellite face");
    // Clear the next data hop
    satTransport->ClearNextHop();
    Ptr<MobilityModel> satMobility = satNode->GetObject<MobilityModel>();
    Ptr<MobilityModel> gsMobility;
    for (Ptr<Node> gsNode : gsNodes) {
      Ptr<ns3::ndn::L3Protocol> gsNdn = gsNode->GetObject<ns3::ndn::L3Protocol>();
      NS_ASSERT_MSG(gsNdn != 0, "Ndn stack should be installed on the gs node");
      shared_ptr<ns3::ndn::Face> gsFace = gsNdn->getFaceByNetDevice(gsNode->GetDevice(0));
      NS_ASSERT_MSG(gsFace != 0, "There is no face associated with the gsl link");
      ns3::ndn::NetDeviceTransport* gsTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(gsFace->getTransport());
      NS_ASSERT_MSG(satTransport != 0, "There is no valid transport associated with the ground station face");
      gsMobility = gsNode->GetObject<MobilityModel>();
      // Calculate the GSL distance
      double distance = satMobility->GetDistanceFrom(gsMobility);
      if (distance <= MAX_GSL_LENGTH_M) {
        // Add/set next hop for interest and data when they are in rance
        satTransport->AddNextHop(gsTransport->GetNetDevice()->GetAddress());
        // Add to the map if the entry doesn't exist
        if (nearestSat.find(gsNode->GetId()) == nearestSat.end()) {
          nearestSat[gsNode->GetId()] = make_pair(distance, satTransport->GetNetDevice()->GetAddress());
        } else {
          // Update the distance and the nearest satellite's address
          if (nearestSat[gsNode->GetId()].first > distance) {
            nearestSat[gsNode->GetId()] = make_pair(distance, satTransport->GetNetDevice()->GetAddress());
          }
        }
      }
    }
  }
  // Set ground station's next hop based on the nearest satellite
  for (auto it = nearestSat.begin(); it != nearestSat.end(); it++) {
    Ptr<Node> gsNode = gsNodes.Get(it->first - satNodes.GetN());
    NS_ASSERT_MSG(gsNode != 0, "Invalid ground station node");
    Ptr<ns3::ndn::L3Protocol> gsNdn = gsNode->GetObject<ns3::ndn::L3Protocol>();
    NS_ASSERT_MSG(gsNdn != 0, "Ndn stack should be installed on the gs node");
    shared_ptr<ns3::ndn::Face> gsFace = gsNdn->getFaceByNetDevice(gsNode->GetDevice(0));
    NS_ASSERT_MSG(gsFace != 0, "There is no face associated with the gsl link");
    ns3::ndn::NetDeviceTransport* gsTransport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(gsFace->getTransport());
    gsTransport->SetNextHop(it->second.second);
  }
}

void NDNSatSimulator::ImportDynamicStateSat(ns3::NodeContainer nodes, string dname, int retx, bool complete) {
  ImportDynamicStateSat(nodes, dname, retx, complete, -1);
}

void NDNSatSimulator::ImportDynamicStateSat(ns3::NodeContainer nodes, string dname, int retx, bool complete, double limit) {
  // Construct a  link inference from dynamic state
  m_cur_next_hop = make_shared<map<pair<uint32_t, string>, tuple<shared_ptr<ns3::ndn::Face>, shared_ptr<ns3::ndn::Face>, Address> >> ();
  // Iterate through the dynamic state directory
  for (const auto & entry : filesystem::directory_iterator(dname)) {
    // Extract nanoseconds from file name
    std::regex rgx(".*fstate_(\\w+)\\.txt.*");
    smatch match;
    string full_path = entry.path();
    if (!std::regex_search(full_path, match, rgx)) continue;
    // Check if network is forced static
    if (m_satellite_network_force_static && match[1].compare("0")) {
      continue; 
    }
    double ms = stod(match[1]) / 1000000;
    if (limit >= 0 && ms > limit * 1000) {
      continue;
    }
    int64_t current_node;
    // int destination_node;
    string prefix;
    int64_t next_hop;

    ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &ReinstallGSL, m_groundStationNodes, m_satelliteNodes);
  
    // Read each file
    ifstream input(full_path);
    string line;
    while(getline(input, line))
    {
      vector<string> result;
      boost::split(result, line, boost::is_any_of(","));
      current_node = stoi(result[0]);
      // destination_node = stoi(result[1]);
      next_hop = stoi(result[2]);;
      // Add AddRoute schedule
      prefix = "/leo/" + result[1];

      // Do client instant retransmission
      if (current_node >= m_satelliteNodes.GetN() && retx == 1) {
        ns3::Simulator::ScheduleWithContext(current_node, ns3::MilliSeconds(ms), &retransmitPitTable, nodes.Get(current_node), prefix);
      }
      // cout << ms / 1000 << "Add Route: " << current_node << "," << prefix << "," << next_hop << endl;
      if (complete) {
        // cout << "Add Route at" << (ns3::MilliSeconds(counter / 100) + ns3::NanoSeconds(counter % 100)).GetNanoSeconds() << endl;
        if (current_node >= m_satelliteNodes.GetN() || next_hop >= m_satelliteNodes.GetN()) {
          ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &SetRouteGSL, nodes.Get(current_node), stoi(result[3]),
                                  prefix, nodes.Get(next_hop), stoi(result[4]));
        } else {
          ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &SetRouteISL, nodes.Get(current_node), stoi(result[3]),
                                  prefix, nodes.Get(next_hop), stoi(result[4]));
        }
      } else {
        if (current_node >= m_satelliteNodes.GetN() || next_hop >= m_satelliteNodes.GetN()) {
          ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteGSL, nodes.Get(current_node), stoi(result[3]),
                                  prefix, nodes.Get(next_hop), stoi(result[4]), m_cur_next_hop);
        } else {
          ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteISL, nodes.Get(current_node), stoi(result[3]),
                                  prefix, nodes.Get(next_hop), stoi(result[4]), m_cur_next_hop);
        }
      }
    }
  }
  std::cout << "Import success" << std::endl;
  std::cout << std::endl;
}

void ForceTimeout(Ptr<ndn::Consumer> app) {
  app->ForceTimeout();
}

} // namespace ns3
