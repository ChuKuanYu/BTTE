#ifndef _EVENT_HANDLER_H
#define _EVENT_HANDLER_H

#include <map>
#include <list>
#include <functional>

#include "args.h"
#include "peer_manager.h"
#include "event.h"


//class PeerManager;

class EventHandler
{
public:
    EventHandler(Args args, PeerManager* const pm, float lambda, float mu);
    ~EventHandler();

    void PushInitEvent();
    void GetNextEvent(Event& e,
                      Event::Type t,
                      Event::Type4BT t_bt,
                      const int index = 0,
                      const int pid = 0);
    void ProcessArrival(Event& e);
    void ProcessDeparture(Event& e);
    void ProcessEvent(Event& e);
    void StartRoutine();

    float get_lambda() { return lambda_; };
    float get_mu() { return mu_; };


private:
    void PeerJoinEvent(Event& e);

    void PeerListReqRecvEvent(Event& e);

    void PeerListGetEvent(Event& e);

    void ReqPieceEvent(Event& e);

    void PieceAdmitEvent(Event& e);

    void PieceGetEvent(Event& e);

    void CompletedEvent(Event& e);

    void PeerLeaveEvent(Event& e);

    float GetNextArrivalEventTime(const Event::Type4BT t_bt,
                                  float time_packet,
                                  const float current_arrival_etime);

    float GetNextDepartureEventTime();

    void MapEvent();
    void MapEventDeps();

    // count hom many peer-join-event was generated
    static int peer_join_counts_;

    typedef void (EventHandler::*Fptr)(Event&);
    typedef std::map<Event::Type4BT, Fptr> FuncMap;
    FuncMap event_map_;
    std::map<Event::Type4BT, Event::Type4BT> event_deps_map_;

    std::list<Event> event_list_;
    std::list<Event> system_;

    Args args_;

    PeerManager* pm_;

    float lambda_;
    float mu_;

    float total_sys_size_;
    float current_time_;
    float waiting_time_;
};

#endif // for #ifndef _EVENT_HANDLER_H
