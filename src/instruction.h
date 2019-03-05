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

#include <bitset>
#include <initializer_list>

#ifndef HEADER_P32_INSTRUCTION_H
#define HEADER_P32_INSTRUCTION_H

#define GP [[gnu::pure]]

namespace metronome32 {
	// The type of a register value.
	typedef std::uint32_t register_value;
	// The type of a memory value.
	typedef std::uint32_t memory_value;
	
	typedef std::bitset<32> instruction;
	typedef std::bitset<5> gpregister;
	typedef std::bitset<6> operation;
	typedef std::bitset<11> function;
	typedef std::bitset<21> immediate_t;
	typedef std::bitset<5> shrot_t;
	typedef std::bitset<26> target_t;
	typedef std::bitset<16> offset_t;
	
	namespace instr_type {
		struct r {
			operation op;
			gpregister rsd;
			gpregister rs;
			shrot_t shrot;
			function func;
		};
		
		struct j {
			operation jcf;
			target_t target;
		};
		
		struct b {
			operation jbop;
			gpregister ra;
			gpregister rb;
			offset_t offset;
		};
		
		struct i {	
			operation op;
			gpregister rsd;
			immediate_t immediate;
		};
	}
	
	// Splits an instruction into a specific instruction type.
	GP instr_type::r instr_to_r(const instruction& instr) noexcept;
	GP instr_type::j instr_to_j(const instruction& instr) noexcept;
	GP instr_type::b instr_to_b(const instruction& instr) noexcept;
	GP instr_type::i instr_to_i(const instruction& instr) noexcept;
	
	// Combines an instruction into a generic instruction type.
	GP instruction type_to_instr(const instr_type::r& ri) noexcept;
	GP instruction type_to_instr(const instr_type::j& ji) noexcept;
	GP instruction type_to_instr(const instr_type::b& bi) noexcept;
	GP instruction type_to_instr(const instr_type::i& ii) noexcept;
	
	// Returns whether an instruction corresponds to a given mnemonic.
	GP bool is_add(const instr_type::r& structure) noexcept;
	GP bool is_addi(const instr_type::i& structure) noexcept;
	GP bool is_and(const instr_type::r& structure) noexcept;
	GP bool is_andi(const instr_type::i& structure) noexcept;
	GP bool is_beq(const instr_type::b& structure) noexcept;
	GP bool is_bgez(const instr_type::b& structure) noexcept;
	GP bool is_bgezal(const instr_type::b& structure) noexcept;
	GP bool is_bgtz(const instr_type::b& structure) noexcept;
	GP bool is_blez(const instr_type::b& structure) noexcept;
	GP bool is_bltz(const instr_type::b& structure) noexcept;
	GP bool is_bltzal(const instr_type::b& structure) noexcept;
	GP bool is_bne(const instr_type::b& structure) noexcept;
	GP bool is_cf(const instr_type::j& structure) noexcept;
	GP bool is_exchange(const instr_type::b& structure) noexcept;
	GP bool is_j(const instr_type::j& structure) noexcept;
	GP bool is_jal(const instr_type::b& structure) noexcept;
	GP bool is_jalr(const instr_type::b& structure) noexcept;
	GP bool is_jr(const instr_type::b& structure) noexcept;
	GP bool is_nor(const instr_type::r& structure) noexcept;
	GP bool is_neg(const instr_type::r& structure) noexcept;
	GP bool is_or(const instr_type::r& structure) noexcept;
	GP bool is_ori(const instr_type::i& structure) noexcept;
	GP bool is_rl(const instr_type::r& structure) noexcept;
	GP bool is_rlv(const instr_type::r& structure) noexcept;
	GP bool is_rr(const instr_type::r& structure) noexcept;
	GP bool is_rrv(const instr_type::r& structure) noexcept;
	GP bool is_sll(const instr_type::r& structure) noexcept;
	GP bool is_sllv(const instr_type::r& structure) noexcept;
	GP bool is_slt(const instr_type::r& structure) noexcept;
	GP bool is_slti(const instr_type::i& structure) noexcept;
	GP bool is_sra(const instr_type::r& structure) noexcept;
	GP bool is_srav(const instr_type::r& structure) noexcept;
	GP bool is_srl(const instr_type::r& structure) noexcept;
	GP bool is_srlv(const instr_type::r& structure) noexcept;
	GP bool is_sub(const instr_type::r& structure) noexcept;
	GP bool is_xor(const instr_type::r& structure) noexcept;
	GP bool is_xori(const instr_type::i& structure) noexcept;
	
	// Creates an instruction.
	GP memory_value new_add(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_addi(
		const gpregister& rsd,
		const immediate_t& imm) noexcept;
	GP memory_value new_and(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_andi(
		const gpregister& rsd,
		const immediate_t& imm) noexcept;
	GP memory_value new_beq(
		const gpregister& ra,
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_bgez(
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_bgezal(
		const gpregister& link,
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_bgtz(
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_blez(
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_bltz(
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_bltzal(
		const gpregister& link,
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP memory_value new_blne(
		const gpregister& ra,
		const gpregister& rb,
		const offset_t& offset) noexcept;
	GP constexpr memory_value new_cf() noexcept {return 0x34000000;}
	GP memory_value new_exchange(
		const gpregister& exch,
		const gpregister& addr) noexcept;
	GP memory_value new_j(
		const target_t& target) noexcept;
	GP memory_value new_jal(
		const gpregister& ra,
		const offset_t& target) noexcept;
	GP memory_value new_jalr(
		const gpregister& ra,
		const gpregister& jreg) noexcept;
	GP memory_value new_jr(
		const gpregister& jreg) noexcept;
	GP memory_value new_neg(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_or(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_ori(
		const gpregister& rsd,
		const immediate_t& imm) noexcept;
	GP memory_value new_rl(
		const gpregister& rsd,
		const shrot_t& amt) noexcept;
	GP memory_value new_rlv(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_rr(
		const gpregister& rsd,
		const shrot_t& amt) noexcept;
	GP memory_value new_rrv(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_sll(
		const gpregister& rsd,
		const shrot_t& amt) noexcept;
	GP memory_value new_sllv(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_slt(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_slti(
		const gpregister& rsd,
		const immediate_t& imm) noexcept;
	GP memory_value new_sra(
		const gpregister& rsd,
		const shrot_t& amt) noexcept;
	GP memory_value new_srav(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_srl(
		const gpregister& rsd,
		const shrot_t& amt) noexcept;
	GP memory_value new_srlv(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_sub(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_xor(
		const gpregister& rsd,
		const gpregister& rs) noexcept;
	GP memory_value new_xori(
		const gpregister& rsd,
		const immediate_t& imm) noexcept;
}

#undef GP

#endif
