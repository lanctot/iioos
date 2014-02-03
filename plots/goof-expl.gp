set terminal postscript eps enhanced color "Helvetica" 20
set out 'goof-expl.eps'
set title 'Exploitabilities for OOS in II-Goof(6)'
set multiplot
set xlabel 'delta'
set xrange [-0.1:1.1]
set yrange [:0.7]
plot "goof-expl.dat" using 1:4 with linespoints lt 1 lw 3 title 'IST 1 second', \
     "goof-expl.dat" using 1:7 with linespoints lt 3 lw 3 title 'IST 2 seconds', \
     "goof-expl.dat" using 1:10 with linespoints lt 4 lw 3 title 'IST 4 seconds'


