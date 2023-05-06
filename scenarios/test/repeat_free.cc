// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"

namespace ns3 {

class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    double simulation_seconds = 500;
    std::string prefix = "/prefix/uid-";
    Ptr<Node> node1 = m_allNodes.Get(1590); // Beijing
    Ptr<Node> node2 = m_allNodes.Get(1593); // New York
    m_prefix = prefix + to_string(1593);;
    // cout << "PREFIX: " << prefix2 << endl;
    // Consumer
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerWindow");
    // Running 50 apps at the same? time
    // I wish multithreading worked
    for (int i = 1; i <= 20; i+= 1) {
      // Run first to fill the cache
      consumerHelper.SetPrefix(m_prefix + "/" + std::to_string(i));
      consumerHelper.SetAttribute("PayloadSize", StringValue("1380"));
      consumerHelper.SetAttribute("Size", StringValue("1")); // 1 Megabytes
      consumerHelper.SetAttribute("Window", StringValue("10"));
      consumerHelper.SetAttribute("RetxTimer", StringValue("1s"));
      consumerHelper.Install(node1).Start(MilliSeconds(1)); // Beijing

      // Repeated run
      consumerHelper.SetPrefix(m_prefix + "/" + std::to_string(i));
      consumerHelper.SetAttribute("PayloadSize", StringValue("1380"));
      consumerHelper.SetAttribute("Size", StringValue("1")); // 1 Megabytes
      consumerHelper.SetAttribute("Window", StringValue("10"));
      consumerHelper.SetAttribute("RetxTimer", StringValue("1s"));
      consumerHelper.Install(node1).Start(Seconds(i)); // Beijing

          // Producer
      ndn::AppHelper producerHelper("ns3::ndn::Producer");
      // Producer will reply to all requests starting with /prefix
      producerHelper.SetPrefix(m_prefix + "/" + std::to_string(i));
      producerHelper.SetAttribute("PayloadSize", StringValue("1380"));
      producerHelper.Install(node2).Start(MilliSeconds(1)); // New York
    }
   

 

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
// ./waf --run=a_b_starlink_BJ_NY_w10 |& tee -a logs/window-loss-4/ndn_w10_le-7.txt
int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/a_b_starlink.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
