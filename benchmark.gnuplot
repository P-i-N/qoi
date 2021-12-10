set datafile separator ';'
set key autotitle columnhead

f = 'benchmark.csv'
stats f using 2 nooutput

set terminal svg size 1000,50+50*STATS_records enhanced font 'Hack,10' background rgb 'white'

set style data histogram
set style histogram cluster gap 1
set style fill solid
set ytics scale 0

array columns[3] = [ 3, 23, 18 ]
array colors[3] = [ 3, 6, 2 ]
barHeight = 1.0/(|columns| + 1)
barOffset = 0.5+0.5*barHeight

set title "Encoding time"
plot for [i=1:|columns|] f \
	using columns[i]:0:(0):columns[i]:($0+i*barHeight-barOffset):($0+(i+1)*barHeight-barOffset):(colors[i]):ytic(1) with boxxy lc var, \
	for [i=1:|columns|] "" using columns[i]:($0+(i+0.45)*barHeight-barOffset):(sprintf("%.1f ms", column(columns[i]))):(colors[i]) with labels offset 0.4,0 left tc var notitle

set title "File size"
plot for [i=1:|columns|] f \
	using columns[i]+3:0:(0):columns[i]+3:($0+i*barHeight-barOffset):($0+(i+1)*barHeight-barOffset):(colors[i]):ytic(1) with boxxy lc var, \
	for [i=1:|columns|] "" using columns[i]+3:($0+(i+0.45)*barHeight-barOffset):(sprintf("%d kB", column(columns[i]+3))):(colors[i]) with labels offset 0.4,0 left tc var notitle
