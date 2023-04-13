// 3nodes_test.cpp
#include "../ndn-sat-simulator.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("test01");
void sched3 () {
  cout << "Sched3" << endl;
  NS_LOG_INFO("Sched3");
}

void sched2 () {
  cout << "Sched2" << endl;
  NS_LOG_INFO("Sched2");
  Simulator::Schedule(Seconds(22), &sched3);
}

void sched () {
  cout << "Sched" << endl;
  NS_LOG_INFO("Sched");
  Simulator::Schedule(Seconds(11), &sched2);
}


class ScenarioSim : public NDNSatSimulator {
public:
  using NDNSatSimulator::NDNSatSimulator;
  void Run() {
    NS_LOG_FUNCTION_NOARGS();
    Simulator::Schedule(Seconds(5), &sched);
    Simulator::Stop(Seconds(500));

    Simulator::Run();
    Simulator::Destroy();
  }
};
}

int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/a_b_3nodes.properties";
  ns3::ScenarioSim sim = ns3::ScenarioSim(ns3_config);
  sim.Run();
  return 0;
}
