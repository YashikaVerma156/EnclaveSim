replacement_policy=lru
config=baseline
num_cores=8
n_warm=10
n_sim=50

TRACE_DIR=~/DPC-traces

binary=(bimodal-no-no-no-no-lru-baseline-off-8core)

traces=(623.xalancbmk_s-10B.champsimtrace.xz 605.mcf_s-994B.champsimtrace.x z602.gcc_s-2226B.champsimtrace.xz
	641.leela_s-1052B.champsimtrace.xz 648.exchange2_s-72B.champsimtrace.xz
	)

mkdir -p results_${num_cores}core_${n_warm}_${n_sim}_baseline

for ((j=0; j<${#traces[@]}; j++))
do 	

	k=0
	mix1=""
	trace=${traces[$j]}
	while [ $k -lt 8 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace}"
	    ((k++))
	done

    (./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $mix1) &> results_${num_cores}core_${n_warm}_${n_sim}_baseline/${binary}_${trace}.txt &

done

traces=(623.xalancbmk_s-10B.champsimtrace.xz 605.mcf_s-1644B.champsimtrace.xz)

trace_fitting=641.leela_s-1052B.champsimtrace.xz

for ((j=0; j<${#traces[@]}; j++))
do 	

	k=0
	mix1=""
	trace=${traces[$j]}
	while [ $k -lt 4 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace}"
	    ((k++))
	done

	while [ $k -lt 8 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace_fitting}"
	    ((k++))
	done

    (./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $mix1) &> results_${num_cores}core_${n_warm}_${n_sim}_baseline/${trace}_${trace_fitting}.txt &

done