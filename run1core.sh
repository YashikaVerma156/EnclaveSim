binary=${1}
n_warm=${2}
n_sim=${3}
trace_name=${4}
trace_type=${5}
num_of_enclave=${6}

if [ $# -ge 6 ]
then
	argv=($(echo $* | tr " " "\n"))
	cur_enclave=0
	enclave_info=${num_of_enclave}
	while [ $cur_enclave -lt $num_of_enclave ]
	do
	   cur_enclave=`expr $cur_enclave + 1`
	   enclave_info+=" "
	   startpoint=$((cur_enclave*2+4))
	   enclave_info+=${argv[startpoint]}
	   enclave_info+=" "
	   endpoint=$((cur_enclave*2+5))
	   enclave_info+=${argv[endpoint]}
	done
fi


trace_path=~/DPC-traces/$trace_name
num_cores=1

if grep -q "baseline" <<< "$binary" ; then
	mkdir -p results_${num_cores}core_${n_warm}_${n_sim}_baseline
	(./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $trace_path) &> results_${num_cores}core_${n_warm}_${n_sim}_baseline/${trace_name}.txt 
else 
	mkdir -p results_${num_cores}core_${n_warm}_${n_sim}_enclave
	if [ $trace_type == "yes" ] ; then
		(./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces  $trace_path $trace_type) &> results_${num_cores}core_${n_warm}_${n_sim}_enclave/${trace_name}.txt
	else
		(./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $trace_path $trace_type $enclave_info) &> results_${num_cores}core_${n_warm}_${n_sim}_enclave/${trace_name}.txt
	fi
fi

