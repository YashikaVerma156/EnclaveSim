<p align="center">
  <h1 align="center"> EnclaveSim </h1>
  <p> A trace-based micro-architectural simulator to support enclave simulations.</p>

# Compile

* How to build it?

```
$ ./build.sh ${replacement-policy} ${configuration} ${memory_encryption_engine} ${num-core}
$ ./build.sh lru enclave off 8
```

# Run simulation [To re-generate EnclaveSim results]

* For single-core results 

```
./run1core_baseline_cal.sh
./run1core_enclave_cal.sh

```
* For multi-core results 

```
./run8core_baseline_cal.sh
./run8core_enclave_cal.sh
```
Good luck! <br>
