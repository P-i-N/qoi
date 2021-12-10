set datafile separator ';'
set key autotitle columnhead

f = 'benchmark.csv'
stats f using 2 nooutput

set terminal svg size 1000,50+40*STATS_records enhanced font 'Hack,11' background rgb 'white'

set style data histogram
set style histogram cluster gap 1
set style fill solid
set ytics scale 0

barHeight = 0.2

plot f \
	using 18:0:(0):18:($0+0.5):($0-barHeight+0.5):(7):ytic(1) with boxxyerror lc var, \
	'' using 13:0:(0):13:($0-barHeight+0.5):($0-2*barHeight+0.5):(4):ytic(1) with boxxyerror lc var, \
	'' using 3:0:(0):3:($0-2*barHeight+0.5):($0-3*barHeight+0.5):(0):ytic(1) with boxxyerror lc var, \
	'' using 8:0:(0):8:($0-3*barHeight+0.5):($0-4*barHeight+0.5):(3):ytic(1) with boxxyerror lc var

plot f \
	using 21:0:(0):21:($0+0.5):($0-barHeight+0.5):(7):ytic(1) with boxxyerror lc var, \
	'' using 16:0:(0):16:($0-barHeight+0.5):($0-2*barHeight+0.5):(4):ytic(1) with boxxyerror lc var, \
	'' using 6:0:(0):6:($0-2*barHeight+0.5):($0-3*barHeight+0.5):(0):ytic(1) with boxxyerror lc var, \
	'' using 11:0:(0):11:($0-3*barHeight+0.5):($0-4*barHeight+0.5):(3):ytic(1) with boxxyerror lc var
