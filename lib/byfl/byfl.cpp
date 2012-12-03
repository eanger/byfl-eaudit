/*
 * Helper library for computing bytes:flops ratios
 * (core functions)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Pat McCormick <pat@lanl.gov>
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <locale>

#include "cachemap.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

const unsigned int HDR_COL_WIDTH = 20;

// Define the different ways a basic block can terminate.
typedef enum {
  BB_NOT_END=0,       // Basic block has not actually terminated.
  BB_END_UNCOND=1,    // Basic block terminated with an unconditional branch.
  BB_END_COND=2       // Basic block terminated with a conditional branch.
} bb_end_t;


// Encapsulate of all of our counters into a single structure.
class ByteFlopCounters {
public:
  uint64_t loads;                // Number of bytes loaded
  uint64_t stores;               // Number of bytes stored

  // TODO: We might want to gahter type counters into an array-based
  // set of counters to clean the code up a bit... 
  uint64_t load_ins;             // Number of load instructions executed
  uint64_t load_float_ins;       // Number of single-precision floating point load "instructions" 
  uint64_t load_double_ins;      // Number of double-precision floating point load "instructions" 
  uint64_t load_int8_ins;        // Number of 8-bit integer load "instructions"
  uint64_t load_int16_ins;       // Number of 16-bit integer load "instructions"
  uint64_t load_int32_ins;       // Number of 32-bit integer load "instructions"
  uint64_t load_int64_ins;       // Number of 64-bit integer load "instructions"
  uint64_t load_ptr_ins;         // Number of pointer load "instructions"
  uint64_t load_other_type_ins;  // Number of "other" data types load "instructions"

  uint64_t store_ins;            // Number of store instructions executed
  uint64_t store_float_ins;      // Number of single-precision floating point store "instructions" 
  uint64_t store_double_ins;     // Number of double-precision floating point store "instructions" 
  uint64_t store_int8_ins;       // Number of 8-bit integer store "instructions"
  uint64_t store_int16_ins;      // Number of 16-bit integer store "instructions"
  uint64_t store_int32_ins;      // Number of 32-bit integer store "instructions"
  uint64_t store_int64_ins;      // Number of 64-bit integer store "instructions"
  uint64_t store_ptr_ins;        // Number of pointer store "instructions"
  uint64_t store_other_type_ins; // Number of "other" data types store "instructions"

  uint64_t flops;                // Number of floating-point operations performed
  uint64_t fp_bits;              // Number of bits consumed or produced by all FP operations
  uint64_t ops;                  // Number of operations of any type performed
  uint64_t op_bits;              // Number of bits consumed or produced by any operation
  uint64_t cond_brs;             // Number of conditional branches performed
  uint64_t b_blocks;             // Number of basic blocks executed

  // Initialize all of the counters.
  ByteFlopCounters (uint64_t initial_loads=0,            uint64_t initial_stores=0,
                    uint64_t initial_load_ins=0, 
		    uint64_t initial_load_float_ins=0,   uint64_t initial_load_double_ins=0,
		    uint64_t initial_load_int8_ins=0,    uint64_t initial_load_int16_ins=0, 
		    uint64_t initial_load_int32_ins=0,   uint64_t initial_load_int64_ins=0, 
		    uint64_t initial_load_ptr_ins=0,     uint64_t initial_load_other_type_ins=0, 
		    uint64_t initial_store_ins=0, 
		    uint64_t initial_store_float_ins=0,  uint64_t initial_store_double_ins=0,
		    uint64_t initial_store_int8_ins=0,   uint64_t initial_store_int16_ins=0, 
		    uint64_t initial_store_int32_ins=0,  uint64_t initial_store_int64_ins=0, 
		    uint64_t initial_store_ptr_ins=0,    uint64_t initial_store_other_type_ins=0, 
                    uint64_t initial_flops=0,            uint64_t initial_fp_bits=0,
                    uint64_t initial_ops=0,              uint64_t initial_op_bits=0,
                    uint64_t initial_cbrs=0,             uint64_t initial_b_blocks=0) {

    loads                = initial_loads;
    stores               = initial_stores;

    load_ins             = initial_load_ins;
    load_float_ins       = initial_load_float_ins;
    load_double_ins      = initial_load_double_ins;
    load_int8_ins        = initial_load_int8_ins;
    load_int16_ins       = initial_load_int16_ins;
    load_int32_ins       = initial_load_int32_ins;
    load_int64_ins       = initial_load_int64_ins;
    load_ptr_ins         = initial_load_ptr_ins;
    load_other_type_ins  = initial_load_other_type_ins;

    store_ins            = initial_store_ins;
    store_float_ins      = initial_store_float_ins;
    store_double_ins     = initial_store_double_ins;
    store_int8_ins       = initial_store_int8_ins;
    store_int16_ins      = initial_store_int16_ins;
    store_int32_ins      = initial_store_int32_ins;
    store_int64_ins      = initial_store_int64_ins;
    store_ptr_ins        = initial_store_ptr_ins;
    store_other_type_ins = initial_store_other_type_ins;

    flops                = initial_flops;
    fp_bits              = initial_fp_bits;
    ops                  = initial_ops;
    op_bits              = initial_op_bits;
    cond_brs             = initial_cbrs;
    b_blocks             = initial_b_blocks;
  }

  // Accumulate new values into our counters.
  void accumulate (uint64_t more_loads,            uint64_t more_stores,
                   uint64_t more_load_ins, 
		   uint64_t more_load_float_ins,   uint64_t more_load_double_ins,
		   uint64_t more_load_int8_ins,    uint64_t more_load_int16_ins, 
		   uint64_t more_load_int32_ins,   uint64_t more_load_int64_ins, 
		   uint64_t more_load_ptr_ins,     uint64_t more_load_other_type_ins, 
		   uint64_t more_store_ins,
		   uint64_t more_store_float_ins,  uint64_t more_store_double_ins,
		   uint64_t more_store_int8_ins,   uint64_t more_store_int16_ins, 
		   uint64_t more_store_int32_ins,  uint64_t more_store_int64_ins, 
		   uint64_t more_store_ptr_ins,    uint64_t more_store_other_type_ins, 
                   uint64_t more_flops,            uint64_t more_fp_bits,
                   uint64_t more_ops,              uint64_t more_op_bits,
                   uint64_t more_cbrs,             uint64_t more_b_blocks) {

    loads += more_loads;
    stores += more_stores;

    load_ins             += more_load_ins;
    load_float_ins       += more_load_float_ins;
    load_double_ins      += more_load_double_ins;
    load_int8_ins        += more_load_int8_ins;
    load_int16_ins       += more_load_int16_ins;
    load_int32_ins       += more_load_int32_ins;
    load_int64_ins       += more_load_int64_ins;
    load_ptr_ins         += more_load_ptr_ins;
    load_other_type_ins  += more_load_other_type_ins;

    store_ins            += more_store_ins;
    store_float_ins      += more_store_float_ins;
    store_double_ins     += more_store_double_ins;
    store_int8_ins       += more_store_int8_ins;
    store_int16_ins      += more_store_int16_ins;
    store_int32_ins      += more_store_int32_ins;
    store_int64_ins      += more_store_int64_ins;
    store_ptr_ins        += more_store_ptr_ins;
    store_other_type_ins += more_store_other_type_ins;

    flops                += more_flops;
    fp_bits              += more_fp_bits;
    ops                  += more_ops;
    op_bits              += more_op_bits;
    cond_brs             += more_cbrs;
    b_blocks             += more_b_blocks;
  }

  // Accumulate another counter's values into our counters.
  void accumulate (ByteFlopCounters* other) {

    loads                += other->loads;
    stores               += other->stores;

    load_ins             += other->load_ins;
    load_float_ins       += other->load_float_ins;
    load_double_ins      += other->load_double_ins;
    load_int8_ins        += other->load_int8_ins;
    load_int16_ins       += other->load_int16_ins;
    load_int32_ins       += other->load_int32_ins;
    load_int64_ins       += other->load_int64_ins;
    load_ptr_ins         += other->load_ptr_ins;
    load_other_type_ins  += other->load_other_type_ins;

    store_ins            += other->store_ins;
    store_float_ins      += other->store_float_ins;
    store_double_ins     += other->store_double_ins;
    store_int8_ins       += other->store_int8_ins;
    store_int16_ins      += other->store_int16_ins;
    store_int32_ins      += other->store_int32_ins;
    store_int64_ins      += other->store_int64_ins;
    store_ptr_ins        += other->store_ptr_ins;
    store_other_type_ins += other->store_other_type_ins;

    flops                += other->flops;
    fp_bits              += other->fp_bits;
    ops                  += other->ops;
    op_bits              += other->op_bits;
    cond_brs             += other->cond_brs;
    b_blocks             += other->b_blocks;
  }

  // Return the difference of our counters and another set of counters.
  ByteFlopCounters* difference (ByteFlopCounters* other) {
    return new ByteFlopCounters(loads - other->loads,
                                stores - other->stores,

                                load_ins - other->load_ins,
				load_float_ins - other->load_float_ins, 
				load_double_ins - other->load_double_ins,
				load_int8_ins - other->load_int8_ins,
				load_int16_ins - other->load_int16_ins,
				load_int32_ins - other->load_int32_ins,
				load_int64_ins - other->load_int64_ins,
				load_ptr_ins - other->load_ptr_ins,
				load_other_type_ins - other->load_other_type_ins,

                                store_ins - other->store_ins,
				store_float_ins - other->store_float_ins,
                                store_double_ins - other->store_double_ins,
                                store_int8_ins - other->store_int8_ins,
				store_int16_ins - other->store_int16_ins,
				store_int32_ins - other->store_int32_ins,
				store_int64_ins - other->store_int64_ins,
				store_ptr_ins - other->store_ptr_ins,
				store_other_type_ins - other->store_other_type_ins,

                                flops - other->flops,
                                fp_bits - other->fp_bits,
                                ops - other->ops,
                                op_bits - other->op_bits,
                                cond_brs - other->cond_brs,
                                b_blocks - other->b_blocks);
  }

  // Reset all of our counters to zero.
  void reset (void) {
    loads                = 0;
    stores               = 0;

    load_ins             = 0;
    load_float_ins       = 0;
    load_double_ins      = 0;
    load_int8_ins        = 0;
    load_int16_ins       = 0;
    load_int32_ins       = 0;
    load_int64_ins       = 0;
    load_ptr_ins         = 0;
    load_other_type_ins  = 0;

    store_ins            = 0;
    store_float_ins      = 0;
    store_double_ins     = 0;
    store_int8_ins       = 0;
    store_int16_ins      = 0;
    store_int32_ins      = 0;
    store_int64_ins      = 0;
    store_ptr_ins        = 0;
    store_other_type_ins = 0;

    flops                = 0;
    fp_bits              = 0;
    ops                  = 0;
    op_bits              = 0;
    cond_brs             = 0;
    b_blocks             = 0;
 }

};

// The following values get reset at the end of every basic block.
__thread uint64_t bf_load_count                 = 0;   // Tally of the number of bytes loaded
__thread uint64_t bf_store_count                = 0;   // Tally of the number of bytes stored

__thread uint64_t bf_load_ins_count             = 0;   // Tally of the number of load instructions performed
__thread uint64_t bf_load_float_ins_count       = 0;   // Tally of the number of single-precision load instructions performed
__thread uint64_t bf_load_double_ins_count      = 0;   // Tally of the number of double-precision load instructions performed
__thread uint64_t bf_load_int8_ins_count        = 0;   // Tally of the number of int8 load instructions performed
__thread uint64_t bf_load_int16_ins_count       = 0;   // Tally of the number of int16 load instructions performed
__thread uint64_t bf_load_int32_ins_count       = 0;   // Tally of the number of int32 load instructions performed
__thread uint64_t bf_load_int64_ins_count       = 0;   // Tally of the number of int64 load instructions performed
__thread uint64_t bf_load_ptr_ins_count         = 0;   // Tally of the number of pointer load instructions performed
__thread uint64_t bf_load_other_type_ins_count  = 0;   // Tally of the number of other type load instructions performed

__thread uint64_t bf_store_ins_count            = 0;   // Tally of the number of store instructions performed
__thread uint64_t bf_store_float_ins_count      = 0;   // Tally of the number of single-precision store instructions performed
__thread uint64_t bf_store_double_ins_count     = 0;   // Tally of the number of double-precision store instructions performed
__thread uint64_t bf_store_int8_ins_count       = 0;   // Tally of the number of int8 store instructions performed
__thread uint64_t bf_store_int16_ins_count      = 0;   // Tally of the number of int16 store instructions performed
__thread uint64_t bf_store_int32_ins_count      = 0;   // Tally of the number of int32 store instructions performed
__thread uint64_t bf_store_int64_ins_count      = 0;   // Tally of the number of int64 store instructions performed
__thread uint64_t bf_store_ptr_ins_count        = 0;   // Tally of the number of pointer store instructions performed
__thread uint64_t bf_store_other_type_ins_count = 0;   // Tally of the number of other type store instructions performed                   

__thread uint64_t bf_flop_count                 = 0;   // Tally of the number of FP operations performed
__thread uint64_t bf_fp_bits_count              = 0;   // Tally of the number of bits used by all FP operations
__thread uint64_t bf_op_count                   = 0;   // Tally of the number of operations performed
__thread uint64_t bf_op_bits_count              = 0;   // Tally of the number of bits used by all operations

// The following values represent more persistent counter and other state.
static uint64_t num_merged = 0;    // Number of basic blocks merged so far
static ByteFlopCounters global_totals;  // Global tallies of all of our counters
static ByteFlopCounters prev_global_totals;  // Previously reported global tallies of all of our counters

// Keep track of counters on a per-function basis, being careful to
// work around the "C++ static initialization order fiasco" (cf. the
// C++ FAQ).
typedef CachedUnorderedMap<const char*, ByteFlopCounters*> str2bfc_t;
typedef str2bfc_t::iterator counter_iterator;
static str2bfc_t& per_func_totals()
{
  static str2bfc_t* mapping = new str2bfc_t();
  return *mapping;
}
typedef CachedUnorderedMap<const char*, uint64_t> str2num_t;
static str2num_t& func_call_tallies()
{
  static str2num_t* mapping = new str2num_t();
  return *mapping;
}

// Keep track of counters on a user-defined basis, being careful to
// work around the "C++ static initialization order fiasco" (cf. the
// C++ FAQ).
static str2bfc_t& user_defined_totals()
{
  static str2bfc_t* mapping = new str2bfc_t();
  return *mapping;
}

// bf_categorize_counters() is intended to be overridden by a
// user-defined function.
extern "C" {
const char* bf_categorize_counters (void) __attribute__((weak));
const char* bf_categorize_counters (void)
{
  return NULL;
}
}

// The following constants are defined by the instrumented code.
extern uint64_t bf_bb_merge;    // Number of basic blocks to merge to compress the output
extern uint8_t bf_all_ops;      // 1=bf_op_count and bf_op_bits_count are valid
extern uint8_t bf_types;        // 1=enables bf_all_ops and count loads/stores per type 
extern uint8_t bf_per_func;     // 1=Tally and output per-function data
extern uint8_t bf_call_stack;   // 1=Maintain a function call stack
extern uint8_t bf_unique_bytes; // 1=Tally and output unique bytes
extern uint8_t bf_vectors;      // 1=Bin then output vector characteristics

namespace bytesflops {

const char* bf_func_and_parents;   // Top of the complete_call_stack stack

extern const char* bf_string_to_symbol (const char *nonunique);
extern void bf_report_vector_operations (size_t call_stack_depth);
extern void bf_get_vector_statistics(uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits);
extern void bf_get_vector_statistics(const char* tag, uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits);
extern void bf_get_reuse_distance (vector<uint64_t>** hist, uint64_t* unique_addrs);
extern void bf_get_median_reuse_distance (uint64_t* median_value, uint64_t* mad_value);


// Define a stack of tallies of all of our counters across <=
// num_merged basic blocks.  This *ought* to be a top-level variable
// defintion, but because of the "C++ static initialization order
// fiasco" (cf. the C++ FAQ) we have to use the following roundabout
// approach.
typedef vector<ByteFlopCounters*> counter_vector_t;
static counter_vector_t& bb_totals (void)
{
  static counter_vector_t* bfc = new counter_vector_t();
  return *bfc;
}

// Define a memory pool for ByteFlopCounters.
class CounterMemoryPool {
private:
  vector<ByteFlopCounters*> freelist;
public:
  // Allocate a new ByteFlopCounters -- from the free list if possible.
  ByteFlopCounters* allocate(void) {
    ByteFlopCounters* newBFC;
    if (freelist.size() > 0) {
      newBFC = freelist.back();
      newBFC->reset();
      freelist.pop_back();
    }
    else
      newBFC = new ByteFlopCounters();
    return newBFC;
  }

  // Return a ByteFlopCounters to the free list.
  void deallocate(ByteFlopCounters* oldBFC) {
    freelist.push_back(oldBFC);
  }
};
static CounterMemoryPool* counter_memory_pool = NULL;

// Maintain a function call stack.
class CallStack {
private:
  vector<const char*> complete_call_stack;  // Stack of function and ancestor names
public:
  size_t max_depth;   // Maximum depth achieved by complete_call_stack

  CallStack() {
    bf_func_and_parents = "-";
    max_depth = 0;
  }

  // Push a function name onto the stack.  Return a string containing
  // the name of the function followed by the names of all of its
  // ancestors.
  const char* push_function (const char* funcname) {
    // Push both the current function name and the combined name of the
    // function and its call stack.
    static char* combined_name = NULL;
    static size_t len_combined_name = 0;
    const char* unique_combined_name;      // Interned version of combined_name
    size_t current_stack_depth = complete_call_stack.size();
    if (current_stack_depth == 0) {
      // First function on the call stack
      len_combined_name = strlen(funcname) + 1;
      combined_name = (char*) malloc(len_combined_name);
      strcpy(combined_name, funcname);
      max_depth = 1;
    }
    else {
      // All other calls (the common case)
      const char* ancestors_names = complete_call_stack.back();
      size_t length_needed = strlen(funcname) + strlen(ancestors_names) + 2;
      if (len_combined_name < length_needed) {
        len_combined_name = length_needed*2;
        combined_name = (char*) realloc(combined_name, len_combined_name);
      }
      sprintf(combined_name, "%s %s", funcname, ancestors_names);
      current_stack_depth++;
      if (current_stack_depth > max_depth)
        max_depth = current_stack_depth;
    }
    unique_combined_name = bf_string_to_symbol(combined_name);
    complete_call_stack.push_back(unique_combined_name);
    return unique_combined_name;
  }

  // Pop a function name from the call stack and return the new top of
  // the call stack (function + ancestors).
  const char* pop_function (void) {
    complete_call_stack.pop_back();
    if (complete_call_stack.size() > 0)
      return complete_call_stack.back();
    else
      return "[EMPTY]";
  }
};
static CallStack* call_stack = NULL;

// As a kludge, set a global variable indicating that all of the
// constructors in this file have been called.  Because of the "C++
// static initialization order fiasco" (cf. the C++ FAQ) we otherwise
// have no guarantee that, in particular, cout (from iostream) has
// been initialized.  If cout hasn't been called, we can force
// suppress_output() to return true until it is.
static bool all_constructors_called = false;
static class CheckConstruction {
public:
  CheckConstruction() {
    all_constructors_called = true;
  }
} check_construction;


extern void initialize_byfl(void);
extern void initialize_reuse(void);
extern void initialize_symtable(void);
extern void initialize_threading(void);
extern void initialize_ubytes(void);
extern void initialize_vectors(void);
extern void bf_push_basic_block (void);
extern uint64_t bf_tally_unique_addresses (void);
extern uint64_t bf_tally_unique_addresses (const char* funcname);


// Initialize some of our variables at first use.
void initialize_byfl (void) {
  call_stack = new CallStack();
  counter_memory_pool = new CounterMemoryPool();
  bf_push_basic_block();
}


// Initialize on first use all top-level variables in all files.  This
// is a kludge to work around the "C++ static initialization order
// fiasco" (cf. the C++ FAQ).  bf_initialize_if_necessary() can safely
// be called multiple times.
void bf_initialize_if_necessary (void)
{
  static bool initialized = false;
  if (!__builtin_expect(initialized, true)) {
    initialize_byfl();
    initialize_reuse();
    initialize_symtable();
    initialize_threading();
    initialize_ubytes();
    initialize_vectors();
    initialized = true;
  }
}


// Push a new basic block onto the stack (before a function call).
void bf_push_basic_block (void)
{
  bb_totals().push_back(counter_memory_pool->allocate());
}


// Pop and discard the top basic block off the stack (after a function
// returns).
void bf_pop_basic_block (void)
{
  counter_memory_pool->deallocate(bb_totals().back());
  bb_totals().pop_back();
}


// Tally the number of calls to each function.
void bf_incr_func_tally (const char* funcname)
{
  const char* unique_name = bf_string_to_symbol(funcname);
  func_call_tallies()[unique_name]++;
}


// Push a function name onto the call stack.  Increment the invocation
// count the call stack as a whole, and ensure the individual function
// name also exists in the hash table.
void bf_push_function (const char* funcname)
{
  const char* unique_combined_name = call_stack->push_function(funcname);
  bf_func_and_parents = unique_combined_name;
  func_call_tallies()[bf_func_and_parents]++;
  const char* unique_name = bf_string_to_symbol(funcname);
  func_call_tallies()[unique_name] += 0;
}


// Pop the top function name from the call stack.
void bf_pop_function (void)
{
  bf_func_and_parents = call_stack->pop_function();
}


// Determine if we should suppress output from this process.
static bool suppress_output (void)
{
  if (!all_constructors_called)
    // If cout hasn't been constructed, force all output to be suppressed.
    return true;
  static enum {UNKNOWN, SUPPRESS, SHOW} output = UNKNOWN;
  if (output == UNKNOWN) {
    // First invocation -- parse the BYFL_OUTPUT_IF environment variable.
    char *value = getenv("BYFL_OUTPUT_IF");
    if (value) {
      // The variable is defined -- ensure that it's of the form VAR=VALUE.
      string output_if(value);
      size_t eqpos = output_if.find_first_of('=');
      if (eqpos == string::npos) {
        cerr << "Failed to parse \"" << value << "\" into VAR=VALUE\n";
        exit(1);
      }
      string output_var = output_if.substr(0, eqpos);
      string output_value = output_if.substr(eqpos+1);
      string actual_value("");
      if ((value = getenv(output_var.c_str())))
        actual_value = value;
      output = output_value == actual_value ? SHOW : SUPPRESS;
    }
    else
      // The variable isn't defined.  Show all output.
      output = SHOW;
  }
  return output == SUPPRESS;
}


// Accumulate the current counter variables (bf_*_count) into the
// current basic block's counters.  At the end of the basic block,
// also transfer the current basic block's counters to the global
// counters.
void bf_accumulate_bb_tallies (bb_end_t end_of_basic_block)
{
  // Add the current values to the per-BB totals.
  ByteFlopCounters* current_bb = bb_totals().back();
  current_bb->accumulate(bf_load_count, bf_store_count,
                         bf_load_ins_count, 
			 bf_load_float_ins_count, 
			 bf_load_double_ins_count,
			 bf_load_int8_ins_count,
			 bf_load_int16_ins_count,
			 bf_load_int32_ins_count,
			 bf_load_int64_ins_count,
			 bf_load_ptr_ins_count, 
			 bf_load_other_type_ins_count, 

			 bf_store_ins_count,
                         bf_store_float_ins_count,
                         bf_store_double_ins_count,
                         bf_store_int8_ins_count,
                         bf_store_int16_ins_count,
                         bf_store_int32_ins_count,
                         bf_store_int64_ins_count,
			 bf_store_ptr_ins_count, 
			 bf_store_other_type_ins_count, 

                         bf_flop_count, bf_fp_bits_count,
                         bf_op_count, bf_op_bits_count,
                         uint64_t(end_of_basic_block == BB_END_COND),
                         uint64_t(end_of_basic_block != BB_NOT_END));

  // At the end of the basic block, transfer all values to the global
  // counters and, if defined, a user-specified set of counters.
  if (end_of_basic_block != BB_NOT_END) {
    global_totals.accumulate(current_bb);
    const char* partition = bf_string_to_symbol(bf_categorize_counters());
    if (partition != NULL) {
      counter_iterator sm_iter = user_defined_totals().find(partition);
      if (sm_iter == per_func_totals().end())
        user_defined_totals()[partition] = new ByteFlopCounters(*current_bb);
      else
        user_defined_totals()[partition]->accumulate(current_bb);
    }
  }
}

// Reset the current basic block's tallies rather than requiring a
// push and a pop for every basic block.
void bf_reset_bb_tallies (void)
{
  bb_totals().back()->reset();
}

// Report what we've measured for the current basic block.
void bf_report_bb_tallies (void)
{
  static bool showed_header = false;         // true=already output our header

  // Do nothing if our output is suppressed.
  if (suppress_output())
    return;

  // If this is our first invocation, output a basic-block header line.
  if (__builtin_expect(!showed_header, 0)) {
    cout << "BYFL_BB_HEADER: "
         << setw(HDR_COL_WIDTH) << "Bytes_LD" << ' '
         << setw(HDR_COL_WIDTH) << "Bytes_ST" << ' '
         << setw(HDR_COL_WIDTH) << "Ops_LD" << ' '
         << setw(HDR_COL_WIDTH) << "Ops_ST" << ' '
         << setw(HDR_COL_WIDTH) << "Flops" << ' '
         << setw(HDR_COL_WIDTH) << "FP_bits";
    if (bf_all_ops) {
      cout << ' '
           << setw(HDR_COL_WIDTH) << "Int_Ops" << ' '
           << setw(HDR_COL_WIDTH) << "Int_Op_bits";
      if (bf_types) {
	cout << ' ' 
	     << setw(HDR_COL_WIDTH) << "Flt_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Dbl_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I8_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I16_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I32_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I64_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Ptr_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Other_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Flt_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "Dbl_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I8_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I16_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I32_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I64_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "Ptr_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "Other_ST";
      }
    }
    cout << '\n';
    showed_header = true;
  }

  // If we've accumulated enough basic blocks, output the aggregate of
  // their values.
  if (++num_merged >= bf_bb_merge) {
    // Output the difference between the current counter values and
    // our previously saved values.
    ByteFlopCounters* counter_deltas = global_totals.difference(&prev_global_totals);
    cout << "BYFL_BB:        "
         << setw(HDR_COL_WIDTH) << counter_deltas->loads << ' '
         << setw(HDR_COL_WIDTH) << counter_deltas->stores << ' '
         << setw(HDR_COL_WIDTH) << counter_deltas->load_ins << ' '
         << setw(HDR_COL_WIDTH) << counter_deltas->store_ins << ' '
         << setw(HDR_COL_WIDTH) << counter_deltas->flops << ' '
         << setw(HDR_COL_WIDTH) << counter_deltas->fp_bits;
    if (bf_all_ops) {
      cout << ' '
           << setw(HDR_COL_WIDTH) << counter_deltas->ops << ' '
           << setw(HDR_COL_WIDTH) << counter_deltas->op_bits;
      if (bf_types) {
	cout << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_float_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_double_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_int8_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_int16_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_int32_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_int64_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_ptr_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->load_other_type_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_float_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_double_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_int8_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_int16_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_int32_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_int64_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_ptr_ins << ' '
	     << setw(HDR_COL_WIDTH) << counter_deltas->store_other_type_ins;
      }
    }
    cout << '\n';
    num_merged = 0;
    prev_global_totals = global_totals;
    delete counter_deltas;
  }
}


// Associate the current counter values with a given function.
void bf_assoc_counters_with_func (const char* funcname, bb_end_t end_of_basic_block)
{
  // Ensure that per_func_totals contains an ByteFlopCounters entry
  // for funcname, then add the current counters to that entry.
  if (bf_call_stack)
    funcname = bf_func_and_parents;
  else
    funcname = bf_string_to_symbol(funcname);
  counter_iterator sm_iter = per_func_totals().find(funcname);
  if (sm_iter == per_func_totals().end())
    // This is the first time we've seen this function name.
    per_func_totals()[funcname] =
      new ByteFlopCounters(bf_load_count,             bf_store_count,

                           bf_load_ins_count, 
			   bf_load_float_ins_count,   bf_load_double_ins_count,
			   bf_load_int8_ins_count,    bf_load_int16_ins_count,
			   bf_load_int32_ins_count,   bf_load_int64_ins_count,
			   bf_load_ptr_ins_count,     bf_load_other_type_ins_count,

			   bf_store_ins_count,
			   bf_store_float_ins_count,  bf_store_double_ins_count,
			   bf_store_int8_ins_count,   bf_store_int16_ins_count,
			   bf_store_int32_ins_count,  bf_store_int64_ins_count,
			   bf_store_ptr_ins_count,    bf_store_other_type_ins_count,

                           bf_flop_count,             bf_fp_bits_count,
                           bf_op_count,               bf_op_bits_count,
                           uint64_t(end_of_basic_block == BB_END_COND),
                           uint64_t(end_of_basic_block != BB_NOT_END));
  else {
    // Accumulate the current counter values into those associated
    // with an existing function name.
    ByteFlopCounters* func_counters = sm_iter->second;
    func_counters->accumulate(bf_load_count,             bf_store_count,
                              bf_load_ins_count, 
			      bf_load_float_ins_count,   bf_load_double_ins_count,
			      bf_load_int8_ins_count,    bf_load_int16_ins_count,
			      bf_load_int32_ins_count,   bf_load_int64_ins_count,
			      bf_load_ptr_ins_count,     bf_load_other_type_ins_count,

			      bf_store_ins_count,
			      bf_store_float_ins_count,  bf_store_double_ins_count,
			      bf_store_int8_ins_count,   bf_store_int16_ins_count,
			      bf_store_int32_ins_count,  bf_store_int64_ins_count,
			      bf_store_ptr_ins_count,    bf_store_other_type_ins_count,

                              bf_flop_count,             bf_fp_bits_count,
                              bf_op_count,               bf_op_bits_count,
                              uint64_t(end_of_basic_block == BB_END_COND),
                              uint64_t(end_of_basic_block != BB_NOT_END));
  }
}


// At the end of the program, report what we measured.
static class RunAtEndOfProgram {
private:
  string separator;    // Horizontal rule to output between sections

  // Compare two strings.
  static bool compare_char_stars (const char* one, const char* two) {
    return strcmp(one, two) < 0;
  }

  // Compare two function names, reporting which was called more
  // times.  Break ties by comparing function names.
  static bool compare_func_totals (const char* one, const char* two) {
    uint64_t one_calls = func_call_tallies()[one];
    uint64_t two_calls = func_call_tallies()[two];
    if (one_calls != two_calls)
      return one_calls > two_calls;
    else
      return compare_char_stars(one, two);
  }

  // Report per-function counter totals.
  void report_by_function (void) {
    // Output a header line.
    cout << "BYFL_FUNC_HEADER: "
         << setw(HDR_COL_WIDTH) << "Bytes_LD" << ' '
         << setw(HDR_COL_WIDTH) << "Bytes_ST" << ' '
         << setw(HDR_COL_WIDTH) << "Ops_LD" << ' '
         << setw(HDR_COL_WIDTH) << "Ops_ST" << ' '
         << setw(HDR_COL_WIDTH) << "Flops" << ' '
         << setw(HDR_COL_WIDTH) << "FP_bits";
    if (bf_all_ops) {
      cout << ' '
           << setw(HDR_COL_WIDTH) << "Int_Ops" << ' '
           << setw(HDR_COL_WIDTH) << "Int_Op_bits";
      if (bf_types) {
	cout << ' ' 
	     << setw(HDR_COL_WIDTH) << "Flt_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Dbl_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I8_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I16_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I32_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "I64_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Ptr_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Other_LD" << ' '
	     << setw(HDR_COL_WIDTH) << "Flt_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "Dbl_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I8_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I16_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I32_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "I64_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "Ptr_ST" << ' '
	     << setw(HDR_COL_WIDTH) << "Other_ST";
      }
    }
    if (bf_unique_bytes)
      cout << ' '
           << setw(HDR_COL_WIDTH) << "Uniq_bytes";
    cout << ' '
         << setw(HDR_COL_WIDTH) << "Cond_brs" << ' '
         << setw(HDR_COL_WIDTH) << "Invocations" << ' '
         << "Function";
    if (bf_call_stack)
      for (size_t i=0; i<call_stack->max_depth-1; i++)
        cout << ' '
             << "Parent_func_" << i+1;
    cout << '\n';

    // Output the data by sorted function name.
    vector<const char*>* all_func_names = per_func_totals().sorted_keys(compare_char_stars);
    for (vector<const char*>::iterator fn_iter = all_func_names->begin();
         fn_iter != all_func_names->end();
         fn_iter++) {
      const string funcname = *fn_iter;
      const char* funcname_c = bf_string_to_symbol(funcname.c_str());
      ByteFlopCounters* func_counters = per_func_totals()[funcname_c];
      cout << "BYFL_FUNC:        "
           << setw(HDR_COL_WIDTH) << func_counters->loads << ' '
           << setw(HDR_COL_WIDTH) << func_counters->stores << ' '
           << setw(HDR_COL_WIDTH) << func_counters->load_ins << ' '
           << setw(HDR_COL_WIDTH) << func_counters->store_ins << ' '
           << setw(HDR_COL_WIDTH) << func_counters->flops << ' '
           << setw(HDR_COL_WIDTH) << func_counters->fp_bits;
      if (bf_all_ops) {
        cout << ' '
             << setw(HDR_COL_WIDTH) << func_counters->ops << ' '
             << setw(HDR_COL_WIDTH) << func_counters->op_bits;

	if (bf_types) { 
	cout << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_float_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_double_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_int8_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_int16_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_int32_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_int64_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_ptr_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->load_other_type_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_float_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_double_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_int8_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_int16_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_int32_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_int64_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_ptr_ins << ' '
	     << setw(HDR_COL_WIDTH) << func_counters->store_other_type_ins;
	}
      }
      if (bf_unique_bytes)
        cout << ' '
             << setw(HDR_COL_WIDTH) << bf_tally_unique_addresses(funcname_c);
      cout << ' '
           << setw(HDR_COL_WIDTH) << func_counters->cond_brs << ' '
           << setw(HDR_COL_WIDTH) << func_call_tallies()[funcname_c] << ' '
           << funcname_c << '\n';
    }
    delete all_func_names;

    // Output invocation tallies for all called functions, not just
    // instrumented functions.
    cout << "BYFL_CALLEE_HEADER: "
         << setw(13) << "Invocations" << ' '
         << "Byfl" << ' '
         << "Function\n";
    vector<const char*> all_called_funcs;
    for (str2num_t::iterator sm_iter = func_call_tallies().begin();
         sm_iter != func_call_tallies().end();
         sm_iter++)
      all_called_funcs.push_back(sm_iter->first);
    sort(all_called_funcs.begin(), all_called_funcs.end(), compare_func_totals);
    for (vector<const char*>::iterator fn_iter = all_called_funcs.begin();
         fn_iter != all_called_funcs.end();
         fn_iter++) {
      const char* funcname = *fn_iter;   // Function name
      uint64_t tally = 0;                // Invocation count
      bool instrumented = true;          // Whether function was instrumented
      if (funcname[0] == '+') {
        const char* unique_name = bf_string_to_symbol(funcname+1);
        str2num_t::iterator tally_iter = func_call_tallies().find(unique_name);
        instrumented = (tally_iter != func_call_tallies().end());
        tally = func_call_tallies()[bf_string_to_symbol(funcname)];
        funcname = unique_name;
      }
      if (tally > 0)
        cout << "BYFL_CALLEE: "
             << setw(HDR_COL_WIDTH) << tally << ' '
             << (instrumented ? "Yes " : "No  ") << ' '
             << funcname << '\n';
    }
  }


  // Report the total counter values across all basic blocks.
  void report_totals (const char* partition, ByteFlopCounters& counter_totals) {
    uint64_t global_bytes = counter_totals.loads + counter_totals.stores;
    uint64_t global_mem_ops = counter_totals.load_ins + counter_totals.store_ins;
    uint64_t global_unique_bytes = 0;
    vector<uint64_t>* reuse_hist;   // Histogram of reuse distances
    uint64_t reuse_unique;          // Unique bytes as measured by the reuse-distance calculator
    bf_get_reuse_distance(&reuse_hist, &reuse_unique);
    if (reuse_unique > 0)
      global_unique_bytes = reuse_unique;
    else
      if (bf_unique_bytes && !partition)
        global_unique_bytes = bf_tally_unique_addresses();

    // Report the dynamic basic-block count.
    string tag("BYFL_SUMMARY");
    cout.imbue(std::locale(""));
    if (partition)
      tag += '(' + string(partition) + ')';
    cout << tag << ": " << separator << '\n';
    if (counter_totals.cond_brs > 0)
      cout << tag << ": " << setw(25) << counter_totals.b_blocks << " basic blocks\n"
           << tag << ": " << setw(25) << counter_totals.cond_brs << " conditional or indirect branches\n"
           << tag << ": " << separator << '\n';

    // Report the raw measurements in terms of bytes and operations.
    cout << tag << ": " << setw(25) << global_bytes << " bytes ("
         << counter_totals.loads << " loaded + "
         << counter_totals.stores << " stored)\n";
    if (bf_unique_bytes && !partition)
      cout << tag << ": " << setw(25) << global_unique_bytes << " unique bytes\n";
    cout << tag << ": " << setw(25) << counter_totals.flops << " flops\n";
    if (bf_all_ops) {
      cout << tag << ": " << setw(25) << counter_totals.ops << " integer ops\n";
      cout << tag << ": " << setw(25) << global_mem_ops << " memory ops ("
           << counter_totals.load_ins << " loads + "
           << counter_totals.store_ins << " stores)\n";
      if (bf_types) {
	cout << tag << ": " << separator << '\n';	
	cout << tag << ": " << setw(25) << counter_totals.load_float_ins      << " single-precision floating point loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_double_ins     << " double-precision floating point loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_int8_ins       <<  "  8-bit integer loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_int16_ins      << " 16-bit integer loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_int32_ins      << " 32-bit integer loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_int64_ins      << " 64-bit integer loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_ptr_ins        << " pointer/address loads\n";
	cout << tag << ": " << setw(25) << counter_totals.load_other_type_ins << " loads of other types\n";
	cout << tag << ": " << separator << '\n';	
	cout << tag << ": " << setw(25) << counter_totals.store_float_ins      << " single-precision floating point stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_double_ins     << " double-precision floating point stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_int8_ins       <<  "  8-bit integer stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_int16_ins      << " 16-bit integer stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_int32_ins      << " 32-bit integer stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_int64_ins      << " 64-bit integer stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_ptr_ins        << " pointer/address stores\n";
	cout << tag << ": " << setw(25) << counter_totals.store_other_type_ins << " stores of other types\n";
      }
    }
    if (reuse_unique > 0) {
      uint64_t median_value;
      uint64_t mad_value;
      bf_get_median_reuse_distance(&median_value, &mad_value);
      cout << tag << ": " << setw(25);
      if (median_value == ~(uint64_t)0)
	cout << "infinite" << " median reuse distance\n";
      else
	cout << median_value << " median reuse distance (+/- "
	     << mad_value << ")\n";
    }
    cout << tag << ": " << separator << '\n';

    // Report the raw measurements in terms of bits and bit operations.
    cout << tag << ": " << setw(25) << global_bytes*8 << " bits ("
         << counter_totals.loads*8 << " loaded + "
         << counter_totals.stores*8 << " stored)\n";
    if (bf_unique_bytes && !partition)
      cout << tag << ": " << setw(25) << global_unique_bytes*8 << " unique bits\n";
    cout << tag << ": " << setw(25) << counter_totals.fp_bits << " flop bits\n";
    if (bf_all_ops)
      cout << tag << ": " << setw(25) << counter_totals.op_bits << " integer op bits\n";
    cout << tag << ": " << separator << '\n';

    // Report vector-operation measurements.
    uint64_t num_vec_ops=0, total_vec_elts, total_vec_bits;
    if (bf_vectors) {
      if (partition)
        bf_get_vector_statistics(partition, &num_vec_ops, &total_vec_elts, &total_vec_bits);
      else
        bf_get_vector_statistics(&num_vec_ops, &total_vec_elts, &total_vec_bits);
      cout << tag << ": " << setw(25) << num_vec_ops << " vector operations\n";
      if (num_vec_ops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)total_vec_elts / (double)num_vec_ops
             << " elements per vector\n"
             << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)total_vec_bits / (double)num_vec_ops
             << " bits per element\n";
      cout << tag << ": " << separator << '\n';
    }

    // Report a bunch of derived measurements.
    cout << tag << ": " << fixed << setw(25) << setprecision(4)
         << (double)counter_totals.loads / (double)counter_totals.stores
         << " bytes loaded per byte stored\n";

    if (bf_all_ops) {
      if (counter_totals.load_ins > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)counter_totals.ops / (double)counter_totals.load_ins
             << " integer ops per load instruction\n";
      if (global_mem_ops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_bytes*8 / (double)global_mem_ops
             << " bits loaded/stored per memory op\n";
    }

    if (counter_totals.cond_brs > 0) {
      if (counter_totals.flops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)counter_totals.flops / (double)counter_totals.cond_brs
             << " flops per conditional/indirect branch\n";
      if (counter_totals.ops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)counter_totals.ops / (double)counter_totals.cond_brs
             << " ops per conditional/indirect branch\n";
      if (num_vec_ops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)num_vec_ops / (double)counter_totals.cond_brs
             << " vector ops per conditional/indirect branch\n";
    }
    if (num_vec_ops > 0) {
      if (counter_totals.flops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)num_vec_ops / (double)counter_totals.flops
             << " vector operations (FP & int) per flop\n";
      if (counter_totals.ops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)num_vec_ops / (double)counter_totals.ops
             << " vector operations per integer op\n";
    }
    cout << tag << ": " << separator << '\n';
    if (counter_totals.flops > 0) {
      cout << tag << ": " << fixed << setw(25) << setprecision(4)
           << (double)global_bytes / (double)counter_totals.flops
           << " bytes per flop\n"
           << tag << ": " << fixed << setw(25) << setprecision(4)
           << (double)global_bytes*8.0 / (double)counter_totals.fp_bits
           << " bits per flop bit\n";
    }
    if (counter_totals.ops > 0)
      cout << tag << ": " << fixed << setw(25) << setprecision(4)
           << (double)global_bytes / (double)counter_totals.ops
           << " bytes per integer op\n"
           << tag << ": " << fixed << setw(25) << setprecision(4)
           << (double)global_bytes*8.0 / (double)counter_totals.op_bits
           << " bits per integer op bit\n";
    if (bf_unique_bytes && (counter_totals.flops > 0 || counter_totals.ops > 0)) {
      cout << tag << ": " << separator << '\n';
      if (counter_totals.flops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_unique_bytes / (double)counter_totals.flops
             << " unique bytes per flop\n"
             << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_unique_bytes*8.0 / (double)counter_totals.fp_bits
             << " unique bits per flop bit\n";
      if (counter_totals.ops > 0)
        cout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_unique_bytes / (double)counter_totals.ops
             << " unique bytes per integer op\n"
             << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_unique_bytes*8.0 / (double)counter_totals.op_bits
             << " unique bits per integer op bit\n";
    }
    if (bf_unique_bytes && !partition)
      cout << tag << ": " << fixed << setw(25) << setprecision(4)
           << (double)global_bytes / (double)global_unique_bytes
           << " bytes per unique byte\n";
    cout << tag << ": " << separator << '\n';
  }

public:
  RunAtEndOfProgram() {
    separator = "-----------------------------------------------------------------";
  }

  ~RunAtEndOfProgram() {
    // Do nothing if our output is suppressed.
    if (suppress_output())
      return;

    // Report per-function counter totals.
    if (bf_per_func)
      report_by_function();

    // Output a histogram of vector usage.
    if (bf_vectors)
      bf_report_vector_operations(call_stack->max_depth);

    // If we're not instrumented on the basic-block level, then we
    // need to accumulate the current values of all of our counters
    // into the global totals.
    if (global_totals.b_blocks == 0)
      global_totals.accumulate(bf_load_count,             bf_store_count,

                               bf_load_ins_count, 
			       bf_load_float_ins_count,   bf_load_double_ins_count,
			       bf_load_int8_ins_count,    bf_load_int16_ins_count,
			       bf_load_int32_ins_count,   bf_load_int64_ins_count,
			       bf_load_ptr_ins_count,     bf_load_other_type_ins_count,

			       bf_store_ins_count,
			       bf_store_float_ins_count,   bf_store_double_ins_count,
			       bf_store_int8_ins_count,    bf_store_int16_ins_count,
			       bf_store_int32_ins_count,   bf_store_int64_ins_count,
			       bf_store_ptr_ins_count,     bf_store_other_type_ins_count,

                               bf_flop_count,              bf_fp_bits_count,
                               bf_op_count,                bf_op_bits_count, 
			       0, 0);

    // If the global counter totals are empty, this means that we were
    // tallying per-function data and resetting the global counts
    // after each tally.  We therefore reconstruct the lost global
    // counts from the per-function tallies.
    if (global_totals.b_blocks == 0) {
      for (counter_iterator sm_iter = per_func_totals().begin();
           sm_iter != per_func_totals().end();
           sm_iter++)
        global_totals.accumulate(sm_iter->second);
      // The above was not a per-BB accumulate.
      global_totals.b_blocks = 0;
      global_totals.cond_brs = 0;
    }

    // Report user-defined counter totals, if any.
    vector<const char*>* all_tag_names = user_defined_totals().sorted_keys(compare_char_stars);
    for (vector<const char*>::const_iterator tag_iter = all_tag_names->begin();
         tag_iter != all_tag_names->end();
         tag_iter++) {
      const char* tag_name = *tag_iter;
      report_totals(tag_name, *user_defined_totals()[tag_name]);
    }

    // Report the global counter totals across all basic blocks.
    report_totals(NULL, global_totals);
  }
} run_at_end_of_program;

} // namespace bytesflops