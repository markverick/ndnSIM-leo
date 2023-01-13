// ndn-simple.cpp


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
#include "ns3/basic-simulation.h"
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
// #include "ns3/ndnSIM/model/ndn-net-device-transport.hpp"
#include "ns3/ndn-multicast-net-device-transport.h"
#include "ns3/ndn-leo-stack-helper.h"

namespace ns3 {

// Input
Ptr<BasicSimulation> m_basicSimulation;       //<! Basic simulation instance
std::string m_satellite_network_dir;          //<! Directory containing satellite network information
std::string m_satellite_network_routes_dir;   //<! Directory containing the routes over time of the network
bool m_satellite_network_force_static;        //<! True to disable satellite movement and basically run
                                              //   it static at t=0 (like a static network)
std::string m_prefix;                         // NDN's prefix


// Generated state
NodeContainer m_allNodes;                           //!< All nodes
NodeContainer m_groundStationNodes;                 //!< Ground station nodes
NodeContainer m_satelliteNodes;                     //!< Satellite nodes
std::vector<Ptr<GroundStation> > m_groundStations;  //!< Ground stations
std::vector<Ptr<Satellite>> m_satellites;           //<! Satellites
std::set<int64_t> m_endpoints;                      //<! Endpoint ids = ground station ids
std::vector<std::tuple<double, Ptr<Node>, string, Ptr<PointToPointLaserNetDevice> > > m_pending_fib;

// ISL devices
Ipv4AddressHelper m_ipv4_helper;
NetDeviceContainer m_islNetDevices;
std::vector<std::pair<int32_t, int32_t>> m_islFromTo;
std::map<std::string, std::string> m_config;

// Values
double m_isl_data_rate_megabit_per_s;
double m_gsl_data_rate_megabit_per_s;
int64_t m_isl_max_queue_size_pkts;
int64_t m_gsl_max_queue_size_pkts;
bool m_enable_isl_utilization_tracking;
int64_t m_isl_utilization_tracking_interval_ns;

std::string getConfigParamOrDefault(std::string key, std::string default_value) {
  auto it = m_config.find(key);
  if (it != m_config.end())
    return it->second;
  return default_value;
}

void readConfig(std::string conf) {
  // Read the config
  m_config = read_config(conf);

  // Print full config
  printf("CONFIGURATION\n-----\nKEY                                       VALUE\n");
  std::map<std::string, std::string>::iterator it;
  for ( it = m_config.begin(); it != m_config.end(); it++ ) {
      printf("%-40s  %s\n", it->first.c_str(), it->second.c_str());
  }
  printf("\n");
}

void ReadSatellites()
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

void ReadGroundStations()
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
  // for (auto node : m_groundStationNodes) {
  //   cout << node->GetId() << endl;
  // }
}

void ReadISLs()
{

    // Link helper
    PointToPointLaserHelper p2p_laser_helper;
    std::string max_queue_size_str = format_string("%" PRId64 "p", m_isl_max_queue_size_pkts);
    p2p_laser_helper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(max_queue_size_str)));
    p2p_laser_helper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string(m_isl_data_rate_megabit_per_s) + "Mbps")));
    std::cout << "    >> ISL data rate........ " << m_isl_data_rate_megabit_per_s << " Mbit/s" << std::endl;
    std::cout << "    >> ISL max queue size... " << m_isl_max_queue_size_pkts << " packets" << std::endl;

    // Traffic control helper
    // TrafficControlHelper tch_isl;
    // tch_isl.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", QueueSizeValue(QueueSize("1p"))); // Will be removed later any case

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

        // // Install traffic control helper
        // tch_isl.Install(netDevices.Get(0));
        // tch_isl.Install(netDevices.Get(1));

        // // Assign some IP address (nothing smart, no aggregation, just some IP address)
        // m_ipv4_helper.Assign(netDevices);
        // m_ipv4_helper.NewNetwork();

        // // Remove the traffic control layer (must be done here, else the Ipv4 helper will assign a default one)
        // TrafficControlHelper tch_uninstaller;
        // tch_uninstaller.Uninstall(netDevices.Get(0));
        // tch_uninstaller.Uninstall(netDevices.Get(1));

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


void AddRouteISL (ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode, int metric)
{
  for (uint32_t deviceId = 0; deviceId < node->GetNDevices(); deviceId++) {
    // if (node->GetId() < m_satelliteNodes.GetN() && otherNode->GetId() < m_satelliteNodes.GetN()) {
    // Laser
    Ptr<PointToPointLaserNetDevice> netDevice = DynamicCast<PointToPointLaserNetDevice>(node->GetDevice(deviceId));
    if (netDevice == 0)
      continue;
    Ptr<Channel> channel = netDevice->GetChannel();
    if (channel == 0)
      continue;
    if (channel->GetDevice(0)->GetNode() == otherNode
        || channel->GetDevice(1)->GetNode() == otherNode) {
      Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
      NS_ASSERT_MSG(ndn != 0, "Ndn stack should be installed on the node");

      shared_ptr<ns3::ndn::Face> face = ndn->getFaceByNetDevice(netDevice);
      NS_ASSERT_MSG(face != 0, "There is no face associated with the p2p link");

      ns3::ndn::FibHelper::AddRoute(node, prefix, face, metric);

      return;
    }
  }
  // else {
  //   // GSL currently not working since one face can have multiple connections to the sats
  //   Ptr<GSLNetDevice> netDevice = DynamicCast<GSLNetDevice>(node->GetDevice(deviceId));
  //   if (netDevice == 0)
  //     continue;
  //   Ptr<Channel> channel = netDevice->GetChannel();
  //   if (channel == 0)
  //     continue;
  //   cout << node->GetId() << ": " << netDevice << ", " << otherNode->GetId() << endl;
  //   // cout << node << ": " << node->GetId() << ", " << otherNode << ": " << otherNode->GetId() << endl;
  //   cout << "GSL 3" << endl;
  //   cout << channel->GetDevice(1683) << ": " << channel->GetDevice(1683)->GetNode()->GetId() << ", " << channel->GetDevice(250) << ": " << channel->GetDevice(250)->GetNode()->GetId() << endl;
  //   Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  //   // Ptr<ns3::ndn::L3Protocol> otherNdn = otherNode->GetObject<ns3::ndn::L3Protocol>();
  //   NS_ASSERT_MSG(ndn != 0, "Ndn stack should be installed on the node");
  //   // Ptr<GSLNetDevice> otherNetDevice = DynamicCast<GSLNetDevice>(channel->GetDevice(otherNode->GetId()));
  //   // cout << otherNetDevice << endl;
  //   // if (channel->GetDevice(otherNode->GetId()))
  //   shared_ptr<ns3::ndn::Face> face = ndn->getFaceByNetDevice(netDevice);
  //   NS_ASSERT_MSG(face != 0, "There is no face associated with the p2p link");

  //   // ns3::ndn::FibHelper::AddRoute(node, prefix, face, metric);

  //   // return;

  // }
}

void AddRouteGSL (ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode, int metric)
{
  ns3::Ptr<ns3::Node> gsNode;
  ns3::Ptr<ns3::Node> satNode;
  // src node is satellite
  if (node->GetId() < m_satelliteNodes.GetN()) {
    satNode = node;
    gsNode = otherNode;
  } else {
    satNode = otherNode;
    gsNode = node;
  }
  // cout << gsNode->GetId() << "," << gsNode->GetNDevices() << endl;
  // cout << satNode->GetId() << "," << satNode->GetNDevices() << endl;
  Ptr<GSLNetDevice> gsNetDevice = DynamicCast<GSLNetDevice>(gsNode->GetDevice(0));
  for (uint32_t deviceId = 0; deviceId < satNode->GetNDevices(); deviceId++) {
    Ptr<GSLNetDevice> satNetDevice = DynamicCast<GSLNetDevice>(satNode->GetDevice(deviceId));
    if (satNetDevice == 0)
      continue;
    Ptr<Channel> channel = satNetDevice->GetChannel();
    if (channel == 0)
      continue;
    // cout << channel->GetNDevices() << endl;
    // cout << gsNode->GetId() << ": " << netDeviceGS << ", "  << satNode->GetId() << ": " << netDevice << endl;
    // cout << node << ": " << node->GetId() << ", " << otherNode << ": " << otherNode->GetId() << endl;
    // cout << channel->GetDevice(1683) << ": " << channel->GetDevice(1683)->GetNode()->GetId() << ", " << channel->GetDevice(250) << ": " << channel->GetDevice(250)->GetNode()->GetId() << endl;

    Ptr<ns3::ndn::L3Protocol> gsNdn = gsNode->GetObject<ns3::ndn::L3Protocol>();
    NS_ASSERT_MSG(gsNdn != 0, "Ndn stack should be installed on the ground station node");
    shared_ptr<ns3::ndn::Face> gsFace = gsNdn->getFaceByNetDevice(gsNetDevice);
    NS_ASSERT_MSG(gsFace != 0, "There is no face associated with the gsl link");
    ns3::ndn::MulticastNetDeviceTransport* gsTransport = dynamic_cast<ns3::ndn::MulticastNetDeviceTransport*>(gsFace->getTransport());
    NS_ASSERT_MSG(gsTransport != 0, "There is no valid transport associated with the ground station face");

    Ptr<ns3::ndn::L3Protocol> satNdn = satNode->GetObject<ns3::ndn::L3Protocol>();
    NS_ASSERT_MSG(satNdn != 0, "Ndn stack should be installed on the satellite node");
    shared_ptr<ns3::ndn::Face> satFace = satNdn->getFaceByNetDevice(satNetDevice);
    NS_ASSERT_MSG(satFace != 0, "There is no face associated with the gsl link");
    // TODO: Maybe unsafe pointer, fix later
    ns3::ndn::MulticastNetDeviceTransport* satTransport = dynamic_cast<ns3::ndn::MulticastNetDeviceTransport*>(satFace->getTransport());
    NS_ASSERT_MSG(satTransport != 0, "There is no valid transport associated with the ground station face");

    if (node == gsNode) {
      // gs -> sat
      ns3::ndn::FibHelper::AddRoute(node, prefix, gsFace, metric);
      gsTransport->AddBroadcastAddress(satNetDevice->GetAddress());
      satTransport->AddBroadcastAddress(gsNetDevice->GetAddress());
      // netDeviceGS->SetDstAddress(netDevice->GetAddress());
    } else {
      // sat -> gs
      
      // cout << "SAT->GS: " << deviceId << "," << satNetDevice->GetAddress() << " -> " << gsNetDevice->GetAddress() << endl;
      ns3::ndn::FibHelper::AddRoute(node, prefix, satFace, metric);
      // cout << "ADD BROADCAST: " << satNetDevice->GetAddress() << ", " << gsNetDevice->GetAddress() << endl;
      gsTransport->AddBroadcastAddress(satNetDevice->GetAddress());
      satTransport->AddBroadcastAddress(gsNetDevice->GetAddress());
      // netDevice->SetDstAddress(netDeviceGS->GetAddress());
    }

      // return;

  }
}

void CreateGSLs() {

  // Link helper
  GSLHelper gsl_helper;
  std::string max_queue_size_str = format_string("%" PRId64 "p", m_gsl_max_queue_size_pkts);
  gsl_helper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(max_queue_size_str)));
  gsl_helper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string(m_gsl_data_rate_megabit_per_s) + "Mbps")));
  std::cout << "    >> GSL data rate........ " << m_gsl_data_rate_megabit_per_s << " Mbit/s" << std::endl;
  std::cout << "    >> GSL max queue size... " << m_gsl_max_queue_size_pkts << " packets" << std::endl;

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

void importDynamicStateSat(ns3::NodeContainer nodes, string dname) {
    // Iterate through the dynamic state directory
    for (const auto & entry : filesystem::directory_iterator(dname)) {
        // Extract nanoseconds from file name
        std::regex rgx(".*fstate_(\\w+)\\.txt.*");
        smatch match;
        string full_path = entry.path();
        if (!std::regex_search(full_path, match, rgx)) continue;
        double ms = stod(match[1]) / 1000000;

        // Add RemoveRoute schedule by emptying temporary set
        int current_node;
        // int destination_node;
        string prefix;
        int next_hop;

        // Read each file
        ifstream input(full_path);
        string line;
        while(getline(input, line))
        {
            vector<string> result;
            boost::split(result, line, boost::is_any_of(","));
            current_node = stoi(result[0]);
            // destination_node = stoi(result[1]);
            next_hop = stoi(result[2]);
            // Add AddRoute schedule
            prefix = "prefix/uid-" + result[1];
  
            if (current_node >= m_satelliteNodes.GetN()) {
              ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteGSL, nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
            } else if(next_hop >= m_satelliteNodes.GetN()) {
              ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteGSL, nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
            } else {
              ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteISL, nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
            }
        }
    }
}

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("20p"));

  // Configuration
  string ns3_config = "scenarios/config/3nodes_test.properties";
  readConfig(ns3_config);
  m_satellite_network_dir = getConfigParamOrDefault("satellite_network_dir", "network_dir");
  m_satellite_network_routes_dir =  getConfigParamOrDefault("satellite_network_routes_dir", "network_dir/routes_dir");
  m_satellite_network_force_static = parse_boolean(getConfigParamOrDefault("satellite_network_force_static", "false"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

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

  ReadISLs();

  CreateGSLs();
  // Install NDN stack on all nodes
  ndn::LeoStackHelper ndnHelper;
  // ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(m_allNodes);

  std::cout << "  > Installed NDN stacks" << std::endl;

  // Choosing forwarding strategy
  std::cout << "  > Installing forwarding strategy" << std::endl;
  ndn::StrategyChoiceHelper::Install(m_allNodes, "/prefix", "/localhost/nfd/strategy/multicast");

  // Installing applications
  std::string prefix = "prefix/uid-";
  int node1_id = stoi(getConfigParamOrDefault("consumer_id", "0"));
  int node2_id = stoi(getConfigParamOrDefault("producer_id", "0"));
  Ptr<Node> node1 = m_allNodes.Get(node1_id);
  Ptr<Node> node2 = m_allNodes.Get(node2_id);
  std::string prefix1 = prefix + to_string(node1_id);
  std::string prefix2 = prefix + to_string(node2_id);
  m_prefix = prefix2;
  // cout << "PREFIX: " << prefix2 << endl;
  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix(m_prefix);
  consumerHelper.SetAttribute("Frequency", StringValue("2")); // 10 interests a second
  consumerHelper.Install(node1); // first node

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix(m_prefix);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(node2); // last node

  cout << "Setting up FIB schedules..."  << endl;

  importDynamicStateSat(m_allNodes, m_satellite_network_routes_dir);

  cout << "Starting the simulation"  << endl;
  Simulator::Stop(Seconds(5.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}