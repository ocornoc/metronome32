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

#include <algorithm>
#include <cassert>
#include "instruction.h"
namespace p32 = metronome32;
using namespace metronome32;
using std::bitset;

typedef p32::instruction inst;
using p32::gpregister;
using p32::operation;
using p32::function;
using p32::shrot_t;
using p32::target_t;
using p32::offset_t;
using p32::memory_value;

typedef p32::instr_type::r rtype;
typedef p32::instr_type::j jtype;
typedef p32::instr_type::b btype;
typedef p32::instr_type::i itype;

/*
	Constants for instruction operation matching
*/

#define SCULL static constexpr unsigned long long

SCULL rtype_op_special  = 0b000000;
SCULL rtype_func_add    = 0b00000000001;
SCULL rtype_func_and    = 0b00000010000;
SCULL rtype_func_nor    = 0b00010000000;
SCULL rtype_func_neg    = 0b00100000000;
SCULL rtype_func_or     = 0b00000100000;
SCULL rtype_func_rl     = 0b10001000000;
SCULL rtype_func_rlv    = 0b10100000000;
SCULL rtype_func_rr     = 0b10010000000;
SCULL rtype_func_rrv    = 0b11000000000;
SCULL rtype_func_sll    = 0b10000000001;
SCULL rtype_func_sllv   = 0b10000001000;
SCULL rtype_func_slt    = 0b10000000000;
SCULL rtype_func_sra    = 0b10000000100;
SCULL rtype_func_srav   = 0b10000100000;
SCULL rtype_func_srl    = 0b10000000010;
SCULL rtype_func_srlv   = 0b10000010000;
SCULL rtype_func_sub    = 0b00000000100;
SCULL rtype_func_xor    = 0b00001000000;

SCULL jtype_op_cf       = 0b001101;
SCULL jtype_op_j        = 0b000001;

SCULL btype_op_beq      = 0b001001;
SCULL btype_op_bgez     = 0b000110;
SCULL btype_op_bgezal   = 0b001000;
SCULL btype_op_bgtz     = 0b001100;
SCULL btype_op_blez     = 0b001011;
SCULL btype_op_bltz     = 0b000101;
SCULL btype_op_bltzal   = 0b000111;
SCULL btype_op_bne      = 0b001010;
SCULL btype_op_exchange = 0b101000;
SCULL btype_op_jal      = 0b000011;
SCULL btype_op_jalr     = 0b000100;
SCULL btype_op_jr       = 0b000010;

SCULL itype_op_addi     = 0b011000;
SCULL itype_op_andi     = 0b011100;
SCULL itype_op_ori      = 0b011101;
SCULL itype_op_slti     = 0b011010;
SCULL itype_op_xori     = 0b011110;

#undef SCULL

/*
	Conversion functions to/from specific instruction forms
*/

#define GP [[gnu::pure]]

// Changes a bitset's size, truncating if it's smaller and optionally sign-extending.
template <size_t N, size_t M>
GP bitset<M> constexpr change_shape(const bitset<N>& bs, bool sign_extend = false) noexcept
{
	bitset<M> newbs;
	size_t min_nm = std::min(N, M);
	
	for (size_t i = 0; i < min_nm; i++) {
		newbs[i] = bs[i];
	}
	
	if (sign_extend and bs.test(N - 1)) {
		for (size_t i = min_nm; i < M; i++) {
			newbs.set(i);
		}
	}
	
	return newbs;
}

// Masks the last nbits of a bitset<N> and returns a bitset<M>.
template <size_t N, size_t M>
GP bitset<M> constexpr mask_last(const bitset<N>& bs, size_t nbits) noexcept
{
	nbits = std::min(nbits, N);
	bitset<N> newbs = bs;
	newbs <<= N - std::min(N, nbits);
	newbs >>= N - std::min(N, nbits);
	newbs &= bs;
	
	return change_shape<N, M>(newbs);
}

// Shifts a bitset<N> shift_bits to the right, masks the last M bits, and return a bitset<M>.
template <size_t N, size_t M>
GP bitset<M> constexpr shift_mask_last(const bitset<N>& bs, size_t shift_bits) noexcept
{
	return mask_last<N, M>(bs >> std::min(shift_bits, N), M);
}

// p32::instr_to_X turns the instruction type to an X instruction type.
GP rtype p32::instr_to_r(const inst& instr) noexcept
{
	return rtype{
		shift_mask_last<32,  6>(instr, 26),
		shift_mask_last<32,  5>(instr, 21),
		shift_mask_last<32,  5>(instr, 16),
		shift_mask_last<32,  5>(instr, 11),
		shift_mask_last<32, 11>(instr,  0),
	};
}

GP jtype p32::instr_to_j(const inst& instr) noexcept
{
	return jtype{
		shift_mask_last<32,  6>(instr, 26),
		shift_mask_last<32, 26>(instr,  0),
	};
}

GP btype p32::instr_to_b(const inst& instr) noexcept
{
	return btype{
		shift_mask_last<32,  6>(instr, 26),
		shift_mask_last<32,  5>(instr, 21),
		shift_mask_last<32,  5>(instr, 16),
		shift_mask_last<32, 16>(instr,  0),
	};
}

GP itype p32::instr_to_i(const inst& instr) noexcept
{
	return itype{
		shift_mask_last<32,  6>(instr, 26),
		shift_mask_last<32,  5>(instr, 21),
		shift_mask_last<32, 21>(instr,  0),
	};
}

// p32::type_to_instr turns the specific instruction type to a generic instruction.

GP instruction p32::type_to_instr(const rtype& ri) noexcept
{
	instruction instr = change_shape<11, 32>(ri.func);
	instr |= change_shape< 5, 32>(ri.shrot) << 11;
	instr |= change_shape< 5, 32>(ri.rs) << 16;
	instr |= change_shape< 5, 32>(ri.rsd) << 21;
	instr |= change_shape< 6, 32>(ri.op) << 26;
	
	return instr;
}

GP instruction p32::type_to_instr(const jtype& ji) noexcept
{
	instruction instr = change_shape<26, 32>(ji.target);
	instr |= change_shape< 6, 32>(ji.jcf) << 26;
	
	return instr;
}

GP instruction p32::type_to_instr(const btype& bi) noexcept
{
	instruction instr = change_shape<16, 32>(bi.offset);
	instr |= change_shape< 5, 32>(bi.rb) << 16;
	instr |= change_shape< 5, 32>(bi.ra) << 21;
	instr |= change_shape< 6, 32>(bi.jbop) << 26;
	
	return instr;
}

GP instruction p32::type_to_instr(const itype& ii) noexcept
{
	instruction instr = change_shape<21, 32>(ii.immediate);
	instr |= change_shape< 5, 32>(ii.rsd) << 21;
	instr |= change_shape< 6, 32>(ii.op) << 26;
	
	return instr;
}

/*
	Checks if an instruction refers to a specific function
*/

GP bool p32::is_add(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_add)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_addi(const itype& structure) noexcept
{
	if (structure.op != operation(itype_op_addi)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_and(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_and)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_andi(const itype& structure) noexcept
{
	if (structure.op != operation(itype_op_andi)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_beq(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_beq)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_bgez(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_bgez)) {
		return false;
	} else if (structure.ra != gpregister()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_bgezal(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_bgezal)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_bgtz(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_bgtz)) {
		return false;
	} else if (structure.ra != gpregister()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_blez(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_blez)) {
		return false;
	} else if (structure.ra != gpregister()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_bltz(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_bltz)) {
		return false;
	} else if (structure.ra != gpregister()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_bltzal(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_bltzal)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_bne(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_bne)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_cf(const jtype& structure) noexcept
{
	if (structure.jcf != operation(jtype_op_cf)) {
		return false;
	} else if (structure.target != target_t()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_exchange(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_exchange)) {
		return false;
	} else if (structure.offset != offset_t()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_j(const jtype& structure) noexcept
{
	if (structure.jcf != operation(jtype_op_j)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_jal(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_jal)) {
		return false;
	} else if (structure.rb != gpregister()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_jalr(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_jalr)) {
		return false;
	} else if (structure.offset != offset_t()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_jr(const btype& structure) noexcept
{
	if (structure.jbop != operation(btype_op_jr)) {
		return false;
	} else if (structure.ra != gpregister()) {
		return false;
	} else if (structure.offset != offset_t()) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_nor(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_nor)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_neg(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_neg)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_or(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_neg)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_ori(const itype& structure) noexcept
{
	if (structure.op != operation(itype_op_ori)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_rl(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.rs != gpregister()) {
		return false;
	} else if (structure.func != function(rtype_func_rl)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_rlv(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_rlv)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_rr(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.rs != gpregister()) {
		return false;
	} else if (structure.func != function(rtype_func_rr)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_rrv(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_rrv)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_sll(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.rs != gpregister()) {
		return false;
	} else if (structure.func != function(rtype_func_sll)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_sllv(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_sllv)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_slt(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_slt)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_slti(const itype& structure) noexcept
{
	if (structure.op != operation(itype_op_slti)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_sra(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.rs != gpregister()) {
		return false;
	} else if (structure.func != function(rtype_func_sra)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_srav(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_srav)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_srl(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.rs != gpregister()) {
		return false;
	} else if (structure.func != function(rtype_func_srl)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_srlv(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_srlv)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_sub(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_sub)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_xor(const rtype& structure) noexcept
{
	if (structure.op != operation(rtype_op_special)) {
		return false;
	} else if (structure.shrot != shrot_t()) {
		return false;
	} else if (structure.func != function(rtype_func_xor)) {
		return false;
	} else {
		return true;
	}
}

GP bool p32::is_xori(const itype& structure) noexcept
{
	if (structure.op != operation(itype_op_xori)) {
		return false;
	} else {
		return true;
	}
}

/*
	Creates a new instruction and returns it.
*/

memory_value p32::new_add(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_add;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_addi(
	const gpregister& rsd,
	const immediate_t& imm) noexcept
{
	instr_type::i instr;
	instr.op = itype_op_addi;
	instr.rsd = rsd;
	instr.immediate = imm;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_and(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_and;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_andi(
	const gpregister& rsd,
	const immediate_t& imm) noexcept
{
	instr_type::i instr;
	instr.op = itype_op_andi;
	instr.rsd = rsd;
	instr.immediate = imm;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_beq(
	const gpregister& ra,
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_beq;
	instr.ra = ra;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_bgez(
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_bgez;
	instr.ra = 0;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_bgezal(
	const gpregister& link,
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_bgezal;
	instr.ra = link;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_bgtz(
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_bgtz;
	instr.ra = 0;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_blez(
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_blez;
	instr.ra = 0;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_bltz(
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_bltz;
	instr.ra = 0;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_bltzal(
	const gpregister& link,
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_bltzal;
	instr.ra = link;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_blne(
	const gpregister& ra,
	const gpregister& rb,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_bne;
	instr.ra = ra;
	instr.rb = rb;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_exchange(
	const gpregister& exch,
	const gpregister& addr) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_exchange;
	instr.ra = exch;
	instr.rb = addr;
	instr.offset = 0;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_j(
	const target_t& target) noexcept
{
	instr_type::j instr;
	instr.jcf = jtype_op_j;
	instr.target = target;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_jal(
	const gpregister& ra,
	const offset_t& offset) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_jal;
	instr.ra = ra;
	instr.rb = 0;
	instr.offset = offset;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_jalr(
	const gpregister& ra,
	const gpregister& jreg) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_jalr;
	instr.ra = ra;
	instr.rb = jreg;
	instr.offset = 0;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_jr(
	const gpregister& jreg) noexcept
{
	instr_type::b instr;
	instr.jbop = btype_op_jr;
	instr.ra = 0;
	instr.rb = jreg;
	instr.offset = 0;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_neg(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_neg;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_or(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_or;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_ori(
	const gpregister& rsd,
	const immediate_t& imm) noexcept
{
	instr_type::i instr;
	instr.op = itype_op_ori;
	instr.rsd = rsd;
	instr.immediate = imm;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_rl(
	const gpregister& rsd,
	const shrot_t& amt) noexcept
{
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = 0;
	instr.shrot = amt;
	instr.func = rtype_func_rl;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_rlv(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_rlv;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_rr(
	const gpregister& rsd,
	const shrot_t& amt) noexcept
{
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = 0;
	instr.shrot = amt;
	instr.func = rtype_func_rr;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_rrv(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_rrv;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_sll(
	const gpregister& rsd,
	const shrot_t& amt) noexcept
{
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = 0;
	instr.shrot = amt;
	instr.func = rtype_func_sll;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_sllv(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_sllv;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_slt(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_slt;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_slti(
	const gpregister& rsd,
	const immediate_t& imm) noexcept
{
	instr_type::i instr;
	instr.op = itype_op_slti;
	instr.rsd = rsd;
	instr.immediate = imm;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_sra(
	const gpregister& rsd,
	const shrot_t& amt) noexcept
{
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = 0;
	instr.shrot = amt;
	instr.func = rtype_func_sra;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_srav(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_srav;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_srl(
	const gpregister& rsd,
	const shrot_t& amt) noexcept
{
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = 0;
	instr.shrot = amt;
	instr.func = rtype_func_srl;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_srlv(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_srlv;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_sub(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_sub;
	
	assert(rsd != rs);
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_xor(
	const gpregister& rsd,
	const gpregister& rs) noexcept
{
	assert(rsd != rs);
	
	instr_type::r instr;
	instr.op = rtype_op_special;
	instr.rsd = rsd;
	instr.rs = rs;
	instr.shrot = 0;
	instr.func = rtype_func_xor;
	
	return type_to_instr(instr).to_ullong();
}

memory_value p32::new_xori(
	const gpregister& rsd,
	const immediate_t& imm) noexcept
{
	instr_type::i instr;
	instr.op = itype_op_xori;
	instr.rsd = rsd;
	instr.immediate = imm;
	
	return type_to_instr(instr).to_ullong();
}

#undef GP
