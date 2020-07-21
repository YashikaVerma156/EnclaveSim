<p align="center">
  <h1 align="center"> ChampSim </h1>
  <p> ChampSim with enclave support.</p>

# Compile

* How to build it?

```
$ ./build.sh lru 4 baseline
$ ./build.sh ${replacement-policy} ${num-core} ${configuration}
```

# Run simulation

* Single-core simulation:

```
Usage:[BINARY][N_WARM][N_SIM] [TRACE][INTENSITY] [LIFETIME]

./bin/bimodal-no-no-no-no-lru-enclave-1core -warmup_instructions 10000000 -simulation_instructions 50000000 -traces ~/DPC-traces/605.mcf_s-484B.champsimtrace.xz 10000 10 
```

* Multi-core simulation: Run simulation with `run.sh` script. <br>

```
**Compile and test**
$ ./me.sh
```
Good luck and be a champion! <br>
