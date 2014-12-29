#!/bin/sh

LOG="../log"

# init
# look for empty dir 

if [ "$(ls -A $LOG)" ]; then
    cp ../log/* time_piece
fi

python3 genr_plot_dat.py 'time_piece'
/usr/bin/gnuplot -e "dat_prefix='output/time_piece_'; outfile='output/time_piece.png'; xlabelname='piece'; algo='$1'" plot.plg
