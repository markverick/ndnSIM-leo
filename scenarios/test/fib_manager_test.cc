#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

void RemoveRouteCustom2(Ptr<Node> node, string prefix, shared_ptr<ns3::ndn::Face> face) {
  ndn::FibHelper::RemoveRoute(node, prefix, face);
}

void AddRouteCustom2(Ptr<Node> node, string prefix, shared_ptr<ns3::ndn::Face> face, int metric) {
  ndn::FibHelper::AddRoute(node, prefix, face, metric);
}

void printFibTable2(Ptr<Node> node) {
  cout << "Node: " << node->GetId() << endl;
  Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  auto fw = ndn->getForwarder();
  auto &fib = fw->getFib();
  for (auto it = fib.begin(); it != fib.end(); it++) {
    for (auto i = it->getNextHops().begin(); i != it->getNextHops().end(); i++) {
      cout << "  " << it->getPrefix() << "," << i->getFace().getId() << endl;
    }
  }
}

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("20p"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(5);

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));
  p2p.Install(nodes.Get(0), nodes.Get(3));
  p2p.Install(nodes.Get(3), nodes.Get(4));
  p2p.Install(nodes.Get(4), nodes.Get(2));
  // p2p.Install(nodes.Get(4), nodes.Get(5));
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(false);
  ndnHelper.InstallAll();

  Ptr<Node> node = nodes.Get(0);
  printf("%d %d %d\n", nodes.Get(0)->GetNDevices(), nodes.Get(1)->GetNDevices(), nodes.Get(2)->GetNDevices());
  // Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  // node->GetDevice(0);
  // ndn->getFaceByNetDevice(node->GetDevice(0));
  shared_ptr<ns3::ndn::Face> face = node->GetObject<ns3::ndn::L3Protocol>()->getFaceByNetDevice(node->GetDevice(0));
   shared_ptr<ns3::ndn::Face> face2 = node->GetObject<ns3::ndn::L3Protocol>()->getFaceByNetDevice(node->GetDevice(1));
  // Install NDN stack on all nodes
  

  
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/best-route");

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue("100")); // 10 interests a second
  auto apps = consumerHelper.Install(nodes.Get(0));                        // first node
  apps.Stop(Seconds(0.0001)); // stop the consumer app at 10 MilliSeconds mark

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(2)); // last node

  ndn::FibHelper::AddRoute(nodes.Get(0), "/prefix", nodes.Get(1), 1);
  ndn::FibHelper::AddRoute(nodes.Get(1), "/prefix", nodes.Get(2), 1);
  // ndn::FibHelper::AddRoute(nodes.Get(0), "/prefix", nodes.Get(3), 1);
  ndn::FibHelper::AddRoute(nodes.Get(3), "/prefix", nodes.Get(4), 1);
  ndn::FibHelper::AddRoute(nodes.Get(4), "/prefix", nodes.Get(2), 1);
  // for (int i = 200; i < 2000; i++) {

  // }
  // 600 works
  // 400 does not works
  Simulator::Schedule(MilliSeconds(200), &RemoveRouteCustom2, nodes.Get(0), "/prefix", face);
  Simulator::Schedule(MilliSeconds(400), &AddRouteCustom2, nodes.Get(0), "/prefix", face2, 2);
  Simulator::Schedule(MilliSeconds(600), &RemoveRouteCustom2, nodes.Get(0), "/prefix", face2);
  Simulator::Schedule(MilliSeconds(800), &AddRouteCustom2, nodes.Get(0), "/prefix", face, 3);
  Simulator::Schedule(MilliSeconds(1000), &RemoveRouteCustom2, nodes.Get(0), "/prefix", face);
  Simulator::Schedule(MilliSeconds(1200), &AddRouteCustom2, nodes.Get(0), "/prefix", face2, 4);
  // Simulator::Schedule(MilliSeconds(1400), &RemoveRouteCustom2, nodes.Get(0), "/prefix", face2);
  // Simulator::Schedule(MilliSeconds(1600), &AddRouteCustom2, nodes.Get(0), "/prefix", face, 1);
  // Choosing forwarding strategy

  Simulator::Stop(MilliSeconds(500000.0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(50), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(250), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(450), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(650), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(850), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(1050), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::MilliSeconds(1250), &printFibTable2, nodes.Get(0));
  // ns3::Simulator::Schedule(ns3::MilliSeconds(3100), &printFibTable2, nodes.Get(0));
  // ns3::Simulator::Schedule(ns3::MilliSeconds(6500), &printFibTable2, nodes.Get(0));
  // ns3::Simulator::Schedule(ns3::MilliSeconds(13000), &printFibTable2, nodes.Get(0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}