#include "eaudit.h"

#include <map>
#include <stack>
#include <string>
#include <iostream>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <cstdio>

#include "papi.h"

namespace{
const unsigned kSleepSecs = 30;
const double kNanoToBase = 1e-9;
}

static std::stack<long long> cur_energy;
static std::map<std::string, long long> total_energy;

void EAUDIT_init(){ return; }
void EAUDIT_push(){ do_push(); }
void EAUDIT_pop(const char* func_name){ do_pop(func_name); }
void EAUDIT_shutdown(){ do_shutdown(); }

int* get_eventset(){
  static int eventset = PAPI_NULL;
  if(eventset == PAPI_NULL){
    do_init(&eventset);
  }
  return &eventset;
}

void do_init(int* eventset){
  printf("init\n");
  if(signal(SIGALRM, timeout) == SIG_ERR){
    fprintf(stderr, "Unable to set signal handler.\n");
    exit(-1);
  }

  int retval;
  if ( ( retval = PAPI_library_init( PAPI_VER_CURRENT ) ) != PAPI_VER_CURRENT ){
    fprintf(stderr, "Unable to init PAPI library.\n");
    exit(-1);
  }
  retval = PAPI_create_eventset( eventset ); 
  if ( retval != PAPI_OK  ){
    std::cerr << "Unable to create PAPI eventset." << std::endl;
    fprintf(stderr, "Unable to create PAPI eventset.\n");
    exit(-1);
  }
  retval=PAPI_add_named_event(*eventset,"rapl:::PACKAGE_ENERGY:PACKAGE0");
  if (retval != PAPI_OK){
    fprintf(stderr, "Unable to add RAPL PACKAGE_ENERGY event.\n");
    PAPI_perror(NULL);
    exit(-1);
  }
}

void do_push(){
  printf("push\n");
  cur_energy.push(0ULL);
  reset_rapl();
}

void do_pop(const char* func_name){
  printf("pop\n");
  timeout(SIGALRM);
  total_energy[func_name] += cur_energy.top();
  cur_energy.pop();
}

void do_shutdown(){
  printf("shutdown\n");
  std::cout << "Energy Profile:" << std::endl;
  for(auto& func : total_energy){
    std::cout << func.first << ":\t" << func.second * kNanoToBase
              << " joules" << std::endl;
  }
}

void timeout(int signum){
  if(signum == SIGALRM){
    cur_energy.top() += get_rapl_energy();
    reset_rapl();
  }
}

void reset_rapl(){
  // reset interrupt timer
  struct itimerval work_time;
  work_time.it_value.tv_sec = kSleepSecs;
  work_time.it_value.tv_usec = 0;
  work_time.it_interval.tv_sec = 0;
  work_time.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &work_time, nullptr);
  
  // reset rapl counter
  int retval;
  retval = PAPI_reset( *get_eventset() );
  if ( retval != PAPI_OK ) {
    PAPI_perror( NULL );
    exit(-1);
  }

  // start counting again
  retval = PAPI_start( *get_eventset() );
  if ( retval != PAPI_OK ) {
    std::cerr << "Unable to start PAPI." << std::endl;
    exit(-1);
  }
}

long long get_rapl_energy(){
  long long energy_val;
  int retval = PAPI_stop( *get_eventset(), &energy_val );
  if ( retval != PAPI_OK ) {
    std::cerr << "Unable to stop RAPL." << std::endl;
    exit(-1);
  }
  return energy_val;
}

