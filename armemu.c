#include <stdbool.h>
#include <stdio.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15

/* Assembly functions to emulate */
int add_a(int a, int b);
int add2_a(int a, int b);
int mov_a(int a, int b);
int sub_a(int a, int b);
int cmp_a(int a, int b);
int ldr_a(int a, int b);
int str_a(int a, int b);
int test_a(int a, int b);

/* The complete machine state */
struct arm_state {
    unsigned int regs[NREGS];
    unsigned int cpsr;
    unsigned char stack[STACK_SIZE];
};

/* Initialize an arm_state struct with a function pointer and arguments */
void arm_state_init(struct arm_state *as, unsigned int *func,
                    unsigned int arg0, unsigned int arg1,
                    unsigned int arg2, unsigned int arg3)
{
    int i;

    /* Zero out all registers */
    for (i = 0; i < NREGS; i++) {
        as->regs[i] = 0;
    }

    /* Zero out CPSR */
    as->cpsr = 0;

    /* Zero out the stack */
    for (i = 0; i < STACK_SIZE; i++) {
        as->stack[i] = 0;
    }

    /* Set the PC to point to the address of the function to emulate */
    as->regs[PC] = (unsigned int) func;

    /* Set the SP to the top of the stack (the stack grows down) */
    as->regs[SP] = (unsigned int) &as->stack[STACK_SIZE];

    /* Initialize LR to 0, this will be used to determine when the function has called bx lr */
    as->regs[LR] = 0;

    /* Initialize the first 4 arguments */
    as->regs[0] = arg0;
    as->regs[1] = arg1;
    as->regs[2] = arg2;
    as->regs[3] = arg3;
}

void arm_state_print(struct arm_state *as)
{
    int i;

    for (i = 0; i < NREGS; i++) {
        printf("reg[%d] = %d\n", i, as->regs[i]);
    }
    printf("cpsr = %X\n", as->cpsr);
}

bool is_data_inst(unsigned int iw)
{
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 00);
}

bool is_mul_inst(unsigned int iw)
{
    /*
    unsigned int op;
    unsigned int op2;
    op = (iw >> 4) & 0xF;
    op2 = (iw >> 22) & 0b111111;
    return (op == 0b1001) && (accumu == 0b000000);
    */
    unsigned int op;
    unsigned int accumu;
    op = (iw >> 26) & 0b11;
    accumu = (iw >> 21) & 0b111;
    return (op == 00) && (accumu == 0b000);
}

bool is_memory_inst(unsigned int iw)
{
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 01);
}

bool is_b_inst(unsigned int iw)
{
    unsigned int b_code;

    b_code = (iw >> 25) & 0b111;

    return (b_code == 0b101);
}

bool is_bx_inst(unsigned int iw)
{
    unsigned int bx_code;

    bx_code = (iw >> 4) & 0x00FFFFFF;

    return (bx_code == 0b000100101111111111110001);
}


//check if the instruction is for register or immediate value
bool is_immediate(unsigned int iw)
{
    unsigned int immop;
    immop = (iw >> 25) & 0b1;
    return (immop == 1);
}

void armemu_cmp(struct arm_state *state, unsigned int iw, unsigned int rn, unsigned int op3)
{
    state->cpsr = 0;
    int rns, op3s, result;
    long long rnl, op3l;

    rns = (int) rn;
    op3s = (int) op3;
    rnl = (long long) rn;
    op3l = (long long) op3;

    result = rns - op3s;

    state->cpsr = (result < 0) ? (state->cpsr | (0b1 << 31)) : (state->cpsr | (0b0 << 31));
    state->cpsr = (result == 0) ? (state->cpsr | (0b1 << 30)) : (state->cpsr | (0b0 << 30));
    state->cpsr = (op3 > rn) ? (state->cpsr | (0b1 << 29)) : (state->cpsr | (0b0 << 29));
    state->cpsr = state->cpsr | (0b0 << 28);
    // (state->cpsr >> 31) & 0b1 = (result < 0); //cpsr->N
    // (state->cpsr >> 30) & 0b1 = (resul == 0); //cpsr->Z
    // (state->cpsr >> 29) & 0b1 = (op3 > rn);   //cpsr->C
    // (state->cpsr >> 28) & 0b1 = 0;            //cpsr->V
    if((rns > 0) && (op3s < 0)){
        if((rnl + op3l) > 0x7FFFFFFF){
            state->cpsr = state->cpsr | (0b1 << 28);
        }
    }else if((rns < 0) && (op3s > 0)){
        if((rnl + op3l) > 0x80000000){
            state->cpsr = state->cpsr | (0b1 << 28);
        }
    }
}

void armemu_mul(struct arm_state *state, unsigned int iw)
{
    unsigned int rd, rs, rm;

    rd = (iw >> 16) & 0xF;
    rs = (iw >> 8) & 0xF;
    rm = iw & 0xF;
    
    state->regs[rd] = state->regs[rs] * state->regs[rm];
    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
}
 
void armemu_data(struct arm_state *state, unsigned int iw)
{
    unsigned int rd, rn, rm, imm;
    unsigned int immediate, opcode, op3;

    rn = (iw >> 16) & 0xF;
    rd = (iw >> 12) & 0xF;
    rm = iw & 0xF;
    imm = iw & 0xFF;

    immediate = (iw >> 25) & 0xF; // 1->true, 0->false
    opcode = (iw >> 21) & 0xF;

    //check immediate
    op3 = immediate ? imm : state->regs[rm];
    if(opcode == 0b1101){
        state->regs[rd] = op3;
    }else if(opcode == 0b0100){
        state->regs[rd] = state->regs[rn] + op3;
    }else if(opcode == 0b0010){
        state->regs[rd] = state->regs[rn] - op3;
    }else if(opcode == 0b1010){
        armemu_cmp(state, iw, rn, op3); 
    }

    if(rd != PC){
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_b(struct arm_state *state)
{
    unsigned int iw;
    unsigned int cond;
    unsigned int offset;

    iw = *((unsigned int *) state->regs[PC]);
    offset = iw & 0xFFFFFF;
    cond = (iw >> 28) & 0xF;

    if(cond != 0b1110){ //check if cond is ignored
        if(cond == ((state->cpsr) >> 28 & 0xF)){ //check cond matches cpsr
            if (cond == 0b0000){ //beq
                if((iw >> 23) & 0b1 == 1){ //offset<0
                    offset = ~ offset + 1;
                    offset = - offset;
                }
                //offset>=0
                offset = offset * 4;
                state->regs[PC] = state->regs[PC] + (8 + offset);
            }
            else if (cond = 0b0001){ //bne
                if((iw >> 23) & 0b1 == 1){ //offset<0
                    offset = ~ offset + 1;
                    offset = - offset;
                }
                //offset>=0
                offset = offset * 4;
                state->regs[PC] = state->regs[PC] + (8 + offset);
            }
        }else{
            state->regs[PC] = state->regs[PC] + 4;
        }
    }else{ //cond is ignored, instruction always execute: bl, b
        if((iw >> 24) & 0b1 == 1){ //bl
        state->regs[LR] = state->regs[PC] + 4;//return to the next instruction after bl 
        }
        //b
        if((iw >> 23) & 0b1 == 1){ //offset<0
            offset = ~ offset + 1;
            offset = - offset;
        }
        //offset>=0
        offset = offset * 4;
        state->regs[PC] = state->regs[PC] + (8 + offset);
    }
        
    
}

void armemu_bx(struct arm_state *state)
{
    unsigned int iw;
    unsigned int rn;

    iw = *((unsigned int *) state->regs[PC]);
    rn = iw & 0b1111;

    state->regs[PC] = state->regs[rn];
}

void armemu_memory(struct arm_state *state, unsigned int iw)
{
    unsigned int rd, rn, rm, offset, target;
 
    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    //check i
    if (is_immediate(iw)){
        rm = iw & 0xF;
        offset = state->regs[rm]; 
    }else{
        offset = iw & 0xFFF;
    }
    //check u
    if((iw >> 23) & 0b1 == 1){
        target = state->regs[rn] + offset;
    }else{
        target = state->regs[rn] - offset;
    }
    printf("target = %d\n", target);
    //check l
    if ((iw >> 20) & 0b1 == 1){ //load
	//if((iw >> 23) & 0b1 == 1){ //add
	   // target = state->regs[rn] + offset;
	//}else{ //substract
	   // target = state->regs[rn] - offset;
        //}
        if((iw >> 22) & 0b1 == 1){ // ldrb
            state->regs[rd] = *((unsigned char*)target);
        }else{ //ldr
            state->regs[rd] = *((unsigned int*)target);
        }
    }else{ //str
	if((iw >> 22) & 0b1 == 1){ //strb
	    *((unsigned char*)target) = state->regs[rd];
        }else{ //str
	    *((unsigned int*)target) = state->regs[rd];
        }
        //target = state->regs[rd];
        //*((unsigned int*)(state->regs[rn] + offset)) = target;
    }

    if(rd != PC){
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_one(struct arm_state *state)
{
    unsigned int iw;
    
    iw = *((unsigned int *) state->regs[PC]);

    if (is_bx_inst(iw)) {
        armemu_bx(state);
    } else if (is_mul_inst(iw)){
        armemu_mul(state, iw);
    } else if (is_data_inst(iw)) {
        armemu_data(state, iw);
    } else if (is_memory_inst(iw)){
        armemu_memory(state, iw);
    }
}

unsigned int armemu(struct arm_state *state)
{
    /* Execute instructions until PC = 0 */
    /* This happens when bx lr is issued and lr is 0 */
    while (state->regs[PC] != 0) {
        armemu_one(state);
    }

    return state->regs[0];
}                
  
int main(int argc, char **argv)
{
    struct arm_state state;
    unsigned int r;
    arm_state_init(&state, (unsigned int *) test_a, 1, 2, 3, 4);
    //arm_state_print(&state);
    r = armemu(&state);
    printf("r0 = %d\n", r);
    arm_state_print(&state);
    return 0;

}
