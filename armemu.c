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

int cache_size = 8;
/*
void cache_direct_mapped_init(struct cache_direct_mapped *dmc)
{
	int i;

	dmc->slots = (struct cache_slot) malloc(sizeof(struct cache_slot) * cache_size);
	
	for(i = 0; i < size; i++){
	    dmc->requests = 0;
	    dmc->hits = 0;
	    dmc->misses = 0;
	}
}
*/

void cache_print(struct cache_direct_mapped *dmc)
{
    printf("Cache:\n");
    printf("Number of requests:\n");
    printf("Number of hits: (hit ratio: )\n");
    printf("Number of misses: (miss ratio: )\n");
}

void init_cpsr_state(struct cpsr_state *cpsr)
{
    cpsr->N = 0;
    cpsr->Z = 0;
    cpsr->C = 0;
    cpsr->V = 0;
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
	    state->num_b_taken++;
            state->regs[LR] = state->regs[PC] + 4;//return to the next instruction after bl  
	    state->regs[PC] = state->regs[PC] + (8 + offset);
	}else{ //b 
	    state->num_b_taken++;
	    state->regs[PC] = state->regs[PC] + (8 + offset);
	}
	
    }else{
	//beq
    	if(cond == 0b0000){ 
	    if(cpsr->Z == 1){
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
                state->regs[PC] = state->regs[PC] + 4;
	    }
	//bne
	}else if(cond == 0b0001){
	    if(cpsr->Z == 0){
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//bgt
	}else if(cond == 0b1100){
	    if((cpsr->Z == 0) && (cpsr->N == cpsr->V)){
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//bge
	}else if(cond == 0b1010){
	    if(cpsr->N == cpsr->V){
		state->num_b_taken++;
		state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
		state->regs[PC] = state->regs[PC] + 4;
	    }
	//blt
	}else if(cond == 0b1011){
	    if(cpsr->N != cpsr->V){
		state->num_b_taken++;
	        state->regs[PC] = state->regs[PC] + (8 + offset);
	    }else{
		state->num_b_ntaken++;
	        state->regs[PC] = state->regs[PC] + 4;
	    }
	//ble
	}else if(cond == 0b1101){
	    if((cpsr->Z == 1) && (cpsr->N != cpsr->V)){
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

int main(int argc, char **argv)
{
    struct arm_state state;
    struct cpsr_state cpsr;
    struct cache_direct_mapped dmc;

    int c, a;
    unsigned int emu;

    printf("-------quadratic_test-------\n");
    arm_state_init(&state, (unsigned int *) quadratic_a, 2, 3, 4, 5);
    init_cpsr_state(&cpsr);
    c = quadratic_c(2,3,4,5);
    a = quadratic_a(2,3,4,5);
    emu = armemu(&state, &cpsr);
    printf("test1 {2, 3, 4, 5}:\n");
    printf("quadratic_c(2, 3, 4, 5) = %d\n", c);
    printf("quadratic_a(2, 3, 4, 5) = %d\n", a);
    printf("emu(quadratic(2, 3, 4, 5)) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    arm_state_init(&state, (unsigned int *) quadratic_a, -1, -2, -3, -4);
    init_cpsr_state(&cpsr);
    c = quadratic_c(-1,-2,-3,-4);
    a = quadratic_a(-1,-2,-3,-4);
    emu = armemu(&state, &cpsr);
    printf("test2 {-1, -2, -3, -4}:\n");
    printf("quadratic_c(-1, -2, -3, -4) = %d\n", c);
    printf("quadratic_a(-1, -2, -3, -4) = %d\n", a);
    printf("emu(quadratic(-1, -2, -3, -4)) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    arm_state_init(&state, (unsigned int *) quadratic_a, 0, 2, 3, 4);
    init_cpsr_state(&cpsr);
    c = quadratic_c(0, 2, 3, 4);
    a = quadratic_a(0, 2, 3, 4);
    emu = armemu(&state, &cpsr);
    printf("test3 {0, 2, 3, 4}:\n");
    printf("quadratic_c(0, 2, 3, 4) = %d\n", c);
    printf("quadratic_a(0, 2, 3, 4) = %d\n", a);
    printf("emu(quadratic(0, 2, 3, 4)) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    arm_state_init(&state, (unsigned int *) quadratic_a, 0, -2, -3, 2);
    init_cpsr_state(&cpsr);
    c = quadratic_c(0, -2, -3, 2);
    a = quadratic_a(0, -2, -3, 2);
    emu = armemu(&state, &cpsr);
    printf("test4 {0, -2, -3, 2}:\n");
    printf("quadratic_c(0, -2, -3, 2) = %d\n", c);
    printf("quadratic_a(0, -2, -3, 2) = %d\n", a);
    printf("emu(quadratic(0, -2, -3, 2)) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");
    
    printf("-------sum_array_test-------\n");
    int arr1[] = {2, 3, 4, 5};
    arm_state_init(&state, (unsigned int *) sum_array_a, (unsigned int) arr1, 4, 0, 0);
    init_cpsr_state(&cpsr);
    c = sum_array_c(arr1, 4);
    a = sum_array_a(arr1, 4);
    emu = armemu(&state, &cpsr);
    printf("test1 {2, 3, 4, 5}:\n");
    printf("sum_array_c(2, 3, 4, 5) = %d\n", c);
    printf("sum_array_a(2, 3, 4, 5) = %d\n", a);
    printf("emu(sum_array{2, 3, 4, 5}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    int arr2[] = {0, 2, -4, 12, -63};
    arm_state_init(&state, (unsigned int *) sum_array_a, (unsigned int) arr2, 5, 0, 0);
    init_cpsr_state(&cpsr);
    c = sum_array_c(arr2, 5);
    a = sum_array_a(arr2, 5);
    emu = armemu(&state, &cpsr);
    printf("test2 {0, 2, -4, 12, -63}:\n");
    printf("sum_array_c(0, 2, -4, 12, -63) = %d\n", c);
    printf("sum_array_a(0, 2, -4, 12, -63) = %d\n", a);
    printf("emu(sum_array{0, 2, -4, 12, -63}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    int arr3[] = {-1, -2, -3, -4, 1, 2, 3, 4};
    arm_state_init(&state, (unsigned int *) sum_array_a, (unsigned int) arr3, 8, 0, 0);
    init_cpsr_state(&cpsr);
    c = sum_array_c(arr3, 8);
    a = sum_array_a(arr3, 8);
    emu = armemu(&state, &cpsr);
    printf("test3 {-1, -2, -3, -4, 1, 2, 3, 4}:\n");
    printf("sum_array_c(-1, -2, -3, -4, 1, 2, 3, 4) = %d\n", c);
    printf("sum_array_a(-1, -2, -3, -4, 1, 2, 3, 4) = %d\n", a);
    printf("emu(sum_array{-1, -2, -3, -4, 1, 2, 3, 4}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    int arr4[1001];
    for (int i = 0; i < 1001; i++){
        arr4[i] = i;
    }
    arm_state_init(&state, (unsigned int *) sum_array_a, (unsigned int) arr4, 1001, 0, 0);
    init_cpsr_state(&cpsr);
    c = sum_array_c(arr4, 1001);
    a = sum_array_a(arr4, 1001);
    emu = armemu(&state, &cpsr);
    printf("test4 {0, 1, 2, 3, 4...999, 1000}:\n");
    printf("sum_array_c(0, 1, 2, 3, 4...999, 1000) = %d\n", c);
    printf("sum_array_a(0, 1, 2, 3, 4...999, 1000) = %d\n", a);
    printf("emu(sum_array{0, 1, 2, 3, 4...999, 1000}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    printf("-------find_max_test-------\n");
    int arr5[] = {6, 7, 8, 9};
    arm_state_init(&state, (unsigned int *) find_max_a, (unsigned int) arr5, 4, 0, 0);
    init_cpsr_state(&cpsr);
    c = find_max_c(arr5, 4);
    a = find_max_a(arr5, 4);
    emu = armemu(&state, &cpsr);
    printf("test1 {6, 7, 8, 9}:\n");
    printf("find_max_c(6, 7, 8, 9) = %d\n", c);
    printf("find_max_a(6, 7, 8, 9) = %d\n", a);
    printf("emu(find_max{6, 7, 8, 9}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    int arr6[] = {-5, -6, -7, -8};
    arm_state_init(&state, (unsigned int *) find_max_a, (unsigned int) arr6, 4, 0, 0);
    init_cpsr_state(&cpsr);
    c = find_max_c(arr6, 4);
    a = find_max_a(arr6, 4);
    emu = armemu(&state, &cpsr);
    printf("test2 {-5, -6, -7, -8}:\n");
    printf("find_max_c(-5, -6, -7, -8) = %d\n", c);
    printf("find_max_a(-5, -6, -7, -8) = %d\n", a);
    printf("emu(find_max{-5, -6, -7, -8}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    int arr7[] = {-13, -26, 0, 23, 45, 108};
    arm_state_init(&state, (unsigned int *) find_max_a, (unsigned int) arr7, 6, 0, 0);
    init_cpsr_state(&cpsr);
    c = find_max_c(arr7, 6);
    a = find_max_a(arr7, 6);
    emu = armemu(&state, &cpsr);
    printf("test3 {-13, -26, 0, 23, 45, 108}:\n");
    printf("find_max_c(-13, -26, 0, 23, 45, 108) = %d\n", c);
    printf("find_max_a(-13, -26, 0, 23, 45, 108) = %d\n", a);
    printf("emu(find_max{-13, -26, 0, 23, 45, 108}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    int arr8[1001];
    for (int i = 0; i < 1001; i++){
        arr8[i] = i - 1000;
    }
    arm_state_init(&state, (unsigned int *) find_max_a, (unsigned int) arr8, 1001, 0, 0);
    init_cpsr_state(&cpsr);
    c = find_max_c(arr8, 1001);
    a = find_max_a(arr8, 1001);
    emu = armemu(&state, &cpsr);
    printf("test4 {-1000, -999. -998...-3, -2, -1, 0}:\n");
    printf("sum_array_c(-1000, -999. -998...-3, -2, -1, 0) = %d\n", c);
    printf("sum_array_a(-1000, -999. -998...-3, -2, -1, 0) = %d\n", a);
    printf("emu(sum_array{-1000, -999. -998...-3, -2, -1, 0}) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    printf("-------fib_iter_test-------\n");
    printf("This test will print the first 20 fibonacci numbers generated by using itertive way.\n");
    for(int i = 0; i < 20; i++){
      int c = fib_iter_c(i);
      printf("fib_iter_c(%d) = %d\n", i, c);
    }
    printf("\n");
    for(int i = 0; i < 20; i++){
      int a = fib_iter_a(i);
      printf("fib_iter_a(%d) = %d\n", i, a);
    }
     printf("\n");

    for(int i = 0; i < 20; i++){
        arm_state_init(&state, (unsigned int *) fib_iter_a, (unsigned int) i, 0, 0, 0);
        init_cpsr_state(&cpsr);
        emu = armemu(&state, &cpsr);
        printf("emu(fib_iter(%d)) = %d\n", i, emu);
    }
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");
   
    printf("-------fib_rec_test-------\n");
    printf("This test will print the first 20 fibonacci numbers generated by using recursive way.\n");
    for(int i = 0; i < 20; i++){
      int c = fib_rec_c(i);
      printf("fib_rec_c(%d) = %d\n", i, c);
    }
    printf("\n");
    for(int i = 0; i < 20; i++){
      int a = fib_rec_a(i);
      printf("fib_rec_a(%d) = %d\n", i, a);
    }
     printf("\n");

    for(int i = 0; i < 20; i++){
        arm_state_init(&state, (unsigned int *) fib_rec_a, (unsigned int) i, 0, 0, 0);
        init_cpsr_state(&cpsr);
        emu = armemu(&state, &cpsr);
        printf("emu(fib_rec(%d)) = %d\n", i, emu); 
    }
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n"); 
    
    printf("-------strlen_test-------\n");
    printf("test 1{hello}:\n");
    char arr9[] = {"hello"};
    arm_state_init(&state, (unsigned int *) strlen_a, (unsigned int) arr9, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = strlen_c(arr9);
    a = strlen_a(arr9);
    emu = armemu(&state, &cpsr);
    printf("strlen\n");
    printf("strlen_c(hello) = %d\n", c);
    printf("strlen_a(hello) = %d\n", a);
    printf("emu(strlen(hello)) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    printf("test 2{Hello World!}:\n");
    char arr10[] = {"Hello World!"};
    arm_state_init(&state, (unsigned int *) strlen_a, (unsigned int) arr10, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = strlen_c(arr10);
    a = strlen_a(arr10);
    emu = armemu(&state, &cpsr);
    printf("strlen\n");
    printf("strlen_c(Hello World!) = %d\n", c);
    printf("strlen_a(Hello World!) = %d\n", a);
    printf("emu(strlen(Hello World!)) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    printf("test 3{'A', 's', 's', 'e', 'm', 'b', 'l', 'y'}:\n");
    char arr11[] = {'A', 's', 's', 'e', 'm', 'b', 'l', 'y', '\0'};
    arm_state_init(&state, (unsigned int *) strlen_a, (unsigned int) arr11, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = strlen_c(arr11);
    a = strlen_a(arr11);
    emu = armemu(&state, &cpsr);
    printf("strlen\n");
    printf("strlen_c('A', 's', 's', 'e', 'm', 'b', 'l', 'y') = %d\n", c);
    printf("strlen_a('A', 's', 's', 'e', 'm', 'b', 'l', 'y') = %d\n", a);
    printf("emu(strlen('A', 's', 's', 'e', 'm', 'b', 'l', 'y')) = %d\n", emu);
    arm_state_print(&state);
    cache_print(&dmc);
    printf("\n");

    printf("test 4{ARM Assembly Programming}:\n");
    char arr12[] = {"ARM Assembly Programming"};
    arm_state_init(&state, (unsigned int *) strlen_a, (unsigned int) arr12, 0, 0, 0);
    init_cpsr_state(&cpsr);
    c = strlen_c(arr12);
    a = strlen_a(arr12);
    emu = armemu(&state, &cpsr);
    printf("strlen\n");
    printf("strlen_c(ARM Assembly Programming) = %d\n", c);
    printf("strlen_a(ARM Assembly Programming) = %d\n", a);
    printf("emu(strlen(ARM Assembly Programming)) = %d\n", emu);
    arm_state_print(&state); 
    cache_print(&dmc);  
    printf("\n");  
    return 0;
}
