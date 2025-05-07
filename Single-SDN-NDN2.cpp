
// Network topology
//
//         n0     n1
//         |      |
//   1Gbps |      |  1Gbps 
//         |      |
//        ----------
//       | OF  |
//       |Controller | 
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
  topologyReader.SetFileName("src/ndnSIM/examples/topologies/topo-tree-Single2.txt");
  topologyReader.Read();


  
     // Getting containers for the Single controller Cont1

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

    // Here is the Second Scenario (two pair of Consumer/Producer):

  // Getting containers for the consumer/producer
  Ptr<Node> consumer1 = Names::Find<Node>("leaf-1");
  Ptr<Node> consumer2 = Names::Find<Node>("leaf-2");
  Ptr<Node> producer1 = Names::Find<Node>("leaf-3");
  Ptr<Node> producer2 = Names::Find<Node>("leaf-4");


// Create NDN applications (The Consumers)

  // on the first consumer node install a Consumer application
  // that will express interests in /example/data1 namespace
  
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetAttribute("Frequency", StringValue("100"));    // 100  interests per second
  consumerHelper.SetAttribute("LifeTime", TimeValue(Seconds(100.0)));
  consumerHelper.SetPrefix("/example/data1");
  ApplicationContainer consumerApps1 = consumerHelper.Install(consumer1); // Install Consumer1 on sender1

  // on the second consumer node install a Consumer application
  // that will express interests in /example/data2 namespace
  consumerHelper.SetPrefix("/example/data2");
  ApplicationContainer consumerApps2 = consumerHelper.Install(consumer2); // Install Consumer2 on sender2


// Create NDN applications (The Producers)
    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetAttribute("PayloadSize", StringValue("102400"));    //100 MB payload. the unit here is kB
    

  // Register /example/data1 prefix with global routing controller and
  // install producer that will satisfy Interests in /example/data1 namespace
  ndnGlobalRoutingHelper.AddOrigins("/example/data1", producer1);
  producerHelper.SetPrefix("/example/data1"); 
  producerHelper.Install(producer1); // Install Producer1 on receiver1

  // Register /example/data2 prefix with global routing controller and
  // install producer that will satisfy Interests in /example/data2 namespace
  ndnGlobalRoutingHelper.AddOrigins("/example/data2", producer2);
  producerHelper.SetPrefix("/example/data2");
  producerHelper.Install(producer2); // Install Producer2 on receiver2


  NS_LOG_INFO ("Configure Tracing.");

// Metrics:
    
    ndn::AppDelayTracer::InstallAll ("Single-Delays-trace.txt"); //Delay
    

    L2RateTracer::InstallAll("Single-drop-trace.txt", Seconds(0.5)); //packet drop rate (overflow)

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
  NS_LOG_INFO ("NS-3 OpenFlow NDN is not enabled. Cannot run simulation.");
  #endif // NS3_OPENFLOW
  
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}




