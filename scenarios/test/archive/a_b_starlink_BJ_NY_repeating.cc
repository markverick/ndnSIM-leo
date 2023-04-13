// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"
// #include <nstime.h>
#include "ns3/ndnSIM/apps/ndn-consumer.hpp"
#define END_TIME 10

namespace ns3 {

class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    std::string prefix = "/prefix/uid-";
    Ptr<Node> node1 = m_allNodes.Get(m_node1_id);
    Ptr<Node> node2 = m_allNodes.Get(m_node2_id);
    std::string prefix1 = prefix + to_string(m_node1_id);
    std::string prefix2 = prefix + to_string(m_node2_id);
    m_prefix = prefix2;

    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix(m_prefix);
    consumerHelper.SetAttribute("Frequency", StringValue("1"));
    consumerHelper.SetAttribute("StartSeq", StringValue("0"));
    consumerHelper.Install(node1); // first node

    consumerHelper.SetAttribute("StartSeq", StringValue("5"));
     consumerHelper.Install(node1); // first node

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("0"));
    producerHelper.Install(node2); // last node

    cout << "Setting up FIB schedules..."  << endl;

    ImportDynamicStateSat(m_allNodes, m_satellite_network_routes_dir);

    cout << "Starting the simulation"  << endl;
    Simulator::Stop(Seconds(10));

    Simulator::Run();
    Simulator::Destroy();
  }
};
}

int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/a_b_starlink.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
