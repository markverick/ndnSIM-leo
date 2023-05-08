// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"

namespace ns3 {

class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    double simulation_seconds = 500;
    std::string prefix = "/prefix/uid-";
    Ptr<Node> node1 = m_allNodes.Get(m_node1_id);
    Ptr<Node> node2 = m_allNodes.Get(m_node2_id);
    std::string prefix1 = prefix + to_string(m_node1_id);
    std::string prefix2 = prefix + to_string(m_node2_id);
    m_prefix = prefix2;
    // cout << "PREFIX: " << prefix2 << endl;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerWindow");
    // Consumer will request /prefix/0, /prefix/1, ...
    consumerHelper.SetPrefix(m_prefix);
    consumerHelper.SetAttribute("PayloadSize", StringValue("1380"));
    consumerHelper.SetAttribute("Window", StringValue("10"));
    consumerHelper.SetAttribute("RetxTimer", StringValue("1s"));
    consumerHelper.Install(node1).Start(MilliSeconds(1)); // first node

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("1380"));
    producerHelper.Install(node2).Start(MilliSeconds(1)); // last node

    cout << "Setting up FIB schedules..."  << endl;

    ImportDynamicStateSat(m_allNodes, m_satellite_network_routes_dir);

    cout << "Starting the simulation"  << endl;
    Simulator::Stop(Seconds(simulation_seconds));

    Simulator::Run();
    Simulator::Destroy();
  }
};
}
// NS_LOG=ndn.Consumer ./waf --run=a_b_starlink_BJ_NY_w10 |& tee -a logs/window/ndn-w10-04.txt
// NS_LOG=ndn.Consumer ./waf --run=a_b_starlink_BJ_NY_window |& tee -a logs/ndn_window_ll_10.txt
// ./waf --run=nocache |& tee -a logs/nocache/free_l0.txt
int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/a_b_starlink_nocache.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
