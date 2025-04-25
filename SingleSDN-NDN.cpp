
// Network topology
//
//         n0     n1
//         |      |
//   1Mbps |      |  1Mbps 
//         |      |
//        ----------
//       | OF Switch |
//       |Controller |  or Remote Controller
//        ----------


#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"

#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-module.h"


//using namespace ns3;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Adding OpenFlowCsmaSwitch to an NDN Senario");


bool verbose = false;
bool use_drop = false;
ns3::Time timeout = ns3::Seconds (0);

bool
SetVerbose (std::string value)
{
  verbose = true;
  return true;
}

bool
SetDrop (std::string value)
{
  use_drop = true;
  return true;
}

bool
SetTimeout (std::string value)
{
  try {
      timeout = ns3::Seconds (atof (value.c_str ()));
      return true;
    }
  catch (...) { return false; }
  return false;
}

int
main (int argc, char *argv[])
{
  
  #ifdef NS3_OPENFLOW
  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  CommandLine cmd;
  cmd.AddValue ("v", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("verbose", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("d", "Use Drop Controller (Learning if not specified).", MakeCallback (&SetDrop));
  cmd.AddValue ("drop", "Use Drop Controller (Learning if not specified).", MakeCallback (&SetDrop));
  cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  cmd.Parse (argc, argv);
  

  if (verbose)
    {
      LogComponentEnable ("OpenFlowCsmaSwitchExample", LOG_LEVEL_INFO);
      LogComponentEnable ("OpenFlowInterface", LOG_LEVEL_INFO);
      LogComponentEnable ("OpenFlowSwitchNetDevice", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("50p"));
  // Creating nodes  from topology text file
  AnnotatedTopologyReader topologyReader("", 1);
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/topo-tree-Single.txt");
  topologyReader.Read();
  
     // Getting containers for the consumer/producer and controller Cont1
  Ptr<Node> consumer = Names::Find<Node>("leaf-1");
  Ptr<Node> producer = Names::Find<Node>("leaf-2");
  Ptr<Node> switchNode = Names::Find<Node>("Cont1"); // The controller node
 
  NodeContainer terminals;
  terminals.Create (5); // Producer & Consumer and 3 NDN Nodes

  NodeContainer SwitchContainer;
  SwitchContainer.Create (1); // OF Switch Controller

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (1000000000));
  
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (30)));
  // the priviose two lines are similar to :
       // setting default parameters for PointToPoint links and channels
       //Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
       //Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));

  // Create the csma links, from each terminals to the switch
  NetDeviceContainer terminalDevices;
  NetDeviceContainer switchDevices;
  
 // for (int i = 0; i < 2; i++)
   // {
     // NetDeviceContainer link = csma.Install (NodeContainer (terminals.Get (i), SwitchContainer));
      //terminalDevices.Add (link.Get (0));
      //switchDevices.Add (link.Get (1));  
    //}

  // Create the Controller netdevice, which will do the packet switching (Install OpenFlow switch)
  //Ptr<Node> switchNode = SwitchContainer.Get (0);
  OpenFlowSwitchHelper swtch;

  if (use_drop)
    {
      Ptr<ns3::ofi::DropController> controller = CreateObject<ns3::ofi::DropController> ();
      swtch.Install (switchNode, switchDevices, controller);
    }
  else
    {
      Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller->SetAttribute ("ExpirationTime", TimeValue (timeout));
      swtch.Install (switchNode, switchDevices, controller);
    }
  // Add internet stack to the terminals
  //InternetStackHelper internet;
  //internet.Install (terminals);
  
  // Install NDN stack
    ndn::StackHelper ndnHelper;
    ndnHelper.SetDefaultRoutes(true);
    ndnHelper.setCsSize(1000); // Set Content Store Size to 1000 Packets
    ndnHelper.InstallAll();
    // Here is the first Scenario (one pair of Consumer/Producer):
    //  Set up producer application
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    // Producer will reply to all requests starting with /example/data
    producerHelper.SetPrefix("/example/data");
    producerHelper.SetAttribute("PayloadSize", StringValue("102400")); // 100 MB - the unit is: KB
    producerHelper.Install(producer); // Install on the producer node

    //  Set up consumer application
    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix("/example/data");
    consumerHelper.SetAttribute("Frequency", StringValue("100")); // Interest frequency within a second
    auto apps =consumerHelper.Install(consumer); // Install on the consumer node
    apps.Stop(Seconds(10.0)); // stop the consumer app at 10 seconds mark

    // Choosing forwarding strategy
    ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
    ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
    ndnGlobalRoutingHelper.InstallAll();
    
 
  NS_LOG_INFO ("Configure Tracing.");

// Metrics:
    
    ndn::AppDelayTracer::InstallAll ("Single-Delays-trace.txt"); //Delay
    

    L2RateTracer::InstallAll("Single-drop-trace.txt", Seconds(0.5)); //packet drop rate (overflow)
  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     openflow-switch-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcapAll ("openflow-switch", false);

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop(Seconds(100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
  #else
  NS_LOG_INFO ("NS-3 OpenFlow is not enabled. Cannot run simulation.");
  #endif // NS3_OPENFLOW
  
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}




