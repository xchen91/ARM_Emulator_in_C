#include <stdbool.h>
#include <stdio.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15

/* Assembly functions to emulate */
int quadratic_c(int x, int a, int b, int c);
int quadratic_a(int x, int a, int b, int c);
int sum_array_c(int *array, int n);
int sum_array_a(int *array, int n);
int find_max_c(int *array, int n);
int find_max_a(int *array, int n);
int fib_iter_c(int n);
int fib_iter_a(int n);
int fib_rec_c(int n);
int fib_rec_a(int n);
int strlen_c(char *s);
int strlen_a(char *s);


/* The complete machine state */
struct arm_state {
    unsigned int regs[NREGS];
    //unsigned int cpsr;
    unsigned char stack[STACK_SIZE];
    int num_inst;
    int num_dp_inst;
    int num_b_inst;
    int num_mem_inst;
    int num_b_taken;
    int num_b_ntaken;
};

struct cpsr_state {
    int N;
    int Z;
    int C;
    int V;
};

struct cache_slot{
    int v;
    unsigned int tag;
};

struct cache_direct_mapped{
    struct cache_slot *slots;
    int requests;
    int hits;
    int misses;
};

int size;
/*
void cache_direct_mapped_init(struct cache_direct_mapped *dmc)
{
	int i;

	dmc->slots = (struct cache_slot) malloc(sizeof(struct cache_slot) * size);
	
	for(i = 0; i < size; i++){
	    dmc->slots[i].requests = 0;
	    dmc->slots[i].hits = 0;
	    dmc->slots[i].misses = 0;
	}

}
*/

void init_cpsr_state(struct cpsr_state *cpsr)
{
    cpsr->N = 0;
    cpsr->Z = 0;
    cpsr->C = 0;
    cpsr->V = 0;
}

/*
void print_cpsr_state(struct cpsr_state *cpsr)
{
    printf("cpsr status:\n");
    printf("N = %d\n", cpsr->N);
    printf("Z = %d\n", cpsr->Z);
    printf("C = %d\n", cpsr->C);
    printf("V = %d\n", cpsr->V);
}
*/

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
    
    as->num_inst = 0;
    as->num_dp_inst = 0;
    as->num_b_inst = 0;
    as->num_mem_inst = 0;
    as->num_b_taken = 0;
    as->num_b_ntaken = 0;

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
    printf("Register Status:\n");
    for (i = 0; i < NREGS; i++) {
        printf("reg[%d] = %d\n", i, as->regs[i]);
    } 

    printf("Dynamic Analysis:\n");
    printf("Number of intructions executed: %d\n", as->num_inst);
    printf("Number of data prosessing inst: %d (%d %)\n", as->num_dp_inst, (100 * as->num_dp_inst) / as->num_inst);
    printf("Number of memory inst: %d (%d %)\n", as->num_mem_inst, (100 * as->num_mem_inst) / as->num_inst);
    printf("Number of branch inst: %d (%d %)\n", as->num_b_inst, (100 * as->num_b_inst) / as->num_inst);
    printf("Number of branches taken: %d\n", as->num_b_taken);
    printf("Number of branches not taken: %d\n", as->num_b_ntaken);
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

//Set N,Z,C,V flags in cpsr_state
void armemu_cmp(struct arm_state *state, struct cpsr_state *cpsr, unsigned int rd, unsigned int a, unsigned int b) {
    int as, bs, result;
    long long al, bl;

    as = (int) a;
    bs = (int) b;
    al = (long long) a;
    bl = (long long) b;

    result = as - bs;
    //printf("cmp_result = %d\n", result);

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
	armemu_cmp(state, cpsr, rd, state->regs[rn], op3);
    }

    if(rd != PC){
        state->regs[PC] = state->regs[PC] + 4;
    }
}

void armemu_b(struct arm_state *state, struct cpsr_state *cpsr, unsigned int iw)
{
    unsigned int cond;
    unsigned int offset;
 
    offset = iw & 0xFFFFFF;
    cond = (iw >> 28) & 0xF;
    //sign-extention
    if((iw >> 23) & 0b1 == 1){ //offset < 0
        offset = offset | 0xFF000000; 
    }
    offset = offset * 4;
    
    if(cond == 0b1110){ //cond is ignored : b and bl
    	if((iw >> 24) & 0b1 == 1){ //bl
	    //printf("bl\n");
	    state->num_b_taken++;
            state->regs[LR] = state->regs[PC] + 4;//return to the next instruction after bl  
	    state->regs[PC] = state->regs[PC] + (8 + offset);
	}else{ //b 
	    state->num_b_taken++;
	    state->regs[PC] = state->regs[PC] + (8 + offset);
	    //printf("b\n");
	}
	
    }else{
	//beq
    	if(cond == 0b0000){ 
	    if(cpsr->Z == 1){
		//printf("beq\n");
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
                state->regs[PC] = state->regs[PC] + 4;
	    }
	//bne
	}else if(cond == 0b0001){
	    if(cpsr->Z == 0){
		//printf("bne\n");
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//bgt
	}else if(cond == 0b1100){
	    if((cpsr->Z == 0) && (cpsr->N == cpsr->V)){
		//printf("bgt\n");
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//bge
	}else if(cond == 0b1010){
	    if(cpsr->N == cpsr->V){
	        //printf("bge\n");
		state->num_b_taken++;
		state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
		state->regs[PC] = state->regs[PC] + 4;
	    }
	//blt
	}else if(cond == 0b1011){
	    if(cpsr->N != cpsr->V){
		//printf("blt\n");
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//ble
	}else if(cond == 0b1101){
	    if((cpsr->Z == 1) && (cpsr->N != cpsr->V)){
	        //printf("ble\n");
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
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
    
    state->num_b_taken++;
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
    offset = immediate ? state->regs[rm] : imm; 
    //check u
    if((iw >> 23) & 0b1 == 1){
        target = state->regs[rn] + offset;
    }else{
        target = state->regs[rn] - offset;
    }
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
	state->num_b_inst++;
    } else if (is_mul_inst(iw)){
        armemu_mul(state, iw);
	state->num_dp_inst++;
    } else if (is_dp_inst(iw)) {
        armemu_dp(state, cpsr, iw);
	state->num_dp_inst++;
    } else if (is_b_inst(iw)){
        armemu_b(state, cpsr, iw);
	state->num_b_inst++;
    } else if (is_mem_inst(iw)){
    	armemu_mem(state, iw);
	state->num_mem_inst++;
    }
}


unsigned int armemu(struct arm_state *state, struct cpsr_state *cpsr)
{
    /* Execute instructions until PC = 0 */
    /* This happens when bx lr is issued and lr is 0 */
    while (state->regs[PC] != 0) {
        armemu_one(state, cpsr);
	state->num_inst++;
    }
    return state->regs[0];
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
int main(int argc, char **argv)
{
    struct arm_state state;
    struct cpsr_state cpsr;
    int c, a;
    unsigned int emu;
    int arr[5] = {1,2,3,4,5};
    char *str = "Hello, world!";

/*
    //quadratic test:
    arm_state_init(&state, (unsigned int *) quadratic_a, 1, 2, 3, 4);
    init_cpsr_state(&cpsr);
    c = quadratic_c(1,2,3,4);
    a = quadratic_a(1,2,3,4);
    emu = armemu(&state, &cpsr);
    printf("quadratic\n");
    printf("quadratic_c(x=1,a=2,b=3,c=4) = %d\n", c);
    printf("quadratic_a(x=1,a=2,b=3,c=4) = %d\n", a);
    printf("emu(quadratic(x=1,a=2,b=3,c=4)) = %d\n", emu);
    arm_state_print(&state);
    //print_cpsr_state(&cpsr);
*/

/*
    // sum_array test:
    arm_state_init(&state, (unsigned int *) sum_array_a, (unsigned int) arr, 5, 0, 0);
    init_cpsr_state(&cpsr);
    c = sum_array_c(arr, 5);
    a = sum_array_a(arr, 5);
    emu = armemu(&state, &cpsr);
    printf("sum_array\n");
    printf("sum_array_c({1,2,3,4,5}) = %d\n", c);
    printf("sum_array_a({1,2,3,4,5}) = %d\n", a);
    printf("emu(sum_array({1,2,3,4,5})) = %d\n", emu);
    arm_state_print(&state);
    //print_cpsr_state(&cpsr);
*/

/*   
    //find_max test:
    arm_state_init(&state, (unsigned int *) find_max_a, (unsigned int) arr, 5, 0, 0);
    init_cpsr_state(&cpsr);
    c = find_max_c(arr, 5);
    a = find_max_a(arr, 5);
    emu = armemu(&state, &cpsr);
    printf("find_max\n");
    printf("find_max_c({1,2,3,4,5}) = %d\n", c);
    printf("find_max_a({1,2,3,4,5}) = %d\n", a);
    printf("emu(find_max({1,2,3,4,5})) = %d\n", emu);
    arm_state_print(&state);
    //print_cpsr_state(&cpsr);
*/

/*
    //fib_iter test:
    arm_state_init(&state, (unsigned int *) fib_iter_a, 5, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = fib_iter_c(5);
    a = fib_iter_a(5);
    emu = armemu(&state, &cpsr);
    printf("fib_iter\n");
    printf("fib_iter_c(5) = %d\n", c);
    printf("fib_iter_a(5) = %d\n", a);
    printf("emu(fib_iter(5)) = %d\n", emu);
    arm_state_print(&state);
    //print_cpsr_state(&cpsr); 
*/
   
/* 
    //fib_rec test:
    arm_state_init(&state, (unsigned int *) fib_rec_a, 9, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = fib_rec_c(9);
    a = fib_rec_a(9);
    emu = armemu(&state, &cpsr);
    printf("fib_rec\n");
    printf("fib_rec_c(9) = %d\n", c);
    printf("fib_rec_a(9) = %d\n", a);
    printf("emu(fib_rec(9)) = %d\n", emu);
    arm_state_print(&state);
    //print_cpsr_state(&cpsr); 
*/
    
    
    //strlen test:
    arm_state_init(&state, (unsigned int *) strlen_a, (unsigned int) str, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = strlen_c(str);
    a = strlen_a(str);
    emu = armemu(&state, &cpsr);
    printf("strlen\n");
    printf("strlen_c(Hello, World!) = %d\n", c);
    printf("strlen_a(Hello, World!) = %d\n", a);
    printf("emu(strlen(Hello, World!)) = %d\n", emu);
    arm_state_print(&state);
    //print_cpsr_state(&cpsr);
    
    
    return 0;
}
