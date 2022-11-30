#ifndef READ_DATA
#define READ_DATA
#include "model/ground-station.h"
#include "model/tle.h"
#include "model/topo.h"
#include "ns3/network-module.h"

#include <vector>

namespace leo
{

// Explicitly pick an overloaded function for Schedule
void RemoveRouteAB (ns3::Ptr<ns3::Node> node, std::string prefix, ns3::Ptr<ns3::Node> otherNode);
void AddRouteAB (ns3::Ptr<ns3::Node> node, std::string prefix, ns3::Ptr<ns3::Node> otherNode, int metric);
struct pairhash {
public:
  template <typename T, typename U>
  std::size_t operator()(const std::pair<T, U> &x) const
  {
    return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
  }
};

std::vector<leo::GroundStation> readGroundStations(std::string fname);
std::vector<leo::GroundStation> readGroundStations(std::string fname, int offset);
std::vector<leo::Tle> readTles(std::string fname);
std::unordered_set<std::pair<int,int>, pairhash > populateTopology(std::string dname);
std::vector<leo::Topo> readIsls(std::string fname);
void importDynamicState(ns3::NodeContainer nodes, std::string dname);

}

#endif // READ_DATA_H