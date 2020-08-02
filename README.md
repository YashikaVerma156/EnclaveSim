<p align="center">
  <h1 align="center"> EnclaveSim </h1>
  <p> A trace-based micro-architectural simulator to support enclave simulations. It is build on top existing Simulator ChampSim.</p>

# Compile

* How to build it?

```
$ ./build.sh ${configuration} ${encryption_operation} ${num_core}
$ ./build.sh enclave off 8
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
