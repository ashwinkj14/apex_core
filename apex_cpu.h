/*
 * apex_cpu.h
 * Contains APEX cpu pipeline declarations
 *
 * Author:
 * Copyright (c) 2022, Ashwin Kandheri Jayaraman (akandhe1@binghamton.edu), Srinidhi Sasidharan (ssasidh1@binghamton.edu)
 * State University of New York at Binghamton
 */
#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_

#include "apex_macros.h"

/* Format of an APEX instruction  */
typedef struct APEX_Instruction
{
    char opcode_str[128];
    int opcode;
    int rd;
    int rs1;
    int rs2;
    int rs3;
    int imm;
} APEX_Instruction;

typedef struct  Forwarding_Bus
{
    int tag;
    int busy;
    int data;
    int cc;
}FB;

/* Format of Physical Register*/
typedef struct Physical_Reg_format
{
    int phy_Reg;
    int reg_invalid;
    int cc_flag;
    int free;
}PRF;

typedef struct Physical_Reg
{
    PRF PR_File[15];
    int head;
    int tail;
}PR;

/* Format of Rename Table*/
typedef struct Rename_Table
{
    int reg[8];
}RT;

typedef struct IQ_Entry
{
    int allocated_bit;
    int fu_type; //INT_FU (1), LOGICAL_FU (2), MUL_FU (3) 
    int literal;
    int src1_valid_bit;
    int src1_tag;
    int src1_value;
    int src2_valid_bit;
    int src2_tag;
    int src2_value;
    int dest;
}IQ_Entry;

typedef struct LSQ_Entry
{
    int established_bit;
    int lost;
    int mem_valid_bit;
    int mem_address;
    int dest_reg_address;
    int src_valid_bit;
    int src_tag;
    int src_value;
    int rob_index;
}LSQ_Entry;

typedef struct ROB_Entry
{
    int established_bit;
    int instruction_type;
    int pc_value;
    int dest_phy_reg;
    int prev_phy_reg;
    int dest_arch_reg;
    int lsq_index;
    int mem_error_code;
}ROB_Entry;

typedef struct BTB_Entry
{
    int pc_value;
    int target_address;
}BTB_Entry;

typedef struct IQ
{
    IQ_Entry *entry[IQ_SIZE];
    int head;
    int tail;
}IQ;

typedef struct LSQ
{
    LSQ_Entry *entry[LSQ_SIZE];
    int head;
    int tail;
}LSQ;

typedef struct ROB
{
    ROB_Entry *entry[ROB_SIZE];
    int head;
    int tail;
}ROB;

typedef struct BTB
{
    BTB_Entry *entry[BTB_SIZE];
    int tail;
}BTB;

/* Model of CPU stage latch */
typedef struct CPU_Stage
{
    int pc;
    char opcode_str[128];
    int opcode;
    int rs1;
    int rs2;
    int rs3;
    int rd;
    int ps1;
    int ps2;
    int ps3;
    int pd;
    int imm;
    int rs1_value;
    int rs2_value;
    int rs3_value;
    int result_buffer;
    int prev_phy_reg;
    int dest_arch_reg;
    int memory_address;
    int has_insn;
    int stall;
} CPU_Stage;

/* Model of APEX CPU */
typedef struct APEX_CPU
{
    int pc;                        /* Current program counter */
    int clock;                     /* Clock cycles elapsed */
    int insn_completed;            /* Instructions retired */
    int regs[REG_FILE_SIZE];       /* Integer register file */
    int code_memory_size;          /* Number of instruction in the input file */
    APEX_Instruction *code_memory; /* Code Memory */
    int data_memory[DATA_MEMORY_SIZE]; /* Data Memory */
    int single_step;               /* Wait for user input after every cycle */
    int zero_flag;                 /* {TRUE, FALSE} Used by BZ and BNZ to branch */
    int fetch_from_next_cycle;
    int prev_cc;
    int branch_reg;
    int LSI;
    int conditional_pc;
    int cmp_flag;
    int bTaken;
    

    /* Pipeline stages */
    CPU_Stage fetch;
    CPU_Stage DR1;
    CPU_Stage DR2;
    CPU_Stage I_Queue;
    //CPU_Stage execute;
    CPU_Stage INT_FU;
    CPU_Stage LOP_FU;
    CPU_Stage MUL1_FU;
    CPU_Stage MUL2_FU;
    CPU_Stage MUL3_FU;
    CPU_Stage MUL4_FU;
    CPU_Stage memory;
    CPU_Stage writeback;
    RT rt;
    PR pr;
    FB fBus[2];

    IQ *iq;
    LSQ *lsq;
    ROB *rob;
    BTB *btb;
} APEX_CPU;

//IQ
void addIQEntry(
    int allocated_bit,
    int fu_type,
    int literal,
    int src1_valid_bit,
    int src1_tag,
    int src1_value,
    int src2_valid_bit,
    int src2_tag,
    int src2_value,
    int dest,
    APEX_CPU *cpu
    );

IQ_Entry* getIQEntry(APEX_CPU *cpu);
int getIQEntry_Index(APEX_CPU *cpu, int index);
int isIQFull(APEX_CPU *cpu);
int isIQEmpty(APEX_CPU *cpu);
int isIQEntryReady(IQ_Entry *entry);
void shiftIQElements(APEX_CPU *cpu, int pos);
void updateIQEntry(APEX_CPU *cpu, int src_tag, int src_value);
static void APEX_IQ(APEX_CPU *cpu);

//LSQ
void addLSQEntry(
    int established_bit,
    int lost,//1 for load, 0 for store
    int mem_valid_bit,
    int mem_address,
    int dest_reg_address,
    int src_valid_bit,
    int src_tag,
    int src_value,
    int rob_index,
    APEX_CPU *cpu
);
LSQ_Entry* getLSQEntry(APEX_CPU *cpu);
int isLSQFull(APEX_CPU *cpu);
int isLSQEmpty(APEX_CPU *cpu);
int isLSQEntryReady(LSQ_Entry *entry);
LSQ_Entry* getLSQEntry(APEX_CPU *cpu);
void updateLSQEntry(APEX_CPU *cpu, int src_tag, int src_value);


//ROB
void addROBEntry(
    int established_bit,
    int instruction_type,
    int pc_value,
    int dest_phy_reg,
    int prev_phy_reg,
    int dest_arch_reg,
    int lsq_index,
    int mem_error_code,
    APEX_CPU *cpu
);
ROB_Entry *getROBHead(APEX_CPU *cpu);
void removeROBHead(APEX_CPU *cpu);
int isROBFull(APEX_CPU *cpu);
int isROBEmpty(APEX_CPU *cpu);
int isROBEntryReady(ROB_Entry *entry);
void updateROBEntry(APEX_CPU *cpu, int src_tag, int src_value);

//BTB
void addBTBEntry(int pc_value, int target_address, APEX_CPU *cpu);
BTB_Entry* getBTBEntry(int pc_value, APEX_CPU *cpu);

APEX_Instruction *create_code_memory(const char *filename, int *size);
APEX_CPU *APEX_cpu_init(const char *filename);
void APEX_cpu_run(APEX_CPU *cpu);
void APEX_cpu_stop(APEX_CPU *cpu);
#endif
