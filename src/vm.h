/*
Copyright (c) 2018 Grayson Burton ( https://github.com/ocornoc/ )

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <string>
#include <vector>
#include <array>
#include <stack>
#include "instruction.h"
#include "memory.h"

#ifndef HEADER_P32_VM_H
#define HEADER_P32_VM_H

namespace metronome32 {
	// An array holding all of the registers for a particular context.
	typedef std::array<register_value, 32> register_context_t;
	// The datapath garbage stack.
	typedef std::stack<register_value> dp_garbage_stack_t;
	// The program counter garbage stack.
	typedef std::stack<register_value> pc_garbage_stack_t;
	// Context error codes.
	enum class context_error {
		// No error currently.
		nothing,
		// The memory at the PC isn't an instruction and is equal to
		// the default memory value (p32::memory_default).
		naidefault,
		// The memory at the PC isn't an instruction.
		nai,
		// The datapath stack is empty when it needs to be popped from.
		dp_stack_empty,
		// The PC stack is empty when it needs to be popped from.
		pc_stack_empty,
		// Nearly all jump and branch instructions require a CF (come
		// from) instruction following a jump/branch target.
		missing_cf,
		// In jump/branch and link operations, the link register must be
		// clear (== 0) iff the jump/branch condition is satisfied.
		unclear_link,
		// In the SUB instruction, you cannot have the RSD and the RS
		// registers be the same exact register.
		sub_same_registers,
	};
	
	// An entire context for the VM.
	struct context_data {
		typedef metronome32::register_value register_value;
		typedef metronome32::memory_value memory_value;
		typedef metronome32::register_context_t register_context_t;
		typedef metronome32::dp_garbage_stack_t dp_garbage_stack_t;
		typedef metronome32::pc_garbage_stack_t pc_garbage_stack_t;
		typedef metronome32::system_memory_t system_memory_t;
		constexpr static memory_value memory_default = metronome32::memory_default;
		
		// Whether the program is running in reverse.
		bool reversing = false;
		// Whether the program is halted.
		bool halted = false;
		// The current context status.
		context_error errcode = metronome32::context_error::nothing;
		// The program counter.
		register_value counter = 0;
		// The current register context.
		register_context_t registers = {};
		// The current datapath garbage stack.
		dp_garbage_stack_t dp_stack;
		// The current program counter garbage stack.
		pc_garbage_stack_t pc_stack;
		// The current VM "system" memory.
		system_memory_t sys_mem;
		
		context_data(const context_data&) = default;
		context_data(context_data&&) = default;
		context_data& operator=(const context_data&) = default;
		context_data& operator=(context_data&&) = default;
		~context_data() = default;
		context_data(const system_memory_t& mem_init, const register_value& counter_init = 0);
		context_data(const register_value& counter_init = 0);
	};
	
	// The type holding bytecode to be loaded into memory.
	typedef metronome32::system_memory_t instructions_t;
	// Returns a context_data with the given instructions and optional
	// starting program counter. If start_pc isn't specified, it is
	// assumed to be zero.
	context_data fresh_context(const instructions_t& instructions, const register_value& start_pc = 0);
	
	// A class of a VM.
	class vm;
}

#define GP [[gnu::pure]]
#define GC [[gnu::const]]

class metronome32::vm {
	public:
		typedef metronome32::context_data context_data;
		typedef metronome32::context_error error;
		
		// Setting, getting, and swapping contexts.
		GC const context_data& get_context() const noexcept;
		void set_context(const context_data& other_context) noexcept;
		void set_context(context_data&& other_context) noexcept;
		
		// Returns whether the VM is executing in reverse.
		GP bool reversing() const noexcept;
		// Toggles the "direction" of execution.
		void reverse() noexcept;
		// Sets the "direction" of execution.
		void reverse(bool set_reverse) noexcept;
		
		// Returns whether the VM has halted.
		GP bool halted() const noexcept;
		// Halts (or un-halts, if there is no error).
		// Returns whether it performed successfully or if the current
		// error code stopped it.
		bool halt(bool set_halt = true) noexcept;
		// Returns the current error code.
		GP error get_error_code() const noexcept;
		// Returns a string containing the name of the current error
		// status.
		std::string get_error_name() const noexcept;
		// Returns whether the current error state is "trivial".
		GP bool is_error_trivial() const noexcept;
		
		// Executes times instructions. times is optional and defaults
		// to 1. If the VM is halted with a nontrivial error code or is
		// already halted prior to execution, this will return false.
		// Otherwise, it returns true for success.
		bool step(size_t times = 1) noexcept;
		
		vm(const vm&) = default;
		vm(vm&&) = default;
		vm& operator=(const vm&) = default;
		vm& operator=(vm&&) = default;
		~vm() = default;
		vm(const std::vector<memory_value>& bytecode = {}, register_value start_at = 0, register_value load_at = 0);
	
	private:
		context_data context;
		
		// Steps a VM once. Same return conditions as step().
		static bool static_step(metronome32::vm& my_vm) noexcept;
};

#undef GP
#undef GC

#endif
