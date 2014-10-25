#include <iostream>
#include <iomanip>

#include "debug.h"
#include "error.h"
#include "random.h"
#include "args.h"
#include "peer.h"
#include "peer_manager.h"
#include "event_handler.h"
//#include "env_manager.h"

namespace  // for debug
{

typedef std::map<RSC, std::string> RSmapStr;  // debug

void RSC2Str(RSmapStr &rs2str)
{
    rs2str[RSC::PEER_LEVEL] = "Peer Level";
    rs2str[RSC::PROB_LEECH] = "Base Prob. of Leech";
    rs2str[RSC::PROB_PIECE] = "Prob of Each Leech's Piece";
    rs2str[RSC::EVENT_TIME] = "Event Time";
    rs2str[RSC::PGDELAY] = "Propagation Delay";
    rs2str[RSC::STD_PEERSELECT] = "Standard Peer Selection";
    rs2str[RSC::LB_PEERSELECT] = "Load Balancing Peer Selection";
    rs2str[RSC::CB_PEERSELECT] = "Cluster-Based Peer Selection";
    rs2str[RSC::RFP_PIECESELECT] = "Random Piece Selection";
    rs2str[RSC::RF_PIECESELECT] = "Rarest First Piece Selection";
    rs2str[RSC::CHOKING] = "Choking";
    rs2str[RSC::FREE_1] = "FREE";
    rs2str[RSC::FREE_2] = "FREE";
    rs2str[RSC::FREE_3] = "FREE";
    rs2str[RSC::FREE_4] = "FREE";
}

}

int main(int argc, const char *argv[])
{
    /////////////////////////////
    // 1. init random seeds group
    //
    uniformrand::InitRandSeeds();

    std::cout << "Initial Random Seeds:\n";
    RSmapStr rs2str;
    RSC2Str(rs2str);
    for(int i = 0; i < g_kNumRSeeds; i++)
    {
        if(i == 11) { std::cout << "\nUnused Rand Seeds: \n"; }
        RSC type_rsc = static_cast<RSC>(i);
        std::string rsc_str = rs2str[type_rsc];
        std::cout << std::left << rsc_str << " : \n";
        std::cout << "\t\t\t\t" << g_rand_grp[i] << "\n\n";
    }
    std::cout << "\n\n";


    ////////////////////
    // 2. read arguments
    //
    if(argc == 1)
    {
        ExitError("First argument: path of config file");
    }
    else if (argc > 2)
    {
        ExitError("Too much arguments.");
    }

    Args args(argv[1]);
    //Args args("../btte.conf.dbg");


    ///////////////////////
    // 3. start simulating
    //
    PeerManager pm(&args);
    pm.CreatePeers();

    EventHandler evh(args, &pm, 0.2, 0.5);
    evh.StartRoutine();

    ShowDbgInfo(args);


    /////////////////////////////////
    // 4. check seeds was being used

    std::cout << "\n\nFinal Random Seeds:\n";
    for(int i = 0; i < g_kNumRSeeds; i++)
    {
        if(i == 11) { std::cout << "\nUnused rand seeds: \n"; }
        RSC type_rsc = static_cast<RSC>(i);
        std::string rsc_str = rs2str[type_rsc];
        std::cout << std::left << rsc_str << " : \n";
        std::cout << "\t\t\t\t" << g_rand_grp[i] << "\n\n";
    }

	return 0;
}
