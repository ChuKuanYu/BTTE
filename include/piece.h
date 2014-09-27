#ifndef _PIECE_H
#define _PIECE_H

#include <iostream>
//#include <string>

//                         512      byte
const int g_k_piece_size = 512 * (1024 * 8);

bool* MakePieces(const int NUM_PIECE);
void GetPieceByProb(bool* pieces,
                    const double& prob_leech,
                    const int NUM_PIECE);

#endif // for #ifndef _PIECE_H
