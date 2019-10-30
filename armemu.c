#include <stdbool.h>
#include <stdio.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15

/* Assembly functions to emulate */
/*
int add_a(int a, int b);
int add2_a(int a, int b);
int mov_a(int a, int b);
int sub_a(int a, int b);
int cmp_a(int a, int b);
int ldr_a(int a, int b);
int str_a(int a, int b);
*/
//int test_a();
//int sum_array_a();
int cmp_a();
int test_b_a();
int sum_array_a();
int fib_rec_a();

/* The complete machine state */
struct arm_state {
    unsigned int regs[NREGS];
    //unsigned int cpsr;
    unsigned char stack[STACK_SIZE];
};

struct cpsr_state {
    int N;
    int Z;
    int C;
    int V;
};

void init_cpsr_state(struct cpsr_state *cpsr)
{
    cpsr->N = 0;
    cpsr->Z = 0;
    cpsr->C = 0;
    cpsr->V = 0;
}
void print_cpsr_state(struct cpsr_state *cpsr)
{
    printf("cpsr status:\n");
    printf("N = %d\n", cpsr->N);
    printf("Z = %d\n", cpsr->Z);
    printf("C = %d\n", cpsr->C);
    printf("V = %d\n", cpsr->V);
}

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
    printf("register status:\n");
    for (i = 0; i < NREGS; i++) {
        printf("reg[%d] = %d\n", i, as->regs[i]);
    } 
}

bool is_dp_inst(unsigned int iw)
{
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 0b00);
}

bool is_mul_inst(unsigned int iw)
{
    unsigned int op;
    unsigned int accumu;
    op = (iw >> 26) & 0b11;
    accumu = (iw >> 21) & 0b111;
    return (op == 0b00) && (accumu == 0b000);
}

bool is_mem_inst(unsigned int iw)
{
    unsigned int op;
    op = (iw >> 26) & 0b11;
    return (op == 0b01);
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
/*
//Check if the instruction is for register or immediate value
bool is_immediate(unsigned int iw)
{
    unsigned int immop;
    immop = (iw >> 25) & 0b1;
    return (immop == 0b1);
}
*/
//Set N,Z,C,V flags in cpsr_state
void armemu_cmp(struct arm_state *state, struct cpsr_state *cpsr, unsigned int rd, unsigned int a, unsigned int b) {
    int as, bs, result;
    long long al, bl;

    as = (int) a;
    bs = (int) b;
    al = (long long) a;
    bl = (long long) b;

    result = as - bs;
    printf("cmp_result = %d\n", result);

    cpsr->N = (result < 0);
    cpsr->Z = (result == 0);
    cpsr->C = (b > a);
    cpsr-> V = 0;
    if ((as > 0) && (bs < 0)) {
        if ((al + bl) > 0x7FFFFFFF) {
            cpsr->V = 1;
        }
    } else if ((as < 0) && (bs > 0)) {
        if ((al + bl) > 0x80000000) {
            cpsr->V = 1;
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
 
void armemu_dp(struct arm_state *state, struct cpsr_state *cpsr, unsigned int iw)
{
    unsigned int rd, rn, rm, imm;
    unsigned int immediate, opcode, op3;

    rn = (iw >> 16) & 0xF;
    rd = (iw >> 12) & 0xF;
    rm = iw & 0xF;
    imm = iw & 0xFF;

    immediate = (iw >> 25) & 0b1; // 1->true, 0->false
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
        //armemu_cmp(state, iw, rn, op3); 
	armemu_cmp(state, cpsr, rd, state->regs[rn], op3);
    }

    if(rd != PC){
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_b(struct arm_state *state, struct cpsr_state *cpsr, unsigned int iw)
{
    printf("branch\n");

    unsigned int cond;
    unsigned int offset;
 
    offset = iw & 0xFFFFFF;
    cond = (iw >> 28) & 0xF;
    printf("cond= %d\n", cond);
    //sign-extention
    if((iw >> 23) & 0b1 == 1){ //offset < 0
        offset = offset | 0xFF000000; 
    }
    offset = offset * 4;
    
    if(cond == 0b1110){ //cond is ignored : b and bl
    	if((iw >> 24) & 0b1 == 1){ //bl
	    printf("bl\n");
            state->regs[LR] = state->regs[PC] + 4;//return to the next instruction after bl  
	    state->regs[PC] = state->regs[PC] + (8 + offset);
	}else{ //b 
	    state->regs[PC] = state->regs[PC] + (8 + offset);
	    printf("b\n");
	}
	
    }else{
	//beq
    	if(cond == 0b0000){ 
	    if(cpsr->Z == 1){
		printf("beq\n");
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
                state->regs[PC] = state->regs[PC] + 4;
	    }
	//bne
	}else if(cond == 0b0001){
	    if(cpsr->Z == 0){
		printf("bne\n");
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//bgt
	}else if(cond == 0b1100){
	    if((cpsr->Z == 0) && (cpsr->N == cpsr->V)){
		printf("bgt\n");
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//bge
	}else if(cond == 0b1010){
	    if(cpsr->N == cpsr->V){
	        printf("bge\n");
		state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->regs[PC] = state->regs[PC] + 4;
	    }
	//blt
	}else if(cond == 0b1011){
	    if(cpsr->N != cpsr->V){
		printf("blt\n");
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//ble
	}else if(cond == 0b1101){
	    if((cpsr->Z == 1) && (cpsr->N != cpsr->V)){
	        printf("ble\n");
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
	        state->regs[PC] = state->regs[PC] + 4; 
	    }
	}
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

void armemu_mem(struct arm_state *state, unsigned int iw)
{
    unsigned int rd, rn, rm, imm, immediate,offset, target;
 
    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    rm = iw & 0xF;
    imm = iw & 0xFFF;
    immediate = (iw >> 25) & 0b1;
    //check i
    /*
    if (is_immediate(iw)){
        rm = iw & 0xF;
        offset = state->regs[rm]; 
    }else{
        offset = iw & 0xFFF;
    }
    */
    offset = immediate ? state->regs[rm] : imm; 
    //check u
    if((iw >> 23) & 0b1 == 1){
        target = state->regs[rn] + offset;
    }else{
        target = state->regs[rn] - offset;
    }
    printf("target = %d\n", target);
    //check l
    if ((iw >> 20) & 0b1 == 1){ //load     
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
    }

    if(rd != PC){
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_one(struct arm_state *state, struct cpsr_state *cpsr)
{
    unsigned int iw;
    
    iw = *((unsigned int *) state->regs[PC]);

    if (is_bx_inst(iw)) {
        armemu_bx(state);
    } else if (is_mul_inst(iw)){
        armemu_mul(state, iw);
    } else if (is_dp_inst(iw)) {
        armemu_dp(state, cpsr, iw);
    } else if (is_b_inst(iw)){
        armemu_b(state, cpsr, iw);
    } else if (is_mem_inst(iw)){
    	armemu_mem(state, iw);
    }
}


unsigned int armemu(struct arm_state *state, struct cpsr_state *cpsr)
{
    /* Execute instructions until PC = 0 */
    /* This happens when bx lr is issued and lr is 0 */
    while (state->regs[PC] != 0) {
        armemu_one(state, cpsr);
    }

    return state->regs[0];
<<<<<<< HEAD
}                

=======
}
/*
void test(struct armemu_state *state, struct cpsr_state *cpsr, unsigned int *func, unsigned int arg0, unsigned int arg1, unsigned int arg2, unsigned int arg3){
    unsigned int r;
    arm_state_init(&state, (unsigned int *)func, arg0, arg1, arg2, arg3);
    init_cpsr_state(&cpsr);
    r = armemu(&state, &cpsr);
    printf("result = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);
}
*/
>>>>>>> 0db509e0d1102320b78c4726ebf602c0d38b5e37
int main(int argc, char **argv)
{
    struct arm_state state;
    struct cpsr_state cpsr;
    unsigned int r;
    int arr[5] = {1,2,3,4,5};

    //test(&state, &cpsr, (unsigned int *)test_b_a, 3, 2, 0, 0);
/*
//quadratic test passed
    arm_state_init(&state, (unsigned int *) test_a, 1, 2, 3, 4);
    //arm_state_print(&state);
    r = armemu(&state);
    printf("quadratic = %d\n", r);
    arm_state_print(&state);
*/
    // sum_array_a.s 
    arm_state_init(&state, (unsigned int *) sum_array_a, (unsigned int) arr, 5, 0, 0);
    //arm_state_print(&state);
    init_cpsr_state(&cpsr);
    r = armemu(&state, &cpsr);
    printf("result = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);
    
    
/*
//cmp test passed
    arm_state_init(&state, (unsigned int *) cmp_a, 3, 2, 0, 0);
    init_cpsr_state(&cpsr);
    r = armemu(&state, &cpsr);
    printf("cmp(3,2) = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);


    arm_state_init(&state, (unsigned int *) cmp_a, 2, 3, 0, 0);
    init_cpsr_state(&cpsr);
    r = armemu(&state, &cpsr);
    printf("cmp(2,3) = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);

    arm_state_init(&state, (unsigned int *) cmp_a, 2, 2, 0, 0);
    r = armemu(&state, &cpsr);
    printf("cmp(2,2) = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);
*/
/*
    //beq test
    arm_state_init(&state, (unsigned int *) test_b_a, 2, 3, 0, 0);
    init_cpsr_state(&cpsr);
    r = armemu(&state, &cpsr);
    printf("beq(2,3) = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);
*/  
    //fib_rec test
    arm_state_init(&state, (unsigned int *) fib_rec_a, 9, 0, 0, 0);
    init_cpsr_state(&cpsr);
    r = armemu(&state, &cpsr);
    printf("fib(5) = %d\n", r);
    arm_state_print(&state);
    print_cpsr_state(&cpsr);

    return 0;
}
