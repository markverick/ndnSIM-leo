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

NodeContainer ReadSatellites(
  std::string m_satellite_network_dir,
  bool m_satellite_network_force_static,
  std::vector<Ptr<Satellite>> m_satellites)
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
    NodeContainer m_satelliteNodes;
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
    // for (auto node : m_satelliteNodes) {
    //   cout << node->GetId() << endl;
    // }
    fs.close();
    return m_satelliteNodes;
}

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("20p"));

  std::string m_satellite_network_dir = "scratch/network_dir";                   //<! Directory containing satellite network information
  std::string m_satellite_network_routes_dir = "scratch/network_dir/routes_dir"; //<! Directory containing the routes over time of the network
  bool m_satellite_network_force_static = false;        //<! True to disable satellite movement and basically run
                                                        //   it static at t=0 (like a static network)
  // Generated state
  NodeContainer m_allNodes;                           //!< All nodes
  NodeContainer m_groundStationNodes;                 //!< Ground station nodes
  NodeContainer m_satelliteNodes;                     //!< Satellite nodes
  std::vector<Ptr<GroundStation> > m_groundStations;  //!< Ground stations
  std::vector<Ptr<Satellite>> m_satellites;           //<! Satellites
  std::set<int64_t> m_endpoints;                      //<! Endpoint ids = ground station ids

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.AddValue ("network_dir", "Directory containing satellite network information", m_satellite_network_dir);
  cmd.AddValue ("routes_dir", "Directory containing the routes over time of the network", m_satellite_network_routes_dir);
  cmd.AddValue ("force_static", "True to disable satellite movement", m_satellite_network_force_static);
  cmd.Parse(argc, argv);

  // Reading nodes
  
  m_satelliteNodes = ReadSatellites(
    m_satellite_network_dir,
    m_satellite_network_force_static,
    m_satellites
  );
  // Installing


  // // Install NDN stack on all nodes
  // ndn::StackHelper ndnHelper;
  // ndnHelper.SetDefaultRoutes(true);
  // ndnHelper.InstallAll();

  // // Choosing forwarding strategy
  // ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");

  // // Installing applications

  // // Consumer
  // ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // // Consumer will request /prefix/0, /prefix/1, ...
  // consumerHelper.SetPrefix("/prefix");
  // consumerHelper.SetAttribute("Frequency", StringValue("10")); // 10 interests a second
  // auto apps = consumerHelper.Install(nodes.Get(0));                        // first node
  // apps.Stop(Seconds(10.0)); // stop the consumer app at 10 seconds mark

  // // Producer
  // ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // // Producer will reply to all requests starting with /prefix
  // producerHelper.SetPrefix("/prefix");
  // producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  // producerHelper.Install(nodes.Get(2)); // last node

  // Simulator::Stop(Seconds(20.0));

  // Simulator::Run();
  // Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}