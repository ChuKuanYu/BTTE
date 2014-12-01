#include <iostream>
#include <cassert>
#include <cmath>

#include "random.h"
#include "piece.h"
#include "pg_delay.h"
#include "peer_level.h"
#include "choking.h"
#include "event_handler.h"


using namespace uniformrand;


namespace
{

typedef std::map<Event::Type4BT, std::string> TBTmapStr;  // debug

void Event2Str(TBTmapStr&);
void EventInfo(Event const& head, float cur_sys_time);

}


const float slowest_bandwidth = static_cast<float>(g_kPieceSize) / g_kPeerLevel[2].bandwidth.downlink;
const float EventHandler::kTimeout_ = 2 * slowest_bandwidth;


EventHandler::EventHandler(PeerManager* const pm, float lambda, float mu)
{
    pm_ = pm;
    lambda_ = lambda;
    mu_ = mu;
    current_time_ = 0.0;
    waiting_time_ = 0.0;
    next_event_idx_ = 0;

    MapEventFuncs();
    //MapFlowDownEventDependencies();
    MapEventCreators();
}

EventHandler::~EventHandler()
{
    pm_ = nullptr;
}

void EventHandler::MapEventFuncs()
{
    event_func_map_[Event::PEER_JOIN] = &EventHandler::PeerJoinEvent;
    event_func_map_[Event::PEERLIST_REQ_RECV] = &EventHandler::PeerListReqRecvEvent;
    event_func_map_[Event::PEERLIST_GET] = &EventHandler::PeerListGetEvent;
    event_func_map_[Event::PIECE_REQ_RECV] = &EventHandler::PieceReqRecvEvent;
    event_func_map_[Event::PIECE_ADMIT] = &EventHandler::PieceAdmitEvent;
    event_func_map_[Event::PIECE_GET] = &EventHandler::PieceGetEvent;
    event_func_map_[Event::COMPLETED] = &EventHandler::CompletedEvent;
    event_func_map_[Event::PEER_LEAVE] = &EventHandler::PeerLeaveEvent;
}

void EventHandler::MapEventCreators()
{
    /* Map Functions */
    event_creator_map_[Event::PEER_JOIN]         = &EventHandler::EC_1;
    event_creator_map_[Event::PEERLIST_REQ_RECV] = &EventHandler::EC_2;
    event_creator_map_[Event::PEERLIST_GET]      = &EventHandler::EC_3;
    event_creator_map_[Event::PIECE_REQ_RECV]    = &EventHandler::EC_4;
    event_creator_map_[Event::PIECE_ADMIT]       = &EventHandler::EC_5;
    event_creator_map_[Event::PIECE_GET]         = &EventHandler::EC_6;
    event_creator_map_[Event::COMPLETED]         = &EventHandler::EC_7;
}

void EventHandler::MapFlowDownEventDependencies()
{
    event_map_[Event::PEER_JOIN] = Event::PEERLIST_REQ_RECV;
    event_map_[Event::PEERLIST_REQ_RECV] = Event::PEERLIST_GET;
    event_map_[Event::PEERLIST_GET] = Event::PIECE_REQ_RECV;
    event_map_[Event::PIECE_REQ_RECV] = Event::PIECE_ADMIT;
    event_map_[Event::PIECE_ADMIT] = Event::PIECE_GET;
    event_map_[Event::PIECE_GET] = Event::COMPLETED;
    event_map_[Event::COMPLETED] = Event::PEER_LEAVE;
}

void EventHandler::GetNewPeerList(Event const& ev)
{
    const int client_pid = ev.pid;
    const float time = ev.time + g_kTrackerPGDelay;

    Event next_ev = Event(Event::Type::ARRIVAL,
                          Event::PEERLIST_REQ_RECV,
                          ++next_event_idx_,
                          client_pid,
                          time);

    PushArrivalEvent(next_ev);
}

void EventHandler::GenrPieceReqRecvEvents(Event const& ev)
{
    if (ev.need_new_neighbors)  // no any possible peers to request
    {
        GetNewPeerList(ev);
    }
    else
    {
        for (PieceMsg const& msg : ev.req_msgs)
        {
            Peer const& client = g_peers.at(ev.pid);
            const float pg_delay = client.get_neighbor_pgdelay(msg.dest_pid);
            const float time = ev.time + pg_delay;

            Event next_ev = Event(Event::Type::ARRIVAL,
                                  Event::PIECE_REQ_RECV,
                                  ++next_event_idx_,
                                  msg.dest_pid,
                                  time);

            PushArrivalEvent(next_ev);
        }
    }
}

/* Create Derived Events of Peer-Join Event */
void EventHandler::EC_1(Event const& ev)
{
    // Peer-Join Event
    assert(ev.type_bt == Event::PEER_JOIN);

    // 再產生下一個 Peer-Join Event
    PushPeerJoinEvent(ev);

    const float time = ev.time + g_kTrackerPGDelay;

    Event next_ev = Event(Event::Type::ARRIVAL,
                          Event::PEERLIST_REQ_RECV,
                          ++next_event_idx_,
                          ev.pid,
                          time);

    PushArrivalEvent(next_ev);
}

/* Create Derived Events of Peer-List-Req-Recv Event */
void EventHandler::EC_2(Event const& ev)
{
    assert(ev.type_bt == Event::PEERLIST_REQ_RECV);

    const float time = ev.time + g_kTrackerPGDelay;

    Event next_ev =  Event(Event::Type::ARRIVAL,
                           Event::PEERLIST_GET,
                           ++next_event_idx_,
                           ev.pid,
                           time);

    PushArrivalEvent(next_ev);
}

/* Create Derived Events of Peer-List-Get Event */
void EventHandler::EC_3(Event const& ev)
{
    GenrPieceReqRecvEvents(ev);
}

/* Create Derived Events of Piece-Req-Recv Event */
void EventHandler::EC_4(Event const& ev)
{
    assert(ev.type_bt == Event::PIECE_REQ_RECV);

    // 因為跟前一個事件(Piece-Req-Recv) 的 peer 是一樣的，
    // 所以不用任何傳輸時間 (pg_delay or trans_time)
    const float time = ev.time;

    // Next round admit-events
    for (PieceMsg const& msg : ev.admitted_reqs)
    {
        Event next_ev = Event(Event::Type::ARRIVAL,
                              Event::PIECE_ADMIT,
                              ++next_event_idx_,
                              ev.pid,
                              time);
        next_ev.admitted_reqs = ev.admitted_reqs;

        PushArrivalEvent(next_ev);
    }
}

/* Create Derived Events of Piece-Admit Event */
void EventHandler::EC_5(Event const& ev)
{
    assert(ev.type_bt == Event::PIECE_ADMIT);

    const int client_pid = ev.pid;  // receiver of request

    // Generate Piece-Get events
    for (PieceMsg const& msg : ev.uploaded_reqs)
    {
        Peer const& peer = g_peers.at(msg.src_pid);  // sender of request
        const float pg_delay = peer.get_neighbor_pgdelay(client_pid);
        const float time = ev.time + pg_delay;

        Event next_ev = Event(Event::Type::ARRIVAL,
                              Event::PIECE_GET,
                              ++next_event_idx_,
                              msg.src_pid, time);

        PushArrivalEvent(next_ev);
    }


    Peer const& client = g_peers.at(client_pid);

    // Generate Next Admit Events
    if (ev.admitted_reqs.size() != 0)
    {
        for (PieceMsg const& msg : ev.admitted_reqs)
        {
            const float time = ev.time + client.get_trans_time();

            Event next_ev = Event(Event::Type::ARRIVAL,
                                  Event::PIECE_ADMIT,
                                  ++next_event_idx_,
                                  ev.pid,
                                  time);

            PushArrivalEvent(next_ev);
        }
    }
}

/* Create Derived Events of Piece-Get Event */
void EventHandler::EC_6(Event const& ev)
{
    assert(ev.type_bt == Event::PIECE_GET);

    if (ev.is_complete)
    {
        Event next_ev = Event(Event::Type::ARRIVAL,
                              Event::COMPLETED,
                              ++next_event_idx_,
                              ev.pid,
                              ev.time);

        PushArrivalEvent(next_ev);
    }
    else
    {
        GenrPieceReqRecvEvents(ev);
    }
}

/* Create Derived Events of Complete Event */
void EventHandler::EC_7(Event const& ev)
{
    assert(ev.type_bt == Event::COMPLETED);

    const float time = ev.time + ExpRand(lambda_, Rand(RSC::EVENT_TIME));

    Event next_ev = Event(Event::Type::ARRIVAL,
                          Event::PEER_LEAVE,
                          ++next_event_idx_,
                          ev.pid,
                          time);

    PushArrivalEvent(next_ev);
}

void EventHandler::PushInitEvent()
{
    const size_t NUM_SEED = g_btte_args.get_num_seed();
    const size_t NUM_LEECH = g_btte_args.get_num_leech();
    const int initial_pid = NUM_SEED + NUM_LEECH;
    const int initial_idx = 1;
    const float time = ExpRand(lambda_, Rand(RSC::EVENT_TIME));

    Event first_event(Event::Type::ARRIVAL,
                      Event::PEER_JOIN,
                      initial_idx,
                      initial_pid,
                      time);

    event_list_.push_back(first_event);

    next_event_idx_ = initial_idx;  // init next event index
}

void EventHandler::PushArrivalEvent(Event const& ev)
{
    event_list_.push_back(ev);
    event_list_.sort();  // 確保離開事件不會發生於再抵達事件之前
}

void EventHandler::PushDepartureEvent(const Event::Type4BT type_bt,
                                      const int next_index,
                                      const int pid)
{
    float next_etime = current_time_ + ExpRand(mu_, Rand(RSC::EVENT_TIME));;

    Event depart_event(Event::Type::DEPARTURE,
                       type_bt,
                       next_index,
                       pid,
                       next_etime);
    event_list_.push_back(depart_event);
    event_list_.sort();  // 確保離開事件不會發生於再抵達事件之前
}


float EventHandler::ComputeArrivalEventTime(const Event& ev, const Event::Type4BT derived_type_bt)
{
    float time = ev.time;
    Event::Type4BT dtbt = derived_type_bt;

    if (dtbt == Event::PEERLIST_REQ_RECV ||
        dtbt == Event::PEERLIST_GET)
    {
        time += g_kTrackerPGDelay;
    }
    else if (dtbt == Event::PEER_JOIN ||
             dtbt == Event::PEER_LEAVE)
    {
        time += ExpRand(lambda_, Rand(RSC::EVENT_TIME));
    }
    else if (dtbt == Event::PIECE_ADMIT)
    {
        const float trans_time = g_kPieceSize / g_peers.at(ev.pid).get_bandwidth().downlink;
        time += trans_time;
    }
    else if (dtbt == Event::PIECE_REQ_RECV ||
             dtbt == Event::PIECE_GET)
    {
        time += ev.pg_delay;
    }

    return time;
}

float EventHandler::ComputeDepartureEventTime()
{
    float time = current_time_ + ExpRand(mu_, Rand(RSC::EVENT_TIME));
    return time;
}

void EventHandler::PushDerivedEvent(Event const& ev)
{
    // 根據此次事件，衍生接下來的事件
    event_creator_map_[ev.type_bt](*this, ev);

    /* Old Version */
    //int pid = ev.pid;
    //Event::Type base_etype = Event::Type::ARRIVAL;
    //Event::Type4BT derived_tbt = event_map_[ev.type_bt];  // tbt == type_bt
    //float time = ComputeArrivalEventTime(ev, derived_tbt);

    //Event next_event = Event(base_etype, derived_tbt, ++next_event_idx_, pid, time);
    //PushArrivalEvent(next_event);

    //switch (ev.type_bt)
    //{
    //    case Event::PEERLIST_GET:  // generate initial piece-req events
    //        for (const PieceMsg& msg : ev.req_msgs)
    //        {
    //            next_event.client_pid = msg.src_pid;
    //            next_event.pid = msg.dest_pid;
    //            PushArrivalEvent(next_event);
    //            // prepare another request for timeout situation
    //            //derived_tbt = Event::PIECE_REQ_RECV;
    //            //time += kTimeout_;
    //        }
    //        return;  // already generate derived events
    //        break;

    //    case Event::PIECE_REQ_RECV:
    //        next_event.client_pid = ev.client_pid;
    //        break;

    //    case Event::PIECE_ADMIT:
    //        next_event.pid = ev.client_pid;
    //        break;

    //    case Event::PIECE_GET:  // if get one piece, generate another piece-req event
    //        if (!g_peers.at(pid).get_type() == Peer::SEED)
    //            derived_tbt = Event::PIECE_REQ_RECV;
    //        break;

    //    default:
    //        break;
    //}
    //PushArrivalEvent(next_event);
}

void EventHandler::PushPeerJoinEvent(Event const& ev)
{
    /// 如果是處理 Peer Join 事件,就再產生下一個 Peer Join 事件
    //  (因為節點加入順序是按照陣列索引）, 直到數量滿足 NUM_PEER
    const int next_join_pid = ev.pid + 1;
    const size_t NUM_PEER = g_btte_args.get_num_peer();

    if ((size_t)next_join_pid < NUM_PEER &&
            !g_in_swarm_set[next_join_pid])
    {
        //float time = ComputeArrivalEventTime(ev, Event::PEER_JOIN);
        float time = ev.time + ExpRand(lambda_, Rand(RSC::EVENT_TIME));
        Event event = Event(Event::Type::ARRIVAL,
                            Event::PEER_JOIN,
                            ++next_event_idx_,
                            next_join_pid,
                            time);
        PushArrivalEvent(event);
    }
}

void EventHandler::ProcessArrival(Event& ev)
{
    total_sys_size_ += system_.size();

    /* Timeout 機制 */
    // 如果 request event 已經 timeout, 直接忽略(跳出函式)
    //if (e.type_bt == Event::PIECE_REQ_RECV && e.is_timeout)
    {
        // 刪除要求者中 "正在要求中的 peers (on_req_peers)"
        // 這個紀錄裡面的 接收者ID (ev.pid)
        // 這樣避免在重新尋找這個 piece 的目標節點時，找不到當前的接收者
        //g_peers.at(ev.client_pid).erase_on_req_peer(ev.pid);
        //
        //return;
    }

    system_.push_back(ev);

    // 處理 System 的頭一個 BT 事件
    // (this->*event_func_map_[ev.type_bt])(ev);  // func ptr version
    event_func_map_[ev.type_bt](*this, ev);  // std::function version

    // 只要不是 Peer Leave 事件，就產生衍生事件
    if (ev.type_bt != Event::PEER_LEAVE)  { PushDerivedEvent(ev); }

    // debug (remove after)
    //if (ev.type_bt == Event::PEER_JOIN) { PushPeerJoinEvent(ev); }

    // 如果系統中只有一個事件，就產生離開事件
    if (system_.size() == 1)
    {
        current_time_ = ev.time;
        PushDepartureEvent(ev.type_bt, next_event_idx_, ev.pid);
    }
}

void EventHandler::ProcessDeparture(Event const& ev)
{
    system_.pop_front();

    if (system_.size() != 0)
    {
        Event head = system_.front();
        current_time_ = ev.time;
        waiting_time_ = waiting_time_ + (current_time_ - head.time);
        PushDepartureEvent(head.type_bt, ++next_event_idx_, head.pid);
    }

    event_list_.sort();
}

bool EventHandler::ReqTimeout(Event const& ev)
{
    assert(current_time_ > ev.time_req_send);

    // TODO: 紀錄送出要求的時間(time_req_send)(產生要求事件當下的系統時間的 current_time_)，
    //       並且將現在時間(cur_time)減掉 time_req_send
    //       如果大於等於 kTimeout 就視為 Timeout
    bool flag = false;
    if (current_time_ - ev.time_req_send >= kTimeout_)
    {
        flag = true;
        std::cout << "This req-event is timeout" << std::endl;
    }

    return flag;
}

void EventHandler::SendPieceReqs(Event& ev)
{
    ev.req_msgs = pm_->GenrAllPieceReqs(ev.pid);

    // 沒有要求訊息產生，需要新的 PeerList
    if (0 == ev.req_msgs.size())
        ev.need_new_neighbors = true;

    for (const PieceMsg& msg : ev.req_msgs)
    {
        Peer& client = g_peers.at(ev.pid);
        Peer& peer = g_peers.at(msg.dest_pid);
        client.insert_on_req_peer(msg.dest_pid);
        peer.push_recv_msg(msg);
    }
}

void EventHandler::PeerJoinEvent(Event& ev)
{
    //pm_->NewPeer(ev.pid, ev.time);
    pm_->NewPeer(ev.pid);
    g_peers.at(ev.pid).set_join_time(current_time_);
    pm_->UpdateSwarmInfo(PeerManager::ISF::JOIN, ev.pid);
}

void EventHandler::PeerListReqRecvEvent(Event& ev)
{
    pm_->AllotNeighbors(ev.pid);
}

void EventHandler::PeerListGetEvent(Event& ev)
{
    // 1. 執行初始的 Piece Selection，並向所有鄰居送出 piece 的要求
    std::cout << "Peer #" << ev.pid << " execute Piece Selection" << "\n";
    SendPieceReqs(ev);

    // DEBUG of Choking
    //std::set<int> dest_set;
    //for (auto const& msg : ev.req_msgs)
    //    dest_set.insert(msg.dest_pid);
    //std::list<std::list<PieceMsg>> all_admit_msgs;
    //for (const int pid : dest_set)
    //{
    //    all_admit_msgs.push_back(Choking(pid));
    //}

    //std::cout << "\n=========================\n";
    //std::cout << "\nAdmit Messages :\n";
    //for (auto const& msg_list : all_admit_msgs)
    //{
    //    for (auto const& msg : msg_list)
    //    {
    //        std::cout << "SRC: " << msg.src_pid << std::endl;
    //        std::cout << "DEST: " << msg.dest_pid << std::endl;
    //        std::cout << "PIECE-NO: " << msg.piece_no << std::endl;
    //        std::cout << "SRC UPLOAD BANDWIDTH: " << msg.src_up_bw << std::endl;
    //        std::cout << "--------------------------------\n\n";
    //    }
    //}
}

void EventHandler::PieceReqRecvEvent(Event& ev)
{
    // TODO 檢查要求者的 piece 狀態，只要有一個是完全沒拿到 piece
    // 就做 choking (先不採用）
    ev.admitted_reqs = Choking(ev.pid);
}

void EventHandler::PieceAdmitEvent(Event& ev)
{
    // TODO : 將 piece 送給要求者並產生 PieceGet 事件
    ev.uploaded_reqs = ev.admitted_reqs;

    const int client_pid = ev.pid;
    // 刪除每一個要求者中有關於接收者的紀錄
    for (auto const& msg : ev.uploaded_reqs)
    {
        Peer& peer = g_peers.at(msg.src_pid);
        peer.erase_on_req_peer(client_pid);
    }

    ev.admitted_reqs.clear();

    // TODO: 執行 choking 來產生下一次的 Piece Admit
    ev.admitted_reqs = Choking(client_pid);

    // DEBUG
    std::cout << std::flush;
    std::cout << "\nAdmit Msg List:";
    std::cout << "\nSize of Admit Msg List: " << ev.admitted_reqs.size() << std::endl;

    //if (ev.admitted_reqs.size() != 0)
    //{
    //    for (auto const& msg : ev.admitted_reqs)
    //    {
    //        std::cout << "SRC: " << msg.src_pid << std::endl;
    //        std::cout << "DEST: " << msg.dest_pid << std::endl;
    //        std::cout << "PIECE-NO: " << msg.piece_no << std::endl;
    //        std::cout << "SRC UPLOAD BANDWIDTH: " << msg.src_up_bw << std::endl;
    //        std::cout << "--------------------------------\n\n";
    //    }
    //}
}

void EventHandler::PieceGetEvent(Event& ev)
{
    // Check client is complete
    Peer& client = g_peers.at(ev.pid);
    if (pm_->CheckAllPiecesGet(ev.pid))
    {
        client.to_seed();
        ev.is_complete = true;
    }
    else
    {
        // download incomplete, so execute Piece Selection
        SendPieceReqs(ev);
    }
}

void EventHandler::CompletedEvent(Event& ev)
{
    // After random time, this peer will leave.
    // Now, nothing to do
}

void EventHandler::PeerLeaveEvent(Event& ev)
{
    g_peers.at(ev.pid).set_in_swarm(false);
    g_peers.at(ev.pid).set_leave_time(current_time_);
    pm_->UpdateSwarmInfo(PeerManager::ISF::LEAVE, ev.pid);
}

void EventHandler::ProcessEvent(Event& ev)
{
    if (ev.type == Event::Type::ARRIVAL)
        ProcessArrival(ev);
    else
        ProcessDeparture(ev);
}

void EventHandler::StartRoutine()
{
    //const int aborigin = args_.NUM_SEED + args_.NUM_LEECH;
    //const int num_avg_peer = args_.NUM_PEER - aborigin;
    PushInitEvent();

    while (!event_list_.empty())
    {
        Event head = event_list_.front();

        event_list_.pop_front();

        EventInfo(head, current_time_);  // DEBUG

        ProcessEvent(head);
    }
}


namespace
{

void Event2Str(TBTmapStr& tbt2str)
{
    tbt2str[Event::PEER_JOIN] = "Peer-Join Event";
    tbt2str[Event::PEERLIST_REQ_RECV] = "Peer-List-Req-Recv Event";
    tbt2str[Event::PEERLIST_GET] = "Peer-List-Get Event";
    tbt2str[Event::PIECE_REQ_RECV] = "Piece-Req-Recv Event";
    tbt2str[Event::PIECE_ADMIT] = "Piece-Admit Event";
    tbt2str[Event::PIECE_GET] = "Piece-Get Event";
    tbt2str[Event::COMPLETED] = "Completed Event";
    tbt2str[Event::PEER_LEAVE] = "Peer-Leave Event";
}

void EventInfo(Event const& head, float sys_cur_time)
{
    std::cout.precision(5);

    TBTmapStr tbt2str;
    Event2Str(tbt2str);

    std::cout << std::flush;
    if (head.type == Event::Type::ARRIVAL)
    {
        std::cout << "\nEvent #" << head.index << " arrival at " << head.time;
        std::cout << "\nCurrent System time: " << sys_cur_time << "\n";
    }
    else
    {
        std::cout << "\nEvent #" << head.index << " departure at " << head.time;
        std::cout << "\nCurrent System time: " << sys_cur_time << "\n";
    }
    std::cout << "It is a " << tbt2str[head.type_bt];
    std::cout << "\nThis is event belongs to peer #" << head.pid
              << "\n\n\n";
}

}  // namespace
