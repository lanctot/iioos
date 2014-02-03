set terminal postscript eps enhanced color "Helvetica" 20
set out 'ismcts-oos-perf.eps'
set title 'OOS vs. ISMCTS head-to-head in LD(1,1)'
set multiplot
set xlabel 'delta'
set xrange [-0.1:1.1]
set yrange [45:70]
plot "ismcts-ist-t1-perf.dat" using 4:13 with linespoints lt 1 lw 3 title 'IST 1 second', \
     "ismcts-ist-t5-perf.dat" using 4:13 with linespoints lt 3 lw 3 title 'IST 5 seconds', \
     "ismcts-ist-t25-perf.dat" using 4:13 with linespoints lt 4 lw 3 title 'IST 25 seconds', \
     "ismcts-pst-t1-perf.dat" using 4:13 with linespoints lt 5 lw 3 title 'PST 1 second', \
     "ismcts-pst-t5-perf.dat" using 4:13 with linespoints lt 8 lw 3 title 'PST 5 seconds', \
     "ismcts-pst-t25-perf.dat" using 4:13 with linespoints lt 7 lw 3 title 'PST 25 seconds', \
     f(x)=50.0
replot f(x) w l lw 1 lt 2 title '50% mark'


