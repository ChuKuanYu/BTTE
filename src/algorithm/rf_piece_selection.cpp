#include <cstdlib>
#include <fstream>
#include "rf_piece_selection.h"


using namespace btte_uniformrand;


namespace btte_piece_selection
{

RarestFirst::~RarestFirst()
{
    if (count_info_ != nullptr)
    {
        delete [] count_info_;
        count_info_ = nullptr;
    }
}

void RarestFirst::CountNumPeerOwnPiece()
{
    //const Neighbor *neighbors = g_peers.at(selector_pid_).neighbors;
    auto& neighbors = g_peers.at(selector_pid_).get_neighbors();

    int index = 0;
    std::vector<POC> tmp_vec;
    IntSetIter begin = candidates_.begin();
    IntSetIter end = candidates_.end();

    for (IntSetIter p_no = begin; p_no != end; p_no++, index++)
    {
        int counts = 0;
        for (auto& nei : neighbors)
        {
            // NOTE: Not only check weather having this piece, but also
            // check the neighbor is in swarm
            if (HavePiece(nei.first, *p_no) &&
                    g_peers_reg_info[nei.first])
            {
                ++counts;
            }
        }

        if (counts > 0)
        {
            POC info = {*p_no, counts};
            tmp_vec.push_back(info);
        }
    }

    num_target_ = tmp_vec.size();
    count_info_ = new POC[num_target_];
    if (nullptr == count_info_)
        ExitError("Memory Allocation Error");

    std::copy(tmp_vec.begin(), tmp_vec.end(), count_info_);

    tmp_vec.clear();
}

void RarestFirst::SortByPieceCounts()
{
    auto cmp = [] (const void* l, const void* r) { const POC* myl = (POC*)l;
        const POC* myr = (POC*)r;
        return myl->count - myr->count;
    };

    // sort pieces counts info
    qsort(count_info_,
          num_target_,
          sizeof(POC),
          cmp);
}

IntSet RarestFirst::GetRarestPiecesSet() const
{
    IntSet target_pieces;
    IntSet dup_count_pieces;

    const size_t NUM_PEERLIST = g_btte_args.get_num_peerlist();
    if (num_target_ == 1)  // only one
    {
        const int no = count_info_[0].piece_no;
        target_pieces.insert(no);
    }
    else
    {
        // Check peer-count of each piece iteratively, if appear same-peer-count situation,
        // then randomly choose one.
        // FIXME :  太冗長，應該有更好的處理邏輯
        const int last_idx = num_target_ - 1;
        for (size_t i = 1; i < num_target_; ++i)
        {
            const int cur_no = count_info_[i].piece_no;
            const int count = count_info_[i].count;
            const int prev_count = count_info_[i - 1].count;

            if (count == prev_count)
            {
                if (dup_count_pieces.empty())
                {
                    const int prev_no = count_info_[i - 1].piece_no;
                    dup_count_pieces.insert(prev_no);
                }

                dup_count_pieces.insert(cur_no);

                // 已經找到所有相同 owner counts 的 pieces，所以亂數從pieces中選一個
                if (i == last_idx)
                {
                    const int rand_no = RandChooseElementInSet(RSC::RF_PIECESELECT,
                                                               dup_count_pieces);
                    target_pieces.insert(rand_no);
                    dup_count_pieces.clear();
                }
            }
            else
            {
                if (i == 1)
                {
                    const int prev_no = count_info_[i - 1].piece_no;
                    target_pieces.insert(prev_no);
                }

                if (!dup_count_pieces.empty()) // 已找到所有相同 owner counts 的 pieces，所以亂數從pieces中選一個
                {
                    const int rand_no = RandChooseElementInSet(RSC::RF_PIECESELECT,
                                                               dup_count_pieces);
                    target_pieces.insert(rand_no);
                    dup_count_pieces.clear();
                    if (i == last_idx)
                        target_pieces.insert(cur_no);
                }

                //else if (!dup_count_pieces.empty()) // 已找到所有相同 owner counts 的 pieces，所以亂數從pieces中選一個
                //{
                //    const int rand_no = RandChooseElementInSet(RSC::RF_PIECESELECT,
                //                                               dup_count_pieces);
                //    target_pieces.insert(rand_no);
                //    dup_count_pieces.clear();
                //}
                //else if (i == last_idx &&
                //         dup_count_pieces.empty())
                //else if (i == last_idx ||
                //         dup_count_pieces.empty())
                //{
                //    target_pieces.insert(cur_no);
                //}
            }
        }
    }

    return target_pieces;
}

void RarestFirst::RefreshInfo()
{
    delete [] count_info_;
    count_info_ = nullptr;
    candidates_.clear();
}

IntSet RarestFirst::StartSelection(const int client_pid)
{
    selector_pid_ = client_pid;

    CheckNeighbors();

    SetCandidates();

    CountNumPeerOwnPiece();

    SortByPieceCounts();

    IntSet result = GetRarestPiecesSet();

    DebugInfo(result);

    RefreshInfo();

    return result;
}

void RarestFirst::DebugInfo(IntSet const& result) const
{
    std::string prefix = " ";
    switch (g_btte_args.get_type_pieceselect())
    {
        case 0:
            prefix = "rand_";
            break;
        case 1:
            prefix = "rarefst_";
            break;
        case 2:
            prefix = "user_";
            break;
        default:
            ExitError("wrong algo ID");
            break;
    }

    std::ofstream ofs;
    ofs.open(prefix + "piecesel_log.txt", std::fstream::app);

    // pieces haven't downloaded yet
    ofs << "client 尚未取得的 pieces :\n";
    ofs << "<piece no>  <count>\n";
    for (int i = 0; i < num_target_; i++)
    {
        ofs << count_info_[i].piece_no << ' '
            << count_info_[i].count << std::endl;
    }

    ofs << "\n\n";

    // piece ready to download
    ofs << "client 準備下載的 pieces :\n";
    for (const int no : result) ofs << no << ' ';

    ofs << "\n----------------------\n\n\n";
    ofs.close();
}
}
