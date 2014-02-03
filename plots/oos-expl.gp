set terminal postscript eps enhanced color "Helvetica" 20
set out 'oos-expl.eps'
set title 'Exploitabilities for OOS in LD(1,1)'
set key top left
set multiplot
set xlabel 'delta'
set xrange [-0.1:1.1]
set yrange [0:1.4]
plot "ist-1s-expl.dat" using 3:7 with linespoints lt 1 lw 3 title 'IST 1 second', \
     "ist-5s-expl.dat" using 3:7 with linespoints lt 3 lw 3 title 'IST 5 seconds', \
     "ist-25s-expl.dat" using 3:7 with linespoints lt 4 lw 3 title 'IST 25 seconds', \
     "pst-1s-expl.dat" using 3:7 with linespoints lt 5 lw 3 title 'PST 1 second', \
     "pst-5s-expl.dat" using 3:7 with linespoints lt 8 lw 3 title 'PST 5 seconds', \
     "pst-25s-expl.dat" using 3:7 with linespoints lt 7 lw 3 title 'PST 25 seconds'


