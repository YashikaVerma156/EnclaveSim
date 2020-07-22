<p align="center">
  <h1 align="center"> EnclaveSim </h1>
  <p> A trace-based micro-architectural simulator to support enclave simulations.</p>

# Compile

* How to build it?

```
$ ./build.sh ${replacement_policy} ${configuration} ${encryption_operation} ${num_core}
$ ./build.sh lru enclave off 8
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
# PIN Tool
 
1. Download: `https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.13-98189-g60a6ef199-gcc-linux.tar.gz`
1. Installation:
   1. To set path variable for pin-tool
    ```
    # Add below line at the end of .bashrc file 
    export PATH=$PATH:path/to/pin-3.2-81205-gcc-linux
    ```
   1. To Build tracer
    ```
    $ cd path/to/EnclaveSim/Tracer
    $ ./make_tracer.sh 
    ```
