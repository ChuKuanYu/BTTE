#include "event.h"

Event::Event(Type t, Type4BT t_bt, int idx, int pid, float ti)
{
    type = t;
    type_bt = t_bt;
    index = idx;
    this->pid = pid;

    time = ti;
    pg_delay = 0.0;

    is_timeout = false;

    // not for all type of events
    requestor_pid = -1;
    piece_no = -1;
}

bool Event::operator<(const Event& e)
{
    return time < e.time;
}
