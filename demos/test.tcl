load ./proj.dll
load ./las.dll

set S "+proj=pipeline  +zone=56 +south +ellps=GRS80"
append S " +step +inv +proj=utm"
append S " +step +proj=vgridshift +grids=./ausgeoid09.gtx"
append S " +step +proj=utm"
puts $S
set P [proj create $S] 
puts [proj fwd $P [list 502810 6964520 0]]


puts [las fwd $P eht.laz ahd2.laz]


set G "+proj=pipeline  +zone=56 +south +ellps=GRS80"
append G " +step +inv +proj=utm"
append G " +step +proj=hgridshift +grids=GDA94_GDA2020_conformal.gsb"
append G " +step +proj=utm"
puts $G

set T [proj create $G]

puts [las fwd $T ahd.laz ahd2020.laz]


