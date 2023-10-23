// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"
#include "ns3/basic-simulation.h"

namespace ns3 {

class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    // Choosing forwarding strategy
    std::cout << "  > Installing forwarding strategy" << std::endl;
    ndn::StrategyChoiceHelper::Install(m_allNodes, "/", "/localhost/nfd/strategy/nack-retx");
    std::string prefix = "/leo/";
    Ptr<Node> node1 = m_allNodes.Get(m_node1_id);
    Ptr<Node> node2 = m_allNodes.Get(m_node2_id);
    std::string prefix1 = prefix + to_string(m_node1_id);
    std::string prefix2 = prefix + to_string(m_node2_id);
    m_prefix = prefix2;
    // cout << "PREFIX: " << prefix2 << endl;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerPing");
    // Consumer will request /leo/0, /leo/1, ...
    consumerHelper.SetPrefix(m_prefix);
    consumerHelper.SetAttribute("Frequency", StringValue("1000"));
    consumerHelper.SetAttribute("RetxTimer", StringValue("10000s"));
    consumerHelper.Install(node1).Start(Seconds(0.5)); // first node

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("1"));
    producerHelper.Install(node2).Start(Seconds(0.5)); // last node

    cout << "Setting up FIB schedules..."  << endl;

    // ImportDynamicStateSatInstantRetx(m_allNodes, m_satellite_network_routes_dir, m_node1_id, m_node2_id);
    ImportDynamicStateSat(m_allNodes, m_satellite_network_routes_dir, 0, false);
    cout << "Starting the simulation"  << endl;
    Simulator::Stop(Seconds(200));
    // int start_index = -1;
    // for (int i = m_satellite_network_dir.size() - 1; i >= 0; i--) {
    //   char c = m_satellite_network_dir[i];
    //   if (c == '/') {
    //     start_index = i + 1;
    //     break;
    //   }
    // }
    // string run_dir = m_satellite_network_dir.substr(start_index);
    // cout << "experiments/a_b/runs/" + m_name + "/app-delays-trace.txt" << endl;
    ndn::AppDelayTracer::InstallAll("experiments/a_b/runs/" + m_name + "/app-delays-trace.txt");
    Simulator::Run();
    Simulator::Destroy();
  }
};
}
// NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=a_b_starlink_BJ_NY_ping |& tee -a logs/ndn_ping_loss_01.txt

// NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=a_b_starlink_BJ_NY_ping |& tee -a logs/ping-multicast/instant-remove.txt
int
main(int argc, char* argv[])
{
  // No buffering of printf
  setbuf(stdout, nullptr);
  // Retrieve run directory
  ns3::CommandLine cmd;
  std::string run_dir = "";
  cmd.Usage("Usage: ./waf --run=\"run --run_dir='<path/to/run/directory>'\"");
  cmd.AddValue("run_dir",  "Run directory", run_dir);
  cmd.Parse(argc, argv);
  if (run_dir.compare("") == 0) {
      printf("Usage: ./waf --run=\"run --run_dir='<path/to/run/directory>'\"");
      return 0;
  }
  string ns3_config = "experiments/a_b/runs/" + run_dir + "/config_ns3.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
