#define _BSD_SOURCE

#include <getopt.h>
#include "ooo_cpu.h"
#include "uncore.h"
#include <fstream>

uint8_t warmup_complete[NUM_CPUS],
    simulation_complete[NUM_CPUS],
    all_warmup_complete = 0,
    all_simulation_complete = 0,
    MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS,
    knob_cloudsuite = 0,
    knob_low_bandwidth = 0;

uint64_t warmup_instructions = 1000000,
         simulation_instructions = 10000000,
         champsim_seed;

time_t start_time;

// PAGE TABLE
uint32_t PAGE_TABLE_LATENCY = 0, SWAP_LATENCY = 0;
queue<uint64_t> page_queue;
map<uint64_t, uint64_t> recent_page, unique_cl[NUM_CPUS];
uint64_t previous_ppage, num_adjacent_page, num_cl[NUM_CPUS], allocated_pages, num_page[NUM_CPUS], minor_fault[NUM_CPUS], major_fault[NUM_CPUS];

// @EnclaveSim: newly added data structures
unordered_map<uint64_t, pair<uint64_t, bool>> page_table;
unordered_map<uint64_t, uint64_t> inverse_table;
ListNode *least_recently_used_page = NULL, *most_recently_used_page = NULL;
unordered_map<uint64_t, pair<uint64_t, ListNode *>> enclave_page_map; // (va, {pa, link-node address})

uint64_t non_enclave_allocated_pages = 0, enclave_allocated_pages = 0, num_adjacent_enclave_page = 0, num_adjacent_non_enclave_page = 0, previous_enclave_ppage = 0, previous_non_enclave_ppage = 0;

uint64_t sizeofListNode = 0;

bool is_deadlock_check_required[NUM_CPUS];
int rob_head[NUM_CPUS], rob_tail[NUM_CPUS], flag[NUM_CPUS];

uint64_t enclave_minor_fault[NUM_CPUS], enclave_major_fault[NUM_CPUS], non_enclave_minor_fault[NUM_CPUS], non_enclave_major_fault[NUM_CPUS];

vector<vector<uint32_t>> start_point(NUM_CPUS);
vector<vector<uint32_t>> end_point(NUM_CPUS);
vector<vector<uint32_t>> lifetime(NUM_CPUS);
int current_enclave[NUM_CPUS];
int max_enclave[NUM_CPUS];
int enclave_mode[NUM_CPUS];
uint64_t counter1 = 0, counter2 = 0;
//end

using namespace std;

void record_roi_stats(uint32_t cpu, CACHE *cache)
{
    for (uint32_t i = 0; i < NUM_TYPES; i++)
    {
        cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
        cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
        cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
#ifdef ENCLAVE
        cache->roi_enclave_access[cpu][i] = cache->sim_enclave_access[cpu][i];
        cache->roi_enclave_hit[cpu][i] = cache->sim_enclave_hit[cpu][i];
        cache->roi_enclave_miss[cpu][i] = cache->sim_enclave_miss[cpu][i];
#endif
    }
}

void print_roi_stats(uint32_t cpu, CACHE *cache)
{

    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0, TOTAL_ENCLAVE_ACCESS = 0, TOTAL_ENCLAVE_HIT = 0, TOTAL_ENCLAVE_MISS = 0;

    for (uint32_t i = 0; i < NUM_TYPES; i++)
    {
        TOTAL_ACCESS += cache->roi_access[cpu][i];
        TOTAL_HIT += cache->roi_hit[cpu][i];
        TOTAL_MISS += cache->roi_miss[cpu][i];

#ifdef ENCLAVE
        TOTAL_ENCLAVE_ACCESS += cache->roi_enclave_access[cpu][i];
        TOTAL_ENCLAVE_HIT += cache->roi_enclave_hit[cpu][i];
        TOTAL_ENCLAVE_MISS += cache->roi_enclave_miss[cpu][i];
#endif
    }

    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

#ifdef ENCLAVE
    cout << cache->NAME;
    cout << " TOTAL   E-ACCESS: " << setw(10) << TOTAL_ENCLAVE_ACCESS << " EHIT: " << setw(10) << TOTAL_ENCLAVE_HIT << " EMISS: " << setw(10) << TOTAL_ENCLAVE_MISS << endl;
#endif

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10) << cache->roi_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10) << cache->roi_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10) << cache->roi_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10) << cache->roi_miss[cpu][3] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << endl;

    cout << cache->NAME;
    cout << " AVERAGE MISS LATENCY: " << (1.0 * (cache->total_miss_latency)) / TOTAL_MISS << " cycles" << endl;
    //cout << " AVERAGE MISS LATENCY: " << (cache->total_miss_latency)/TOTAL_MISS << " cycles " << cache->total_miss_latency << "/" << TOTAL_MISS<< endl;

#ifdef ENCLAVE

    const string s = "L1I";
    if (!s.compare(cache->NAME))
    {

        for (uint32_t i = 0; i < NUM_TYPES; i++)
        {
            TOTAL_ACCESS += ooo_cpu[cpu].L1D.roi_access[cpu][i];
            TOTAL_HIT += ooo_cpu[cpu].L1D.roi_hit[cpu][i];
            TOTAL_MISS += ooo_cpu[cpu].L1D.roi_miss[cpu][i];
            TOTAL_ENCLAVE_ACCESS += ooo_cpu[cpu].L1D.roi_enclave_access[cpu][i];
            TOTAL_ENCLAVE_HIT += ooo_cpu[cpu].L1D.roi_enclave_hit[cpu][i];
            TOTAL_ENCLAVE_MISS += ooo_cpu[cpu].L1D.roi_enclave_miss[cpu][i];
        }

        cout << "L1 ";
        cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

        cout << "L1 ";
        cout << " TOTAL   E-ACCESS: " << setw(10) << TOTAL_ENCLAVE_ACCESS << " EHIT: " << setw(10) << TOTAL_ENCLAVE_HIT << " EMISS: " << setw(10) << TOTAL_ENCLAVE_MISS << endl;
    }

#endif
}

void print_sim_stats(uint32_t cpu, CACHE *cache)
{

    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0, TOTAL_ENCLAVE_ACCESS = 0, TOTAL_ENCLAVE_HIT = 0, TOTAL_ENCLAVE_MISS = 0;

    for (uint32_t i = 0; i < NUM_TYPES; i++)
    {

        TOTAL_ACCESS += cache->sim_access[cpu][i];
        TOTAL_HIT += cache->sim_hit[cpu][i];
        TOTAL_MISS += cache->sim_miss[cpu][i];

#ifdef ENCLAVE
        TOTAL_ENCLAVE_ACCESS += cache->sim_enclave_access[cpu][i];
        TOTAL_ENCLAVE_HIT += cache->sim_enclave_hit[cpu][i];
        TOTAL_ENCLAVE_MISS += cache->sim_enclave_miss[cpu][i];
#endif
    }

    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

#ifdef ENCLAVE
    cout << cache->NAME;
    cout << " TOTAL   E-ACCESS: " << setw(10) << TOTAL_ENCLAVE_ACCESS << " EHIT: " << setw(10) << TOTAL_ENCLAVE_HIT << " EMISS: " << setw(10) << TOTAL_ENCLAVE_MISS << endl;
#endif

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10) << cache->sim_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10) << cache->sim_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10) << cache->sim_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10) << cache->sim_miss[cpu][3] << endl;

#ifdef ENCLAVE
    const string s = "L1I";
    if (!s.compare(cache->NAME))
    {

        for (uint32_t i = 0; i < NUM_TYPES; i++)
        {
            TOTAL_ACCESS += ooo_cpu[cpu].L1D.sim_access[cpu][i];
            TOTAL_HIT += ooo_cpu[cpu].L1D.sim_hit[cpu][i];
            TOTAL_MISS += ooo_cpu[cpu].L1D.sim_miss[cpu][i];
            TOTAL_ENCLAVE_ACCESS += ooo_cpu[cpu].L1D.sim_enclave_access[cpu][i];
            TOTAL_ENCLAVE_HIT += ooo_cpu[cpu].L1D.sim_enclave_hit[cpu][i];
            TOTAL_ENCLAVE_MISS += ooo_cpu[cpu].L1D.sim_enclave_miss[cpu][i];
        }

        cout << "L1 ";
        cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

        cout << "L1 ";
        cout << " TOTAL   E-ACCESS: " << setw(10) << TOTAL_ENCLAVE_ACCESS << " EHIT: " << setw(10) << TOTAL_ENCLAVE_HIT << " EMISS: " << setw(10) << TOTAL_ENCLAVE_MISS << endl;
    }
#endif
}

void print_branch_stats()
{
    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
        cout << endl
             << "CPU " << i << " Branch Prediction Accuracy: ";
        cout << (100.0 * (ooo_cpu[i].num_branch - ooo_cpu[i].branch_mispredictions)) / ooo_cpu[i].num_branch;
        cout << "% MPKI: " << (1000.0 * ooo_cpu[i].branch_mispredictions) / (ooo_cpu[i].num_retired - ooo_cpu[i].warmup_instructions);
        cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i].total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i].branch_mispredictions << endl
             << endl;

        cout << "Branch types" << endl;
        cout << "NOT_BRANCH: " << ooo_cpu[i].total_branch_types[0] << " " << (100.0 * ooo_cpu[i].total_branch_types[0]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_DIRECT_JUMP: " << ooo_cpu[i].total_branch_types[1] << " " << (100.0 * ooo_cpu[i].total_branch_types[1]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_INDIRECT: " << ooo_cpu[i].total_branch_types[2] << " " << (100.0 * ooo_cpu[i].total_branch_types[2]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_CONDITIONAL: " << ooo_cpu[i].total_branch_types[3] << " " << (100.0 * ooo_cpu[i].total_branch_types[3]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_DIRECT_CALL: " << ooo_cpu[i].total_branch_types[4] << " " << (100.0 * ooo_cpu[i].total_branch_types[4]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_INDIRECT_CALL: " << ooo_cpu[i].total_branch_types[5] << " " << (100.0 * ooo_cpu[i].total_branch_types[5]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_RETURN: " << ooo_cpu[i].total_branch_types[6] << " " << (100.0 * ooo_cpu[i].total_branch_types[6]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
        cout << "BRANCH_OTHER: " << ooo_cpu[i].total_branch_types[7] << " " << (100.0 * ooo_cpu[i].total_branch_types[7]) / (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl
             << endl;
    }
}

void print_dram_stats()
{
    cout << endl;
    cout << "DRAM Statistics" << endl;
    for (uint32_t i = 0; i < DRAM_CHANNELS; i++)
    {
        cout << " CHANNEL " << i << endl;
        cout << " RQ ROW_BUFFER_HIT: " << setw(10) << uncore.DRAM.RQ[i].ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << setw(10) << uncore.DRAM.RQ[i].ROW_BUFFER_MISS << endl;
        cout << " DBUS_CONGESTED: " << setw(10) << uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES] << endl;
        cout << " WQ ROW_BUFFER_HIT: " << setw(10) << uncore.DRAM.WQ[i].ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << setw(10) << uncore.DRAM.WQ[i].ROW_BUFFER_MISS;
        cout << "  FULL: " << setw(10) << uncore.DRAM.WQ[i].FULL << endl;
        cout << endl;
    }

    uint64_t total_congested_cycle = 0;
    for (uint32_t i = 0; i < DRAM_CHANNELS; i++)
        total_congested_cycle += uncore.DRAM.dbus_cycle_congested[i];
    if (uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES])
        cout << " AVG_CONGESTED_CYCLE: " << (total_congested_cycle / uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]) << endl;
    else
        cout << " AVG_CONGESTED_CYCLE: -" << endl;

#ifdef ENCLAVE_DEBUG_PRINT
    cout << " Total enclave dirty Pages in DRAM: " << counter1 << "Total Enclave dirty pages which are not present in DRAM: " << counter2 << endl;
#endif
}

void reset_cache_stats(uint32_t cpu, CACHE *cache)
{
    for (uint32_t i = 0; i < NUM_TYPES; i++)
    {
        cache->ACCESS[i] = 0;
        cache->HIT[i] = 0;
        cache->MISS[i] = 0;
        cache->MSHR_MERGED[i] = 0;
        cache->STALL[i] = 0;

        cache->sim_access[cpu][i] = 0;
        cache->sim_hit[cpu][i] = 0;
        cache->sim_miss[cpu][i] = 0;

#ifdef ENCLAVE
        cache->sim_enclave_access[cpu][i] = 0;
        cache->sim_enclave_hit[cpu][i] = 0;
        cache->sim_enclave_miss[cpu][i] = 0;
#endif
    }

    cache->total_miss_latency = 0;

    cache->RQ.ACCESS = 0;
    cache->RQ.MERGED = 0;
    cache->RQ.TO_CACHE = 0;

    cache->WQ.ACCESS = 0;
    cache->WQ.MERGED = 0;
    cache->WQ.TO_CACHE = 0;
    cache->WQ.FORWARD = 0;
    cache->WQ.FULL = 0;
}

void finish_warmup()
{
    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    // reset core latency
    // note: since re-ordering he function calls in the main simulation loop, it's no longer necessary to add
    //       extra latency for scheduling and execution, unless you want these steps to take longer than 1 cycle.
    SCHEDULING_LATENCY = 0;
    EXEC_LATENCY = 0;
    DECODE_LATENCY = 2;
    PAGE_TABLE_LATENCY = 100;
    SWAP_LATENCY = 100000;

    cout << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
        cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i].num_retired << " cycles: " << current_core_cycle[i];
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

        ooo_cpu[i].begin_sim_cycle = current_core_cycle[i];
        ooo_cpu[i].begin_sim_instr = ooo_cpu[i].num_retired;

        // reset branch stats
        ooo_cpu[i].num_branch = 0;
        ooo_cpu[i].branch_mispredictions = 0;
        ooo_cpu[i].total_rob_occupancy_at_branch_mispredict = 0;

        for (uint32_t j = 0; j < 8; j++)
        {
            ooo_cpu[i].total_branch_types[j] = 0;
        }

        reset_cache_stats(i, &ooo_cpu[i].L1I);
        reset_cache_stats(i, &ooo_cpu[i].L1D);
        reset_cache_stats(i, &ooo_cpu[i].L2C);
        reset_cache_stats(i, &uncore.LLC);
    }
    cout << endl;

    // reset DRAM stats
    for (uint32_t i = 0; i < DRAM_CHANNELS; i++)
    {
        uncore.DRAM.RQ[i].ROW_BUFFER_HIT = 0;
        uncore.DRAM.RQ[i].ROW_BUFFER_MISS = 0;
        uncore.DRAM.WQ[i].ROW_BUFFER_HIT = 0;
        uncore.DRAM.WQ[i].ROW_BUFFER_MISS = 0;
    }

    // set actual cache latency
    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
        ooo_cpu[i].ITLB.LATENCY = ITLB_LATENCY;
        ooo_cpu[i].DTLB.LATENCY = DTLB_LATENCY;
        ooo_cpu[i].STLB.LATENCY = STLB_LATENCY;
        ooo_cpu[i].L1I.LATENCY = L1I_LATENCY;
        ooo_cpu[i].L1D.LATENCY = L1D_LATENCY;
        ooo_cpu[i].L2C.LATENCY = L2C_LATENCY;
    }
    uncore.LLC.LATENCY = LLC_LATENCY;
}

void print_deadlock(uint32_t i)
{
    cout << "DEADLOCK! CPU " << i << " instr_id: " << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].instr_id;
    cout << " translated: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].translated;
    cout << " fetched: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].fetched;
    cout << " scheduled: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].scheduled;
    cout << " executed: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].executed;
    cout << " is_memory: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].is_memory;
    cout << " event: " << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle;
    cout << " current: " << current_core_cycle[i] << endl;

    // print LQ entry
    cout << endl
         << "Load Queue Entry" << endl;
    for (uint32_t j = 0; j < LQ_SIZE; j++)
    {
        cout << "[LQ] entry: " << j << " instr_id: " << ooo_cpu[i].LQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].LQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].LQ.entry[j].translated << " fetched: " << +ooo_cpu[i].LQ.entry[i].fetched << endl;
    }

    // print SQ entry
    cout << endl
         << "Store Queue Entry" << endl;
    for (uint32_t j = 0; j < SQ_SIZE; j++)
    {
        cout << "[SQ] entry: " << j << " instr_id: " << ooo_cpu[i].SQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].SQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].SQ.entry[j].translated << " fetched: " << +ooo_cpu[i].SQ.entry[i].fetched << endl;
    }

    // print L1D MSHR entry
    PACKET_QUEUE *queue;
    queue = &ooo_cpu[i].L1D.MSHR;
    cout << endl
         << queue->NAME << " Entry" << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] entry: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << " lq_index: " << queue->entry[j].lq_index << " sq_index: " << queue->entry[j].sq_index << endl;
    }

    assert(0);
}

void signal_handler(int signal)
{
    cout << "Caught signal: " << signal << endl;
    exit(1);
}

// log base 2 function from efectiu
int lg2(int n)
{
    int i, m = n, c = -1;
    for (i = 0; m; i++)
    {
        m /= 2;
        c++;
    }
    return c;
}

uint64_t rotl64(uint64_t n, unsigned int c)
{
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    assert((c <= mask) && "rotate by type width or more");
    c &= mask; // avoid undef behaviour with NDEBUG.  0 overhead for most types / compilers
    return (n << c) | (n >> ((-c) & mask));
}

uint64_t rotr64(uint64_t n, unsigned int c)
{
    const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
    assert((c <= mask) && "rotate by type width or more");
    c &= mask; // avoid undef behaviour with NDEBUG.  0 overhead for most types / compilers
    return (n >> c) | (n << ((-c) & mask));
}

RANDOM champsim_rand(champsim_seed);

uint64_t print_rob(int cpu)
{

    cout << endl;
    cout << "Occupancy: " << ooo_cpu[cpu].ROB.occupancy << " Head: " << ooo_cpu[cpu].ROB.head << " Tail: " << ooo_cpu[cpu].ROB.tail << endl;
    for (int i = 0; i < ROB_SIZE; i++)
    {
        cout << "Index: " << i << " Event_Cycle: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].event_cycle);
        cout << " Execution_Begin_Cycle: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].execute_begin_cycle);
        cout << " Traslated_Cycle: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].translated_cycle);
        cout << " Fetched_Cycle: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].fetched_cycle);
        cout << " Retired_Cycle: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].retired_cycle);
        cout << " PA: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].physical_address);
        cout << " VA: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].virtual_address);
        cout << " enclave-id: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].enclave_id);
        cout << " Instruction_ID: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].instr_id);
        cout << " Instruction_PA: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].instruction_pa);
        cout << " Instruction_IP: " << (uint64_t)(ooo_cpu[cpu].ROB.entry[i].ip) << endl;
    }
    cout << endl;
    return 0;
}

uint64_t print_cache_mshr(int i)
{

    uint full_address = 0;

    PACKET_QUEUE *queue;
    queue = &ooo_cpu[i].L1D.MSHR;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &ooo_cpu[i].L2C.RQ;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &ooo_cpu[i].L2C.WQ;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &ooo_cpu[i].L2C.MSHR;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &uncore.LLC.RQ;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &uncore.LLC.WQ;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &uncore.LLC.MSHR;
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &uncore.DRAM.RQ[0];
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    queue = &uncore.DRAM.WQ[0];
    cout << endl
         << queue->NAME << endl;
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        uint64_t ppage = queue->entry[j].full_addr >> 12;
        cout << "[" << queue->NAME << "] index: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        cout << " address: " << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << " page_number: " << ppage << " type: " << +queue->entry[j].type;
        cout << " fill_level: " << queue->entry[j].fill_level << endl;
    }

    return 0;
}

uint64_t print_block_status_in_cache(int i)
{
    uint64_t uid = ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].instr_id;

    uint full_address = 0;

    PACKET_QUEUE *queue;
    queue = &ooo_cpu[i].L1D.MSHR;
    cout << endl << queue->NAME << endl;
   
    for (uint32_t j = 0; j < queue->SIZE; j++)
    {
        if (queue->entry[j].instr_id == uid)
            full_address = queue->entry[j].full_addr;
    }

    full_address = full_address >> 6;

    PACKET *packet;
    packet->address = full_address;
    packet->cpu = i;

    if (ooo_cpu[i].L1D.check_hit(packet) != -1)
        cout << "Packet will hit at L1D";
    else
        cout << "Packet will get miss at L1D";

    if (ooo_cpu[i].L2C.check_hit(packet) != -1)
        cout << "Packet will hit at L2C";
    else
        cout << "Packet will get miss at L2C";

    if (uncore.LLC.check_hit(packet) != -1)
        cout << "Packet will hit at LLC";
    else
        cout << "Packet will get miss at LL C";
}

void disable_deadlock_check(int cpu) {

#ifdef ENCLAVE_DEBUG_PRINT
    cout << "Deadlock check is disable for CPU: " << cpu << endl;
    print_rob(cpu);
#endif

    // disable deadlock checks for rob entries present at teardown stage
    is_deadlock_check_required[cpu] = false;
    rob_head[cpu] = ooo_cpu[cpu].ROB.head;
    rob_tail[cpu] = ooo_cpu[cpu].ROB.tail;
}

void enable_deadlock_check(int i) {

    // deadlock is already enabled
    if (is_deadlock_check_required[i])  
        return;
    else 
    {
        // when rob is not full
        if ((ooo_cpu[i].ROB.head == rob_tail[i] % ROB_SIZE) && abs(rob_head[i] - rob_tail[i]) != 0) {
            is_deadlock_check_required[i] = true;
            rob_head[i] = -1;
            rob_tail[i] = -1;
#ifdef ENCLAVE_DEBUG_PRINT
            cout << endl << "CPU " << i << "  Deadlock check is enabled!" << " ROB_HEAD: " << ooo_cpu[i].ROB.head << " ROB_TAIL: " << ooo_cpu[i].ROB.tail   << " Current Cycle: " << current_core_cycle[i] << endl << endl;
            print_rob(i);
#endif
        }
        // when rob is full
        else if (abs(rob_head[i] - rob_tail[i]) == 0) {
            if (ooo_cpu[i].ROB.head != rob_head[i]) {
                flag[i] = true;
            }
            if ((ooo_cpu[i].ROB.head == (rob_tail[i] % ROB_SIZE))) {
                if (flag[i] == true) {
                    is_deadlock_check_required[i] = true;
                    rob_head[i] = -1;
                    rob_tail[i] = -1;
                    flag[i] = false;
#ifdef ENCLAVE_DEBUG_PRINT
                    cout << endl << "CPU " << i << "  Deadlock check is enabled!" << " ROB_HEAD: " << ooo_cpu[i].ROB.head << " ROB_TAIL: " << ooo_cpu[i].ROB.tail   << " Current Cycle: " << current_core_cycle[i] << endl << endl;
                    print_rob(i);
#endif
                }
            }
        }
    }
}

void invalidate_linknode(unordered_map<uint64_t, pair<uint64_t, ListNode *>>::iterator pr_enclave)
{

    uint64_t vpage = pr_enclave->first;

    // remove entry from linked-list
    ListNode *current = enclave_page_map[vpage].second;
    uint64_t enclave_pages = enclave_page_map.size();

    // when only 1 page is there in enclave region.
    if (enclave_pages == 1)
    {
        least_recently_used_page = NULL;
        most_recently_used_page = NULL;
    }
    // when 2 page is there in enclave region.
    else if (enclave_pages == 2)
    {
        // check if the required page is head.
        if (current == least_recently_used_page)
        {
            most_recently_used_page->prev = NULL;
            least_recently_used_page = most_recently_used_page;
        }
        // check if the required page is tail.
        else if (current == most_recently_used_page)
        {
            least_recently_used_page->next = NULL;
            most_recently_used_page = least_recently_used_page;
        }
    }
    // when there are more than 2 page is there in enclave region.
    else
    {
        // check if the required page is head.
        if (current == least_recently_used_page)
        {
            least_recently_used_page = least_recently_used_page->next;
            least_recently_used_page->prev = NULL;
        }
        // check if the required page is tail.
        else if (current == most_recently_used_page)
        {
            most_recently_used_page = most_recently_used_page->prev;
            most_recently_used_page->next = NULL;
        }
        // required page is middle node.
        else
        {
            ListNode *prevNode = current->prev;
            ListNode *nextNode = current->next;
            prevNode->next = current->next;
            nextNode->prev = current->prev;
        }
    }
    free(current);
    enclave_page_map.erase(pr_enclave);
    enclave_allocated_pages--;
}

void invalidate_page(unordered_map<uint64_t, pair<uint64_t, bool>>::iterator pr, uint32_t cpu)
{


    uint64_t mapped_ppage = (pr->second).first;
    uint64_t vpage = pr->first;

    for (int i= 0; i<NUM_CPUS; i++) {
        // invalidate corresponding vpage and ppage from the cache hierarchy
        ooo_cpu[i].ITLB.invalidate_entry(vpage, cpu);
        ooo_cpu[i].DTLB.invalidate_entry(vpage, cpu);
        ooo_cpu[i].STLB.invalidate_entry(vpage, cpu);
        for (uint32_t j = 0; j < BLOCK_SIZE; j++)
        {
            uint64_t cl_addr = (mapped_ppage << 6) | j;
            ooo_cpu[i].L1I.invalidate_entry(cl_addr, cpu);
            ooo_cpu[i].L1D.invalidate_entry(cl_addr, cpu);
            ooo_cpu[i].L2C.invalidate_entry(cl_addr, cpu);
            uncore.LLC.invalidate_entry(cl_addr, cpu);
        }
    }

    //remove entries from page table
    page_table.erase(vpage);
    inverse_table.erase(mapped_ppage);
}


// invalidate tlbs & caches. set enclave status exit.
void enclave_teardown(uint32_t cpu)
{
    // add context-switch latency
    if (stall_cycle[cpu] <= current_core_cycle[cpu])
        stall_cycle[cpu] = current_core_cycle[cpu] + ENCLAVE_CONTEXT_SWITCH_LATENCY;  
    else
        stall_cycle[cpu] += ENCLAVE_CONTEXT_SWITCH_LATENCY;

    int dirty_enclave_pages = 0, total_enclave_pages = 0;

    for (uint64_t ppage = 1; ppage <= ENCLAVE_PAGES; ppage++)
    {
        unordered_map<uint64_t, uint64_t>::iterator ppage_check = inverse_table.find(ppage);
        if (ppage_check != inverse_table.end())
        {
            uint64_t vpage = ppage_check->second;
            uint64_t cpu_id = vpage >> (int)(64 - log2(NUM_CPUS));
            unordered_map<uint64_t, pair<uint64_t, bool>>::iterator pr = page_table.find(vpage);

            if (pr == page_table.end()) assert(0 && "Page Table inconsistency");

            if (cpu_id == cpu) {
                
                // invalidate page from page table
                invalidate_page(pr, cpu);

                // invalidate page from enclave page map
                unordered_map<uint64_t, pair<uint64_t, ListNode *>>::iterator pr_enclave = enclave_page_map.find(vpage);
                invalidate_linknode(pr_enclave);

                // add latency for the pages which are dirty
                if ((pr->second).second) {
                    stall_cycle[cpu] += ENCLAVE_TEARDOWN_LATENCY;
                    dirty_enclave_pages++;
                }

                total_enclave_pages++;
            }
        }
    }
    disable_deadlock_check(cpu);

    cout << "E-Pages: " << total_enclave_pages << " Dirty-pages:" << dirty_enclave_pages << " Latency-Delay: " << stall_cycle[cpu] - current_core_cycle[cpu] << " rob head: " << rob_head[cpu] << " rob tail: " << rob_tail[cpu] << " occupancy: " << ooo_cpu[cpu].ROB.occupancy << endl << endl;

}

void check_enclave_init(int cpu)
{
    // all enclaves of an application are already executed
    if (current_enclave[cpu] >= max_enclave[cpu]) return;

    // processor is already in enclave mode
    if (enclave_mode[cpu] == 1) return;

    
    int enclave_number = current_enclave[cpu];
    int enclave_start_point = start_point[cpu][enclave_number];
    int enclave_end_point = end_point[cpu][enclave_number];
    int current_instruction = ooo_cpu[cpu].num_retired - ooo_cpu[cpu].begin_sim_instr;

    // if the application reaches the checkpoint, set up the enclave
    if (enclave_mode[cpu] == 0 and current_instruction >= enclave_start_point and current_instruction < enclave_end_point) {
            
        cout << endl << "Enclave INIT! " << " CPU:" << cpu << " Enclave-Number:" << enclave_number << " Current Intruction: " << current_instruction << " EENTRY POINT:" << enclave_start_point << " Current Cycle: " << current_core_cycle[cpu] << endl << endl;

        // update the stall cycle
        if (stall_cycle[cpu] <= current_core_cycle[cpu]) {
            stall_cycle[cpu] = current_core_cycle[cpu];
        }
        stall_cycle[cpu] += ENCLAVE_CONTEXT_SWITCH_LATENCY;
        
        // switch processor into enclave mode
        enclave_mode[cpu] = 1;
    }
}

void check_enclave_exit(int cpu)
{

    // all enclaves are already executed
    if (current_enclave[cpu] >= max_enclave[cpu]) return;

    // processor is already in non-enclave mode 
    if (enclave_mode[cpu] == 0) return; 

    int enclave_number = current_enclave[cpu];
    int enclave_start_point = start_point[cpu][enclave_number];
    int enclave_end_point = end_point[cpu][enclave_number];
    int current_instruction = ooo_cpu[cpu].num_retired - ooo_cpu[cpu].begin_sim_instr;
    

    if (enclave_mode[cpu] == 1 and current_instruction >= enclave_end_point) {

        cout << endl << "Enclave EXIT! " << " CPU:" << cpu << " Enclave-Number:" << enclave_number << " Current_Intruction: " << current_instruction << " EEXIT POINT:" << enclave_end_point << " Current_Cycle: " << current_core_cycle[cpu] << endl;

#ifdef ENCLAVE_DEBUG_PRINT        
        print_rob(cpu);
#endif

        // they all run atomically
        enclave_teardown(cpu);
        enclave_mode[cpu] = 0;
        current_enclave[cpu]++; 
    }
}

void init_exit_assert(int cpu){

    if (current_enclave[cpu] >= max_enclave[cpu]) return;

    int enclave_number = current_enclave[cpu];
    int enclave_start_point = start_point[cpu][enclave_number];
    int enclave_end_point = end_point[cpu][enclave_number];
    int current_instruction = ooo_cpu[cpu].num_retired - ooo_cpu[cpu].begin_sim_instr;

    if (current_instruction <=0 ) return;

    if ((enclave_mode[cpu] == 0 ) and (current_instruction  > enclave_start_point + ROB_SIZE and current_instruction < enclave_end_point)){
        cout << " Current Intruction: " << current_instruction << " START_POINT: "<< enclave_start_point << " END_POINT: "<< enclave_end_point << endl;
        assert(0 && "Enclave mode is off, but instructions are in enclave range!");
    }
    else if ((enclave_mode[cpu] == 1 ) and (current_instruction  < enclave_start_point or current_instruction > enclave_end_point + ROB_SIZE)) {
        // execution should not be inside the enclave
        cout << " Current Intruction: " << current_instruction << " START_POINT: "<< enclave_start_point << " END_POINT: "<< enclave_end_point << endl;
        assert(0 && "Enclave mode is on, but instructions are outside enclave range!");
    }

}

uint64_t lru_list_size() {
    uint64_t size = 0;
    ListNode *current = least_recently_used_page;
    while(current) {
        size++;
        current = current->next;
    }
    return size;    
}

// it returns enclave-id corresponding to the physical address.
uint8_t return_enclave_id(uint32_t cpu, uint64_t physical_address)
{
    int ppage = physical_address >> LOG2_PAGE_SIZE;
    uint32_t enclave_id;
    if (ppage > 0 and ppage <= ENCLAVE_PAGES)
        enclave_id = cpu;
    else
        enclave_id = NUM_CPUS;
    return enclave_id;
}

uint64_t va_to_pa_baseline(uint32_t cpu, uint64_t instr_id, uint64_t va, uint64_t unique_vpage, uint8_t is_code)
{

#ifdef SANITY_CHECK
    if (va == 0)
        assert(0);
#endif

    uint8_t swap = 0;
    uint64_t high_bit_mask = rotr64(cpu, lg2(NUM_CPUS)),
             unique_va = va | high_bit_mask;
    //uint64_t vpage = unique_va >> LOG2_PAGE_SIZE,
    uint64_t vpage = unique_vpage | high_bit_mask,
             voffset = unique_va & ((1 << LOG2_PAGE_SIZE) - 1);

    // smart random number generator
    uint64_t random_ppage;

    unordered_map<uint64_t, pair<uint64_t, bool>>::iterator pr = page_table.begin();
    unordered_map<uint64_t, uint64_t>::iterator ppage_check = inverse_table.begin();

    // check unique cache line footprint
    map<uint64_t, uint64_t>::iterator cl_check = unique_cl[cpu].find(unique_va >> LOG2_BLOCK_SIZE);
    if (cl_check == unique_cl[cpu].end())
    { // we've never seen this cache line before
        unique_cl[cpu].insert(make_pair(unique_va >> LOG2_BLOCK_SIZE, 0));
        num_cl[cpu]++;
    }
    else
        cl_check->second++;

    pr = page_table.find(vpage);
    if (pr == page_table.end())
    { // no VA => PA translation found

        if (allocated_pages >= DRAM_PAGES)
        { // not enough memory

            // TODO: elaborate page replacement algorithm
            // here, ChampSim randomly selects a page that is not recently used and we only track 32K recently accessed pages
            uint8_t found_NRU = 0;
            uint64_t NRU_vpage = 0; // implement it

            for (pr = page_table.begin(); pr != page_table.end(); pr++)
            {
                NRU_vpage = pr->first;
                if (recent_page.find(NRU_vpage) == recent_page.end())
                {
                    found_NRU = 1;
                    break;
                }
            }
#ifdef SANITY_CHECK
            if (found_NRU == 0)
                assert(0);

            if (pr == page_table.end())
                assert(0);
#endif
            DP(if (warmup_complete[cpu]) { cout << "[SWAP] update page table NRU_vpage: " << hex << pr->first << " new_vpage: " << vpage << " ppage: " << pr->second.first << dec << endl; });

            // update page table with new VA => PA mapping
            // since we cannot change the key value already inserted in a map structure, we need to erase the old node and add a new node

            uint64_t mapped_ppage = (pr->second).first;
            page_table.erase(pr);
            page_table[vpage] = {mapped_ppage, false};

            pr = page_table.find(vpage);

            // update inverse_table.findse table with new PA => VA mapping
            ppage_check = inverse_table.find(mapped_ppage);
#ifdef SANITY_CHECK
            if (ppage_check == inverse_table.end())
                assert(0);
#endif
            ppage_check->second = vpage;

            DP(if (warmup_complete[cpu]) {
            cout << "[SWAP] update inverse table NRU_vpage: " << hex << NRU_vpage << " new_vpage: ";
            cout << ppage_check->second << " ppage: " << ppage_check->first << dec << endl; });

            // update page_queue
            page_queue.pop();
            page_queue.push(vpage);

            // invalidate corresponding vpage and ppage from the cache hierarchy
            ooo_cpu[cpu].ITLB.invalidate_entry(NRU_vpage, cpu);
            ooo_cpu[cpu].DTLB.invalidate_entry(NRU_vpage, cpu);
            ooo_cpu[cpu].STLB.invalidate_entry(NRU_vpage, cpu);
            for (uint32_t i = 0; i < BLOCK_SIZE; i++)
            {
                uint64_t cl_addr = (mapped_ppage << 6) | i;
                ooo_cpu[cpu].L1I.invalidate_entry(cl_addr, cpu);
                ooo_cpu[cpu].L1D.invalidate_entry(cl_addr, cpu);
                ooo_cpu[cpu].L2C.invalidate_entry(cl_addr, cpu);
                uncore.LLC.invalidate_entry(cl_addr, cpu);
            }

            // swap complete
            swap = 1;
        }
        else
        {
            uint8_t fragmented = 0;
            if (num_adjacent_page > 0)
                random_ppage = ++previous_ppage;
            else
            {
                random_ppage = champsim_rand.draw_rand();
                fragmented = 1;
            }

            // encoding cpu number
            // this allows ChampSim to run homogeneous multi-programmed workloads without VA => PA aliasing
            // (e.g., cpu0: astar  cpu1: astar  cpu2: astar  cpu3: astar...)
            //random_ppage &= (~((NUM_CPUS-1)<< (32-LOG2_PAGE_SIZE)));
            //random_ppage |= (cpu<<(32-LOG2_PAGE_SIZE));

            while (1)
            {                                                   // try to find an empty physical page number
                ppage_check = inverse_table.find(random_ppage); // check if this page can be allocated
                if (ppage_check != inverse_table.end())
                { // random_ppage is not available
                    DP(if (warmup_complete[cpu]) { cout << "vpage: " << hex << ppage_check->first << " is already mapped to ppage: " << random_ppage << dec << endl; });

                    if (num_adjacent_page > 0)
                        fragmented = 1;

                    // try one more time
                    random_ppage = champsim_rand.draw_rand();

                    // encoding cpu number
                    //random_ppage &= (~((NUM_CPUS-1)<<(32-LOG2_PAGE_SIZE)));
                    //random_ppage |= (cpu<<(32-LOG2_PAGE_SIZE));
                }
                else
                    break;
            }

            // insert translation to page tables
            //printf("Insert  num_adjacent_page: %u  vpage: %lx  ppage: %lx\n", num_adjacent_page, vpage, random_ppage);

            page_table[vpage] = {random_ppage, false};

            inverse_table.insert(make_pair(random_ppage, vpage));
            page_queue.push(vpage);
            previous_ppage = random_ppage;
            num_adjacent_page--;
            num_page[cpu]++;
            allocated_pages++;

            // try to allocate pages contiguously
            if (fragmented)
            {
                num_adjacent_page = 1 << (rand() % 10);
                DP(if (warmup_complete[cpu]) { cout << "Recalculate num_adjacent_page: " << num_adjacent_page << endl; });
            }
        }

        if (swap)
            major_fault[cpu]++;
        else
            minor_fault[cpu]++;
    }
    else
    {
        //printf("Found  vpage: %lx  random_ppage: %lx\n", vpage, pr->second);
    }

    pr = page_table.find(vpage);
#ifdef SANITY_CHECK
    if (pr == page_table.end())
        assert(0);
#endif
    uint64_t ppage = (pr->second).first;

    uint64_t pa = ppage << LOG2_PAGE_SIZE;
    pa |= voffset;

    DP(if (warmup_complete[cpu]) {
    cout << "[PAGE_TABLE] instr_id: " << instr_id << " vpage: " << hex << vpage;
    cout << " => ppage: " << (pa >> LOG2_PAGE_SIZE) << " vadress: " << unique_va << " paddress: " << pa << dec << endl; });

    // as a hack for code prefetching, code translations are magical and do not pay these penalties
    if (!is_code)
    {
        // if it's data, pay these penalties
        if (swap)
            stall_cycle[cpu] = current_core_cycle[cpu] + SWAP_LATENCY;
        else
            stall_cycle[cpu] = current_core_cycle[cpu] + PAGE_TABLE_LATENCY;
    }

    //cout << "cpu: " << cpu << " allocated unique_vpage: " << hex << unique_vpage << " to ppage: " << ppage << dec << endl;
    return pa;

}

uint64_t va_to_pa_enclave(uint32_t cpu, uint64_t instr_id, uint64_t va, uint64_t unique_vpage, uint8_t is_code)
{

    init_exit_assert(cpu);

    if (enclave_page_map.size() != enclave_allocated_pages) {
        assert(0 && "Enclave page counter is not consistent");
    }
    
    if (inverse_table.size() != page_table.size()) {
        cout << endl<< inverse_table.size() << " : " << page_table.size() << endl;  
        assert(0 && "Page table is not consistent");
    }

#ifdef SANITY_CHECK
    if (va == 0)
        assert(0);
#endif

    if (stall_cycle[cpu] <= current_core_cycle[cpu]) {
        stall_cycle[cpu] = current_core_cycle[cpu];
    }


    uint8_t swap = 0;
    bool is_page_hit = false;

    uint64_t high_bit_mask = rotr64(cpu, lg2(NUM_CPUS)),
             unique_va = va | high_bit_mask;

    uint64_t vpage = unique_vpage | high_bit_mask, voffset = unique_va & ((1 << LOG2_PAGE_SIZE) - 1);

    uint64_t random_ppage;
    
    // initializing iterators for page-table 
    unordered_map<uint64_t, pair<uint64_t, ListNode *>>::iterator pr_enclave = enclave_page_map.begin();
    unordered_map<uint64_t, pair<uint64_t, bool>>::iterator pr = page_table.begin();
    unordered_map<uint64_t, uint64_t>::iterator ppage_check = inverse_table.begin();

    // set page-table iterators   
    pr_enclave = enclave_page_map.find(vpage);
    pr = page_table.find(vpage);

    // (transition between untrusted code to trusted code, when enclave is enabled)
    if (enclave_mode[cpu] == 1 and (pr_enclave == enclave_page_map.end() and pr != page_table.end())) {
        invalidate_page(pr, cpu);
        non_enclave_allocated_pages--;
        // set iterator again
        pr = page_table.find(vpage);
    }

    if (enclave_mode[cpu] == 0 and (pr_enclave != enclave_page_map.end())) {
        assert(0 && "Illegal Enclave Page Found");
    }

    // page miss
    if (pr == page_table.end()) {

        // major fault
        if ((enclave_allocated_pages >= ENCLAVE_PAGES and enclave_mode[cpu] == 1) or
            (non_enclave_allocated_pages >= NON_ENCLAVE_PAGES and enclave_mode[cpu] == 0)) {

            // if (enclave_mode[cpu] == 0) {
            //     cout << non_enclave_allocated_pages << endl;
            //     assert(0);
            // }

            uint64_t old_vpage = 0;

            // enclave major fault
            if (enclave_mode[cpu] == 1) {

                    // create new page for linked-list
                    ListNode *new_page = new ListNode(vpage);

                    old_vpage = least_recently_used_page->val;
                    ListNode *old_least_recently_used_page = least_recently_used_page;

                    // update least recently used page
                    least_recently_used_page = least_recently_used_page->next;
                    least_recently_used_page->prev = NULL;
                    free(old_least_recently_used_page);

                    // update most recently used page
                    most_recently_used_page->next = new_page;
                    new_page->prev = most_recently_used_page;
                    most_recently_used_page = new_page;

                    pr = page_table.find(old_vpage);

#ifdef SANITY_CHECK
                    if (pr == page_table.end())
                        assert(0);
                    if (old_vpage == 0)
                        assert(0);
#endif

                    // update the enclave page map with the new node address with the victim ppage.
                    enclave_page_map[vpage] = {(pr->second).first, new_page};
                    enclave_page_map.erase(old_vpage);

            }
            // non-enclave major fault
            else {
                 
                    uint8_t found_NRU = 0;
                    uint64_t NRU_vpage = 0; // not implement yet by the champsim creator

                    for (pr = page_table.begin(); pr != page_table.end(); pr++) {
                        NRU_vpage = pr->first;
                        if (recent_page.find(NRU_vpage) == recent_page.end()) {
                            found_NRU = 1;
                            break;
                        }
                    }

                    old_vpage = NRU_vpage;

#ifdef SANITY_CHECK
                    if (found_NRU == 0)
                        assert(0);
                    if (pr == page_table.end())
                        assert(0);
#endif
            }

            if (old_vpage == 0)
                assert(0);

            DP(if (warmup_complete[cpu]) { cout << "[SWAP] update page table victim_vpage: " << hex << pr->first << " new_vpage: " << vpage << " ppage: " << pr->second << dec << endl; });

            // update page table
            uint64_t mapped_ppage = (pr->second).first;
            page_table.erase(pr);
            page_table[vpage] = {mapped_ppage, false};

            // update inverse table
            ppage_check = inverse_table.find(mapped_ppage);

#ifdef SANITY_CHECK
            if (ppage_check == inverse_table.end())
                assert(0);
#endif

            ppage_check->second = vpage;

            // sanity check for physical page number
            if (enclave_mode[cpu] == 1) {
                if (mapped_ppage <= 0 or mapped_ppage > ENCLAVE_PAGES) {
                    cout << mapped_ppage << endl;
                    assert(0 && "Illegal page allocation");
                }
                    
            }
            else {
                if (mapped_ppage <= ENCLAVE_PAGES or mapped_ppage > DRAM_PAGES) {
                    cout << mapped_ppage << endl;
                    assert(0 && "Illegal page allocation");
                }
            }

            DP(if (warmup_complete[cpu]) {
            cout << "[SWAP] update inverse table least_recent_used_page: " << hex << victim_vpage << " new_vpage: ";
            cout << ppage_check->second << " ppage: " << ppage_check->first << dec << endl; });

            // invalidate corresponding vpage and ppage from the cache hierarchy
            for (int i = 0; i<NUM_CPUS; i++) {
                // invalidate corresponding vpage and ppage from the cache hierarchy
                ooo_cpu[i].ITLB.invalidate_entry(old_vpage, cpu);
                ooo_cpu[i].DTLB.invalidate_entry(old_vpage, cpu);
                ooo_cpu[i].STLB.invalidate_entry(old_vpage, cpu);
                for (uint32_t j = 0; j < BLOCK_SIZE; j++)
                {
                    uint64_t cl_addr = (mapped_ppage << 6) | j;
                    ooo_cpu[i].L1I.invalidate_entry(cl_addr, cpu);
                    ooo_cpu[i].L1D.invalidate_entry(cl_addr, cpu);
                    ooo_cpu[i].L2C.invalidate_entry(cl_addr, cpu);
                    uncore.LLC.invalidate_entry(cl_addr, cpu);
                }
            }

            // set page-swap bit as true 
            swap = 1;
        }
        // Minor Page-fault
        else
        {

            uint8_t fragmented = 0;

            // try to allocate continuos page if possible;
            
            // enclave page allocation
            if (enclave_mode[cpu] == 1) {
                if (num_adjacent_enclave_page > 0 and previous_enclave_ppage < ENCLAVE_PAGES) {
                    random_ppage = ++previous_enclave_ppage;
                }
                else {
                    random_ppage = champsim_rand.draw_rand() % ENCLAVE_PAGES + 1;
                    fragmented = 1;
                }
            }
            // non enclave page allocation
            else {
                if (num_adjacent_non_enclave_page > 0 and previous_non_enclave_ppage < DRAM_PAGES) {
                    random_ppage = ++previous_non_enclave_ppage;
                }
                else {
                        random_ppage = champsim_rand.draw_rand() + ENCLAVE_PAGES + 1;
                        if (random_ppage > DRAM_PAGES) {
                            random_ppage = 0;
                        }
                        fragmented = 1;
                }
            }

            while (1)
            {   
                // try to find an empty physical page number
                ppage_check = inverse_table.find(random_ppage); 
                
                // check if this page can be allocated
                if (ppage_check != inverse_table.end() or random_ppage == 0) { 
                    
                    // random_ppage is not available
                    DP(if (warmup_complete[cpu]) { cout << "vpage: " << hex << ppage_check->first << " is already mapped to ppage: " << random_ppage << dec << endl; });

                    if (num_adjacent_page > 0)
                        fragmented = 1;

                    // try one more time
                    if (enclave_mode[cpu] == 1)
                        random_ppage = champsim_rand.draw_rand() % ENCLAVE_PAGES + 1;
                    else
                        random_ppage = champsim_rand.draw_rand() + ENCLAVE_PAGES + 1;

                }
                else if (random_ppage >= DRAM_PAGES) {
                    random_ppage = 0;
                }
                else
                    break;
            }

            // sanity checks for valid page number
            if (enclave_mode[cpu] == 1) {
                if (random_ppage > ENCLAVE_PAGES or random_ppage <= 0) {
                    cout << random_ppage << endl;
                    assert(0 && "Illegal page allocation");
                }
            }
            else {
                if (random_ppage <= ENCLAVE_PAGES or random_ppage > DRAM_PAGES)
                    assert(0 && "Illegal page allocation");
            }

            if (random_ppage == 0) assert(0 && "Bad page allocation");

            // update page table
            page_table[vpage] = {random_ppage, false};
            inverse_table.insert(make_pair(random_ppage, vpage));

            // minor fault variable updation
            if (enclave_mode[cpu] == 1) {
                num_adjacent_enclave_page--;
                enclave_allocated_pages++;
                previous_enclave_ppage = random_ppage;
                ListNode *new_page = new ListNode(vpage);
                // when new page is the first page
                if (least_recently_used_page == NULL)
                {
                    least_recently_used_page = new_page;
                    most_recently_used_page = new_page;
                }
                // add new page to most-recently used page
                else
                {
                    most_recently_used_page->next = new_page;
                    new_page->prev = most_recently_used_page;
                    most_recently_used_page = new_page;
                }
                if (enclave_page_map.find(vpage) != enclave_page_map.end())
                    assert(0);
                // update the enclave map
                enclave_page_map[vpage] = {random_ppage, new_page};
            }
            else {
                num_adjacent_non_enclave_page--;
                non_enclave_allocated_pages++;
                previous_non_enclave_ppage = random_ppage;
            }
    
            // try to allocate pages contiguously
            if (fragmented) {
                if (enclave_mode[cpu] == 1)
                    num_adjacent_enclave_page = 1 << (rand() % 5);
                else
                    num_adjacent_non_enclave_page = 1 << (rand() % 10);

                DP(if (warmup_complete[cpu]) { cout << "Recalculate num_adjacent_page: " << num_adjacent_page << endl; });
            }
        }
        
        // update major and minor faults number
        if (swap) {
            // update major faults number
            if (enclave_mode[cpu] == 1)
                enclave_major_fault[cpu]++;
            else
                non_enclave_major_fault[cpu]++;
            major_fault[cpu]++;
        }
        else {
            // update minor faults number
            if (enclave_mode[cpu] == 1)
                enclave_minor_fault[cpu]++;
            else
                non_enclave_minor_fault[cpu]++;
            minor_fault[cpu]++;
        }

    }
    // page-hit
    else {
        // enclave page hit
        if (enclave_mode[cpu] == 1) {
            // update lru list
            if (pr_enclave == enclave_page_map.end())
                assert(0);
            ListNode *hit_page = pr_enclave->second.second;
            if (least_recently_used_page == most_recently_used_page) {
                ;
            }
            else if (hit_page == most_recently_used_page) {
                ;
            }
            else if (hit_page == least_recently_used_page) {
                most_recently_used_page->next = hit_page;
                least_recently_used_page = least_recently_used_page->next;
                least_recently_used_page->prev = NULL;
                hit_page->next = NULL;
                hit_page->prev = most_recently_used_page;
                most_recently_used_page = hit_page;
            }
            // middle page
            else {
                ListNode *left = hit_page->prev;
                ListNode *right = hit_page->next;
                left->next = right;
                right->prev = left;
                hit_page->next = NULL;
                most_recently_used_page->next = hit_page;
                hit_page->prev = most_recently_used_page;
                most_recently_used_page = hit_page;
            }
            if (least_recently_used_page->prev)
                assert(0);
            if (most_recently_used_page->next)
                assert(0);
        }
        is_page_hit = true;
    }

    pr = page_table.find(vpage);

#ifdef SANITY_CHECK
    if (pr == page_table.end())
        assert(0);
#endif

    uint64_t ppage = (pr->second).first;

    // sanity check for page number
    if (enclave_mode[cpu] == 1) {
        if (ppage > ENCLAVE_PAGES or ppage <= 0) {
            cout << ppage << endl;
            assert(0 && "Illegal page allocation");
        }
    }
    else {
        if (ppage <= ENCLAVE_PAGES or ppage > DRAM_PAGES) {
            cout << ppage << endl;
            assert(0 && "Illegal page allocation");
        }
    }

    uint64_t pa = ppage << LOG2_PAGE_SIZE;
    pa |= voffset;

    DP(if (warmup_complete[cpu]) {
    cout << "[PAGE_TABLE] instr_id: " << instr_id << " vpage: " << hex << vpage;
    cout << " => ppage: " << (pa >> LOG2_PAGE_SIZE) << " vadress: " << unique_va << " paddress: " << pa << dec << endl; });

    // latency overhead for va to pa translation
    if (!is_code) {
        
        // enclave request
        if (enclave_mode[cpu] == 1) {
            // enclave page hit
            if (is_page_hit) 
                stall_cycle[cpu] += PAGE_TABLE_LATENCY;
            // enclave major fault
            else if (swap) {
                if (non_enclave_allocated_pages >= NON_ENCLAVE_PAGES)
                    stall_cycle[cpu] += (SWAP_LATENCY + ENCLAVE_MAJOR_PAGE_FAULT_LATENCY);
                else
                    stall_cycle[cpu] += ENCLAVE_MAJOR_PAGE_FAULT_LATENCY;
            }
            // enclave minor fault
            else 
                stall_cycle[cpu] += ENCLAVE_MINOR_PAGE_FAULT_LATENCY;
        }
        // non-enclave request
        else {
            // major fault
            if (swap)
                stall_cycle[cpu] += SWAP_LATENCY;
            // minor fault
            else
                stall_cycle[cpu] += PAGE_TABLE_LATENCY;
        }
    }

    // cout << "cpu: " << cpu << " allocated unique_vpage: " << unique_vpage << " to ppage: " << ppage << endl;
    return pa;
}

uint64_t va_to_pa(uint32_t cpu, uint64_t instr_id, uint64_t va, uint64_t unique_vpage, uint8_t is_code)
{
#ifdef ENCLAVE
    return va_to_pa_enclave(cpu, instr_id, va, unique_vpage, is_code);
#endif
#ifdef BASELINE
    return va_to_pa_baseline(cpu, instr_id, va, unique_vpage, is_code);
#endif
    return 0;
}

int main(int argc, char **argv)
{

    // interrupt signal hanlder

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    cout << endl
         << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl
         << endl;

    // initialize knobs
    uint8_t show_heartbeat = 1;
    uint32_t seed_number = 0;

    // check to see if knobs changed using getopt_long()

    int c;
    while (1)
    {
        static struct option long_options[] =
            {
                {"warmup_instructions", required_argument, 0, 'w'},
                {"simulation_instructions", required_argument, 0, 'i'},
                {"hide_heartbeat", no_argument, 0, 'h'},
                {"cloudsuite", no_argument, 0, 'c'},
                {"low_bandwidth", no_argument, 0, 'b'},
                {"traces", no_argument, 0, 't'},
                {0, 0, 0, 0}};

        int option_index = 0;

#ifdef ENCLAVE
        c = getopt_long_only(argc, argv, "wiphsb", long_options, &option_index);
#endif

#ifdef BASELINE
        c = getopt_long_only(argc, argv, "wiphsb", long_options, &option_index);
#endif
        // no more option characters
        if (c == -1)
            break;

        int traces_encountered = 0;

        switch (c)
        {
        case 'w':
            warmup_instructions = atol(optarg);
            break;
        case 'i':
            simulation_instructions = atol(optarg);
            break;
        case 'h':
            show_heartbeat = 0;
            break;
        case 'c':
            knob_cloudsuite = 1;
            MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
            break;
        case 'b':
            knob_low_bandwidth = 1;
            break;
        case 't':
            traces_encountered = 1;
            break;
        default:
            abort();
        }

        if (traces_encountered == 1)
            break;
    }

    // consequences of knobs
    cout << "Warmup Instructions: " << warmup_instructions << endl;
    cout << "Simulation Instructions: " << simulation_instructions << endl;

    //cout << "Scramble Loads: " << (knob_scramble_loads ? "true" : "false") << endl;
    cout << "Number of CPUs: " << NUM_CPUS << endl;
    cout << "LLC sets: " << LLC_SET << endl;
    cout << "LLC ways: " << LLC_WAY << endl;

    if (knob_low_bandwidth)
        DRAM_MTPS = DRAM_IO_FREQ / 4;
    else
        DRAM_MTPS = DRAM_IO_FREQ;

    // DRAM access latency
    tRP = (uint32_t)((1.0 * tRP_DRAM_NANOSECONDS * CPU_FREQ) / 1000);
    tRCD = (uint32_t)((1.0 * tRCD_DRAM_NANOSECONDS * CPU_FREQ) / 1000);
    tCAS = (uint32_t)((1.0 * tCAS_DRAM_NANOSECONDS * CPU_FREQ) / 1000);

    // default: 16 = (64 / 8) * (3200 / 1600)
    // it takes 16 CPU cycles to tranfser 64B cache block on a 8B (64-bit) bus
    // note that dram burst length = BLOCK_SIZE/DRAM_CHANNEL_WIDTH
    DRAM_DBUS_RETURN_TIME = (BLOCK_SIZE / DRAM_CHANNEL_WIDTH) * (CPU_FREQ / DRAM_MTPS);

    printf("Off-chip DRAM Size: %u MB Channels: %u Width: %u-bit Data Rate: %u MT/s\n",
           DRAM_SIZE, DRAM_CHANNELS, 8 * DRAM_CHANNEL_WIDTH, DRAM_MTPS);

#ifdef ENCLAVE
    cout << "ENCLAVE Size: " << ENCLAVE_SIZE << " MB "
         << "ENCLAVE Pagess: " << ENCLAVE_PAGES << endl;
    cout << "NON-ENCLAVE Size: " << NON_ENCLAVE_SIZE << " MB "
         << "NON-ENCLAVE Pagess: " << NON_ENCLAVE_PAGES << endl;
#endif

#if ENCRYPTION_OPERATION
    cout << "ENCRYPTION_OPERATION: ON" << endl;
#endif


#if !(ENCRYPTION_OPERATION)
    cout << "ENCRYPTION_OPERATION: OFF" << endl;
#endif

    // end consequence of knobs

    // search through the argv for "-traces"
    int found_traces = 0;
    int count_traces = 0;
    cout << endl;

#ifdef ENCLAVE
    bool is_trace = true;
    int cpu_id = 0;
    char trace_name[1024];
#endif

    for (int i = 0; i < argc; i++)
    {
        if (found_traces)
        {

#ifdef BASELINE
            printf("CPU %d runs %s\n", count_traces, argv[i]);
#endif

// This code is added just to support enclave execution with enclave checkpoint and its life-time.
#ifdef ENCLAVE
            if (is_trace)
            {
                is_trace = false;
                sprintf(trace_name, "%s", argv[i]);
            }
            else
            {

                is_trace = true;

                int NUM_ENCLAVE = atoi(argv[i]);

                for (int j = 0; j < NUM_ENCLAVE; j++)
                {
                    uint32_t current_start_point = (uint32_t)atoi(argv[++i]);
                    start_point[cpu_id].push_back(current_start_point);
                }

                for (int j = 0; j < NUM_ENCLAVE; j++)
                {
                    uint32_t current_start_point = start_point[cpu_id][j];
                    uint32_t life_time = (uint32_t)atoi(argv[++i]);
                    lifetime[cpu_id].push_back(life_time);
                    end_point[cpu_id].push_back(current_start_point + life_time);
                }

                cout << "CPU " << count_traces - 1 << " runs " << trace_name << endl
                     << "Num of Enclaves: " << NUM_ENCLAVE << endl;

                for (int enclave_number = 0; enclave_number < NUM_ENCLAVE; enclave_number++)
                {
                    cout << "       " << "E-NO:" << enclave_number + 1 << " erange:(" << start_point[cpu_id][enclave_number] << "," << end_point[cpu_id][enclave_number] << "] lifetime: " << lifetime[cpu_id][enclave_number] << endl;
                }

                cout << endl;

                max_enclave[cpu_id] = NUM_ENCLAVE;
                current_enclave[cpu_id] = 0;
                enclave_mode[cpu_id] = 0;

                for (int j = 0; j < NUM_ENCLAVE; j++)
                {
                    start_point[cpu_id][j] *= 1000000;
                    end_point[cpu_id][j] *= 1000000;
                    lifetime[cpu_id][j] *= 1000000;
                }

                rob_head[cpu_id] = -1;
                rob_tail[cpu_id] = -1;
                flag[cpu_id] = false;
                is_deadlock_check_required[cpu_id] = true;

                cpu_id++;
                continue;
            }
#endif

            sprintf(ooo_cpu[count_traces].trace_string, "%s", argv[i]);

            char *full_name = ooo_cpu[count_traces].trace_string,
                 *last_dot = strrchr(ooo_cpu[count_traces].trace_string, '.');

            ifstream test_file(full_name);
            if (!test_file.good())
            {
                printf("TRACE FILE DOES NOT EXIST\n");
                assert(false);
            }

            if (full_name[last_dot - full_name + 1] == 'g') // gzip format
                sprintf(ooo_cpu[count_traces].gunzip_command, "gunzip -c %s", argv[i]);
            else if (full_name[last_dot - full_name + 1] == 'x') // xz
                sprintf(ooo_cpu[count_traces].gunzip_command, "xz -dc %s", argv[i]);
            else
            {
                cout << "ChampSim does not support traces other than gz or xz compression!" << endl;
                assert(0);
            }

            char *pch[100];
            int count_str = 0;
            pch[0] = strtok(argv[i], " /,.-");
            while (pch[count_str] != NULL)
            {
                //printf ("%s %d\n", pch[count_str], count_str);
                count_str++;
                pch[count_str] = strtok(NULL, " /,.-");
            }

            //printf("max count_str: %d\n", count_str);
            //printf("application: %s\n", pch[count_str-3]);

            int j = 0;
            while (pch[count_str - 3][j] != '\0')
            {
                seed_number += pch[count_str - 3][j];
                //printf("%c %d %d\n", pch[count_str-3][j], j, seed_number);
                j++;
            }

            ooo_cpu[count_traces].trace_file = popen(ooo_cpu[count_traces].gunzip_command, "r");
            if (ooo_cpu[count_traces].trace_file == NULL)
            {
                printf("\n*** Trace file not found: %s ***\n\n", argv[i]);
                assert(0);
            }

            count_traces++;
            if (count_traces > NUM_CPUS)
            {
                printf("\n*** Too many traces for the configured number of cores ***\n\n");
                assert(0);
            }
        }
        else if (strcmp(argv[i], "-traces") == 0)
        {
            found_traces = 1;
        }
    }

    if (count_traces != NUM_CPUS)
    {
        printf("\n*** Not enough traces for the configured number of cores ***\n\n");
        assert(0);
    }

    // end trace file setup
    // TODO: can we initialize these variables from the class constructor?
    srand(seed_number);
    champsim_seed = seed_number;

    for (int i = 0; i < NUM_CPUS; i++)
    {

        ooo_cpu[i].cpu = i;
        ooo_cpu[i].warmup_instructions = warmup_instructions;
        ooo_cpu[i].simulation_instructions = simulation_instructions;
        ooo_cpu[i].begin_sim_cycle = 0;
        ooo_cpu[i].begin_sim_instr = warmup_instructions;

        // ROB
        ooo_cpu[i].ROB.cpu = i;

        // BRANCH PREDICTOR
        ooo_cpu[i].initialize_branch_predictor();

        // TLBs
        ooo_cpu[i].ITLB.cpu = i;
        ooo_cpu[i].ITLB.cache_type = IS_ITLB;
        ooo_cpu[i].ITLB.MAX_READ = 2;
        ooo_cpu[i].ITLB.fill_level = FILL_L1;
        ooo_cpu[i].ITLB.extra_interface = &ooo_cpu[i].L1I;
        ooo_cpu[i].ITLB.lower_level = &ooo_cpu[i].STLB;

        ooo_cpu[i].DTLB.cpu = i;
        ooo_cpu[i].DTLB.cache_type = IS_DTLB;
        //ooo_cpu[i].DTLB.MAX_READ = (2 > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : 2;
        ooo_cpu[i].DTLB.MAX_READ = 2;
        ooo_cpu[i].DTLB.fill_level = FILL_L1;
        ooo_cpu[i].DTLB.extra_interface = &ooo_cpu[i].L1D;
        ooo_cpu[i].DTLB.lower_level = &ooo_cpu[i].STLB;

        ooo_cpu[i].STLB.cpu = i;
        ooo_cpu[i].STLB.cache_type = IS_STLB;
        ooo_cpu[i].STLB.MAX_READ = 1;
        ooo_cpu[i].STLB.fill_level = FILL_L2;
        ooo_cpu[i].STLB.upper_level_icache[i] = &ooo_cpu[i].ITLB;
        ooo_cpu[i].STLB.upper_level_dcache[i] = &ooo_cpu[i].DTLB;

        // PRIVATE CACHE
        ooo_cpu[i].L1I.cpu = i;
        ooo_cpu[i].L1I.cache_type = IS_L1I;
        //ooo_cpu[i].L1I.MAX_READ = (FETCH_WIDTH > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : FETCH_WIDTH;
        ooo_cpu[i].L1I.MAX_READ = 2;
        ooo_cpu[i].L1I.fill_level = FILL_L1;
        ooo_cpu[i].L1I.lower_level = &ooo_cpu[i].L2C;

        ooo_cpu[i].L1D.cpu = i;
        ooo_cpu[i].L1D.cache_type = IS_L1D;
        ooo_cpu[i].L1D.MAX_READ = (2 > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : 2;
        ooo_cpu[i].L1D.fill_level = FILL_L1;
        ooo_cpu[i].L1D.lower_level = &ooo_cpu[i].L2C;
        ooo_cpu[i].L1D.l1d_prefetcher_initialize();

        ooo_cpu[i].L2C.cpu = i;
        ooo_cpu[i].L2C.cache_type = IS_L2C;
        ooo_cpu[i].L2C.fill_level = FILL_L2;
        ooo_cpu[i].L2C.upper_level_icache[i] = &ooo_cpu[i].L1I;
        ooo_cpu[i].L2C.upper_level_dcache[i] = &ooo_cpu[i].L1D;
        ooo_cpu[i].L2C.lower_level = &uncore.LLC;
        ooo_cpu[i].L2C.l2c_prefetcher_initialize();

        // SHARED CACHE
        uncore.LLC.cache_type = IS_LLC;
        uncore.LLC.fill_level = FILL_LLC;
        uncore.LLC.MAX_READ = NUM_CPUS;
        uncore.LLC.upper_level_icache[i] = &ooo_cpu[i].L2C;
        uncore.LLC.upper_level_dcache[i] = &ooo_cpu[i].L2C;
        uncore.LLC.lower_level = &uncore.DRAM;

        // OFF-CHIP DRAM
        uncore.DRAM.fill_level = FILL_DRAM;
        uncore.DRAM.upper_level_icache[i] = &uncore.LLC;
        uncore.DRAM.upper_level_dcache[i] = &uncore.LLC;
        for (uint32_t i = 0; i < DRAM_CHANNELS; i++)
        {
            uncore.DRAM.RQ[i].is_RQ = 1;
            uncore.DRAM.WQ[i].is_WQ = 1;
        }

        warmup_complete[i] = 0;
        //all_warmup_complete = NUM_CPUS;
        simulation_complete[i] = 0;
        current_core_cycle[i] = 0;
        stall_cycle[i] = 0;

        previous_ppage = 0;
        num_adjacent_page = 0;
        num_cl[i] = 0;
        allocated_pages = 0;
        num_page[i] = 0;
        minor_fault[i] = 0;
        major_fault[i] = 0;

// @champsim-enclave
#ifdef ENCLAVE
        non_enclave_major_fault[i] = 0;
        non_enclave_minor_fault[i] = 0;
        enclave_major_fault[i] = 0;
        enclave_minor_fault[i] = 0;
#endif
    }

    uncore.LLC.llc_initialize_replacement();
    uncore.LLC.llc_prefetcher_initialize();

    // simulation entry point
    start_time = time(NULL);
    uint8_t run_simulation = 1;
    while (run_simulation)
    {

        uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
                 elapsed_minute = elapsed_second / 60,
                 elapsed_hour = elapsed_minute / 60;
        elapsed_minute -= elapsed_hour * 60;
        elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

        for (int i = 0; i < NUM_CPUS; i++)
        {
            // proceed one cycle
            current_core_cycle[i]++;

            // cout << "Trying to process instr_id: " << ooo_cpu[i].instr_unique_id << " fetch_stall: " << +ooo_cpu[i].fetch_stall;
            // cout << " stall_cycle: " << stall_cycle[i] << " current: " << current_core_cycle[i] << endl;

#ifdef ENCLAVE
        // starts enclave execution only after wamp-up completion
        if (all_warmup_complete > NUM_CPUS) {
            check_enclave_init(i);
            check_enclave_exit(i);
            init_exit_assert(i);
            enable_deadlock_check(i);
        }      
#endif

            // core might be stalled due to page fault or branch misprediction
            if (stall_cycle[i] <= current_core_cycle[i])
            {

                // retire
                if ((ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].executed == COMPLETED) && (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle <= current_core_cycle[i]))
                    ooo_cpu[i].retire_rob();

                // complete
                ooo_cpu[i].update_rob();

                // schedule
                uint32_t schedule_index = ooo_cpu[i].ROB.next_schedule;
                if ((ooo_cpu[i].ROB.entry[schedule_index].scheduled == 0) && (ooo_cpu[i].ROB.entry[schedule_index].event_cycle <= current_core_cycle[i]))
                    ooo_cpu[i].schedule_instruction();
                
                // execute
                ooo_cpu[i].execute_instruction();

                ooo_cpu[i].update_rob();

                // memory operation
                ooo_cpu[i].schedule_memory_instruction();
                ooo_cpu[i].execute_memory_instruction();

                ooo_cpu[i].update_rob();

                // decode
                if (ooo_cpu[i].DECODE_BUFFER.occupancy > 0)
                {
                    ooo_cpu[i].decode_and_dispatch();
                }

                // fetch
                ooo_cpu[i].fetch_instruction();

                // read from trace
                if ((ooo_cpu[i].IFETCH_BUFFER.occupancy < ooo_cpu[i].IFETCH_BUFFER.SIZE) && (ooo_cpu[i].fetch_stall == 0))
                {
                    // add the instruction to the ifectch-buffer.
                    ooo_cpu[i].read_from_trace();
                }
            }

            // heartbeat information
            if (show_heartbeat && (ooo_cpu[i].num_retired >= ooo_cpu[i].next_print_instruction))
            {
                float cumulative_ipc;
                if (warmup_complete[i])
                    cumulative_ipc = (1.0 * (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle);
                else
                    cumulative_ipc = (1.0 * ooo_cpu[i].num_retired) / current_core_cycle[i];
                float heartbeat_ipc = (1.0 * ooo_cpu[i].num_retired - ooo_cpu[i].last_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].last_sim_cycle);

                cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i].num_retired << " cycles: " << current_core_cycle[i];
                cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
                ooo_cpu[i].next_print_instruction += STAT_PRINTING_PERIOD;

                ooo_cpu[i].last_sim_instr = ooo_cpu[i].num_retired;
                ooo_cpu[i].last_sim_cycle = current_core_cycle[i];
            }

#ifdef ENCLAVE



            // Disable deadlock checking, when the enclave is in exiting phase.
            if (is_deadlock_check_required[i] && ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].ip && (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle + DEADLOCK_CYCLE) <= current_core_cycle[i])
            {
                cout << "Deadlock occurs! ON CPU :" << i << endl;
                cout << "ROB head: " << ooo_cpu[i].ROB.head << "ROB tail: " << ooo_cpu[i].ROB.tail << " occupancy: " << ooo_cpu[i].ROB.occupancy << endl;
                cout << "CPU: " << i << " Enclave Mode: " << enclave_mode[i] << " stall cycle value: " << stall_cycle[i] << " current core cycle value: " << current_core_cycle[i] << " Entry event cycle value:" << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle;
                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
                print_rob(i);
                print_cache_mshr(i);
                print_block_status_in_cache(i);
                print_deadlock(i);
            }

#endif

#ifdef BASELINE
            if (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].ip && (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle + DEADLOCK_CYCLE) <= current_core_cycle[i])
                print_deadlock(i);
#endif

            // check for warmup
            // warmup complete
            if ((warmup_complete[i] == 0) && (ooo_cpu[i].num_retired > warmup_instructions))
            {
                warmup_complete[i] = 1;
                all_warmup_complete++;
            }
            if (all_warmup_complete == NUM_CPUS)
            { // this part is called only once when all cores are warmed up
                all_warmup_complete++;
                finish_warmup();
            }

            /*
            if (all_warmup_complete == 0) { 
                all_warmup_complete = 1;
                finish_warmup();
            }
            if (ooo_cpu[1].num_retired > 0)
                warmup_complete[1] = 1;
            */

            // simulation complete
            if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0) && (ooo_cpu[i].num_retired >= (ooo_cpu[i].begin_sim_instr + ooo_cpu[i].simulation_instructions)))
            {
                simulation_complete[i] = 1;
                ooo_cpu[i].finish_sim_instr = ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr;
                ooo_cpu[i].finish_sim_cycle = current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle;
                cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i].finish_sim_instr << " cycles: " << ooo_cpu[i].finish_sim_cycle;
                cout << " cumulative IPC: " << ((float)ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle);

                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

                record_roi_stats(i, &ooo_cpu[i].L1D);
                record_roi_stats(i, &ooo_cpu[i].L1I);
                record_roi_stats(i, &ooo_cpu[i].L2C);
                record_roi_stats(i, &uncore.LLC);

                all_simulation_complete++;
            }

            if (all_simulation_complete == NUM_CPUS)
                run_simulation = 0;
        }

        // TODO: should it be backward?
        uncore.DRAM.operate();
        uncore.LLC.operate();
    }
    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    cout << endl
         << "ChampSim completed all CPUs" << endl;
    if (NUM_CPUS > 1)
    {
        cout << endl
             << "Total Simulation Statistics (not including warmup)" << endl;
        for (uint32_t i = 0; i < NUM_CPUS; i++)
        {
            cout << endl
                 << "CPU " << i << " cumulative IPC: " << (float)(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle);
            cout << " instructions: " << ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr << " cycles: " << current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle << endl;
#ifndef CRC2_COMPILE
            print_sim_stats(i, &ooo_cpu[i].L1D);
            print_sim_stats(i, &ooo_cpu[i].L1I);
            print_sim_stats(i, &ooo_cpu[i].L2C);
            ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
            ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
#endif
            print_sim_stats(i, &uncore.LLC);
        }
        uncore.LLC.llc_prefetcher_final_stats();
    }

    cout << endl
         << "Region of Interest Statistics" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
        cout << endl
             << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle);
        cout << " instructions: " << ooo_cpu[i].finish_sim_instr << " cycles: " << ooo_cpu[i].finish_sim_cycle << endl;
#ifndef CRC2_COMPILE
        print_roi_stats(i, &ooo_cpu[i].L1D);
        print_roi_stats(i, &ooo_cpu[i].L1I);
        print_roi_stats(i, &ooo_cpu[i].L2C);
#endif
        print_roi_stats(i, &uncore.LLC);
        cout << "Major fault: " << major_fault[i] << " Minor fault: " << minor_fault[i] << endl;

#ifdef ENCLAVE
        cout << "Major enclave fault: " << enclave_major_fault[i] << " Enclave Minor fault: " << enclave_minor_fault[i] << endl;
        cout << "Major non enclave fault: " << non_enclave_major_fault[i] << " Non Enclave Minor fault: " << non_enclave_minor_fault[i] << endl;
#endif
    }

    for (uint32_t i = 0; i < NUM_CPUS; i++)
    {
        ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
        ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
    }

    uncore.LLC.llc_prefetcher_final_stats();

#ifndef CRC2_COMPILE
    uncore.LLC.llc_replacement_final_stats();
    print_dram_stats();
    print_branch_stats();
#endif

    return 0;
}
