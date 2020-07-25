replacement_policy=lru
config=enclave
num_cores=1
n_warm=10
n_sim=50

TRACE_DIR=~/DPC-traces

binary=(bimodal-no-no-no-no-lru-enclave-on-1core)

traces=( 605.mcf_s-994B.champsimtrace.xz 602.gcc_s-2226B.champsimtrace.xz 641.leela_s-1052B.champsimtrace.xz 648.exchange2_s-72B.champsimtrace.xz
	)

mkdir -p results_${num_cores}core_${n_warm}_${n_sim}_enclave

trace="${TRACE_DIR}/605.mcf_s-994B.champsimtrace.xz"

(./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 -enclave_aware_trace no ${option} -traces $trace 1 5 10) &> results_${num_cores}core_${n_warm}_${n_sim}_enclave/hello.txt 

cat results_1core_10_50_enclave/hello.txt