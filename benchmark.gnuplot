set datafile separator ';'
set key autotitle columnhead
set style data histogram
set style fill solid
set boxwidth 0.5
set logscale y 2
plot 'benchmark.csv' using 3, '' using 8, '' using 13, '' using 18
