set terminal epslatex color standalone colortext 10
set out 'os-vs-pbos.tex'
set title 'Playout-based OS vs. OS in Bluff(1,1)'
set xlabel 'Nodes visited'
set log xy
set key bottom left
set yrange [0.05:0.85]
set xrange [900000:150000008]
plot "os.dat" using 6:9 with linespoints lt 1 title 'Outcome Sampling', \
     "pbos.dat" using 6:9 with linespoints lt 3 title 'Playout-based Outcome Sampling' 

