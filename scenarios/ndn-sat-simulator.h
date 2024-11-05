#ifndef NDN_SAT_SIMULATOR_H
#define NDN_SAT_SIMULATOR_H

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
// #include "ns3/ndn-multicast-net-device-transport.h"
#include "ns3/ndn-leo-stack-helper.h"

namespace ns3 {

class NDNSatSimulator {
public:

  NDNSatSimulator(string config);

  void Run();

  std::string getConfigParamOrDefault(std::string key, std::string default_value);

  void ReadConfig(std::string conf);

  void ReadSatellites();

  void ReadGroundStations();

  void ReadISLs();

  // void AddRouteISL(ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode, int metric);

  // void AddRouteGSL(ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode, int metric);

  void AddGSLs();

  void ImportDynamicStateSat(ns3::NodeContainer nodes, string dname, int retx, bool complete);

  void ImportDynamicStateSat(ns3::NodeContainer nodes, string dname, int retx, bool complete, double limit);

  // Input
  std::string m_satellite_network_dir;          //<! Directory containing satellite network information
  std::string m_satellite_network_routes_dir;   //<! Directory containing the routes over time of the network
  bool m_satellite_network_force_static;        //<! True to disable satellite movement and basically run
                                              //   it static at t=0 (like a static network)
  std::string m_prefix;                         // NDN's prefix
  std::string m_name;

  // Generated state
  NodeContainer m_allNodes;                           //!< All nodes
  NodeContainer m_groundStationNodes;                 //!< Ground station nodes
  NodeContainer m_satelliteNodes;                     //!< Satellite nodes
  std::vector<Ptr<GroundStation> > m_groundStations;  //!< Ground stations
  std::vector<Ptr<Satellite>> m_satellites;           //<! Satellites
  std::set<int64_t> m_endpoints;                      //<! Endpoint ids = ground station ids
  std::shared_ptr<map<pair<uint32_t, string>, tuple<shared_ptr<ns3::ndn::Face>, shared_ptr<ns3::ndn::Face>, Address> > > m_cur_next_hop;
  std::shared_ptr<map<pair<uint32_t, string>, pair<shared_ptr<ns3::ndn::Face>, Address > > > m_active_hop_count;
  // std::vector<std::tuple<double, Ptr<Node>, string, Ptr<PointToPointLaserNetDevice> > > m_pending_fib;

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
  double m_isl_error_rate;
  double m_gsl_error_rate;
  bool m_enable_isl_utilization_tracking;
  int64_t m_isl_utilization_tracking_interval_ns;
  int64_t m_node1_id;
  int64_t m_node2_id;
};

}

#endif // NDN_SAT_SIMUALTOR_H