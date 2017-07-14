set title "Throughput as a function of #threads" font "sans, 30" offset 0, -4
set encoding utf8
set xlabel "Threads"
set ylabel "MOPS"
set key inside left top vertical Left reverse enhanced autotitle box 
set key at graph 0.02, 0.82
set datafile separator ","
set style data linespoints
set xtics  norangelimit
#set xtics (1)
set grid xtics
set logscale x 2
set grid ytics
set term postscript enhanced color eps
set output "results.eps"
#lb-bst_bronson,lb-bst-drachsler,lf-bst-aravind,lf-bst_ellen,lf-bst-cohen,lf-bst-seqlock-cohen,lf-bst-rwcohen
plot \
