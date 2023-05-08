// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"

namespace ns3 {

class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    double simulation_seconds = 155;
    std::string prefix = "/prefix/uid-";
    Ptr<Node> node1 = m_allNodes.Get(1590); // Beijing
    Ptr<Node> node2 = m_allNodes.Get(1586); // Shanghai
    Ptr<Node> node3 = m_allNodes.Get(1593); // New York
    m_prefix = prefix + to_string(1593);;
    // cout << "PREFIX: " << prefix2 << endl;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    // Beijing First
    consumerHelper.SetPrefix(m_prefix);
    // consumerHelper.SetAttribute("PayloadSize", StringValue("1380"));
    consumerHelper.SetAttribute("Frequency", StringValue("100")); // 1 Megabytes
    consumerHelper.SetAttribute("StartSeq", StringValue("0"));
    consumerHelper.SetAttribute("MaxSeq", StringValue("10000"));
    consumerHelper.Install(node1).Start(MilliSeconds(1)); // Beijing

    // Shanghai Second
    consumerHelper.SetPrefix(m_prefix);
    // consumerHelper.SetAttribute("PayloadSize", StringValue("1380"));
    consumerHelper.SetAttribute("Frequency", StringValue("100")); // 1 Megabytes
    consumerHelper.SetAttribute("StartSeq", StringValue("0"));
    consumerHelper.SetAttribute("MaxSeq", StringValue("10000"));
    consumerHelper.Install(node2).Start(Seconds(50) + MilliSeconds(1)); // Shanghai

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("1380"));
    producerHelper.Install(node3).Start(MilliSeconds(1)); // last node

    cout << "Setting up FIB schedules..."  << endl;

    ImportDynamicStateSat(m_allNodes, m_satellite_network_routes_dir);

    cout << "Starting the simulation"  << endl;
    Simulator::Stop(Seconds(simulation_seconds));

    Simulator::Run();
    Simulator::Destroy();
  }
};
}
// ./waf --run=cache_cbr |& tee -a logs/cache-cbr/overlap-100-50/ndn-cbr-both
// NS_LOG=ndn.Consumer ./waf --run=a_b_starlink_BJ_NY_w10 |& tee -a logs/window/ndn-w10-04.txt
// NS_LOG=ndn.Consumer ./waf --run=a_b_starlink_BJ_NY_window |& tee -a logs/ndn_window_ll_10.txt
// ./waf --run=a_b_starlink_BJ_NY_w10 |& tee -a logs/window-loss-4/ndn_w10_le-7.txt
int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/a_b_starlink.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
