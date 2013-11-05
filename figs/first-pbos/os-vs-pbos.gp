set terminal epslatex color standalone colortext 10
set out 'os-vs-pbos.tex'
set title 'Playout-based OS vs. OS'
set xlabel 'Nodes visited'
set ylabel 'Exploitability'
set log xy
set key bottom left
plot "os.dat" using 6:9 with linespoints lt 1 title 'Outcome Sampling', \
     "pbos.dat" using 6:9 with linespoints lt 3 title 'Playout-based Outcome Sampling' 

