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
  nodes.Create(6);

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));
  p2p.Install(nodes.Get(2), nodes.Get(3));
  p2p.Install(nodes.Get(0), nodes.Get(3));
  p2p.Install(nodes.Get(3), nodes.Get(4));
  p2p.Install(nodes.Get(4), nodes.Get(5));
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(false);
  ndnHelper.InstallAll();

  Ptr<Node> node = nodes.Get(0);
  printf("%d %d %d\n", nodes.Get(0)->GetNDevices(), nodes.Get(1)->GetNDevices(), nodes.Get(2)->GetNDevices());
  // Ptr<ns3::ndn::L3Protocol> ndn = node->GetObject<ns3::ndn::L3Protocol>();
  // node->GetDevice(0);
  // ndn->getFaceByNetDevice(node->GetDevice(0));
  shared_ptr<ns3::ndn::Face> face = node->GetObject<ns3::ndn::L3Protocol>()->getFaceByNetDevice(node->GetDevice(0));
  // Install NDN stack on all nodes
  

  
  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/best-route");

  // Installing applications

  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue("0.01")); // 10 interests a second
  auto apps = consumerHelper.Install(nodes.Get(0));                        // first node
  apps.Stop(Seconds(1000.0)); // stop the consumer app at 10 seconds mark

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(2)); // last node

  ndn::FibHelper::AddRoute(nodes.Get(0), "/prefix", nodes.Get(1), 1);
  ndn::FibHelper::AddRoute(nodes.Get(1), "/prefix", nodes.Get(2), 1);
  Simulator::Schedule(Seconds(300), &RemoveRouteCustom2, nodes.Get(0), "/prefix", face);
  Simulator::Schedule(Seconds(500), &AddRouteCustom2, nodes.Get(0), "/prefix", face, 2);
  Simulator::Schedule(Seconds(700), &RemoveRouteCustom2, nodes.Get(0), "/prefix", face);
  Simulator::Schedule(Seconds(900), &AddRouteCustom2, nodes.Get(0), "/prefix", face, 3);
  // Choosing forwarding strategy

  Simulator::Stop(Seconds(5000.0));
  ns3::Simulator::Schedule(ns3::Seconds(200), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::Seconds(400), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::Seconds(600), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::Seconds(800), &printFibTable2, nodes.Get(0));
  ns3::Simulator::Schedule(ns3::Seconds(1000), &printFibTable2, nodes.Get(0));
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