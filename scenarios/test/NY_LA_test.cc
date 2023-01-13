// isl_test.cpp
#include "../ndn-sat-simulator.h"

using namespace ns3;
int
main(int argc, char* argv[])
{
  string ns3_config = "scenarios/config/NY_LA_test.properties";
  NDNSatSimulator sim = NDNSatSimulator(ns3_config);
  sim.Run();
}