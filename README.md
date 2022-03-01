<p align="center">
  <h1 align="center"> EnclaveSim (IEEE HOST'22) </h1>
  <p> A trace-based micro-architectural simulator to support enclave simulations. It is built on top of an existing simulator, ChampSim.
  <br/>
<b>Citation</b>
If you use this code as part of your evaluation in your research/development, please be sure to cite the HOST 22 paper for proper acknowledgment.
</p>

### 1. Build & Run
#### 1.1 Binary generation
* Command-line arguments.
  * Branch predictor
  * L1I, L1D, L2C, LLC prefetcher
  * LLC replacement policy
  * Configuration : baseline(non-enclave execution)/enclave(execution with enclave)
  * Encryption operation : on/off
  * Number of cores
* Build command syntax. ``` $ ./build_enclavesim.sh ${BRANCH_PREDICTOR} ${L1I_PREFETCHER} ${L1D_PREFETCHER} ${L2C_PREFETCHER} ${LLC_PREFETCHER} ${LLC_REPLACEMENT} ${CONFIG} ${ENCRYPT_OPERATION} $NUM_CORE} ```
* Build command example. ```$ ./build_enclavesim.sh bimodal no no no no lru enclave on 1```
* Generated binary. ```bin/bimodal-no-no-no-no-lru-enclave-on-1core```

#### 1.2 Run simulation
* Command-line arguments. 
  * Binary generated in build process
  * Number of warmup instructions (in millions)
  * Number of simulation instructions (in millions) 
  * Trace information
* Trace information 
  * If ```baseline``` configuration is used while binary generation. <br/>
    ```${TRACE_INFO}: {trace_path/trace_name} {605.mcf_s-994B.champsimtrace.xz}```
    * Run command syntax. 
    ``` $ ./bin/${binary} -warmup_instructions ${n_warm}000000 -simulation_instructions ${n_sim}000000 ${option} -traces $trace_path ```
    * Run command example: ``` $ ./bin/bimodal-no-no-no-no-lru-baseline-off-1core -warmup_instructions 1000000 -simulation_instructions 1000000 -traces 605.mcf_s-994B.champsimtrace.xz ```
    * **``` $ ./run1core.sh bimodal-no-no-no-no-lru-baseline-off-1core 10 50 605.mcf_s-994B.champsimtrace.xz ```
  * If ```enclave``` configuration is used while binary generation.
    * Enclave unaware trace: <br/>
      ```${TRACE_INFO}: {trace name, trace type, number of enclave, start-point, end-point} {605.mcf_s-994B.champsimtrace.xz no 1 20 35}``` <br/>
      [NOTE] start-point and end-point is instruction number (in millions).
      * Run command example: ``` $ ./bin/bimodal-no-no-no-no-lru-enclave-on-1core -warmup_instructions 10000000 -simulation_instructions 50000000 ${option} -traces 605.mcf_s-994B.champsimtrace.xz no 1 10 35 ```
      * ** ```$ ./run1core.sh bimodal-no-no-no-no-lru-enclave-on-1core 10 50 605.mcf_s-994B.champsimtrace.xz no 1 10 35 ```
    * Enclave aware trace: <br/>
      ```${TRACE_INFO}: {trace name, trace type} {example1.champsimtrace.xz yes}```
      * Run command example: ``` $ ./bin/bimodal-no-no-no-no-lru-enclave-on-1core -warmup_instructions 10000000 -simulation_instructions 50000000 ${option} -traces example1.champsimtrace.xz yes ```
      * ** ``` $ ./run1core.sh bimodal-no-no-no-no-lru-enclave-on-1core 10 50 example1.champsimtrace.xz yes ```   
     <br/>
    ** Refer to run1core.sh to write your own script and run any customized configuration for a single core simulation.       
<!--      
``` 
Usage: ./run1core.sh [BINARY] [N_WARM] [N_SIM] [TRACE_INFO]

${BINARY}: EnclaveSim binary compiled by "build.sh" (bimodal-no-no-no-no-lru-enclave-on-1core)
${N_WARM}: number of instructions for warmup (10 million)
${N_SIM}:  number of instructinos for detailed simulation (50 million)
${TRACE_INFO}: 
  i) For Baseline config: 
      ${TRACE_INFO}: {trace name} {605.mcf_s-994B.champsimtrace.xz}
  ii) For EnclaveSim config with Enclave aware trace: 
      ${TRACE_INFO}: {trace name, trace type} {example1.champsimtrace.xz yes} 
  iii) For EnclaveSim config with Non-enclave aware trace: 
      ${TRACE_INFO}: {trace name, trace type, number of encalve, start-point, end-point} {605.mcf_s-994B.champsimtrace.xz no 1 20 35}
      *here start-point and end-point is instruction number in million.

Variant-1: $ ./run1core.sh bimodal-no-no-no-no-lru-baseline-off-1core 10 50 605.mcf_s-994B.champsimtrace.xz
Variant-2: $ ./run1core.sh bimodal-no-no-no-no-lru-enclave-on-1core 10 50 example1.champsimtrace.xz yes 
Variant-3: $ ./run1core.sh bimodal-no-no-no-no-lru-enclave-on-1core 10 50 605.mcf_s-994B.champsimtrace.xz no 1 10 35
 ```
-->

#### 1.3 Run simulation [To re-generate EnclaveSim results]

```
$ ./run1core_baseline_cal.sh
$ ./run8core_baseline_cal.sh
$ ./run8core_enclave_cal.sh
```
* Refer to ```run8core_enclave_cal.sh``` to write your own script and run any customized configuration for 8-core simulation.



### 2. PIN Tool support for enclave-aware trace generation <!-- [Supports Enclave-aware trace generation] -->
 
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
### 3. Result generation
* Result for each simulation can be stored in ``` results_*/*.txt ``` as that of ``` run1core.sh ```
* The result file stores enclave and non-enclave stats (like cache hit/misses, DRAM pages allocated, etc.) for a simulation.

### 4. Concluding notes
* EnclaveSim is not limited to its present functionality and its modular design be easily extended for any enclave-related extensions.
