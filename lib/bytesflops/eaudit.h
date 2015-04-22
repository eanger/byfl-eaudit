#ifndef EAUDIT_H
#define EAUDIT_H

extern "C" {
  void EAUDIT_init();
  void EAUDIT_push();
  void EAUDIT_pop(const char* func_name);
  void EAUDIT_shutdown();
}

void timeout(int signum);
void reset_rapl();
double rapl_energy_multiplier();
long long get_rapl_energy();
void do_init(int* eventset);
void do_push();
void do_pop(const char* func_name);
void do_shutdown();

#endif // EAUDIT_H
