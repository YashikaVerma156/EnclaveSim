<p align="center">
  <h1 align="center"> EnclaveSim </h1>
  <p> A trace-based micro-architectural simulator to support enclave simulations. It is build on top of an existing simulator ChampSim.</p>

# Compile

* How to build it?

```
a) Default build
$ ./build.sh ${configuration} ${encryption_operation} ${num_core}
$ ./build.sh enclave off 8

b) Customized build
$ ./build_enclavesim.sh ${BRANCH} ${L1I_PREFETCHER} ${L1D_PREFETCHER} ${L2C_PREFETCHER} ${LLC_PREFETCHER} ${LLC_REPLACEMENT} ${CONFIG} ${ENCRYPT_OPER} $NUM_CORE}
$ ./build_enclavesim.sh bimodal no no no no lru enclave off 8

```

# Run simulation

* For single-core

``` 
./run1core.sh [BINARY] [N_WARM] [N_SIM] [TRACE_INFO]

${BINARY}: EnclaveSim binary compiled by "build.sh" (bimodal-no-no-no-no-lru-enclave-off-1core)
${N_WARM}: number of instructions for warmup (1 million)
${N_SIM}:  number of instructinos for detailed simulation (10 million)
For Baseline config: 
        ${TRACE_INFO}: {trace name} {400.perlbench-41B.champsimtrace.xz}
For EnclaveSim config with Enclave aware trace: 
        ${TRACE_INFO}: {trace name, trace type} {example1.champsimtrace.xz yes}
For EnclaveSim config with Non-enclave aware trace): 
        ${TRACE_INFO}: {trace name, trace type, number of encalve, start-point, end-point} {400.perlbench-41B.champsimtrace.xz no 1 20 35}
* Here start-point and end-point is instruction number in million.

```

* For multi-core 
``` 
./run2core.sh [BINARY] [N_WARM] [N_SIM] [TRACE1_INFO] [TRACE2_INFO]
```


# Run simulation [To re-generate EnclaveSim results]

* For single-core results 

```
$ ./run1core_baseline_cal.sh
$ ./run1core_enclave_cal.sh
```
* For multi-core results 

```
$ ./run8core_baseline_cal.sh
$ ./run8core_enclave_cal.sh
```


# PIN Tool [Supports Enclave-aware trace generation]
 
* Download: https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.13-98189-g60a6ef199-gcc-linux.tar.gz

* Installation:

   * To set path variable for pin-tool

    ```
    # Add below line at the end of .bashrc file 
    export PATH=$PATH:path/to/pin-3.13-98189-g60a6ef199-gcc-linux
    ```

* To build tracer

 ```
  $ cd path/to/EnclaveSim/Tracer
  $ ./make_tracer.sh 
 ```

* To generate trace of an application

```
$ pin -t ${share_object_tracerfile_path} -- ${application_path} > /dev/null
$ pin -t /home/dixit/EnclaveSim/tracer/obj-intel64/champsim_tracer.so -- ../example/example1 >/dev/null
```
