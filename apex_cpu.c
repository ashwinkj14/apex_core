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
#include <stdbool.h>

// #include "file_parser.c"
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
enqueueFreeList(int index, APEX_CPU *cpu)
{
    cpu->pr.tail = (++cpu->pr.tail) % 15;
    cpu->pr.PR_File[cpu->pr.tail].free = index;
}

static void reverse_insert_pr(int index, APEX_CPU *cpu)
{
    int head = cpu->pr.head;
    if(head == 0)
    {
        head = PR_FILE_SIZE-1;
    }else
    {
        head = head - 1;
    }
    cpu->pr.head = head;
    cpu->pr.PR_File[cpu->pr.head].free = index;
}

static int isPRF_empty(APEX_CPU *cpu)
{
    if (cpu->pr.tail == -1 && cpu->pr.head == -1)
    {
        return 1;
    }
    return 0;
}

static int getFreeRegFromPR(APEX_CPU *cpu)
{
    if (isPRF_empty(cpu))
    {
        return -1;
    }
    int index = cpu->pr.head;
    
    if (cpu->DR1.opcode == OPCODE_ADD || cpu->DR1.opcode == OPCODE_SUB || cpu->DR1.opcode == OPCODE_DIV || cpu->DR1.opcode == OPCODE_MUL || cpu->DR1.opcode == OPCODE_ADDL || cpu->DR1.opcode == OPCODE_SUBL || cpu->DR1.opcode == OPCODE_CMP)
    {
        cpu->prev_cc = cpu->pr.PR_File[index].free;
    }
    
    if (cpu->pr.tail == cpu->pr.head)
    {
        cpu->pr.tail = -1;
        cpu->pr.head = -1;
    }
    else
    {
        cpu->pr.head = (++cpu->pr.head) % 15;
    }

    int free = cpu->pr.PR_File[index].free;
    cpu->pr.PR_File[free].reg_invalid = 1;
    return free;
}

static void setSrcRegWithPR(int r1, int r2, int r3, APEX_CPU *cpu)
{
    if (r1 != -1)
    {
        cpu->DR1.ps1 = cpu->rt.reg[cpu->DR1.rs1];
    }
    if (r2 != -1)
    {
        cpu->DR1.ps2 = cpu->rt.reg[cpu->DR1.rs2];
        if (r3 != -1)
        {
            cpu->DR1.ps3 = cpu->rt.reg[cpu->DR1.rs3];
        }
    }
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

static void print_instruction_PR(const CPU_Stage *stage)
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

        printf("P%d,P%d,P%d ", stage->pd, stage->ps1, stage->ps2);
        break;
    }

    case OPCODE_MOVC:
    {

        printf("P%d", stage->pd);
        break;
    }

    case OPCODE_LOAD:
    {

        printf("P%d,P%d ", stage->pd, stage->ps1);
        break;
    }

    case OPCODE_STORE:
    {

        printf("P%d,P%d ", stage->ps1, stage->ps2);
        break;
    }

    case OPCODE_BZ:
    case OPCODE_BNZ:
    {
        printf("P%d,P%d ", stage->pd, stage->imm);
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

    if (strcmp(name, "Fetch"))
    {
        printf("%-15s: pc(%d) ", name, stage->pc);
        print_instruction_PR(stage);
        printf("\n");
    }
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

    for (int i = 0; i < REG_FILE_SIZE; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");
}


void print_rename_table(APEX_CPU *cpu)
{
    int i;

    printf("----------\n%s\n----------\n", "Rename Table:");

    for (int i = 0; i < REG_FILE_SIZE; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->rt.reg[i]);
    }

    printf("\n");
}

void print_physical_reg_file(APEX_CPU *cpu)
{
    int i;

    printf("----------\n%s\n----------\n", "Physical Register File:");

    for (int i = 0; i < PR_FILE_SIZE/2; ++i)
    {
        printf("P%-3d[%-3d] ", i, cpu->pr.PR_File[i].phy_Reg);
    }

    printf("\n");

    for (int i = PR_FILE_SIZE/2; i < PR_FILE_SIZE; ++i)
    {
        printf("P%-3d[%-3d] ", i, cpu->pr.PR_File[i].phy_Reg);
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
    if(cpu->waitingForBranch)
    {
        strcpy(cpu->fetch.opcode_str, "NOP");
        cpu->fetch.opcode = OPCODE_NOP;
        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Fetch", &cpu->fetch);
        }
        cpu->DR1 = cpu->fetch;
        cpu->DR1.has_insn = FALSE;
    }
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
        cpu->fetch.rs3 = current_ins->rs3;
        cpu->fetch.imm = current_ins->imm;

        /* Update PC for next instruction */
        cpu->pc += 4;

        /* Copy data from fetch latch to decode latch*/
        cpu->DR1 = cpu->fetch;

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
APEX_DR1(APEX_CPU *cpu)
{
    if (cpu->DR1.has_insn)
    {
        switch (cpu->DR1.opcode)
        {
        case OPCODE_MUL:
        {
            setSrcRegWithPR(cpu->DR1.rs1, cpu->DR1.rs2, -1, cpu);
            int free = getFreeRegFromPR(cpu);
            if (free != -1)
            {
                cpu->DR1.prev_phy_reg = cpu->rt.reg[cpu->DR1.rd];
                cpu->DR1.dest_arch_reg = cpu->DR1.rd;
                cpu->rt.reg[cpu->DR1.rd] = free;
                cpu->DR1.pd = free;
                cpu->DR1.stall = 0;
            }
            else
            {
                cpu->DR1.stall = 1;
                // stall nd break;
            }
            if (cpu->fBus[0].tag == cpu->DR1.ps1)
            {
                cpu->pr.PR_File[cpu->DR1.ps1].reg_invalid = 0;
            }
            if (cpu->fBus[1].tag == cpu->DR1.ps1)
            {
                cpu->pr.PR_File[cpu->DR1.ps1].reg_invalid = 0;
            }
            if (cpu->fBus[0].tag == cpu->DR1.ps2)
            {
                cpu->pr.PR_File[cpu->DR1.ps2].reg_invalid = 0;
            }
            if (cpu->fBus[1].tag == cpu->DR1.ps2)
            {
                cpu->pr.PR_File[cpu->DR1.ps2].reg_invalid = 0;
            }
            break;
        }
        case OPCODE_ADD:
        case OPCODE_DIV:
        case OPCODE_SUB:
        case OPCODE_XOR:
        case OPCODE_OR:
        case OPCODE_AND:
        case OPCODE_LDR:
        {
            setSrcRegWithPR(cpu->DR1.rs1, cpu->DR1.rs2, -1, cpu);
            int free = getFreeRegFromPR(cpu);
            if (free != -1)
            {
                cpu->DR1.prev_phy_reg = cpu->rt.reg[cpu->DR1.rd];
                cpu->DR1.dest_arch_reg = cpu->DR1.rd;
                cpu->rt.reg[cpu->DR1.rd] = free;
                cpu->DR1.pd = free;
                cpu->DR1.stall = 0;
            }
            else
            {
                cpu->DR1.stall = 1;
                // stall nd break;
            }
            if (cpu->fBus[0].tag == cpu->DR1.ps1)
            {
                cpu->pr.PR_File[cpu->DR1.ps1].reg_invalid = 0;
            }
            if (cpu->fBus[1].tag == cpu->DR1.ps1)
            {
                cpu->pr.PR_File[cpu->DR1.ps1].reg_invalid = 0;
            }
            if (cpu->fBus[0].tag == cpu->DR1.ps2)
            {
                cpu->pr.PR_File[cpu->DR1.ps2].reg_invalid = 0;
            }
            if (cpu->fBus[1].tag == cpu->DR1.ps2)
            {
                cpu->pr.PR_File[cpu->DR1.ps2].reg_invalid = 0;
            }
            break;
            /*Must do: check if the forwarding bus has any valid src tag or data and update the IQ so that as soon as it enters into the issue queue it is ready to be processed
            Also update the clock cycle of the dispatched instruction in IQ*/
        }

        case OPCODE_ADDL:
        case OPCODE_SUBL:
        case OPCODE_LOAD:
        {
            setSrcRegWithPR(cpu->DR1.rs1, -1, -1, cpu);
            int free = getFreeRegFromPR(cpu);
            if (free != -1)
            {
                cpu->DR1.prev_phy_reg = cpu->rt.reg[cpu->DR1.rd];
                cpu->rt.reg[cpu->DR1.rd] = free;
                cpu->DR1.dest_arch_reg = cpu->DR1.rd;
                cpu->DR1.pd = free;
                cpu->DR1.stall = 0;
            }
            else
            {
                cpu->DR1.stall = 1;
                // stall nd break;
            }
            if (cpu->fBus[0].tag == cpu->DR1.ps1)
            {
                cpu->pr.PR_File[cpu->DR1.ps1].reg_invalid = 0;
            }
            if (cpu->fBus[1].tag == cpu->DR1.ps1)
            {
                cpu->pr.PR_File[cpu->DR1.ps1].reg_invalid = 0;
            }

            cpu->DR1.imm = cpu->DR1.imm;
            break;
            /*Must do: check if the forwarding bus has any valid src tag or data and update the IQ so that as soon as it enters into the issue queue it is ready to be processed*/
        }
        case OPCODE_MOVC:
        {
            int free = getFreeRegFromPR(cpu);
            if (free != -1)
            {
                cpu->DR1.prev_phy_reg = cpu->rt.reg[cpu->DR1.rd];
                cpu->rt.reg[cpu->DR1.rd] = free;
                cpu->DR1.dest_arch_reg = cpu->DR1.rd;
                cpu->DR1.pd = free;
                cpu->DR1.stall = 0;
            }
            else
            {
                cpu->DR1.stall = 1;
                // stall nd break;
            }
            break;
        }
        case OPCODE_NOP:
        {
            break;
        }
        case OPCODE_STR:
        {
            setSrcRegWithPR(cpu->DR1.rs1, cpu->DR1.rs2, cpu->DR1.rs3, cpu);
            break;
            /*Must do: check if the forwarding bus has any valid src tag or data and update the IQ so that as soon as it enters into the issue queue it is ready to be processed*/
        }
        case OPCODE_STORE:
        {
            setSrcRegWithPR(cpu->DR1.rs1, cpu->DR1.rs2, -1, cpu);
            break;
        }
        case OPCODE_CMP:
        {
            setSrcRegWithPR(cpu->DR1.rs1, cpu->DR1.rs2, -1, cpu);
            int free = getFreeRegFromPR(cpu);
            cpu->DR1.prev_phy_reg = cpu->rt.reg[cpu->DR1.rd];
            cpu->DR1.pd = free;
            break;
            /*Must do: check if the forwarding bus has any valid src tag or data and update the IQ so that as soon as it enters into the issue queue it is ready to be processed*/
        }
        case OPCODE_JUMP:
        {
            setSrcRegWithPR(cpu->DR1.rs1, -1, -1, cpu);
            break;
            /*Must do: check if the forwarding bus has any valid src tag or data and update the IQ so that as soon as it enters into the issue queue it is ready to be processed*/
        }
        case OPCODE_BZ:
        case OPCODE_BNZ:
        {
            // prediction
            cpu->DR1.branch_reg = cpu->prev_cc;
            if (cpu->pr.PR_File[cpu->DR1.branch_reg].reg_invalid == 0)
            {
                if (cpu->pr.PR_File[cpu->DR1.branch_reg].cc_flag == 1 ^ cpu->DR1.opcode == OPCODE_BZ)
                {
                    if(cpu->DR1.branch_prediction){
                        updateBTBEntry(cpu->DR1.pc, 0, cpu);
                        cpu->fetch_from_next_cycle = TRUE;
                        cpu->pc = cpu->DR1.pc + 4;
                }
            }
        }
            if(cpu->DR1.imm < 0)
            {
                cpu->DR1.branch_prediction = 1;
                cpu->DR1.waitingForBranch = 1;
                cpu->waitingForBranch = 1;
            }
        }
        case OPCODE_HALT:
        {
            break;
        }
        }
        print_stage_content("DR1", &cpu->DR1);
        if (cpu->DR1.stall)
        {
            return;
        }
        cpu->DR2 = cpu->DR1;
        cpu->DR1.has_insn = FALSE;
        /*If the IQ is full stall the fetch and DR2 stage}*/
    }
}

static void
APEX_DR2(APEX_CPU *cpu)
{
    if (cpu->DR2.has_insn)
    {

        if (isIQFull(cpu) || isLSQFull(cpu) || isROBFull(cpu) || ((cpu->DR2.opcode == OPCODE_BNZ || cpu->DR2.opcode == OPCODE_BZ) && isBISFull(cpu)))
        {
            return;
        }
        int fu_type = 0;
        int src1_tag = 0;
        int src1_valid = 0;
        int src1_value = 0;
        
        int src2_tag = 0;
        int src2_valid = 0;
        int src2_value = 0;
        int dest = 0;

        int rob_index = (cpu->rob.tail + 1) % ROB_SIZE;
        int lsq_index = (cpu->lsq.tail + 1) % LSQ_SIZE;

        int instruction_type = R2R;

        switch (cpu->DR2.opcode)
        {
        case OPCODE_ADD:
        case OPCODE_DIV:
        case OPCODE_SUB:
        case OPCODE_CMP:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = !cpu->pr.PR_File[cpu->DR2.ps2].reg_invalid;
            src2_value = cpu->pr.PR_File[cpu->DR2.ps2].phy_Reg;
            dest = cpu->DR2.pd;
            break;
        }

        case OPCODE_MUL:
        {
            fu_type = MUL_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = !cpu->pr.PR_File[cpu->DR2.ps2].reg_invalid;
            src2_value = cpu->pr.PR_File[cpu->DR2.ps2].phy_Reg;
            dest = cpu->DR2.pd;
            break;
        }

        case OPCODE_ADDL:
        case OPCODE_SUBL:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = 1;
            dest = cpu->DR2.pd;
            break;
        }

        case OPCODE_XOR:
        case OPCODE_OR:
        case OPCODE_AND:
        {
            fu_type = LOP_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = !cpu->pr.PR_File[cpu->DR2.ps2].reg_invalid;
            src2_value = cpu->pr.PR_File[cpu->DR2.ps2].phy_Reg;
            dest = cpu->DR2.pd;
            break;
        }

        case OPCODE_MOVC:
        {
            fu_type = INT_U;
            src1_valid = 1;
            src2_valid = 1;
            dest = cpu->DR2.pd;
            break;
        }

        case OPCODE_HALT:
        {
            fu_type = INT_U;
            src1_valid = 1;
            src2_valid = 1;
            instruction_type = HALT;
            break;
        }

        case OPCODE_NOP:
        {
            fu_type = INT_U;
            src1_valid = 1;
            src2_valid = 1;
            instruction_type = SKIP;
            break;
        }

        case OPCODE_LDR:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = !cpu->pr.PR_File[cpu->DR2.ps2].reg_invalid;
            src2_value = cpu->pr.PR_File[cpu->DR2.ps2].phy_Reg;
            dest = lsq_index;
            instruction_type = LOAD;

            addLSQEntry(1, 1, 0, 0, cpu->DR2.pd, 1, 0, 0, rob_index, cpu);
            break;
        }

        case OPCODE_LOAD:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = 1;
            dest = lsq_index;
            instruction_type = LOAD;

            addLSQEntry(1, 1, 0, 0, cpu->DR2.pd, 1, 0, 0, rob_index, cpu);
            break;
        }

        case OPCODE_STORE:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = !cpu->pr.PR_File[cpu->DR2.ps2].reg_invalid;
            src2_value = cpu->pr.PR_File[cpu->DR2.ps2].phy_Reg;
            dest = lsq_index;
            instruction_type = STORE;

            addLSQEntry(1, 0, 0, 0, dest, src1_valid, src1_tag, src1_value, rob_index, cpu);
            break;
        }

        case OPCODE_STR:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = !cpu->pr.PR_File[cpu->DR2.ps2].reg_invalid;
            src2_value = cpu->pr.PR_File[cpu->DR2.ps2].phy_Reg;
            int src3_valid = !cpu->pr.PR_File[cpu->DR2.ps3].reg_invalid;
            int src3_value = cpu->pr.PR_File[cpu->DR2.ps3].phy_Reg;
            dest = lsq_index;
            instruction_type = STORE;

            addLSQEntry(1, 0, 0, 0, dest, src3_valid, cpu->DR2.ps3, src3_value, rob_index, cpu);
            break;
        }

        case OPCODE_JUMP:
        {
            fu_type = INT_U;
            src1_valid = !cpu->pr.PR_File[cpu->DR2.ps1].reg_invalid;
            src1_value = cpu->pr.PR_File[cpu->DR2.ps1].phy_Reg;
            src2_valid = 1;
            instruction_type = SKIP;
            break;
        }

        case OPCODE_BZ:
        case OPCODE_BNZ:
        {
            fu_type = INT_U;
            src1_valid = 1;
            src2_valid = 1;
            instruction_type = SKIP;
            cpu->new_bis = 1;
            if (cpu->pr.PR_File[cpu->DR2.branch_reg].reg_invalid == 0)
            {
                if (cpu->pr.PR_File[cpu->DR2.branch_reg].cc_flag == 1 ^ cpu->DR2.opcode == OPCODE_BZ)
                {
                    if(cpu->DR2.branch_prediction){
                        updateBTBEntry(cpu->DR2.pc, 0, cpu);
                        cpu->fetch_from_next_cycle = TRUE;
                        cpu->pc = cpu->DR2.pc + 4;
                        cpu->DR1.has_insn = FALSE;
                    }
                }
            }
            break;
        }

        default:
        {
            break;
        }
        }
        if(cpu->new_bis)
        {
            addBISEntry(cpu, cpu->DR2.pc, rob_index);
            cpu->new_bis = 0;
        }
        addROBEntry(1, instruction_type, cpu->DR2.pc, dest, cpu->DR2.prev_phy_reg, cpu->DR2.dest_arch_reg, lsq_index, 0, cpu);
        addIQEntry(1, fu_type, cpu->DR2.imm, src1_valid, cpu->DR2.ps1, src1_value, src2_valid, cpu->DR2.ps2, src2_value, dest, cpu->DR2.waitingForBranch, cpu->bis.tail, cpu);
        print_stage_content("DR2", &cpu->DR2);
        cpu->I_Queue = cpu->DR2;
        cpu->DR2.has_insn = FALSE;
    }
}

static void APEX_LSQ(APEX_CPU *cpu)
{
    if (cpu->fBus[0].busy)
    {
        updateLSQEntry(cpu, cpu->fBus[0].tag, cpu->fBus[0].data);
    }
    if (cpu->fBus[1].busy)
    {
        updateLSQEntry(cpu, cpu->fBus[1].tag, cpu->fBus[1].data);
    }
    
}

static void APEX_IQ(APEX_CPU *cpu)
{
    if (cpu->fBus[0].busy)
    {
        updateIQEntry(cpu, cpu->fBus[0].tag, cpu->fBus[0].isDataFwd, cpu->fBus[0].data);
    }
    if (cpu->fBus[1].busy)
    {
        updateIQEntry(cpu, cpu->fBus[1].tag, cpu->fBus[1].isDataFwd, cpu->fBus[1].data);
    }
    int index = 0;
    int loop = 1;
    while (loop)
    {
        index = getIQEntry_Index(cpu, index);
        if (index == -1)
        {
            return;
        }

        int fu_type = cpu->iq.entry[index]->fu_type;
        switch (fu_type)
        {
        case INT_U:
        {
            if (cpu->INT_FU.has_insn)
            {
                index++;
                continue;
            }
            cpu->I_Queue.rs1_value = cpu->iq.entry[index]->src1_value;
            cpu->I_Queue.rs2_value = cpu->iq.entry[index]->src2_value;
            cpu->I_Queue.imm = cpu->iq.entry[index]->literal;
            cpu->I_Queue.has_insn = TRUE;
            cpu->I_Queue.pd = cpu->iq.entry[index]->dest;
            cpu->I_Queue.waitingForBranch = cpu->iq.entry[index]->waitingForBranch;
            cpu->INT_FU = cpu->I_Queue;
            break;
        }

        case LOP_U:
        {
            if (cpu->LOP_FU.has_insn)
            {
                index++;
                continue;
            }
            cpu->I_Queue.rs1_value = cpu->iq.entry[index]->src1_value;
            cpu->I_Queue.rs2_value = cpu->iq.entry[index]->src2_value;
            cpu->I_Queue.imm = cpu->iq.entry[index]->literal;
            cpu->I_Queue.has_insn = TRUE;
            cpu->I_Queue.pd = cpu->iq.entry[index]->dest;
            cpu->LOP_FU = cpu->I_Queue;
            break;
        }

        case MUL_U:
        {
            if (cpu->MUL1_FU.has_insn)
            {
                index++;
                continue;
            }
            cpu->I_Queue.rs1_value = cpu->iq.entry[index]->src1_value;
            cpu->I_Queue.rs2_value = cpu->iq.entry[index]->src2_value;
            cpu->I_Queue.imm = cpu->iq.entry[index]->literal;
            cpu->I_Queue.has_insn = TRUE;
            cpu->I_Queue.pd = cpu->iq.entry[index]->dest;
            cpu->MUL1_FU = cpu->I_Queue;
            break;
        }

        default:
        {
            break;
        }
        }
        break;
    }
    shiftIQElements(cpu, index);
}

static void
APEX_INT_FU(APEX_CPU *cpu)
{
    if (cpu->INT_FU.has_insn)
    {
        print_stage_content("INT_FU", &cpu->INT_FU);
        if (cpu->fBus[0].busy && cpu->fBus[1].busy)
        {
            return;
        }
        switch (cpu->INT_FU.opcode)
        {
        case OPCODE_ADD:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value + cpu->INT_FU.rs2_value; // recieved from IQ or DR2(need to confirm)
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;       // PR write
            if (cpu->INT_FU.result_buffer == 0)
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 1;
            }
            else
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 0;
            }
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[0].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = cpu->INT_FU.pd;
                cpu->fBus[1].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            break;
        }
        case OPCODE_DIV:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value / cpu->INT_FU.rs2_value;
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            if (cpu->INT_FU.result_buffer == 0)
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 1;
            }
            else
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 0;
            }
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[0].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = cpu->INT_FU.pd;
                cpu->fBus[1].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            break;
        }
        case OPCODE_SUB:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value - cpu->INT_FU.rs2_value;
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            if (cpu->INT_FU.result_buffer == 0)
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 1;
            }
            else
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 0;
            }
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[0].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = cpu->INT_FU.pd;
                cpu->fBus[1].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            break;
        }
        case OPCODE_SUBL:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value - cpu->INT_FU.imm;
            if (cpu->INT_FU.result_buffer == 0)
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 1;
            }
            else
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 0;
            }
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[0].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = cpu->INT_FU.pd;
                cpu->fBus[1].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            break;
        }
        case OPCODE_ADDL:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value + cpu->INT_FU.imm;
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            if (cpu->INT_FU.result_buffer == 0)
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 1;
            }
            else
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 0;
            }
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[0].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].cc = cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag;
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = cpu->INT_FU.pd;
                cpu->fBus[1].busy = 1;
                cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            }
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;

            break;
        }
        case OPCODE_CMP: // need to discuss
        {

            if (cpu->INT_FU.rs1_value == cpu->INT_FU.rs2_value)
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 1;
            }
            else
            {
                cpu->pr.PR_File[cpu->INT_FU.pd].cc_flag = 0;
            }

            break;
        }
        case OPCODE_BZ:
        case OPCODE_BNZ:
        {
            cpu->conditional_pc = cpu->INT_FU.pc + cpu->INT_FU.imm;
            if(cpu->I_Queue.waitingForBranch)
            {
                flush_instructions(cpu, cpu->INT_FU.pc);
                cpu->waitingForBranch = 0;
                cpu->pc = cpu->conditional_pc;
                addBTBEntry(cpu->INT_FU.pc, cpu->conditional_pc, cpu);
            }
            break;
        }

        case OPCODE_LDR:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value + cpu->INT_FU.rs2_value;

            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[1].busy = 1;
            }
            break;
        }
        case OPCODE_LOAD:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value + cpu->INT_FU.imm;
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[1].busy = 1;
            }
            break;
        }
        case OPCODE_MOVC:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.imm;
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = cpu->INT_FU.pd;
                cpu->fBus[1].busy = 1;
            }
            cpu->pr.PR_File[cpu->INT_FU.pd].reg_invalid = 0;
            cpu->pr.PR_File[cpu->INT_FU.pd].phy_Reg = cpu->INT_FU.result_buffer;
            break;
        }

        case OPCODE_STORE:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value + cpu->INT_FU.imm;
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[1].busy = 1;
            }
            break;
        }
        case OPCODE_STR:
        {
            cpu->INT_FU.result_buffer = cpu->INT_FU.rs1_value + cpu->INT_FU.rs2_value;
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->INT_FU.result_buffer;
                cpu->fBus[0].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->INT_FU.result_buffer;
                cpu->fBus[1].tag = (cpu->INT_FU.pd + 1) * (-1);
                cpu->fBus[1].busy = 1;
            }
            break;
        }
        case OPCODE_JUMP:
        {
            cpu->conditional_pc = cpu->INT_FU.pc + cpu->INT_FU.imm;
            break;
        }
        }
        cpu->INT_FU.has_insn = FALSE;
    }
}

static void
APEX_LOP_FU(APEX_CPU *cpu)
{
    if (cpu->LOP_FU.has_insn)
    {
        print_stage_content("LOP_FU", &cpu->LOP_FU);
        if (cpu->fBus[0].busy && cpu->fBus[1].busy)
        {
            return;
        }
        switch (cpu->LOP_FU.opcode)
        {
        case OPCODE_XOR:
        {
            cpu->LOP_FU.result_buffer = cpu->LOP_FU.rs1_value ^ cpu->LOP_FU.rs2_value;
            cpu->pr.PR_File[cpu->LOP_FU.pd].phy_Reg = cpu->LOP_FU.result_buffer;

            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->LOP_FU.result_buffer;
                cpu->fBus[0].tag = cpu->LOP_FU.pd;
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->LOP_FU.result_buffer;
                cpu->fBus[1].tag = cpu->LOP_FU.pd;
                cpu->fBus[1].busy = 1;
            }

            break;
        }
        case OPCODE_OR:
        {
            cpu->LOP_FU.result_buffer = cpu->LOP_FU.rs1_value | cpu->LOP_FU.rs2_value;
            cpu->pr.PR_File[cpu->LOP_FU.pd].phy_Reg = cpu->LOP_FU.result_buffer;
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->LOP_FU.result_buffer;
                cpu->fBus[0].tag = cpu->LOP_FU.pd;
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->LOP_FU.result_buffer;
                cpu->fBus[1].tag = cpu->LOP_FU.pd;
                cpu->fBus[1].busy = 1;
            }
            break;
        }
        case OPCODE_AND:
        {
            cpu->LOP_FU.result_buffer = cpu->LOP_FU.rs1_value & cpu->LOP_FU.rs2_value;
            cpu->pr.PR_File[cpu->LOP_FU.pd].phy_Reg = cpu->LOP_FU.result_buffer;
            if (!cpu->fBus[0].busy)
            {
                cpu->fBus[0].data = cpu->LOP_FU.result_buffer;
                cpu->fBus[0].tag = cpu->LOP_FU.pd;
                cpu->fBus[0].busy = 1;
            }
            else if (!cpu->fBus[1].busy) // check for forw
            {
                cpu->fBus[1].data = cpu->LOP_FU.result_buffer;
                cpu->fBus[1].tag = cpu->LOP_FU.pd;
                cpu->fBus[1].busy = 1;
            }
            break;
        }
        }
        cpu->pr.PR_File[cpu->LOP_FU.pd].reg_invalid = 0;
    }
}

static void
APEX_MUL1_FU(APEX_CPU *cpu)
{
    if (cpu->MUL1_FU.has_insn)
    {
        print_stage_content("MUL1_FU", &cpu->MUL1_FU);
        if (cpu->MUL1_FU.opcode == OPCODE_MUL)
        {
            cpu->MUL1_FU.result_buffer = cpu->MUL1_FU.rs1_value * cpu->MUL1_FU.rs2_value;
        }
        cpu->MUL2_FU = cpu->MUL1_FU;
        cpu->MUL1_FU.has_insn = FALSE;
    }
}

static void
APEX_MUL2_FU(APEX_CPU *cpu)
{
    if (cpu->MUL2_FU.has_insn)
    {
        print_stage_content("MUL2_FU", &cpu->MUL2_FU);
        if (cpu->MUL2_FU.opcode == OPCODE_MUL)
        {
            cpu->MUL2_FU.result_buffer = cpu->MUL2_FU.rs1_value * cpu->MUL2_FU.rs2_value;
        }
        cpu->MUL3_FU = cpu->MUL2_FU;
        cpu->MUL2_FU.has_insn = FALSE;
        cpu->MUL3_FU.has_insn = TRUE;
    }
}

static void
APEX_MUL3_FU(APEX_CPU *cpu)
{
    if (cpu->MUL3_FU.has_insn)
    {
        print_stage_content("MUL3_FU", &cpu->MUL3_FU);
        if (cpu->fBus[0].busy && cpu->fBus[1].busy)
        {
            return;
        }
        if (cpu->MUL3_FU.opcode == OPCODE_MUL)
        {
            cpu->MUL3_FU.result_buffer = cpu->MUL3_FU.rs1_value * cpu->MUL3_FU.rs2_value;
        }

        if (!cpu->fBus[0].busy)
        {
            cpu->fBus[0].tag = cpu->MUL3_FU.pd;
            cpu->fBus[0].busy = 1;
        }
        else if (!cpu->fBus[1].busy) // check for forw
        {
            cpu->fBus[1].tag = cpu->MUL3_FU.pd;
            cpu->fBus[1].busy = 1;
        }
        cpu->MUL4_FU = cpu->MUL3_FU;
        cpu->MUL3_FU.has_insn = FALSE;
    }
    // implement forwarding for tag
}

static void
APEX_MUL4_FU(APEX_CPU *cpu)
{
    if (cpu->MUL4_FU.has_insn)
    {
        print_stage_content("MUL4_FU", &cpu->MUL4_FU);
        if (cpu->fBus[0].busy && cpu->fBus[1].busy)
        {
            return;
        }
        if (cpu->MUL4_FU.opcode == OPCODE_MUL)
        {
            cpu->MUL4_FU.result_buffer = cpu->MUL4_FU.rs1_value * cpu->MUL4_FU.rs2_value;
            cpu->pr.PR_File[cpu->MUL4_FU.pd].phy_Reg = cpu->MUL4_FU.result_buffer;
        }
        if (cpu->MUL4_FU.result_buffer == 0)
        {
            cpu->pr.PR_File[cpu->MUL4_FU.pd].cc_flag = 1;
        }
        else
        {
            cpu->pr.PR_File[cpu->MUL4_FU.pd].cc_flag = 0;
        }
        if (!cpu->fBus[0].busy)
        {
            cpu->fBus[0].cc = cpu->pr.PR_File[cpu->MUL4_FU.pd].cc_flag;
            cpu->fBus[0].data = cpu->MUL4_FU.result_buffer;
            cpu->fBus[0].tag = cpu->MUL4_FU.pd;
            cpu->fBus[0].busy = 1;
        }
        else if (!cpu->fBus[1].busy) // check for forw
        {
            cpu->fBus[1].cc = cpu->pr.PR_File[cpu->MUL4_FU.pd].cc_flag;
            cpu->fBus[1].data = cpu->MUL4_FU.result_buffer;
            cpu->fBus[1].tag = cpu->MUL4_FU.pd;
            cpu->fBus[1].busy = 1;
        }
        cpu->pr.PR_File[cpu->MUL4_FU.pd].reg_invalid = 0;
        cpu->MUL4_FU.has_insn = FALSE;
    }
}

int do_commit(APEX_CPU *cpu)
{
    ROB_Entry *entry = getROBHead(cpu);
    if (entry == NULL)
    {
        return 0;
    }
    switch (entry->instruction_type)
    {
    case R2R:
    {   
        int isInvalid = cpu->pr.PR_File[entry->dest_phy_reg].reg_invalid;
        if (isInvalid)
        {
            return 0;
        }
        cpu->regs[entry->dest_arch_reg] = cpu->pr.PR_File[entry->dest_phy_reg].phy_Reg;
        enqueueFreeList(entry->prev_phy_reg, cpu);
        break;
    }

    case LOAD:
    {
        if (entry->lsq_index == cpu->lsq.head)
        {
            APEX_D_cache(cpu);
            if (entry->lsq_index == cpu->lsq.head)
            {
                return 0;
            }
            cpu->regs[entry->dest_arch_reg] = cpu->pr.PR_File[entry->dest_phy_reg].phy_Reg;
            enqueueFreeList(entry->prev_phy_reg, cpu);
        }
        break;
    }

    case STORE:
    {
        if (entry->lsq_index == cpu->lsq.head)
        {
            APEX_D_cache(cpu);
            if (entry->lsq_index == cpu->lsq.head)
            {
                return 0;
            }
        }
        break;
    }

    case SKIP:
    case HALT:
    {
        break;
    }
    }
    removeROBHead(cpu);
    if (entry->instruction_type == HALT)
    {
        return 1;
    }
    return 0;
}

void APEX_D_cache(APEX_CPU *cpu)
{
    int head = cpu->lsq.head;
    int tail = cpu->lsq.tail;
    LSQ_Entry *entry = cpu->lsq.entry[head];
    if (!entry->mem_valid_bit)
    {
        return;
    }
    int lost = entry->lost;
    if (lost)
    {
        int pr = entry->dest_reg_address;
        cpu->pr.PR_File[pr].phy_Reg = cpu->data_memory[entry->mem_address];
    }
    else
    {
        cpu->data_memory[entry->mem_address] = entry->src_value;
    }
    if (head == tail)
    {
        cpu->lsq.head = -1;
        cpu->lsq.tail = -1;
        return;
    }
    cpu->lsq.head = (head + 1) % LSQ_SIZE;
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
/*Intialise PR and RT with default setup*/
static void
intialize_PR_RT(APEX_CPU *cpu)
{

    while (cpu->pr.head < 8)
    {
        cpu->rt.reg[cpu->pr.head] = cpu->pr.head;
        cpu->pr.PR_File[cpu->pr.head].cc_flag = -1;
        cpu->pr.PR_File[cpu->pr.head].free = cpu->pr.head;
        cpu->pr.head++;
    }
    int head = cpu->pr.head;
    while (head < 15)
    {
        cpu->pr.PR_File[head].free = head;
        head++;
    }
}

static void initialize_bus(APEX_CPU *cpu)
{
    for (int i = 0; i < 2; i++)
    {
        cpu->fBus[i].data = 0;
        cpu->fBus[i].tag = 0;
        cpu->fBus[i].cc = 0;
        cpu->fBus[i].isDataFwd = FALSE;
        cpu->fBus[i].busy = 0;
    }
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

    cpu->pr.head = 0;
    cpu->pr.tail = 14;
    initialize_bus(cpu);
    intialize_PR_RT(cpu);

    cpu->iq.tail = -1;

    cpu->lsq.head = -1;
    cpu->lsq.tail = -1;

    cpu->rob.head = -1;
    cpu->rob.tail = -1;

    cpu->btb.tail = -1;

    cpu->bis.head = -1;
    cpu->bis.tail = -1;

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
    int waitingForBranch,
    int bis_index,
    APEX_CPU *cpu)
{

    if (isIQFull(cpu))
    {
        return;
    }
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
    entry->bis_index = bis_index;
    entry->dest = dest;

    int tail = ++cpu->iq.tail;
    cpu->iq.entry[tail] = entry;
}

int isIQFull(APEX_CPU *cpu)
{
    int tail = cpu->iq.tail;
    if (tail == IQ_SIZE - 1)
    {
        return 1;
    }
    return 0;
}

int isIQEmpty(APEX_CPU *cpu)
{
    int tail = cpu->iq.tail;
    if (tail == -1)
    {
        return 1;
    }
    return 0;
}

int getIQEntry_Index(APEX_CPU *cpu, int index)
{
    if (isIQEmpty(cpu))
    {
        return -1;
    }
    int tail = cpu->iq.tail;
    int i = index;
    while (i <= tail)
    {

        if (isIQEntryReady(cpu->iq.entry[i]))
        {
            return i;
        }
        i++;
    }
    return -1;
}

IQ_Entry *getIQEntry(APEX_CPU *cpu)
{
    if (isIQEmpty(cpu))
    {
        return NULL;
    }
    int tail = cpu->iq.tail;
    int i = 0;
    while (i <= tail)
    {
        IQ_Entry *entry = cpu->iq.entry[i];
        if (isIQEntryReady(entry))
        {
            shiftIQElements(cpu, i);
            return entry;
        }
        i++;
    }
    return NULL;
}

int isIQEntryReady(IQ_Entry *entry)
{
    return entry->allocated_bit && entry->src1_valid_bit && entry->src2_valid_bit;
}

void shiftIQElements(APEX_CPU *cpu, int pos)
{
    int tail = cpu->iq.tail;
    while (pos < tail)
    {
        cpu->iq.entry[pos] = cpu->iq.entry[pos + 1];
        pos++;
    }
    cpu->iq.tail--;
}

void updateIQEntry(APEX_CPU *cpu, int src_tag, int isDataAvailable, int src_value)
{
    int tail = cpu->iq.tail;
    int i = 0;
    while (i <= tail)
    {
        if (cpu->iq.entry[i]->src1_tag == src_tag)
        {
            cpu->iq.entry[i]->src1_valid_bit = 1;
            if (isDataAvailable)
            {
                cpu->iq.entry[i]->src1_value = src_value;
            }
        }
        if (cpu->iq.entry[i]->src2_tag == src_tag)
        {
            cpu->iq.entry[i]->src2_valid_bit = 1;
            if (isDataAvailable)
            {
                cpu->iq.entry[i]->src2_value = src_value;
            }
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

    int tail = cpu->lsq.tail;
    int head = cpu->lsq.head;
    if(head == -1){
        cpu->lsq.head = 0;
    }
    tail = (tail + 1) % LSQ_SIZE;
    cpu->lsq.entry[tail] = entry;
    cpu->lsq.tail = tail;
}

int getLSQEntry(APEX_CPU *cpu)
{
    if (isLSQEmpty(cpu))
    {
        return -2;
    }
    int head = cpu->lsq.head;
    int tail = cpu->lsq.tail;

    if (head == tail)
    {
        cpu->lsq.head = -1;
        cpu->lsq.tail = -1;
        return -1;
    }
    head = (head + 1) % LSQ_SIZE;
    cpu->lsq.head = head;
    return head;
}

int isLSQFull(APEX_CPU *cpu)
{
    int head = cpu->lsq.head;
    int tail = cpu->lsq.tail;
    if ((head == tail + 1) || (head == 0 && tail == LSQ_SIZE - 1))
    {
        return 1;
    }
    return 0;
}

int isLSQEmpty(APEX_CPU *cpu)
{
    int head = cpu->lsq.head;
    int tail = cpu->lsq.tail;
    if (head == -1 && tail == -1)
    {
        return 1;
    }
    return 0;
}

void updateLSQEntry(APEX_CPU *cpu, int src_tag, int src_value)
{
    if (src_tag < 0)
    {
        int index = (src_tag * -1) - 1;
        LSQ_Entry *entry = cpu->lsq.entry[index];
        cpu->lsq.entry[index]->mem_address = src_value;
        entry->mem_valid_bit = 1;
        return;
    }else{
        int i = cpu->lsq.head;
        if(i == -1){
            return;
        }
        int tail = cpu->lsq.tail;
        while(i<=tail)
        {
            LSQ_Entry *entry = cpu->lsq.entry[i];
            if(entry->lost == 0)
            {
                if(entry->src_tag == src_tag)
                {
                    entry->src_valid_bit = 1;
                    entry->src_value = src_value;
                    return;
                }
            }
            i++;
        }
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
        printf("ROB full");
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

    int tail = cpu->rob.tail;
    tail = (tail + 1) % ROB_SIZE;
    cpu->rob.entry[tail] = entry;
    cpu->rob.tail = tail;
}

ROB_Entry *getROBHead(APEX_CPU *cpu)
{
    if (isROBEmpty(cpu))
    {
        return NULL;
    }

    return cpu->rob.entry[cpu->rob.head];
}

void removeROBHead(APEX_CPU *cpu)
{
    if (isROBEmpty(cpu))
    {
        return;
    }

    int head = cpu->rob.head;
    int tail = cpu->rob.tail;

    if (head == tail && head != -1)
    {
        cpu->rob.head = -1;
        cpu->rob.tail = -1;
        return;
    }
    head = (head + 1) % ROB_SIZE;
    cpu->rob.head = head;
    return;
}

int isROBFull(APEX_CPU *cpu)
{
    int head = cpu->rob.head;
    int tail = cpu->rob.tail;
    if ((head == tail + 1) || (head == 0 && tail == ROB_SIZE - 1))
    {
        return 1;
    }
    return 0;
}

int isROBEmpty(APEX_CPU *cpu)
{
    int head = cpu->rob.head;
    int tail = cpu->rob.tail;
    if (head == -1 && tail == -1)
    {
        return 1;
    }
    if (head == -1 && tail != -1)
    {
        cpu->rob.head = 0;
    }
    return 0;
}

void updateROBEntry(APEX_CPU *cpu, int rob_index, int mem_error_code)
{
    if (rob_index >= cpu->rob.head && rob_index <= cpu->rob.tail)
    {
        cpu->rob.entry[rob_index]->mem_error_code = mem_error_code;
    }
}

/*----------------------------------Reorder buffer queue utilities end-----------------------------------*/

/*----------------------------------Branch target buffer utilities start-----------------------------------*/
int isBTBFull(APEX_CPU *cpu)
{
   int i=0; 
   while(i<BTB_SIZE)
   {
    BTB_Entry *entry = cpu->btb.entry[i];
    if(entry->prediction == 0)
    {
        return i;
    }
    if(entry ==NULL)
    {
        return -1;
    }
    i++;
   }
   return -2;
}
void BTBReplacement(APEX_CPU *cpu, BTB_Entry *entry)
{
    int i = 0;
    int min = 100000000;
    int tail;
    while (i < BTB_SIZE)
    {
        BTB_Entry *entry = cpu->btb.entry[i];
        if (entry->pc_value <min)
        {
            min = entry->pc_value;
            tail =i;
        }
        i++;
    }
    cpu->btb.entry[tail] = entry;
    cpu->btb.tail = tail;
}

void addBTBEntry(int pc_value, int target_address, APEX_CPU *cpu)
{
    BTB_Entry *entry = malloc(sizeof(BTB_Entry));
    entry->pc_value = pc_value;
    entry->target_address = target_address;
    int btbInd = isBTBFull(cpu);
    if (btbInd==-2)
    {
        BTBReplacement(cpu,entry);
    }
    else if(btbInd != -1)
    {
        int tail = btbInd;
        cpu->btb.entry[tail] = entry;
        cpu->btb.tail = tail;
    }
    else{
    int tail = cpu->btb.tail;
    tail = (tail + 1) % BTB_SIZE;
    cpu->btb.entry[tail] = entry;
    cpu->btb.tail = tail;
    }
}

void updateBTBEntry(int pc_value, int prediction, APEX_CPU *cpu)
{
    int tail = cpu->btb.tail;
    int i = 0;
    while(i<=tail)
    {
        BTB_Entry *entry = cpu->btb.entry[i];
        if(entry->pc_value == pc_value){
            entry->prediction = prediction;
            return;
        }
        i++;
    }
}

BTB_Entry *getBTBEntry(int pc_value, APEX_CPU *cpu)
{
    int i = 0;
    while (i < BTB_SIZE)
    {
        BTB_Entry *entry = cpu->btb.entry[i];
        if (entry->pc_value == pc_value)
        {
            return entry;
        }
    }
    return NULL;
}

/*----------------------------------Branch target buffer utilities end-----------------------------------*/

/*----------------------------------Branch Instruction stack utilities start-----------------------------------*/

void addBISEntry(APEX_CPU *cpu, int pc_value, int rob_index)
{
    if(isBISFull(cpu)){
        return;
    }
    BIS_Entry *entry = malloc(sizeof(BIS_Entry));
    entry->rob_index = rob_index;
    entry->pc_value = pc_value;

    int tail = cpu->bis.tail;
    int head = cpu->bis.head;
    if(head == -1){
        cpu->bis.head = 0;
    }
    tail = (tail + 1) % BIS_SIZE;
    cpu->bis.entry[tail] = entry;
    cpu->bis.tail = tail;
}

int isBISFull(APEX_CPU *cpu)
{
    int head = cpu->bis.head;
    int tail = cpu->bis.tail;
    if ((head == tail + 1) || (head == 0 && tail == ROB_SIZE - 1))
    {
        return 1;
    }
    return 0;
}

int getBIS_index(APEX_CPU *cpu, int pc_value)
{
    int i = cpu->bis.head;
    int tail = cpu->bis.tail;
    while(i<=tail)
    {
        BIS_Entry *entry = cpu->bis.entry[i];
        if(entry->pc_value == pc_value)
        {
            return i;
        }
        i = (i+1) % BIS_SIZE;
    }
    return -1;
}
/*----------------------------------Branch Instruction stack utilities end-----------------------------------*/

/*----------------------------------FLUSH instruction utilities start-----------------------------------*/

void flush_instructions(APEX_CPU *cpu, int pc_value)
{
    flush_bisEntries(cpu,pc_value);
}

void flush_bisEntries(APEX_CPU *cpu, int pc_value)
{
    int bis_index = getBIS_index(cpu, pc_value);
    flush_iqEntries(cpu, bis_index);
    cpu->bis.tail = bis_index;
    BIS_Entry *entry = cpu->bis.entry[bis_index];
    int rob_index = entry->rob_index;
    flush_robEntries(cpu, rob_index);
}

void flush_iqEntries(APEX_CPU *cpu, int bis_index)
{
    int i = 0;
    int tail = cpu->iq.tail;
    while(i<=tail)
    {
        IQ_Entry *entry = cpu->iq.entry[i];
        if(entry->bis_index == bis_index){
            cpu->iq.tail = i-1;
            return;
        }
        i--;
    }
}

void flush_lsqEntries(APEX_CPU *cpu, int lsq_index)
{
    cpu->lsq.tail = lsq_index;
}

void flush_robEntries(APEX_CPU *cpu, int rob_index)
{
    int lsq_index = cpu->rob.entry[rob_index]->lsq_index;
    flush_lsqEntries(cpu, lsq_index);//lsq instructions are flushed here

    int start = cpu->rob.tail;
    int end = rob_index+1;
    while(start>=end)
    {
        ROB_Entry *entry = cpu->rob.entry[start];
        int prev_pr = entry->prev_phy_reg;
        int curr_pr = entry->dest_phy_reg;
        int arc_reg = entry->dest_arch_reg;
        cpu->rt.reg[arc_reg] = prev_pr;
        reverse_insert_pr(curr_pr, cpu);
        
        if(start == 0)
        {
            start = ROB_SIZE - 1;
        }else
        {
            start = start - 1;
        }
    }
    cpu->rob.tail = rob_index;
}
/*----------------------------------FLUSH instruction utilities end-----------------------------------*/


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
        if (do_commit(cpu))
        {
            /* Halt in writeback stage */
            printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
            break;
        }

        APEX_MUL4_FU(cpu);
        APEX_MUL3_FU(cpu);
        APEX_MUL2_FU(cpu);
        APEX_MUL1_FU(cpu);

        APEX_LOP_FU(cpu);

        APEX_INT_FU(cpu);

        APEX_LSQ(cpu);
        APEX_IQ(cpu);

        APEX_DR2(cpu);
        APEX_DR1(cpu);

        APEX_fetch(cpu);

        print_reg_file(cpu);
        print_rename_table(cpu);
        print_physical_reg_file(cpu);

        cpu->fBus[0].busy = 0;
        cpu->fBus[1].busy = 0;

        if (cpu->single_step)
        {
            user_prompt_val = 'r';
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