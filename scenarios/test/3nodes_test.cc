// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"

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
    // cout << "PREFIX: " << prefix2 << endl;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    // Consumer will request /prefix/0, /prefix/1, ...

    consumerHelper.SetPrefix(m_prefix);
    consumerHelper.SetAttribute("Frequency", StringValue(to_string(m_interest_per_second)));
    consumerHelper.Install(node1); // first node

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue(to_string(m_payload_size)));
    producerHelper.Install(node2); // last node

    cout << "Setting up FIB schedules..."  << endl;

    ImportDynamicStateSat(m_allNodes, m_satellite_network_routes_dir);

    cout << "Starting the simulation"  << endl;
    Simulator::Stop(Seconds(500));

    Simulator::Run();
    Simulator::Destroy();
  }
};
}

int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/3nodes_test.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
