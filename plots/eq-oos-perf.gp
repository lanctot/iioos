set terminal postscript eps enhanced color "Helvetica" 20
set out 'eq-oos-perf.eps'
set title 'OOS vs. 0.001-equilibrium head-to-head in LD(1,1)'
set multiplot
set xlabel 'delta'
set xrange [-0.1:1.1]
set yrange [40:60]
plot "eq-ist-1s-perf.dat" using 4:12 with linespoints lt 1 lw 3 title 'IST 1 second', \
     "eq-ist-5s-perf.dat" using 4:12 with linespoints lt 3 lw 3 title 'IST 5 seconds', \
     "eq-ist-25s-perf.dat" using 4:12 with linespoints lt 4 lw 3 title 'IST 25 seconds', \
     "eq-pst-1s-perf.dat" using 4:12 with linespoints lt 5 lw 3 title 'PST 1 second', \
     "eq-pst-5s-perf.dat" using 4:12 with linespoints lt 8 lw 3 title 'PST 5 seconds', \
     "eq-pst-25s-perf.dat" using 4:12 with linespoints lt 7 lw 3 title 'PST 25 seconds', \
     f(x)=50.0
replot f(x) w l lw 1 lt 2 title '50% mark'


