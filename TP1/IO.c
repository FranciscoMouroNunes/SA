// ----------------- IO.c
#undef MODBUS_DEBUG


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <modbus/modbus.h>


/* Inputs from process (i.e. sensors) */
bool BA = 0;
bool BSA = 0;
bool BSV = 0;
bool SS = 0;
bool SIE = 0;
bool SIR = 0;
bool SZ = 0;
bool ST = 0;
bool SC = 0; // fica inativo quando a caixa chega a sua posição sensor de caixa (SC: 0 quando caixa presente)
uint16_t SV = 0;



/* Outputs to process (i.e. actuators) */
bool  E1 = 0;
bool  E2 = 0;
bool  T1 = 0; // tapete principal
bool  T2 = 0; // tapete para levar a caixa da zona de seleção até à caixa (pode ser usado para expulsar a caixa, se necessário)
bool  IP = 0;
bool  MZ = 0;
bool  LP = 0;
bool  LA = 0;
bool  LV = 0;
uint16_t CC = 0;



// Modbus variables
#define IP_ADDR "127.0.0.1"
#define PORT 5502
#define MAX_D_INPUTS 9
#define MAX_D_OUTPUTS 9
#define MAX_R_INPUTS 1
#define MAX_R_OUTPUTS 1

modbus_t *mb = NULL;
int modbus_result = 0;
uint8_t tab_d_inputs[MAX_D_INPUTS];
uint8_t tab_d_outputs[MAX_D_OUTPUTS];
uint16_t tab_r_inputs[MAX_R_INPUTS];
uint16_t tab_r_outputs[MAX_R_OUTPUTS];

// Modbus functions
int mb_init();
void mb_shutdown();
int mb_read_I_D();
int mb_write_Q_D();
int mb_read_I_R();
int mb_write_Q_R();

// Functions
int read_inputs();
int write_outputs();
void sleep_rel(uint64_t);
void sleep_abs(uint64_t);
uint64_t get_time();



// -- CONTROLLER INTERFACE ---

int read_inputs() {
	
	modbus_result = mb_read_I_D();
	modbus_result =  mb_read_I_R() || modbus_result;
	return modbus_result;
}

int write_outputs() {
	
	modbus_result = mb_write_Q_D();
	modbus_result = mb_write_Q_R() || modbus_result ;
	
	return modbus_result;
}

// ---- TIME -----

// Sleep relative number of milliseconds       
void sleep_rel(uint64_t milliseconds) {
  uint seconds = milliseconds / 1000;
  milliseconds -= seconds*1000;
  struct timespec period = {seconds,               //tv_sec;  /* seconds */
                            milliseconds*1000*1000 //tv_nsec; /* nanoseconds */
                           };
                           
  //struct timespec start, end;
  
  clock_nanosleep(CLOCK_MONOTONIC, 0, &period, NULL);
}



// Sleep absolute number of milliseconds       
void sleep_abs(uint64_t milliseconds) {
  static struct timespec next_wakeup = {0, 0};
  
  uint seconds = milliseconds / 1000;
  milliseconds -= seconds*1000;
  
  // initialise next_wakeup if it is the first time this function is called
  if ((next_wakeup.tv_sec == 0) && (next_wakeup.tv_nsec == 0))
    clock_gettime(CLOCK_MONOTONIC, &next_wakeup);
  
  // determine next absolute time to wake up
  next_wakeup.tv_sec  += seconds;
  next_wakeup.tv_nsec += milliseconds * 1000 * 1000;
  if (next_wakeup.tv_nsec >= 1000 * 1000 * 1000) {
      next_wakeup.tv_nsec -= 1000 * 1000 * 1000;
      next_wakeup.tv_sec  += 1;
  }
                             
  clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_wakeup, NULL);
}



// return time in milliseconds
uint64_t get_time(void) {
  struct timespec now;
  
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t) (now.tv_sec*1000) + (now.tv_nsec/1000/1000);  
}
      



// ---- MODBUS ------------------------------------------------------------------------------------------------------------------------

void mb_shutdown(void) {
  modbus_close(mb);
  modbus_free(mb);
}

int mb_init(void) {
  // create new structure
  if ((mb = modbus_new_tcp(IP_ADDR, PORT)) == NULL) return 1;
  
  // set UID
  modbus_set_slave(mb, 1);
  
  // setup clean_up function
  if (atexit(mb_shutdown) < 0) return 1;
  
  // establish connection  
  return modbus_connect(mb);
}


int mb_read_I_D(void) {
	
	/* Read input discretes */
	
	if (MAX_D_INPUTS > 0)
	{
	
		if (mb == NULL)  mb_init();
	  
		#ifdef MODBUS_DEBUG
		printf("Read Inputs\n");
		#endif
		
		modbus_result= modbus_read_input_bits(mb, 0, MAX_D_INPUTS, tab_d_inputs);
		
	  
		if (modbus_result < 0) return modbus_result;
	  
		/* MAPPING */
		BA = tab_d_inputs[0];
		BSA = tab_d_inputs[1];
		BSV = tab_d_inputs[2];
		SC = tab_d_inputs[3];
		SIE = tab_d_inputs[4];
		SIR = tab_d_inputs[5];
		SS = tab_d_inputs[6];
		ST = tab_d_inputs[7];
		SZ = tab_d_inputs[8];
	}
	  
  return 0;  
}



int mb_write_Q_D(void) {
  
  	/* Write coils */
	
	if (MAX_D_OUTPUTS > 0)
	{
		
		if (mb == NULL)  mb_init();
	
		#ifdef MODBUS_DEBUG
		printf("Write Coils\n");
		#endif
		
		
		/* MAPPING */
		tab_d_outputs[0] = E1;
		tab_d_outputs[1] = E2;
		tab_d_outputs[2] = IP;
		tab_d_outputs[3] = LA;
		tab_d_outputs[4] = LP;
		tab_d_outputs[5] = LV;
		tab_d_outputs[6] = MZ;
		tab_d_outputs[7] = T1;
		tab_d_outputs[8] = T2;
		
		
		modbus_result = modbus_write_bits(mb, 0, MAX_D_OUTPUTS, tab_d_outputs);		
		
		if (modbus_result < 0) return modbus_result;
		
		/* MAPPING */
	}
	
 return 0;  
}

int mb_read_I_R(void) {
	
	/* Read input registers */
	
	if (MAX_R_INPUTS > 0)
	{
	
		if (mb == NULL)  mb_init();
	  
		#ifdef MODBUS_DEBUG
		printf("Read Input Registers\n");
		#endif
		
		modbus_result= modbus_read_input_registers(mb, 0, MAX_R_INPUTS, tab_r_inputs);
		
		SV = tab_r_inputs[0];
		
		if (modbus_result < 0) return modbus_result;
		
		/* MAPPING */
	}
  
  return 0;  
}



int mb_write_Q_R(void) {
  
  	/* Write holding registers */
	
	if (MAX_R_OUTPUTS > 0)
	{
	
		#ifdef MODBUS_DEBUG
		printf("Write Holding Registers\n");
		#endif
		
		
		/* MAPPING */
		tab_r_outputs[0]=CC;
		
		modbus_result= modbus_write_registers(mb, 0, MAX_R_OUTPUTS, tab_r_outputs);
		
		if (modbus_result < 0) return modbus_result;
	
	}
 return 0; 
}