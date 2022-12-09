/*
 * apex_cpu.c
 * Contains APEX cpu pipeline implementation
 *
 * Author:
 * Copyright (c) 2022, Ashwin Kandheri Jayaraman (akandhe1@binghamton.edu), Srinidhi Sasidharan (ssasidh1@binghamton.edu)
 * State University of New York at Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apex_cpu.h"
#include "apex_macros.h"

/* Converts the PC(4000 series) into array index for code memory
 *
 * Note: You are not supposed to edit this function
 */
static int
get_code_memory_index_from_pc(const int pc)
{
    return (pc - 4000) / 4;
}

static void
print_instruction(const CPU_Stage *stage)
{
    switch (stage->opcode)
    {
    case OPCODE_ADD:
    case OPCODE_SUB:
    case OPCODE_MUL:
    case OPCODE_DIV:
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    {
        printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rd, stage->rs1,
               stage->rs2);
        break;
    }

    case OPCODE_MOVC:
    {
        printf("%s,R%d,#%d ", stage->opcode_str, stage->rd, stage->imm);
        break;
    }

    case OPCODE_LOAD:
    {
        printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
               stage->imm);
        break;
    }

    case OPCODE_STORE:
    {
        printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rs1, stage->rs2,
               stage->imm);
        break;
    }

    case OPCODE_BZ:
    case OPCODE_BNZ:
    {
        printf("%s,#%d ", stage->opcode_str, stage->imm);
        break;
    }

    case OPCODE_HALT:
    {
        printf("%s", stage->opcode_str);
        break;
    }
    }
}

/* Debug function which prints the CPU stage content
 *
 * Note: You can edit this function to print in more detail
 */
static void
print_stage_content(const char *name, const CPU_Stage *stage)
{
    printf("%-15s: pc(%d) ", name, stage->pc);
    print_instruction(stage);
    printf("\n");
}

/* Debug function which prints the register file
 *
 * Note: You are not supposed to edit this function
 */
static void
print_reg_file(const APEX_CPU *cpu)
{
    int i;

    printf("----------\n%s\n----------\n", "Registers:");

    for (int i = 0; i < REG_FILE_SIZE / 2; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");

    for (i = (REG_FILE_SIZE / 2); i < REG_FILE_SIZE; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");
}

/*
 * Fetch Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_fetch(APEX_CPU *cpu)
{
    APEX_Instruction *current_ins;

    if (cpu->fetch.has_insn)
    {
        /* This fetches new branch target instruction from next cycle */
        if (cpu->fetch_from_next_cycle == TRUE)
        {
            cpu->fetch_from_next_cycle = FALSE;

            /* Skip this cycle*/
            return;
        }

        /* Store current PC in fetch latch */
        cpu->fetch.pc = cpu->pc;

        /* Index into code memory using this pc and copy all instruction fields
         * into fetch latch  */
        current_ins = &cpu->code_memory[get_code_memory_index_from_pc(cpu->pc)];
        strcpy(cpu->fetch.opcode_str, current_ins->opcode_str);
        cpu->fetch.opcode = current_ins->opcode;
        cpu->fetch.rd = current_ins->rd;
        cpu->fetch.rs1 = current_ins->rs1;
        cpu->fetch.rs2 = current_ins->rs2;
        cpu->fetch.imm = current_ins->imm;

        /* Update PC for next instruction */
        cpu->pc += 4;

        /* Copy data from fetch latch to decode latch*/
        cpu->decode = cpu->fetch;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Fetch", &cpu->fetch);
        }

        /* Stop fetching new instructions if HALT is fetched */
        if (cpu->fetch.opcode == OPCODE_HALT)
        {
            cpu->fetch.has_insn = FALSE;
        }
    }
}

/*
 * Decode Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_decode(APEX_CPU *cpu)
{
    if (cpu->decode.has_insn)
    {
        /* Read operands from register file based on the instruction type */
        switch (cpu->decode.opcode)
        {
        case OPCODE_ADD:
        {
            cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
            cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
            break;
        }

        case OPCODE_LOAD:
        {
            cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
            break;
        }

        case OPCODE_MOVC:
        {
            /* MOVC doesn't have register operands */
            break;
        }
        }

        /* Copy data from decode latch to execute latch*/
        cpu->execute = cpu->decode;
        cpu->decode.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Decode/RF", &cpu->decode);
        }
    }
}

/*
 * Execute Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_execute(APEX_CPU *cpu)
{
    if (cpu->execute.has_insn)
    {
        /* Execute logic based on instruction type */
        switch (cpu->execute.opcode)
        {
        case OPCODE_ADD:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.rs2_value;

            /* Set the zero flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
            }
            break;
        }

        case OPCODE_LOAD:
        {
            cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
            break;
        }

        case OPCODE_BZ:
        {
            if (cpu->zero_flag == TRUE)
            {
                /* Calculate new PC, and send it to fetch unit */
                cpu->pc = cpu->execute.pc + cpu->execute.imm;

                /* Since we are using reverse callbacks for pipeline stages,
                 * this will prevent the new instruction from being fetched in the current cycle*/
                cpu->fetch_from_next_cycle = TRUE;

                /* Flush previous stages */
                cpu->decode.has_insn = FALSE;

                /* Make sure fetch stage is enabled to start fetching from new PC */
                cpu->fetch.has_insn = TRUE;
            }
            break;
        }

        case OPCODE_BNZ:
        {
            if (cpu->zero_flag == FALSE)
            {
                /* Calculate new PC, and send it to fetch unit */
                cpu->pc = cpu->execute.pc + cpu->execute.imm;

                /* Since we are using reverse callbacks for pipeline stages,
                 * this will prevent the new instruction from being fetched in the current cycle*/
                cpu->fetch_from_next_cycle = TRUE;

                /* Flush previous stages */
                cpu->decode.has_insn = FALSE;

                /* Make sure fetch stage is enabled to start fetching from new PC */
                cpu->fetch.has_insn = TRUE;
            }
            break;
        }

        case OPCODE_MOVC:
        {
            cpu->execute.result_buffer = cpu->execute.imm;

            /* Set the zero flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
            }
            break;
        }
        }

        /* Copy data from execute latch to memory latch*/
        cpu->memory = cpu->execute;
        cpu->execute.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Execute", &cpu->execute);
        }
    }
}

/*
 * Memory Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_memory(APEX_CPU *cpu)
{
    if (cpu->memory.has_insn)
    {
        switch (cpu->memory.opcode)
        {
        case OPCODE_ADD:
        {
            /* No work for ADD */
            break;
        }

        case OPCODE_LOAD:
        {
            /* Read from data memory */
            cpu->memory.result_buffer = cpu->data_memory[cpu->memory.memory_address];
            break;
        }
        }

        /* Copy data from memory latch to writeback latch*/
        cpu->writeback = cpu->memory;
        cpu->memory.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Memory", &cpu->memory);
        }
    }
}

/*
 * Writeback Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static int
APEX_writeback(APEX_CPU *cpu)
{
    if (cpu->writeback.has_insn)
    {
        /* Write result to register file based on instruction type */
        switch (cpu->writeback.opcode)
        {
        case OPCODE_ADD:
        {
            cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
            break;
        }

        case OPCODE_LOAD:
        {
            cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
            break;
        }

        case OPCODE_MOVC:
        {
            cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
            break;
        }
        }

        cpu->insn_completed++;
        cpu->writeback.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Writeback", &cpu->writeback);
        }

        if (cpu->writeback.opcode == OPCODE_HALT)
        {
            /* Stop the APEX simulator */
            return TRUE;
        }
    }

    /* Default */
    return 0;
}

/*
 * This function creates and initializes APEX cpu.
 *
 * Note: You are free to edit this function according to your implementation
 */
APEX_CPU *
APEX_cpu_init(const char *filename)
{
    int i;
    APEX_CPU *cpu;

    if (!filename)
    {
        return NULL;
    }

    cpu = calloc(1, sizeof(APEX_CPU));

    if (!cpu)
    {
        return NULL;
    }

    /* Initialize PC, Registers and all pipeline stages */
    cpu->pc = 4000;
    memset(cpu->regs, 0, sizeof(int) * REG_FILE_SIZE);
    memset(cpu->data_memory, 0, sizeof(int) * DATA_MEMORY_SIZE);
    cpu->single_step = ENABLE_SINGLE_STEP;
    
    cpu->iq = calloc(1, sizeof(IQ));
    cpu->iq->tail = -1;
    
    cpu->lsq = calloc(1, sizeof(LSQ));
    cpu->lsq->head = -1;
    cpu->lsq->tail = -1;
    
    cpu->rob = calloc(1, sizeof(ROB));
    cpu->rob->head = -1;
    cpu->rob->tail = -1;

    cpu->btb = calloc(1, sizeof(BTB));
    cpu->btb->tail = -1;

    /* Parse input file and create code memory */
    cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);
    if (!cpu->code_memory)
    {
        free(cpu);
        return NULL;
    }

    if (ENABLE_DEBUG_MESSAGES)
    {
        fprintf(stderr,
                "APEX_CPU: Initialized APEX CPU, loaded %d instructions\n",
                cpu->code_memory_size);
        fprintf(stderr, "APEX_CPU: PC initialized to %d\n", cpu->pc);
        fprintf(stderr, "APEX_CPU: Printing Code Memory\n");
        printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode_str", "rd", "rs1", "rs2",
               "imm");

        for (i = 0; i < cpu->code_memory_size; ++i)
        {
            printf("%-9s %-9d %-9d %-9d %-9d\n", cpu->code_memory[i].opcode_str,
                   cpu->code_memory[i].rd, cpu->code_memory[i].rs1,
                   cpu->code_memory[i].rs2, cpu->code_memory[i].imm);
        }
    }

    /* To start fetch stage */
    cpu->fetch.has_insn = TRUE;
    return cpu;
}

/*----------------------------------Issue Queue utilities start-----------------------------------*/

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
    APEX_CPU *cpu)
{

    if (isIQFull(cpu))
    {
        return;
    }
    else
    {
        IQ_Entry *entry = malloc(sizeof(IQ_Entry));
        entry->allocated_bit = allocated_bit;
        entry->fu_type = fu_type;
        entry->literal = literal;
        entry->src1_valid_bit = src1_valid_bit;
        entry->src1_tag = src1_tag;
        entry->src1_value = src1_value;
        entry->src2_valid_bit = src2_valid_bit;
        entry->src2_tag = src2_tag;
        entry->src2_value = src2_value;
        entry->dest = dest;

        int tail = ++cpu->iq->tail;
        cpu->iq->entry[tail] = entry;
    }
}

int isIQFull(APEX_CPU *cpu)
{
    int tail = cpu->iq->tail;
    if (tail == IQ_SIZE - 1)
    {
        return 1;
    }
    return 0;
}

int isIQEmpty(APEX_CPU *cpu)
{
    int tail = cpu->iq->tail;
    if (tail == -1)
    {
        return 1;
    }
    return 0;
}

IQ_Entry *getIQEntry(APEX_CPU *cpu)
{
    if (isIQEmpty(cpu))
    {
        printf("IQ is empty\n");
        return NULL;
    }
    else
    {
        int tail = cpu->iq->tail;
        int i = 0;
        while (i <= tail)
        {
            IQ_Entry *entry = cpu->iq->entry[i];
            if (isIQEntryReady(entry))
            {
                shiftIQElements(cpu, i);
                return entry;
            }
            i++;
        }
        return NULL;
    }
}

int isIQEntryReady(IQ_Entry *entry)
{
    return entry->allocated_bit && entry->src1_valid_bit && entry->src2_valid_bit;
}

void shiftIQElements(APEX_CPU *cpu, int pos)
{
    int tail = cpu->iq->tail;
    while (pos < tail)
    {
        cpu->iq->entry[pos] = cpu->iq->entry[pos + 1];
        pos++;
    }
    cpu->iq->tail--;
}

void updateIQEntry(APEX_CPU *cpu, int src_tag, int src_value)
{
    int tail = cpu->iq->tail;
    int i = 0;
    while (i <= tail)
    {
        IQ_Entry *entry = cpu->iq->entry[i];
        if (entry->src1_tag == src_tag)
        {
            entry->src1_valid_bit = 1;
            cpu->iq->entry[i] = entry;
        }
        if (entry->src2_tag == src_tag)
        {
            entry->src2_valid_bit = 1;
            cpu->iq->entry[i] = entry;
        }
        i++;
    }
}

/*----------------------------------Issue Queue utilities end-----------------------------------*/

/*----------------------------------Load Store Queue utilities start-----------------------------------*/

void addLSQEntry(
    int established_bit,
    int lost,
    int mem_valid_bit,
    int mem_address,
    int dest_reg_address,
    int src_valid_bit,
    int src_tag,
    int src_value,
    int rob_index,
    APEX_CPU *cpu)
{
    if (isLSQFull(cpu))
    {
        return;
    }
    LSQ_Entry *entry = malloc(sizeof(LSQ_Entry));
    entry->established_bit = established_bit;
    entry->lost = lost;
    entry->mem_valid_bit = mem_valid_bit;
    entry->mem_address = mem_address;
    entry->dest_reg_address = dest_reg_address;
    entry->src_valid_bit = src_valid_bit;
    entry->src_tag = src_tag;
    entry->src_value = src_value;
    entry->rob_index = rob_index;

    int tail = cpu->lsq->tail;
    tail = (tail + 1) % LSQ_SIZE;
    cpu->lsq->entry[tail] = entry;
    cpu->lsq->tail = tail;
}

LSQ_Entry* getLSQEntry(APEX_CPU *cpu)
{
    if (isLSQEmpty(cpu))
    {
        return NULL;
    }
    int head = cpu->lsq->head;
    int tail = cpu->lsq->tail;
    LSQ_Entry *entry = cpu->lsq->entry[head];
    if(head == tail)
    {
        cpu->lsq->head = -1;
        cpu->lsq->tail = -1;
        return entry;
    }
    head = (head + 1) % LSQ_SIZE;
    cpu->lsq->head = head;
    return entry;
}

int isLSQFull(APEX_CPU *cpu)
{
    int head = cpu->lsq->head;
    int tail = cpu->lsq->tail;
    if ((head == tail + 1) || (head == 0 && tail == LSQ_SIZE - 1))
    {
        return 1;
    }
    return 0;
}

int isLSQEmpty(APEX_CPU *cpu)
{
    int head = cpu->lsq->head;
    int tail = cpu->lsq->tail;
    if (head == -1 && tail == -1)
    {
        return 1;
    }
    return 0;
}

void updateLSQEntry(APEX_CPU *cpu, int src_tag, int src_value)
{
    int tail = cpu->lsq->tail;
    int i = 0;
    while (i <= tail)
    {
        LSQ_Entry *entry = cpu->lsq->entry[i];
        if (entry->lost == 0)
        {
            if (entry->src_tag == src_tag)
            {
                entry->src_valid_bit = 1;
                cpu->lsq->entry[i] = entry;
            }
        }
        i++;
    }
}
/*----------------------------------Load Store Queue utilities end-----------------------------------*/

/*----------------------------------Reorder buffer queue utilities start-----------------------------------*/

void addROBEntry(
    int established_bit,
    int instruction_type,
    int pc_value,
    int dest_phy_reg,
    int prev_phy_reg,
    int dest_arch_reg,
    int lsq_index,
    int mem_error_code,
    APEX_CPU *cpu)
{
    if (isROBFull(cpu))
    {
        return;
    }
    ROB_Entry *entry = malloc(sizeof(ROB_Entry));
    entry->established_bit = established_bit;
    entry->instruction_type = instruction_type;
    entry->pc_value = pc_value;
    entry->dest_phy_reg = dest_phy_reg;
    entry->prev_phy_reg = prev_phy_reg;
    entry->dest_arch_reg = dest_arch_reg;
    entry->lsq_index = lsq_index;
    entry->mem_error_code = mem_error_code;

    int tail = cpu->rob->tail;
    tail = (tail + 1) % ROB_SIZE;
    cpu->rob->entry[tail] = entry;
    cpu->rob->tail = tail;
}

ROB_Entry* getROBEntry(APEX_CPU *cpu)
{
    if (isROBEmpty(cpu))
    {
        return NULL;
    }
    int head = cpu->rob->head;
    int tail = cpu->rob->tail;
    ROB_Entry *entry = cpu->rob->entry[head];
    if(head == tail)
    {
        cpu->rob->head = -1;
        cpu->rob->tail = -1;
        return entry;
    }
    head = (head + 1) % ROB_SIZE;
    cpu->rob->head = head;
    return entry;
}

int isROBFull(APEX_CPU *cpu)
{
    int head = cpu->rob->head;
    int tail = cpu->rob->tail;
    if ((head == tail + 1) || (head == 0 && tail == ROB_SIZE - 1))
    {
        return 1;
    }
    return 0;
}

int isROBEmpty(APEX_CPU *cpu)
{
    int head = cpu->rob->head;
    int tail = cpu->rob->tail;
    if (head == -1 && tail == -1)
    {
        return 1;
    }
    return 0;
}

void updateROBEntry(APEX_CPU *cpu, int rob_index, int mem_error_code)
{
    if(rob_index>=cpu->rob->head && rob_index<=cpu->rob->tail){
        cpu->rob->entry[rob_index]->mem_error_code = mem_error_code;
    } 
}

/*----------------------------------Reorder buffer queue utilities end-----------------------------------*/


/*----------------------------------Branch target buffer utilities start-----------------------------------*/

void addBTBEntry(int pc_value, int target_address, APEX_CPU *cpu)
{
    BTB_Entry *entry = malloc(sizeof(BTB_Entry));
    entry->pc_value = pc_value;
    entry->target_address = target_address;
    
    int tail = cpu->btb->tail;
    tail = (tail + 1) % BTB_SIZE;
    cpu->btb->entry[tail] = entry;
    cpu->btb->tail = tail;
}

BTB_Entry* getBTBEntry(int pc_value, APEX_CPU *cpu)
{
    BTB_Entry *entries[] = cpu->btb->entry;
    int i = 0;
    while(i<BTB_SIZE)
    {
        BTB_Entry *entry = entries[i];
        if(entry->pc_value == pc_value){
            return entry;
        }
    }
    return NULL;
}

/*----------------------------------Branch target buffer utilities end-----------------------------------*/


/*
 * APEX CPU simulation loop
 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_run(APEX_CPU *cpu)
{
    char user_prompt_val;

    while (TRUE)
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("--------------------------------------------\n");
            printf("Clock Cycle #: %d\n", cpu->clock);
            printf("--------------------------------------------\n");
        }
        if (APEX_writeback(cpu))
        {
            /* Halt in writeback stage */
            printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
            break;
        }

        APEX_memory(cpu);
        APEX_execute(cpu);
        APEX_decode(cpu);
        APEX_fetch(cpu);

        print_reg_file(cpu);

        if (cpu->single_step)
        {
            printf("Press any key to advance CPU Clock or <q> to quit:\n");
            scanf("%c", &user_prompt_val);

            if ((user_prompt_val == 'Q') || (user_prompt_val == 'q'))
            {
                printf("APEX_CPU: Simulation Stopped, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
                break;
            }
        }

        cpu->clock++;
    }
}

/*
 * This function deallocates APEX CPU.
 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_stop(APEX_CPU *cpu)
{
    free(cpu->code_memory);
    free(cpu);
}