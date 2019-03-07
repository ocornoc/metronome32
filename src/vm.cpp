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

#include <utility>
#include <string>
#include <vector>
#include <cstdint>
#include "instruction.h"
#include "memory.h"
#include "vm.h"
namespace p32 = metronome32;

using p32::context_data;
using p32::context_error;
using p32::instructions_t;
using p32::register_value;
using p32::memory_value;
using p32::register_context_t;
using p32::dp_garbage_stack_t;
using p32::pc_garbage_stack_t;
using p32::system_memory_t;

#define GP [[gnu::pure]]
#define GC [[gnu::const]]

context_data::context_data(const system_memory_t& mem_init, const register_value& counter_init)
	: counter(counter_init), sys_mem(mem_init)
{}

context_data::context_data(const register_value& counter_init)
	: counter(counter_init)
{}

context_data p32::fresh_context(const instructions_t& instructions, const register_value& start_pc)
{
	return context_data(instructions, start_pc);
}

p32::vm::vm(const std::vector<p32::memory_value>& bytecode, register_value start_at, register_value load_at)
	: context(start_at)
{
	if (not bytecode.empty()) {
		for (const auto& bc : bytecode) {
			auto br = static_cast<decltype(context.sys_mem)::mapped_type>(bc);
			context.sys_mem[load_at] = br;
			load_at++;
		}
	}
}

GC const context_data& p32::vm::get_context() const noexcept
{
	return context;
}

void p32::vm::set_context(const context_data& other_context) noexcept
{
	context = other_context;
}

void p32::vm::set_context(context_data&& other_context) noexcept
{
	context = std::move(other_context);
}

GP bool p32::vm::reversing() const noexcept
{
	return context.reversing;
}

void p32::vm::reverse() noexcept
{
	reverse(!context.reversing); 
}

void p32::vm::reverse(const bool set_reverse) noexcept
{
	context.reversing = set_reverse;
}

GP bool p32::vm::halted() const noexcept
{
	return context.halted;
}

bool p32::vm::halt(const bool set_halt) noexcept
{
	if (halted() and not set_halt and not is_error_trivial()) {
		return false;
	} else {
		context.halted = set_halt;
		
		return true;
	}
}

GP p32::vm::error p32::vm::get_error_code() const noexcept
{
	return context.errcode;
}

std::string p32::vm::get_error_name() const noexcept
{
	switch (context.errcode) {
		case (context_error::nothing): return "nothing";
		case (context_error::nai): return "not an instruction";
		case (context_error::naidefault): return "not an instruction, but memory default";
		case (context_error::dp_stack_empty): return "DP stack empty";
		case (context_error::pc_stack_empty): return "PC stack empty";
		case (context_error::missing_cf): return "missing CF instruction";
		case (context_error::unclear_link): return "link register isn't clear";
		case (context_error::sub_same_registers): return "can't subtract from self";
		default: return "unknown";
	}
}

// Using SD-6 feature tests, if [[fallthrough]] is supported AND the CPP
// standard is C++17, use it. Otherwise, use [[gnu::fallthrough]].
#if __has_cpp_attribute(fallthrough) && (__cplusplus >= 201703L)
 #define _VMCPP_FALLTHROUGH [[fallthrough]];
#else
 #define _VMCPP_FALLTHROUGH [[gnu::fallthrough]];
#endif
GP bool p32::vm::is_error_trivial() const noexcept
{
	switch (get_error_code()) {
		case p32::vm::error::nothing: _VMCPP_FALLTHROUGH
		case p32::vm::error::naidefault:
			return true;
		default: return false;
	}
}

bool p32::vm::step(size_t times) noexcept
{
	bool still_good = true;
	
	for (size_t i = 0; i < times and still_good; i++) {
		still_good = static_step(*this);
	}
	
	return still_good;
}

#undef GP
#undef GC
#undef _VM_CPP_FALLTHROUGH

// Using SD-6 feature tests, if [[maybe_unused]] is supported AND the CPP
// standard is C++17, use it. Otherwise, use [[gnu::maybe_unused]].
#if __has_cpp_attribute(maybe_unused) && (__cplusplus >= 201703L)
 #define _VMCPP_UNUSED [[maybe_unused]]
#else
 #define _VMCPP_UNUSED [[gnu::unused]]
#endif

static p32::instruction load_instruction(const system_memory_t& sysmem, const register_value& pc)
{
	return p32::memory::read_word(sysmem, pc);
}

// Sign extends to the left using the nth digit from the left, starting at 1.
static register_value sign_extend(register_value x, unsigned int n)
{
	register_value m = 1U << (n - 1);
	
	return (x ^ m) - m;
}

// "fex" means "forward execute", "bex" means "backwards execute".
// Assumes there are no preexisting non-trivial errors or halts.
// Returns whether there is now an error EXCEPT for NAI errors.

static bool fex_add(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	context.registers[rsd] += context.registers[rs];
	context.counter++;
	
	return true;
}

static bool fex_addi(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value imm = instruct.immediate.to_ullong();
	context.registers[rsd] += sign_extend(imm, 21);
	context.counter++;
	
	return true;
}

static bool fex_and(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] &= context.registers[rs];
	context.counter++;
	
	return true;
}

static bool fex_andi(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value imm = instruct.immediate.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] &= sign_extend(imm, 21);
	context.counter++;
	
	return true;
}

static bool fex_beq(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int ra = instruct.ra.to_ullong();
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[ra] == context.registers[rb]) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		}
		
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_bgez(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[rb] >> 31 == 0) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		}
		
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_bgezal(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int link = instruct.ra.to_ullong();
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[rb] >> 31 == 0) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		} else if (context.registers[link] != 0) {
			context.errcode = p32::context_error::unclear_link;
			context.halted = true;
			
			return false;
		}
			
		
		context.registers[link] = context.counter + 1;
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_bgtz(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[rb] >> 31 == 0 and context.registers[rb] != 0) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		}
		
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_blez(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[rb] >> 31 == 1 or context.registers[rb] == 0) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		}
		
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_bltz(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[rb] >> 31 == 1) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		}
		
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_bltzal(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int link = instruct.ra.to_ullong();
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[rb] >> 31 == 1) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		} else if (context.registers[link] != 0) {
			context.errcode = p32::context_error::unclear_link;
			context.halted = true;
			
			return false;
		}
			
		
		context.registers[link] = context.counter + 1;
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_bne(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int ra = instruct.ra.to_ullong();
	const unsigned int rb = instruct.rb.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	if (context.registers[ra] != context.registers[rb]) {
		p32::instruction instr = load_instruction(
			context.sys_mem,
			context.counter + offset
		);
		
		if (not p32::is_cf(p32::instr_to_j(instr))) {
			context.errcode = p32::context_error::missing_cf;
			context.halted = true;
			
			return false;
		}
		
		context.pc_stack.push(context.counter);
		context.counter += offset;
	}
	
	context.counter++;
	
	return true;
}

static bool fex_cf(_VMCPP_UNUSED const p32::instr_type::j& instruct, context_data& context) noexcept
{
	context.pc_stack.push(context.counter);
	context.counter++;
	
	return true;
}

static bool fex_exchange(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int ra = instruct.ra.to_ullong();
	const unsigned int rb = instruct.rb.to_ullong();
	register_value temp = context.registers[ra];
	register_value address = context.registers[rb];
	context.registers[ra] = p32::memory::read_word(context.sys_mem, address);
	p32::memory::write_word(context.sys_mem, address, temp);
	context.counter++;
	
	return true;
}

static bool fex_j(const p32::instr_type::j& instruct, context_data& context) noexcept
{
	auto new_counter = context.counter & 0b11111100000000000000000000000000;
	new_counter += sign_extend(instruct.target.to_ullong(), 26);
	
	p32::instruction instr = load_instruction(context.sys_mem, new_counter);
	
	if (not p32::is_cf(p32::instr_to_j(instr))) {
		context.errcode = p32::context_error::missing_cf;
		context.halted = true;
		
		return false;
	}
	
	context.pc_stack.push(context.counter);
	context.counter = new_counter + 1;
	
	return true;
}

static bool fex_jal(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int link = instruct.ra.to_ullong();
	const register_value offset = sign_extend(instruct.offset.to_ullong(), 16);
	
	p32::instruction instr = load_instruction(
		context.sys_mem,
		context.counter + offset
	);
	
	if (not p32::is_cf(p32::instr_to_j(instr))) {
		context.errcode = p32::context_error::missing_cf;
		context.halted = true;
		
		return false;
	} else if (context.registers[link] != 0) {
		context.errcode = p32::context_error::unclear_link;
		context.halted = true;
		
		return false;
	}
	
	context.counter++;
	context.pc_stack.push(context.counter);
	context.registers[link] = context.counter;
	context.counter += offset;
	
	return true;
}

static bool fex_jalr(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int link = instruct.ra.to_ullong();
	const unsigned int jreg = instruct.rb.to_ullong();
	const register_value new_counter = context.registers[jreg];
	
	p32::instruction instr = load_instruction(
		context.sys_mem,
		new_counter
	);
	
	if (not p32::is_cf(p32::instr_to_j(instr))) {
		context.errcode = p32::context_error::missing_cf;
		context.halted = true;
		
		return false;
	} else if (context.registers[link] != 0) {
		context.errcode = p32::context_error::unclear_link;
		context.halted = true;
		
		return false;
	}
	
	context.pc_stack.push(context.counter);
	context.registers[link] = context.counter + 1;
	context.counter = new_counter + 1;
	
	return true;
}

static bool fex_jr(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int jreg = instruct.rb.to_ullong();
	const register_value new_counter = context.registers[jreg];
	
	p32::instruction instr = load_instruction(context.sys_mem, new_counter);
	
	if (not p32::is_cf(p32::instr_to_j(instr))) {
		context.errcode = p32::context_error::missing_cf;
		context.halted = true;
		
		return false;
	}
	
	context.pc_stack.push(context.counter);
	context.counter = new_counter + 1;
	
	return true;
}

static bool fex_nor(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] |= context.registers[rs];
	context.registers[rsd] = ~context.registers[rsd];
	context.counter++;
	
	return true;
}

static bool fex_neg(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	context.registers[rsd] = -context.registers[rsd];
	context.counter++;
	
	return true;
}

static bool fex_or(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] |= context.registers[rs];
	context.counter++;
	
	return true;
}

static bool fex_ori(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value imm = instruct.immediate.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] |= sign_extend(imm, 21);
	context.counter++;
	
	return true;
}

static bool fex_rl(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval << amt) | (rsdval >> (-amt & 0b11111));
	context.counter++;
	
	return true;
}

static bool fex_rlv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval << amt) | (rsdval >> (-amt & 0b11111));
	context.counter++;
	
	return true;
}

static bool fex_rr(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval >> amt) | (rsdval << (-amt & 0b11111));
	context.counter++;
	
	return true;
}

static bool fex_rrv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval >> amt) | (rsdval << (-amt & 0b11111));
	context.counter++;
	
	return true;
}

static bool fex_sll(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] <<= amt;
	context.counter++;
	
	return true;
}

static bool fex_sllv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] <<= amt;
	context.counter++;
	
	return true;
}

static bool fex_slt(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value rsdval = context.registers[rsd];
	const register_value rsval = context.registers[rs];
	context.dp_stack.push(context.registers[rsd]);
	context.counter++;
	
	constexpr register_value mask = -1;
	if (rsdval >> 31 == 0 and rsval >> 31 == 1) {
		context.registers[rsd] = 0;
	} else if (rsdval >> 31 == 1 and rsval >> 31 == 0) {
		context.registers[rsd] = 1;
	} else if ((rsdval & mask) < (rsval & mask)) {
		context.registers[rsd] = 1;
	} else {
		context.registers[rsd] = 0;
	}
	
	return true;
}

static bool fex_slti(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value rsdval = context.registers[rsd];
	const register_value imm = sign_extend(instruct.immediate.to_ullong(), 21);
	context.dp_stack.push(context.registers[rsd]);
	context.counter++;
	
	constexpr register_value mask = -1;
	if (rsdval >> 31 == 0 and imm >> 31 == 1) {
		context.registers[rsd] = 0;
	} else if (rsdval >> 31 == 1 and imm >> 31 == 0) {
		context.registers[rsd] = 1;
	} else if ((rsdval & mask) < (imm & mask)) {
		context.registers[rsd] = 1;
	} else {
		context.registers[rsd] = 0;
	}
	
	return true;
}

static bool fex_sra(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] = sign_extend(context.registers[rsd] >> amt, 32 - amt);
	context.counter++;
	
	return true;
}

static bool fex_srav(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] = sign_extend(context.registers[rsd] >> amt, 32 - amt);
	context.counter++;
	
	return true;
}

static bool fex_srl(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] >>= amt;
	context.counter++;
	
	return true;
}

static bool fex_srlv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	context.dp_stack.push(context.registers[rsd]);
	context.registers[rsd] >>= amt;
	context.counter++;
	
	return true;
}

static bool fex_sub(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	
	if (rsd == rs) {
		context.errcode = p32::context_error::sub_same_registers;
		context.halted = true;
		
		return false;
	} else {
		context.registers[rsd] -= context.registers[rs];
	}
	
	context.counter++;
	
	return true;
}

static bool fex_xor(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	context.registers[rsd] ^= context.registers[rs];
	context.counter++;
	
	return true;
}

static bool fex_xori(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value imm = instruct.immediate.to_ullong();
	context.registers[rsd] ^= sign_extend(imm, 21);
	context.counter++;
	
	return true;
}

static bool pop_from_dpstack(const unsigned int rsd, context_data& context) noexcept
{
	if (context.dp_stack.empty()) {
		context.errcode = p32::context_error::dp_stack_empty;
		context.halted = true;
		
		return false;
	}
	
	context.registers[rsd] = context.dp_stack.top();
	context.dp_stack.pop();
	context.counter--;
	
	return true;
}

static bool bex_add(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	
	if (rsd == rs) {
		context.registers[rsd] >>= 1;
	} else {
		context.registers[rsd] -= context.registers[rs];
	}
	
	context.counter--;
	
	return true;
}

static bool bex_addi(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value imm = instruct.immediate.to_ullong();
	context.registers[rsd] -= sign_extend(imm, 21);
	context.counter--;
	
	return true;
}

static bool bex_and(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_andi(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_beq(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_bgez(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_bgezal(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int link = instruct.ra.to_ullong();
	context.registers[link] = 0;
	context.counter--;
	
	return true;
}

static bool bex_bgtz(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_blez(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_bltz(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_bltzal(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int link = instruct.ra.to_ullong();
	context.registers[link] = 0;
	context.counter--;
	
	return true;
}

static bool bex_bne(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_cf(_VMCPP_UNUSED const p32::instr_type::j& instruct, context_data& context) noexcept
{
	if (context.pc_stack.empty()) {
		context.errcode = p32::context_error::pc_stack_empty;
		context.halted = true;
		
		return false;
	
	}
	
	context.counter = context.pc_stack.top();
	context.pc_stack.pop();
	
	return true;
}

static bool bex_exchange(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	const unsigned int ra = instruct.ra.to_ullong();
	const unsigned int rb = instruct.rb.to_ullong();
	register_value temp = context.registers[ra];
	register_value address = context.registers[rb];
	context.registers[ra] = p32::memory::read_word(context.sys_mem, address);
	p32::memory::write_word(context.sys_mem, address, temp);
	context.counter--;
	
	return true;
}

static bool bex_j(_VMCPP_UNUSED const p32::instr_type::j& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_jal(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.registers[instruct.ra.to_ullong()] = 0;
	context.counter--;
	
	return true;
}

static bool bex_jalr(const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.registers[instruct.ra.to_ullong()] = 0;
	context.counter--;
	
	return true;
}

static bool bex_jr(_VMCPP_UNUSED const p32::instr_type::b& instruct, context_data& context) noexcept
{
	context.counter--;
	
	return true;
}

static bool bex_nor(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_neg(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	context.registers[rsd] = -context.registers[rsd];
	context.counter--;
	
	return true;
}

static bool bex_or(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_ori(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_rl(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval >> amt) | (rsdval << (-amt & 0b11111));
	context.counter--;
	
	return true;
}

static bool bex_rlv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval >> amt) | (rsdval << (-amt & 0b11111));
	context.counter--;
	
	return true;
}

static bool bex_rr(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value amt = instruct.shrot.to_ullong();
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval << amt) | (rsdval >> (-amt & 0b11111));
	context.counter--;
	
	return true;
}

static bool bex_rrv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	const register_value amt = context.registers[rs] & 0b11111;
	const register_value rsdval = context.registers[rsd];
	context.registers[rsd] = (rsdval << amt) | (rsdval >> (-amt & 0b11111));
	context.counter--;
	
	return true;
}

static bool bex_sll(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_sllv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_slt(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_slti(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_sra(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_srav(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_srl(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_srlv(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	return pop_from_dpstack(instruct.rsd.to_ullong(), context);
}

static bool bex_sub(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	
	if (rsd == rs) {
		context.errcode = p32::context_error::sub_same_registers;
		context.halted = true;
		
		return false;
	} else {
		context.registers[rsd] += context.registers[rs];
	}
	
	context.counter--;
	
	return true;
}

static bool bex_xor(const p32::instr_type::r& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const unsigned int rs = instruct.rs.to_ullong();
	context.registers[rsd] ^= context.registers[rs];
	context.counter--;
	
	return true;
}

static bool bex_xori(const p32::instr_type::i& instruct, context_data& context) noexcept
{
	const unsigned int rsd = instruct.rsd.to_ullong();
	const register_value imm = instruct.immediate.to_ullong();
	context.registers[rsd] ^= sign_extend(imm, 21);
	context.counter--;
	
	return true;
}

#undef _VMCPP_UNUSED

bool p32::vm::static_step(p32::vm& my_vm) noexcept
{
	bool success = false;
	p32::instruction instr;
	
	if (my_vm.reversing()) {
		instr = load_instruction(my_vm.context.sys_mem, my_vm.context.counter - 1);
	} else {
		instr = load_instruction(my_vm.context.sys_mem, my_vm.context.counter);
	}
	
	if (my_vm.halted() or not my_vm.is_error_trivial()) {
		return false;
	}
	
	auto ir = p32::instr_to_r(instr);
	auto ib = p32::instr_to_b(instr);
	auto ij = p32::instr_to_j(instr);
	auto ii = p32::instr_to_i(instr);
	
	if (my_vm.reversing()) {
		if (p32::is_add(ir)) {
			success = bex_add(ir, my_vm.context);
		} else if (p32::is_addi(ii)) {
			success = bex_addi(ii, my_vm.context);
		} else if (p32::is_and(ir)) {
			success = bex_and(ir, my_vm.context);
		} else if (p32::is_andi(ii)) {
			success = bex_andi(ii, my_vm.context);
		} else if (p32::is_beq(ib)) {
			success = bex_beq(ib, my_vm.context);
		} else if (p32::is_bgez(ib)) {
			success = bex_bgez(ib, my_vm.context);
		} else if (p32::is_bgezal(ib)) {
			success = bex_bgezal(ib, my_vm.context);
		} else if (p32::is_bgtz(ib)) {
			success = bex_bgtz(ib, my_vm.context);
		} else if (p32::is_blez(ib)) {
			success = bex_blez(ib, my_vm.context);
		} else if (p32::is_bltz(ib)) {
			success = bex_bltz(ib, my_vm.context);
		} else if (p32::is_bltzal(ib)) {
			success = bex_bltzal(ib, my_vm.context);
		} else if (p32::is_bne(ib)) {
			success = bex_bne(ib, my_vm.context);
		} else if (p32::is_cf(ij)) {
			success = bex_cf(ij, my_vm.context);
		} else if (p32::is_exchange(ib)) {
			success = bex_exchange(ib, my_vm.context);
		} else if (p32::is_j(ij)) {
			success = bex_j(ij, my_vm.context);
		} else if (p32::is_jal(ib)) {
			success = bex_jal(ib, my_vm.context);
		} else if (p32::is_jalr(ib)) {
			success = bex_jalr(ib, my_vm.context);
		} else if (p32::is_jr(ib)) {
			success = bex_jr(ib, my_vm.context);
		} else if (p32::is_nor(ir)) {
			success = bex_nor(ir, my_vm.context);
		} else if (p32::is_neg(ir)) {
			success = bex_neg(ir, my_vm.context);
		} else if (p32::is_or(ir)) {
			success = bex_or(ir, my_vm.context);
		} else if (p32::is_ori(ii)) {
			success = bex_ori(ii, my_vm.context);
		} else if (p32::is_rl(ir)) {
			success = bex_rl(ir, my_vm.context);
		} else if (p32::is_rlv(ir)) {
			success = bex_rlv(ir, my_vm.context);
		} else if (p32::is_rr(ir)) {
			success = bex_rr(ir, my_vm.context);
		} else if (p32::is_rrv(ir)) {
			success = bex_rrv(ir, my_vm.context);
		} else if (p32::is_sll(ir)) {
			success = bex_sll(ir, my_vm.context);
		} else if (p32::is_sllv(ir)) {
			success = bex_sllv(ir, my_vm.context);
		} else if (p32::is_slt(ir)) {
			success = bex_slt(ir, my_vm.context);
		} else if (p32::is_slti(ii)) {
			success = bex_slti(ii, my_vm.context);
		} else if (p32::is_sra(ir)) {
			success = bex_sra(ir, my_vm.context);
		} else if (p32::is_srav(ir)) {
			success = bex_srav(ir, my_vm.context);
		} else if (p32::is_srl(ir)) {
			success = bex_srl(ir, my_vm.context);
		} else if (p32::is_srlv(ir)) {
			success = bex_srlv(ir, my_vm.context);
		} else if (p32::is_sub(ir)) {
			success = bex_sub(ir, my_vm.context);
		} else if (p32::is_xor(ir)) {
			success = bex_xor(ir, my_vm.context);
		} else if (p32::is_xori(ii)) {
			success = bex_xori(ii, my_vm.context);
		} else if (instr == p32::memory_default) {
			my_vm.context.errcode = p32::context_error::naidefault;
		} else {
			my_vm.halt(true);
			my_vm.context.errcode = context_error::nai;
		}
	} else {
		if (p32::is_add(ir)) {
			success = fex_add(ir, my_vm.context);
		} else if (p32::is_addi(ii)) {
			success = fex_addi(ii, my_vm.context);
		} else if (p32::is_and(ir)) {
			success = fex_and(ir, my_vm.context);
		} else if (p32::is_andi(ii)) {
			success = fex_andi(ii, my_vm.context);
		} else if (p32::is_beq(ib)) {
			success = fex_beq(ib, my_vm.context);
		} else if (p32::is_bgez(ib)) {
			success = fex_bgez(ib, my_vm.context);
		} else if (p32::is_bgezal(ib)) {
			success = fex_bgezal(ib, my_vm.context);
		} else if (p32::is_bgtz(ib)) {
			success = fex_bgtz(ib, my_vm.context);
		} else if (p32::is_blez(ib)) {
			success = fex_blez(ib, my_vm.context);
		} else if (p32::is_bltz(ib)) {
			success = fex_bltz(ib, my_vm.context);
		} else if (p32::is_bltzal(ib)) {
			success = fex_bltzal(ib, my_vm.context);
		} else if (p32::is_bne(ib)) {
			success = fex_bne(ib, my_vm.context);
		} else if (p32::is_cf(ij)) {
			success = fex_cf(ij, my_vm.context);
		} else if (p32::is_exchange(ib)) {
			success = fex_exchange(ib, my_vm.context);
		} else if (p32::is_j(ij)) {
			success = fex_j(ij, my_vm.context);
		} else if (p32::is_jal(ib)) {
			success = fex_jal(ib, my_vm.context);
		} else if (p32::is_jalr(ib)) {
			success = fex_jalr(ib, my_vm.context);
		} else if (p32::is_jr(ib)) {
			success = fex_jr(ib, my_vm.context);
		} else if (p32::is_nor(ir)) {
			success = fex_nor(ir, my_vm.context);
		} else if (p32::is_neg(ir)) {
			success = fex_neg(ir, my_vm.context);
		} else if (p32::is_or(ir)) {
			success = fex_or(ir, my_vm.context);
		} else if (p32::is_ori(ii)) {
			success = fex_ori(ii, my_vm.context);
		} else if (p32::is_rl(ir)) {
			success = fex_rl(ir, my_vm.context);
		} else if (p32::is_rlv(ir)) {
			success = fex_rlv(ir, my_vm.context);
		} else if (p32::is_rr(ir)) {
			success = fex_rr(ir, my_vm.context);
		} else if (p32::is_rrv(ir)) {
			success = fex_rrv(ir, my_vm.context);
		} else if (p32::is_sll(ir)) {
			success = fex_sll(ir, my_vm.context);
		} else if (p32::is_sllv(ir)) {
			success = fex_sllv(ir, my_vm.context);
		} else if (p32::is_slt(ir)) {
			success = fex_slt(ir, my_vm.context);
		} else if (p32::is_slti(ii)) {
			success = fex_slti(ii, my_vm.context);
		} else if (p32::is_sra(ir)) {
			success = fex_sra(ir, my_vm.context);
		} else if (p32::is_srav(ir)) {
			success = fex_srav(ir, my_vm.context);
		} else if (p32::is_srl(ir)) {
			success = fex_srl(ir, my_vm.context);
		} else if (p32::is_srlv(ir)) {
			success = fex_srlv(ir, my_vm.context);
		} else if (p32::is_sub(ir)) {
			success = fex_sub(ir, my_vm.context);
		} else if (p32::is_xor(ir)) {
			success = fex_xor(ir, my_vm.context);
		} else if (p32::is_xori(ii)) {
			success = fex_xori(ii, my_vm.context);
		} else if (instr == p32::memory_default) {
			my_vm.context.errcode = p32::context_error::naidefault;
		} else {
			my_vm.halt(true);
			my_vm.context.counter++;
			my_vm.context.errcode = context_error::nai;
		}
	}
	
	return success;
}
