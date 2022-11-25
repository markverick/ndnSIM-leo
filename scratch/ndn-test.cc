// ndn-simple.cpp


#include <utility>
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

namespace ns3 {

// Input
Ptr<BasicSimulation> m_basicSimulation;       //<! Basic simulation instance
std::string m_satellite_network_dir;          //<! Directory containing satellite network information
std::string m_satellite_network_routes_dir;   //<! Directory containing the routes over time of the network
bool m_satellite_network_force_static;        //<! True to disable satellite movement and basically run
                                              //   it static at t=0 (like a static network)

// Generated state
NodeContainer m_allNodes;                           //!< All nodes
NodeContainer m_groundStationNodes;                 //!< Ground station nodes
NodeContainer m_satelliteNodes;                     //!< Satellite nodes
std::vector<Ptr<GroundStation> > m_groundStations;  //!< Ground stations
std::vector<Ptr<Satellite>> m_satellites;           //<! Satellites
std::set<int64_t> m_endpoints;                      //<! Endpoint ids = ground station ids

// ISL devices
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

void readConfig() {
  // Read the config
  m_config = read_config("scratch/config_ns3.properties");

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
  TrafficControlHelper tch_isl;
  tch_isl.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", QueueSizeValue(QueueSize("1p"))); // Will be removed later any case

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

      // Install traffic control helper
      // tch_isl.Install(netDevices.Get(0));
      // tch_isl.Install(netDevices.Get(1));

      // Assign some IP address (nothing smart, no aggregation, just some IP address)
      // m_ipv4_helper.Assign(netDevices);
      // m_ipv4_helper.NewNetwork();

      // Remove the traffic control layer (must be done here, else the Ipv4 helper will assign a default one)
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

void CreateGSLs() {

  // Link helper
  GSLHelper gsl_helper;
  std::string max_queue_size_str = format_string("%" PRId64 "p", m_gsl_max_queue_size_pkts);
  gsl_helper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue(QueueSize(max_queue_size_str)));
  gsl_helper.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (std::to_string(m_gsl_data_rate_megabit_per_s) + "Mbps")));
  std::cout << "    >> GSL data rate........ " << m_gsl_data_rate_megabit_per_s << " Mbit/s" << std::endl;
  std::cout << "    >> GSL max queue size... " << m_gsl_max_queue_size_pkts << " packets" << std::endl;

  // Traffic control helper
  TrafficControlHelper tch_gsl;
  tch_gsl.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", QueueSizeValue(QueueSize("1p")));  // Will be removed later any case

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
  tch_gsl.Install(devices);
  std::cout << "    >> Finished installing traffic control layer qdisc which will be removed later" << std::endl;

  // Assign IP addresses
  //
  // This is slow because of an inefficient implementation, if you want to speed it up, you can need to edit:
  // src/internet/helper/ipv4-address-helper.cc
  //
  // And then within function Ipv4AddressHelper::NewAddress (void), comment out:
  // Ipv4AddressGenerator::AddAllocated (addr);
  //
  // Beware that if you do this, and there are IP assignment conflicts, they are not detected.
  //
  // std::cout << "    >> Assigning IP addresses..." << std::endl;
  // std::cout << "       (with many interfaces, this can take long due to an inefficient IP assignment conflict checker)" << std::endl;
  // std::cout << "       Progress (as there are more entries, it becomes slower):" << std::endl;
  // int64_t start_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  // int64_t last_time_ns = start_time_ns;
  // for (uint32_t i = 0; i < devices.GetN(); i++) {

  //   // Assign IPv4 address
  //   m_ipv4_helper.Assign(devices.Get(i));
  //   m_ipv4_helper.NewNetwork();

  //   // Give a progress update if at an even 10%
  //   int update_interval = (int) std::ceil(devices.GetN() / 10.0);
  //   if (((i + 1) % update_interval) == 0 || (i + 1) == devices.GetN()) {
  //       int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  //       printf("       - %.2f%% (t = %.2f s, update took %.2f s)\n",
  //           (float) (i + 1) / (float) devices.GetN() * 100.0,
  //           (now_ns - start_time_ns) / 1e9,
  //           (now_ns - last_time_ns) / 1e9
  //       );
  //       last_time_ns = now_ns;
  //   }

  // }
  // std::cout << "    >> Finished assigning IPs" << std::endl;

  // Remove the traffic control layer (must be done here, else the Ipv4 helper will assign a default one)
  TrafficControlHelper tch_uninstaller;
  std::cout << "    >> Removing traffic control layers (qdiscs)..." << std::endl;
  for (uint32_t i = 0; i < devices.GetN(); i++) {
      tch_uninstaller.Uninstall(devices.Get(i));
  }
  std::cout << "    >> Finished removing GSL queueing disciplines" << std::endl;

  // Check that all interfaces were created
  NS_ABORT_MSG_IF(total_num_gsl_ifs != devices.GetN(), "Not the expected amount of interfaces has been created.");

  std::cout << "    >> GSL interfaces are setup" << std::endl;

}

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("20p"));

  // Configuration
  readConfig();
  m_satellite_network_dir = getConfigParamOrDefault("satellite_network_dir", "network_dir");
  m_satellite_network_routes_dir =  getConfigParamOrDefault("satellite_network_routes_dir", "network_dir/routes_dir");
  m_satellite_network_force_static = parse_boolean(getConfigParamOrDefault("satellite_network_force_static", "false"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.AddValue ("network_dir", "Directory containing satellite network information", m_satellite_network_dir);
  cmd.AddValue ("routes_dir", "Directory containing the routes over time of the network", m_satellite_network_routes_dir);
  cmd.AddValue ("force_static", "True to disable satellite movement", m_satellite_network_force_static);
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
  m_isl_data_rate_megabit_per_s = parse_positive_double(getConfigParamOrDefault("isl_data_rate_megabit_per_s", "10000"));
  m_gsl_data_rate_megabit_per_s = parse_positive_double(getConfigParamOrDefault("gsl_data_rate_megabit_per_s", "10000"));
  m_isl_max_queue_size_pkts = parse_positive_int64(getConfigParamOrDefault("isl_max_queue_size_pkts", "10000"));
  m_gsl_max_queue_size_pkts = parse_positive_int64(getConfigParamOrDefault("gsl_max_queue_size_pkts", "10000"));

  ReadISLs();

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(m_allNodes);
  std::cout << "  > Installed NDN stacks" << std::endl;

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");

  // Installing applications
  Ptr<Node> node1 = m_allNodes.Get(m_satellites.size() + 35); // 35,Krung-Thep-(Bangkok)
  Ptr<Node> node2 = m_allNodes.Get(m_satellites.size() + 20); // 20, Los-Angeles-Long-Beach-Santa-Ana
  std::string prefix1 = "/prefix";
  std::string prefix2 = "/prefix";
  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix(prefix1);
  consumerHelper.SetAttribute("Frequency", StringValue("2")); // 10 interests a second
  consumerHelper.Install(node1); // first node

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix(prefix1);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(node2); // last node

  Simulator::Stop(Seconds(20.0));

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