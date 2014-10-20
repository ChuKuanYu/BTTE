#ifndef _STD_PEER_SELECTION_H
#define _STD_PEER_SELECTION_H

#include "peer_selection.h"


namespace btte_peer_selection
{

class Standard : public IPeerSelect
{
public:
    Standard(Args args) : IPeerSelect(args),
                          pg_delays_(nullptr) {};
    ~Standard();

private:
    Neighbor* SelectNeighbors(const int self_pid, const iSet& in_swarm_set) override;
    float* pg_delays_;
};

}

#endif // for #ifndef _STD_PEER_SELECTION_H