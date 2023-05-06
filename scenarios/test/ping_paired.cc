// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"

namespace ns3 {

class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    std::string prefix = "/prefix/uid-";
    Ptr<Node> node1 = m_allNodes.Get(1590);
    Ptr<Node> node2 = m_allNodes.Get(1593);
    std::string prefix1 = prefix + to_string(m_node1_id);
    std::string prefix2 = prefix + to_string(m_node2_id);
    m_prefix = prefix2;

    ImportDynamicStateSatGSLUnicast(m_allNodes, m_satellite_network_routes_dir, 1590, 1593);
    // cout << "PREFIX: " << prefix2 << endl;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerPing");
    // Consumer will request /prefix/0, /prefix/1, ...
    consumerHelper.SetPrefix(m_prefix);
    consumerHelper.SetAttribute("MaxSeq", StringValue("1"));
    consumerHelper.SetAttribute("Frequency", StringValue("100"));
    consumerHelper.Install(node1).Start(Seconds(184.48)); // first node

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("0"));
    producerHelper.Install(node2).Start(MilliSeconds(1)); // last node

    cout << "Setting up FIB schedules..."  << endl;

    cout << "Starting the simulation"  << endl;
    Simulator::Stop(Seconds(500));

    Simulator::Run();
    Simulator::Destroy();
  }
};
}
// NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=a_b_starlink_BJ_NY_ping |& tee -a logs/ndn_ping_loss_01.txt

// NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=a_b_starlink_BJ_NY_ping_unicast |& tee -a logs/gsl-unicast-multicast/gsl-unicast-01.txt
int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/a_b_starlink_unicast.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
