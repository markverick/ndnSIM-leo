// read-data.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <regex>
#include <unordered_set>
#include <tuple>
#include <boost/algorithm/string.hpp>
#include "model/ground-station.h"
#include "read-data.h"
#include "model/tle.h"
#include "model/topo.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/helper/ndn-fib-helper.hpp"

#include "model/point-to-point-sat-net-device.h"
#include "model/point-to-point-sat-channel.h"
#include "model/point-to-point-sat-remote-channel.h"


using namespace std;
using namespace ns3;

namespace leo {

void RemoveRouteAB (ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode)
{
    ns3::ndn::FibHelper::RemoveRoute(node, prefix, otherNode);
}

void AddRouteAB (ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode, int metric)
{
    ns3::ndn::FibHelper::AddRoute(node, prefix, otherNode, metric);
}

// void AddSatRoute (ns3::Ptr<ns3::Node> node, string prefix, ns3::Ptr<ns3::Node> otherNode, int metric)
// {
//   for (uint32_t deviceId = 0; deviceId < node->GetNDevices(); deviceId++) {
//     Ptr<PointToPointSatNetDevice> netDevice =
//       DynamicCast<PointToPointSatNetDevice>(node->GetDevice(deviceId));
//     if (netDevice == 0)
//       continue;

//     Ptr<Channel> channel = netDevice->GetChannel();
//     if (channel == 0)
//       continue;

//     if (channel->GetDevice(0)->GetNode() == otherNode
//         || channel->GetDevice(1)->GetNode() == otherNode) {
//       Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
//       NS_ASSERT_MSG(ndn != 0, "Ndn stack should be installed on the node");

//       shared_ptr<ns3::ndn::Face> face = ndn->getFaceByNetDevice(netDevice);
//       NS_ASSERT_MSG(face != 0, "There is no face associated with the p2p link");
//       ns3::ndn::FibHelper::AddRoute(node, prefix, face, metric);

//       return;
//     }
//   }

//   NS_FATAL_ERROR("Cannot add route: Node# " << node->GetId() << " and Node# " << otherNode->GetId()
//                                             << " are not connected");
// }
vector<leo::GroundStation> readGroundStations(string fname, int offset)
{
    vector<leo::GroundStation> groundStations; 
    ifstream input(fname);
    string line;
    while(getline(input, line))
    {
        vector<string> result;
        boost::split(result, line, boost::is_any_of(","));
        groundStations.emplace_back(leo::GroundStation(result, offset));
        // for (size_t i = 0; i < result.size(); i++)
        //     cout << result[i] << " ";
        // cout << "\n";
    }
    cout << "Imported " << groundStations.size() << " ground stations from " << fname << endl; 
  if (!groundStations.empty()) cout << "ID(s): " << groundStations[0].m_uid << " <--> "<< groundStations [groundStations.size() - 1].m_uid << endl;
    return groundStations;
}

vector<leo::GroundStation> readGroundStations(string fname)
{
    return readGroundStations(fname, 0);
}

vector<leo::Tle> readTles(string fname)
{
    vector<leo::Tle> Tles; 
    ifstream input(fname);

    int orbit_count;
    int sat_count_per_orbit;
    string nums;
    getline(input, nums);
    stringstream ss(nums);
    ss >> orbit_count >> sat_count_per_orbit;

    string title;
    string line1;
    string line2;
    for (int orbit = 0; orbit < orbit_count; orbit++) {
        for (int sat = 0; sat < sat_count_per_orbit; sat++) {
            getline(input, title);
            getline(input, line1);
            getline(input, line2);
            // cout << title << endl;
            Tles.emplace_back(leo::Tle(orbit, orbit * sat_count_per_orbit + sat,
                              title, line1, line2));
        }
    }
    cout << "Imported " << Tles.size() << " TLEs from " << fname << endl;
    if (!Tles.empty()) cout << "ID(s): " << Tles[0].m_uid << " <--> "<< Tles [Tles.size() - 1].m_uid << endl;
    return Tles;
}

unordered_set<pair<int,int>, leo::pairhash> populateTopology(string dname) {
    unordered_set<pair<int, int>, leo::pairhash> topos;
    for (const auto & entry : filesystem::directory_iterator(dname)) {
        string full_path = entry.path();

        // Add RemoveRoute schedule by emptying temporary set
        int current_node;
        int next_hop;

        // Read each file
        ifstream input(full_path);
        string line;
        while(getline(input, line))
        {
            vector<string> result;
            boost::split(result, line, boost::is_any_of(","));
            current_node = stoi(result[0]);
            next_hop = stoi(result[2]);
            pair<int,int> p = current_node > next_hop? make_pair(next_hop, current_node): make_pair(current_node, next_hop);
            topos.insert(p);
        }
    }
    return topos;
}

vector<leo::Topo> readIsls(string fname)
{
    vector<leo::Topo> topo; 
    ifstream input(fname);
    string line;
    while(getline(input, line))
    {
        int uid_1;
        int uid_2;
        (stringstream) (line) >> uid_1 >> uid_2;
        topo.emplace_back(leo::Topo(uid_1, uid_2));
    }
    cout << "Imported " << topo.size() << " topology entries from " << fname << endl;
    return topo;
}

void importDynamicState(ns3::NodeContainer nodes, string dname) {
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
            prefix = "/uid-" + result[1];
            // cout << current_node << ", " << prefix << ", " << next_hop << endl;
            // AddRouteAB(nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
            // ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteAB, nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
            ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteAB, nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
        }
    }
}

/*
void importDynamicState(ns3::NodeContainer nodes, string dname) {
    cout << "Importing FIB" << endl;
    // Alternating states between two adjacent epoch
    set<tuple<int, int, int> > states[2];
    int i = 0;

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
        int destination_node;
        string prefix;
        int next_hop;
        auto it = states[i].begin();
        while (it != states[i].end()) {
            tie(current_node, destination_node, next_hop) = *it;
            prefix = "/uid-" + destination_node;
            ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &RemoveRouteAB, nodes.Get(current_node), prefix, nodes.Get(next_hop));
            states[i].erase(it);
        }

        // Read each file
        ifstream input(full_path);
        string line;
        while(getline(input, line))
        {
            input >> current_node >> destination_node >> next_hop;
            // Add AddRoute schedule
            tuple hop = make_tuple(current_node, destination_node, next_hop);
            if (states[1 - i].find(hop) == states[1 - i].end()) {
                ns3::Simulator::Schedule(ns3::MilliSeconds(ms), &AddRouteAB, nodes.Get(current_node), prefix, nodes.Get(next_hop), 1);
            }
        }
        // Alternate between states to save computation and memory
        i = 1 - i;
    }
}
*/

}
