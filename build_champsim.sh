function log2 {
    local x=0
    for (( y=$1-1 ; $y > 0; y >>= 1 )) ; do
        let x=$x+1
    done
    echo $x
}

if [ "$#" -ne 9 ]; then
    echo "Illegal number of parameters"
    echo "Usage: ./build_champsim.sh [branch_pred] [l1d_pref] [l2c_pref] [llc_pref] [llc_repl] [num_core]"
    exit 1
fi

# ChampSim configuration
BRANCH=$1               # branch/*.bpred
L1I_PREFETCHER=$2       # prefetcher/*.l1i_pref
L1D_PREFETCHER=$3       # prefetcher/*.l1d_pref
L2C_PREFETCHER=$4       # prefetcher/*.l2c_pref
LLC_PREFETCHER=$5       # prefetcher/*.llc_pref
LLC_REPLACEMENT=$6      # replacement/*.llc_repl
CONFIG=$7               # enclave or baseline
ENCRYPTION_OPERATION=$8   # on/off
NUM_CORE=${9}          # tested up to 8-core system

############## Some useful macros ###############
BOLD=$(tput bold)
NORMAL=$(tput sgr0)
#################################################

# Sanity check
if [ ! -f ./branch/${BRANCH}.bpred ]; then
    echo "[ERROR] Cannot find branch predictor"
	echo "[ERROR] Possible branch predictors from branch/*.bpred "
    find branch -name "*.bpred"
    exit 1
fi

if [ ! -f ./prefetcher/${L1I_PREFETCHER}.l1i_pref ]; then
    echo "[ERROR] Cannot find L1I prefetcher"
	echo "[ERROR] Possible L1I prefetchers from prefetcher/*.l1i_pref "
    find prefetcher -name "*.l1i_pref"
    exit 1
fi

if [ ! -f ./prefetcher/${L1D_PREFETCHER}.l1d_pref ]; then
    echo "[ERROR] Cannot find L1D prefetcher"
	echo "[ERROR] Possible L1D prefetchers from prefetcher/*.l1d_pref "
    find prefetcher -name "*.l1d_pref"
    exit 1
fi


if [ ! -f ./prefetcher/${L2C_PREFETCHER}.l2c_pref ]; then
    echo "[ERROR] Cannot find L2C prefetcher"
	echo "[ERROR] Possible L2C prefetchers from prefetcher/*.l2c_pref "
    find prefetcher -name "*.l2c_pref"
    exit 1
fi

if [ ! -f ./prefetcher/${LLC_PREFETCHER}.llc_pref ]; then
    echo "[ERROR] Cannot find LLC prefetcher"
	echo "[ERROR] Possible LLC prefetchers from prefetcher/*.llc_pref "
    find prefetcher -name "*.llc_pref"
    exit 1
fi

if [ ! -f ./replacement/${LLC_REPLACEMENT}.llc_repl ]; then
    echo "[ERROR] Cannot find LLC replacement policy"
	echo "[ERROR] Possible LLC replacement policy from replacement/*.llc_repl"
    find replacement -name "*.llc_repl"
    exit 1
fi


# Check num_core
re='^[0-9]+$'
if ! [[ $NUM_CORE =~ $re ]] ; then
    echo "[ERROR]: num_core is NOT a number" >&2;
    exit 1
fi

# Check for multi-core
if [ "$NUM_CORE" -gt "1" ]; then
    echo "Building multi-core EnclaveSim..."
    sed -i.bak 's/\<NUM_CPUS 1\>/NUM_CPUS '${NUM_CORE}'/g' inc/champsim.h
else
    if [ "$NUM_CORE" -lt "1" ]; then
        echo "Number of core: $NUM_CORE must be greater or equal than 1"
        exit 1
    else
        echo "Building single-core EnclaveSim..."
    fi
fi


# DRAM CHANNEL Mutli-core support 
if [ "$NUM_CORE" -ge "4" ]; then
    DRAM_CHANNEL=`echo "${NUM_CORE} / 4" | bc `    
    LOG_DRAM_CHANNEL=$(log2 ${DRAM_CHANNEL})
else
    DRAM_CHANNEL=1    
    LOG_DRAM_CHANNEL=0
fi

sed -i.bak 's/\<DRAM_CHANNELS 1\>/DRAM_CHANNELS '${DRAM_CHANNEL}'/g' inc/champsim.h
sed -i.bak 's/\<LOG2_DRAM_CHANNELS 0\>/LOG2_DRAM_CHANNELS '${LOG_DRAM_CHANNEL}'/g' inc/champsim.h

echo

# Check for valid memory option
if [ "$CONFIG" != "baseline" ] && [ "$CONFIG" != "enclave" ]; then
	echo "[ERROR] Choose correct configuration [BASELINE / ENCLAVE]"
	exit 1
fi

# Set the memory system operation
if [ "$CONFIG" = "baseline" ]; then
    sed -i.bak 's/\<CONFIG\>/BASELINE/g' inc/champsim.h
else
	sed -i.bak 's/\<CONFIG\>/ENCLAVE/g' inc/champsim.h	    
fi

if [ "$ENCRYPTION_OPERATION" = "on" ]; then
    sed -i.bak 's/\<ENCRYPTION_OPERATION 0\>/ENCRYPTION_OPERATION 1/g' inc/champsim.h
fi


# Change prefetchers and replacement policy
cp branch/${BRANCH}.bpred branch/branch_predictor.cc
cp prefetcher/${L1I_PREFETCHER}.l1i_pref prefetcher/l1i_prefetcher.cc
cp prefetcher/${L1D_PREFETCHER}.l1d_pref prefetcher/l1d_prefetcher.cc
cp prefetcher/${L2C_PREFETCHER}.l2c_pref prefetcher/l2c_prefetcher.cc
cp prefetcher/${LLC_PREFETCHER}.llc_pref prefetcher/llc_prefetcher.cc
cp replacement/${LLC_REPLACEMENT}.llc_repl replacement/llc_replacement.cc

# Build
mkdir -p bin
rm -f bin/champsim
make clean
make

# Sanity check
echo ""
if [ ! -f bin/champsim ]; then
    echo "${BOLD}EnclaveSim build FAILED!"
    echo ""
    exit 1
fi

echo "${BOLD}EnclaveSim is successfully built"
echo "Branch Predictor: ${BRANCH}"
echo "L1I Prefetcher: ${L1I_PREFETCHER}"
echo "L1D Prefetcher: ${L1D_PREFETCHER}"
echo "L2C Prefetcher: ${L2C_PREFETCHER}"
echo "LLC Prefetcher: ${LLC_PREFETCHER}"
echo "LLC Replacement: ${LLC_REPLACEMENT}"
echo "CONFIG:  ${CONFIG}"
echo "ENCRYPTION_OPERATION: ${ENCRYPTION_OPERATION}"
echo "Cores: ${NUM_CORE}"
BINARY_NAME="${BRANCH}-${L1I_PREFETCHER}-${L1D_PREFETCHER}-${L2C_PREFETCHER}-${LLC_PREFETCHER}-${LLC_REPLACEMENT}-${CONFIG}-${ENCRYPTION_OPERATION}-${NUM_CORE}core"
echo "Binary: bin/${BINARY_NAME}"
echo ""
mv bin/champsim bin/${BINARY_NAME}

# Restore to the default configuration
sed -i.bak 's/\<NUM_CPUS '${NUM_CORE}'\>/NUM_CPUS 1/g' inc/champsim.h
sed -i.bak 's/\<DRAM_CHANNELS '${DRAM_CHANNEL}'\>/DRAM_CHANNELS 1/g' inc/champsim.h
sed -i.bak 's/\<LOG2_DRAM_CHANNELS '$(log2 ${DRAM_CHANNEL})'\>/LOG2_DRAM_CHANNELS 0/g' inc/champsim.h

if [ "$CONFIG" = "baseline" ]; then
    sed -i.bak 's/\<BASELINE\>/CONFIG/g' inc/champsim.h
else
	sed -i.bak 's/\<ENCLAVE\>/CONFIG/g' inc/champsim.h	    
fi


if [ "$ENCRYPTION_OPERATION" = "on" ]; then
    sed -i.bak 's/\<ENCRYPTION_OPERATION 1\>/ENCRYPTION_OPERATION 0/g' inc/champsim.h
fi

cp branch/bimodal.bpred branch/branch_predictor.cc
cp prefetcher/no.l1i_pref prefetcher/l1i_prefetcher.cc
cp prefetcher/no.l1d_pref prefetcher/l1d_prefetcher.cc
cp prefetcher/no.l2c_pref prefetcher/l2c_prefetcher.cc
cp prefetcher/no.llc_pref prefetcher/llc_prefetcher.cc
cp replacement/lru.llc_repl replacement/llc_replacement.cc