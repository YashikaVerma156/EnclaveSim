configuration=${1}			 # enclave/baseline
encryption_operation=${2}    # yes/no
num_core=${3}				 # any integer number

./build_champsim.sh bimodal no no no no lru ${configuration} ${encryption_operation} ${num_core}


