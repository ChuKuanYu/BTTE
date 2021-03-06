#ifndef _PIECE_H
#define _PIECE_H

//                       512    byte
const int g_kPieceSize = 512 * (1024 * 8);

bool* MakePieces(const int NUM_PIECE);

struct PieceMsg
{
    PieceMsg() {};
    PieceMsg(const int src_pid,
             const int dest_pid,
             const int piece_no,
             const float pg_delay,
             const float src_up_bw);

    bool operator<(PieceMsg const& p) const;

    int src_pid;
    int dest_pid;
    int piece_no;
    float pg_delay;
    float src_up_bw;  // upload bandwidth
};

#endif // for #ifndef _PIECE_H
