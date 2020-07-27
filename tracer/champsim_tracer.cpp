#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <string>

#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

using namespace std;

typedef struct trace_instr_format {

    unsigned long long int ip;  // instruction pointer (program counter) value
    uint8_t trusted_instruction_id; // is this trusted code intructions

    unsigned char is_branch;    // is this branch
    unsigned char branch_taken; // if so, is this taken

    unsigned char destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    unsigned char source_registers[NUM_INSTR_SOURCES];           // input registers

    unsigned long long int destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    unsigned long long int source_memory[NUM_INSTR_SOURCES];           // input memory

} trace_instr_format_t;

UINT64 instrCount = 0;

FILE* out;
ofstream outFile;

bool output_file_closed = false;
bool tracing_on = false;
bool enclave_mode = false;

UINT64 total_instructions_executed=0;
UINT64 trusted_instructions_executed=0;
UINT64 untrusted_instructions_executed=0;

UINT64 trusted_function_called=0;
UINT64 untrusted_function_called=0;

UINT64 first_trusted_address=0;
UINT64 last_trusted_address=0; 

trace_instr_format_t curr_instr;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool", "o", "champsim.trace", "specify file name for Champsim tracer output");

// 2,000,000
KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "2000000", "How many instructions to skip before tracing begins");

// 5,000,000
KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "50000000", "How many instructions to trace");

INT32 Usage()
{
    cerr << "This tool creates a register and memory access trace" << endl 
        << "Specify the output trace file with -o" << endl 
        << "Specify the number of instructions to skip before tracing with -s" << endl
        << "Specify the number of instructions to trace with -t" << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;

}

void EndInstruction()
{
    if(instrCount > KnobSkipInstructions.Value()) {
        tracing_on = true;
        if(instrCount <= (KnobTraceInstructions.Value()+KnobSkipInstructions.Value())) {
            // write to the trace file
            fwrite(&curr_instr, sizeof(trace_instr_format_t), 1, out);
            ;
        }
        else {
            tracing_on = false;
            // close down the file, we're done tracing
            if(!output_file_closed) {
                fclose(out);
                output_file_closed = true;
            }
            exit(0);
        }
    }
}

void BranchOrNot(UINT32 taken)
{
    curr_instr.is_branch = 1;
    if(taken != 0)  curr_instr.branch_taken = 1;
}

void RegRead(UINT32 i, UINT32 index)
{
    if(!tracing_on) return;

    REG r = (REG)i;

    int already_found = 0;
    for(int i=0; i<NUM_INSTR_SOURCES; i++)
    {
        if(curr_instr.source_registers[i] == ((unsigned char)r))
        {
            already_found = 1;
            break;
        }
    }
    if(already_found == 0)
    {
        for(int i=0; i<NUM_INSTR_SOURCES; i++)
        {
            if(curr_instr.source_registers[i] == 0)
            {
                curr_instr.source_registers[i] = (unsigned char)r;
                break;
            }
        }
    }
}

void RegWrite(REG i, UINT32 index)
{
    if(!tracing_on) return;

    REG r = (REG)i;

    int already_found = 0;
    for(int i=0; i<NUM_INSTR_DESTINATIONS; i++)
    {
        if(curr_instr.destination_registers[i] == ((unsigned char)r))
        {
            already_found = 1;
            break;
        }
    }
    if(already_found == 0)
    {
        for(int i=0; i<NUM_INSTR_DESTINATIONS; i++)
        {
            if(curr_instr.destination_registers[i] == 0)
            {
                curr_instr.destination_registers[i] = (unsigned char)r;
                break;
            }
        }
    }
}

void MemoryRead(VOID* addr, UINT32 index, UINT32 read_size)
{
    if(!tracing_on) return;

    int already_found = 0;
    for(int i=0; i<NUM_INSTR_SOURCES; i++)
    {
        if(curr_instr.source_memory[i] == ((unsigned long long int)addr))
        {
            already_found = 1;
            break;
        }
    }
    if(already_found == 0)
    {
        for(int i=0; i<NUM_INSTR_SOURCES; i++)
        {
            if(curr_instr.source_memory[i] == 0)
            {
                curr_instr.source_memory[i] = (unsigned long long int)addr;
                break;
            }
        }
    }
}

void MemoryWrite(VOID* addr, UINT32 index)
{
    if(!tracing_on) return;

    int already_found = 0;
    for(int i=0; i<NUM_INSTR_DESTINATIONS; i++)
    {
        if(curr_instr.destination_memory[i] == ((unsigned long long int)addr))
        {
            already_found = 1;
            break;
        }
    }
    if(already_found == 0)
    {
        for(int i=0; i<NUM_INSTR_DESTINATIONS; i++)
        {
            if(curr_instr.destination_memory[i] == 0)
            {
                curr_instr.destination_memory[i] = (unsigned long long int)addr;
                break;
            }
        }
    }
}

void BeginInstruction(VOID *ip, ADDRINT InsAdd)
{

    instrCount++;

    if(instrCount > KnobSkipInstructions.Value()) {
        tracing_on = true;

        if(instrCount > (KnobTraceInstructions.Value()+KnobSkipInstructions.Value())) {

            outFile << "Total Instructions Executed: "<< total_instructions_executed << " (" << float(100.0*total_instructions_executed/total_instructions_executed) <<"%)" << endl;
            outFile << "Trusted Instructions Executed: "<< trusted_instructions_executed << " (" << float(100.0*trusted_instructions_executed/total_instructions_executed) <<"%)" << endl;
            outFile << "Untrusted Instructions Executed: "<< untrusted_instructions_executed << " ("<<float(100.0*untrusted_instructions_executed/total_instructions_executed) <<"%)" << endl;   
            // outFile << "Trusted function called: " << trusted_function_called << " Untrusted function called: " << untrusted_function_called << endl;

        }
        
    }

    if(!tracing_on) 
        return;

    if (first_trusted_address == InsAdd) {
        trusted_function_called++;
        enclave_mode = true;
    } 
        
    else if (last_trusted_address == InsAdd) {
        untrusted_function_called++;
        enclave_mode = false;
    } 
              
    if (enclave_mode) {
        trusted_instructions_executed++;
        if (first_trusted_address == InsAdd)
            curr_instr.trusted_instruction_id = 100;
        else 
            curr_instr.trusted_instruction_id = 1;
    }
    else {
        untrusted_instructions_executed++;
        if (last_trusted_address == InsAdd)
            curr_instr.trusted_instruction_id = 200;
        else 
            curr_instr.trusted_instruction_id = 0;

    }
    total_instructions_executed++;


    curr_instr.ip = (unsigned long long int)ip;
    curr_instr.is_branch = 0;
    curr_instr.branch_taken = 0;

    for(int i=0; i<NUM_INSTR_DESTINATIONS; i++) {
        curr_instr.destination_registers[i] = 0;
        curr_instr.destination_memory[i] = 0;
    }

    for(int i=0; i<NUM_INSTR_SOURCES; i++) {
        curr_instr.source_registers[i] = 0;
        curr_instr.source_memory[i] = 0;
    }
    
}

bool search_substring(string sub_string, string complete_string) {
   int pos = 0;
   int index;
   while((index = (int)complete_string.find(sub_string, pos)) != (int)string::npos) {
        return true;
   }
   return false;
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    string rtnName;
    RTN rtn = INS_Rtn(ins);
    if (RTN_Valid (rtn)) {
        IMG img = SEC_Img(RTN_Sec(rtn));
        if (IMG_Valid(img)) {
            if (search_substring("enable_trusted_code_execution",  RTN_Name(rtn))) {
                first_trusted_address = INS_Address(ins);
            }
            if (search_substring("disable_trusted_code_execution",  RTN_Name(rtn))) {
                last_trusted_address = INS_Address(ins);
            }
        }    
    }
            
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BeginInstruction, IARG_INST_PTR, IARG_ADDRINT, INS_Address(ins), IARG_END);

    if(INS_IsBranch(ins)) 
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

    // initialiezed the registers values
    UINT32 readRegCount = INS_MaxNumRRegs(ins);
    
    for(UINT32 i=0; i<readRegCount; i++) {
        UINT32 regNum = INS_RegR(ins, i);

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegRead,
                IARG_UINT32, regNum, IARG_UINT32, i,
                IARG_END);
    }                                                                                                                                                                                               
    UINT32 writeRegCount = INS_MaxNumWRegs(ins);
    for(UINT32 i=0; i<writeRegCount; i++)  {
        UINT32 regNum = INS_RegW(ins, i);

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegWrite,
                IARG_UINT32, regNum, IARG_UINT32, i,
                IARG_END);
    }

    // initiliazed the read/write memory variables
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
        if (INS_MemoryOperandIsRead(ins, memOp))  {
            UINT32 read_size = INS_MemoryReadSize(ins);

            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryRead,
                    IARG_MEMORYOP_EA, memOp, IARG_UINT32, memOp, IARG_UINT32, read_size,
                    IARG_END);
        }
        if (INS_MemoryOperandIsWritten(ins, memOp)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryWrite,
                    IARG_MEMORYOP_EA, memOp, IARG_UINT32, memOp,
                    IARG_END);
        }
    } 

    // write the traces in a file
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EndInstruction, IARG_END);

}


VOID Fini(INT32 code, VOID *v)
{ 
    outFile << "Total Instructions Executed: "<< total_instructions_executed << " (" << float(100.0*total_instructions_executed/total_instructions_executed) <<"%)" << endl;
    outFile << "Trusted Instructions Executed: "<< trusted_instructions_executed << " (" << float(100.0*trusted_instructions_executed/total_instructions_executed) <<"%)" << endl;
    outFile << "Untrusted Instructions Executed: "<< untrusted_instructions_executed << " ("<<float(100.0*untrusted_instructions_executed/total_instructions_executed) <<"%)" << endl;      
    // outFile << "Trusted function called: " << trusted_function_called << " Untrusted function called: " << untrusted_function_called << endl;
    
    // close the file if it hasn't already been closed
    if(!output_file_closed) 
    {
        fclose(out);
        output_file_closed = true;
    }
}

string executable_filename(string filepath) {
    string filename;
    reverse(filepath.begin(), filepath.end());
    for (long unsigned int i=0;i<filepath.length();i++) {
        if (filepath[i]!='/') {
            filename.push_back(filepath[i]);
        }
        else {
            reverse(filename.begin(), filename.end());
            return filename;
        }
    } 
    reverse(filename.begin(), filename.end());
    return filepath;
}

int main(int argc, char *argv[])
{
        
    PIN_InitSymbols();

    if( PIN_Init(argc,argv))
        return Usage();

    string filename =  executable_filename(argv[argc-1]);

    // open stream file for output
    const char* streamfile = (filename+".out").c_str();
    outFile.open(streamfile);

    // open trace file 
    const char* tracefile = (filename+".trace").c_str();;
    
    remove(tracefile);
    out = fopen(tracefile, "ab");

    if (!out) 
    {
        cout << "Couldn't open output trace file. Exiting." << endl;
        exit(1);
    }

    // Register function to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);    

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
