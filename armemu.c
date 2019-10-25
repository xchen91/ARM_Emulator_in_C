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

// bool is_mov_inst(unsigned int iw)
// {
//     unsigned int op;
//     unsigned int opcode;

//     op = (iw >> 26) & 0b11;
//     opcode = (iw >> 21) & 0b1111;

//     return (op == 0) && (opcode == 0b1101);
// }

void armemu_mov(struct arm_state *state, unsigned int iw, unsigned int rd)
{
    unsigned int op2;

    if (is_immediate(iw)){
        op2 = iw & 0xFF;
        state->regs[rd] = op2;
    }else{
        op2 = iw & 0xF;
        state->regs[rd] = state->regs[op2];
    }
}

// bool is_add_inst(unsigned int iw)
// {
//     unsigned int op;
//     unsigned int opcode;

//     op = (iw >> 26) & 0b11;
//     opcode = (iw >> 21) & 0b1111;

//     return (op == 0) && (opcode == 0b0100);
// }

void armemu_add(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    unsigned int rm;

    iw = *((unsigned int *) state->regs[PC]);
    
    if (is_immediate(iw)){
        rm = iw & 0xFF;
        state->regs[rd] = state->regs[rn] + rm;
    }else{
    rm = iw & 0xF;
        state->regs[rd] = state->regs[rn] + state->regs[rm];
    }
}

// bool is_sub_inst(unsigned int iw)
// {
//     unsigned int op;
//     unsigned int opcode;

//     op = (iw >> 26) & 0b11;
//     opcode = (iw >> 21) & 0b1111;

//     return (op == 0) && (opcode == 0b0010);
// }

void armemu_sub(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    unsigned int rm;

    if (is_immediate(iw)){
        rm = iw & 0xFF;
        state->regs[rd] = state->regs[rn] - rm;
    }else{
        rm = iw & 0xF;
        state->regs[rd] = state->regs[rn] - state->regs[rm];
    }
}

// bool is_cmp_inst(unsigned int iw)
// {
//     unsigned int op;
//     unsigned int opcode;

//     op = (iw >> 26) & 0b11;
//     opcode = (iw >> 21) & 0b1111;

//     return (op == 0) && (opcode == 0b1010);
// }

void armemu_cmp(struct arm_state *state, unsigned int iw, unsigned int rd, unsigned int rn)
{
    unsigned int rm;
    unsigned int cond;

    state->cpsr = 0;

    if (is_immediate(iw)){
        rm = iw & 0xFF;
        cond = (unsigned int)(state->regs[rn] - rm);
    }else{
        rm = iw & 0xF;
        cond = (unsigned int)(state->regs[rn] - state->regs[rm]);
    }

    if(cond == 0){
        state->cpsr = state->cpsr | (0b0000 << 28);
    }else if(cond < 0){
        state->cpsr = state->cpsr | (0b1011 << 28);
    }else if(cond > 0){
        state->cpsr = state->cpsr | (0b1100 << 28);
    }
}

bool is_mul_inst(unsigned int iw)
{
    unsigned int op;
    unsigned int accumu;

    op = (iw >> 26) & 0b11;
    accumu = (iw >> 21) & 0b111;

    return (op == 0) && (accumu == 0b000);
}

void armemu_mul(struct arm_state *state, unsigned int iw)
{
    unsigned int rd, rn, rm;

    rd = (iw >> 16) & 0xF;
    rn = (iw >> 12) & 0xF;
    rm = iw & 0xF;
    
    state->regs[rd] = state->regs[rn] * state->regs[rm];
    if (rd != PC) {
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_data(struct arm_state *state, unsigned int iw)
{
    unsigned int rd, rn;
    unsigned int opcode;

    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    iw = *((unsigned int *) state->regs[PC]);
    opcode = (iw >> 21) & 0xF;
    
    if(opcode == 0b1101){
        armemu_mov(state, iw, rd);
    }else if(opcode == 0b0100){
        armemu_add(state, iw, rd, rn);
    }else if(opcode == 0b0010){
        armemu_sub(state, iw, rd, rn);
    }else if(opcode == 0b1010){
        armemu_cmp(state, iw, rd, rn);
    }

    if(rd != PC){
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_b(struct arm_state *state)
{
    unsigned int iw;
    unsigned int offset;

    iw = *((unsigned int *) state->regs[PC]);
    offset = iw & 0xFFFFFF;
}

// bool is_bx_inst(unsigned int iw)
// {
//     unsigned int bx_code;

//     bx_code = (iw >> 4) & 0x00FFFFFF;

//     return (bx_code == 0b000100101111111111110001);
// }

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
    if((iw >>23) & 0b1 == 1){
        target = state->regs[rn] + offset;
    }else{
        target = state->regs[rn] - offset;
    }

    //
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
    } else if (is_data_inst(iw)) {
        armemu_data(state, iw);
    } else if (is_mul_inst(iw)){
        armemu_mul(state, iw);
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

    /* Emulate add_a */
    arm_state_init(&state, (unsigned int *) add_a, 1, 2, 0, 0);
    arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(add_a(1,2)) = %d\n", r);

    /* Emulate add2_a */
    arm_state_init(&state, (unsigned int *) add2_a, 1, 2, 0, 0);
    arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(add2_a(1,2)) = %d\n", r);

    /* Emulate sub_a */
    arm_state_init(&state, (unsigned int *) sub_a, 3, 2, 0, 0);
    arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(sub_a(3,2)) = %d\n", r);

    /* Emulate mov_a.s */
    arm_state_init(&state, (unsigned int *) mov_a, 1, 2, 0, 0);
    arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(mov_a(1,2)) = %d\n", r);

    /* Emulate cmp_a.s */
    arm_state_init(&state, (unsigned int *) cmp_a, 1, 2, 0, 0);
    //arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(cmp_a(1,2)) = %d\n", r);
    arm_state_print(&state);

    int arr[5]={99999,2,3,4,5};
    /* Emulate ldr_a.s */
    arm_state_init(&state, (unsigned int *) ldr_a, 2, arr, 0, 0);
    //arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(ldr_a(1,2)) = %d\n", r);
    arm_state_print(&state);

    /* Emulate str_a.s */
    arm_state_init(&state, (unsigned int *) str_a, 1, arr, 0, 0);
    //arm_state_print(&state);
    r = armemu(&state);
    printf("armemu(str_a(1,2)) = %d\n", r);
    arm_state_print(&state);
    return 0;

}
