replacement_policy=lru
config=enclave
num_cores=8
n_warm=10
n_sim=50

TRACE_DIR=~/DPC-traces

binary=(bimodal-no-no-no-no-lru-enclave-on-8core)

traces=(623.xalancbmk_s-10B.champsimtrace.xz 605.mcf_s-994B.champsimtrace.xz 602.gcc_s-2226B.champsimtrace.xz)

mkdir -p results_${num_cores}core_${n_warm}_${n_sim}_enclave

# 3 mixes [ET-LT]
for ((j=0; j<3; j++))
do 	
	k=0
	mix1=""
	trace=${traces[$j]}
	while [ $k -lt 8 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace} no 2 10 35 40 45"
	    ((k++))
	done
    (./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $mix1) &> results_${num_cores}core_${n_warm}_${n_sim}_enclave/${trace}_2.txt &
done

# 3 mixes [EF-LT]
traces=(623.xalancbmk_s-10B.champsimtrace.xz 605.mcf_s-994B.champsimtrace.xz 602.gcc_s-2226B.champsimtrace.xz)

for ((j=0; j<3; j++))
do 	
	k=0
	mix1=""
	trace=${traces[$j]}
	while [ $k -lt 4 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace} no 1 25 30"
	    ((k++))
	done
	while [ $k -lt 8 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace} no 0"
	    ((k++))
	done
    (./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $mix1) &> results_${num_cores}core_${n_warm}_${n_sim}_enclave/${trace}_1.txt &
done


# 2 mixes  [EF-LF]
traces=(641.leela_s-1052B.champsimtrace.xz 648.exchange2_s-72B.champsimtrace.xz)

for ((j=0; j<2; j++))
do 	
	k=0
	mix1=""
	trace=${traces[$j]}
	while [ $k -lt 8 ]
	do
	    mix1="$mix1 ${TRACE_DIR}/${trace} no 1 10 35"
	    ((k++))
	done
    (./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $mix1) &> results_${num_cores}core_${n_warm}_${n_sim}_enclave/${trace}_1.txt &
done
