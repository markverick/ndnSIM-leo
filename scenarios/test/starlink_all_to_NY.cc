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
    Ptr<Node> producerNode = m_allNodes.Get(1593); // New York
    m_prefix = prefix + to_string(1593);
    int satNum = 1584;
    for (int i = 0; i < 100; i++) {
      Ptr<Node> consumerNode = m_allNodes.Get(satNum + i); // New York
      ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
      consumerHelper.SetAttribute("Frequency", StringValue("1"));
      consumerHelper.SetPrefix(m_prefix);
      consumerHelper.SetAttribute("StartSeq", StringValue("0"));
      consumerHelper.Install(consumerNode).Start(Seconds(i + 1));
    }

    // Producer
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix(m_prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("0"));
    producerHelper.Install(producerNode).Start(Seconds(0));

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
  string ns3_config = "scenarios/config/a_b_starlink_static.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
