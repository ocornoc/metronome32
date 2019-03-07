/*
Copyright (c) 2019 Grayson Burton ( https://github.com/ocornoc/ )

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

#include "instruction.h"
#include "memory.h"
#include "vm.h"
namespace m32 = metronome32;

[[gnu::pure]] int test_instruction_conversions()
{
	typedef m32::instr_type::r rtype;
	typedef m32::instr_type::j jtype;
	typedef m32::instr_type::b btype;
	typedef m32::instr_type::i itype;
	typedef m32::instruction instruct;
	
	constexpr instruct testval = 0x12345678;
	
	const rtype requiv = m32::instr_to_r(testval);
	const jtype jequiv = m32::instr_to_j(testval);
	const btype bequiv = m32::instr_to_b(testval);
	const itype iequiv = m32::instr_to_i(testval);
	
	const instruct rundo = m32::type_to_instr(requiv);
	const instruct jundo = m32::type_to_instr(jequiv);
	const instruct bundo = m32::type_to_instr(bequiv);
	const instruct iundo = m32::type_to_instr(iequiv);
	
	if (testval != rundo) return 1;
	else if (testval != jundo) return 1;
	else if (testval != bundo) return 1;
	else if (testval != iundo) return 1;
	else return 0;
}

int test_program1()
{
	m32::vm my_vm(std::vector<m32::memory_value>({
		// Function: MAIN
		// arguments: none.
		// dirties: R0, R1, R2, R32
		// result: none.
		
		// Multiplies two constants.
		m32::new_addi(0, 300),
		m32::new_addi(1, 300),
		// Call MULTIPLY.
		m32::new_jal(31, 0x02),
		m32::new_cf(),
		
		
		// Function: MULTIPLY
		// arguments: R0, R1
		// dirties: R2
		// result: R0 *= R1
		// Preconditions: R0 and R1 are non-negative.
		
		// Function entry.
		m32::new_cf(),
		// R2 = R0.
		m32::new_andi(2, 0),
		m32::new_add(2, 0),
		// Clear R0.
		m32::new_andi(0, 0),
		// If R1 == R0 (== 0), jump to LOOPSKIP.
		m32::new_beq(0, 1, +6),
		// If R2 <= 0, jump to LOOPSKIP.
		m32::new_blez(2, +5),
		// LOOP1
		m32::new_cf(),
		// R0 += R1.
		m32::new_add(0, 1),
		// R2 -= 1.
		m32::new_addi(2, -1),
		// If R2 > 0, jump to LOOP1.
		m32::new_bgtz(2, -3),
		// LOOPSKIP
		m32::new_cf(),
		// Returns to caller (R31).
		m32::new_jr(31),
	}));
	
	// The program ends when the MULTIPLY function returns, so that's where
	// we know to stop the virtual machine.
	while (my_vm.get_context().counter != 4) my_vm.step();
	
	my_vm.reverse();
	
	// The program starts at 0, so that's where we know to stop the virtual
	// machine.
	while (my_vm.get_context().counter != 0) my_vm.step();
	
	return 0;
}

int main()
{
	int success = 0;
	
	success |= test_instruction_conversions();
	success |= test_program1();
	
	return success;
}
