replacement_policy=lru
config=baseline
num_cores=1
n_warm=10
n_sim=50

TRACE_DIR=~/DPC-traces

mkdir -p results_${num_cores}core_${n_warm}_${n_sim}_baseline

binary=(bimodal-no-no-no-no-lru-baseline-off-1core)

traces=(623.xalancbmk_s-10B.champsimtrace.xz 605.mcf_s-994B.champsimtrace.xz 602.gcc_s-2226B.champsimtrace.xz 648.exchange2_s-72B.champsimtrace.xz)

for ((j=0; j<5; j++))
do 	

	k=0
	mix1=""
	trace=${traces[$j]}
	while [ $k -lt 1 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace}"
	    ((k++))
	done

    (./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $mix1) &> results_${num_cores}core_${n_warm}_${n_sim}_baseline/${trace}.txt &

done