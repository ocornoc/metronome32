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

#include "vm.h"

int main() {
	metronome32::vm my_vm(std::vector<metronome32::memory_value>({
		// Function: MAIN
		// arguments: none.
		// dirties: R0, R1, R2, R32
		// result: none.
		
		// Multiplies two constants.
		metronome32::new_addi(0, 300),
		metronome32::new_addi(1, 300),
		// Call MULTIPLY.
		metronome32::new_jal(31, 0x02),
		metronome32::new_cf(),
		
		
		// Function: MULTIPLY
		// arguments: R0, R1
		// dirties: R2
		// result: R0 *= R1
		// Preconditions: R0 and R1 are non-negative.
		
		// Function entry.
		metronome32::new_cf(),
		// R2 = R0.
		metronome32::new_andi(2, 0),
		metronome32::new_add(2, 0),
		// Clear R0.
		metronome32::new_andi(0, 0),
		// If R1 == R0 (== 0), jump to LOOPSKIP.
		metronome32::new_beq(0, 1, +6),
		// If R2 <= 0, jump to LOOPSKIP.
		metronome32::new_blez(2, +5),
		// LOOP1
		metronome32::new_cf(),
		// R0 += R1.
		metronome32::new_add(0, 1),
		// R2 -= 1.
		metronome32::new_addi(2, -1),
		// If R2 > 0, jump to LOOP1.
		metronome32::new_bgtz(2, -3),
		// LOOPSKIP
		metronome32::new_cf(),
		// Returns to caller (R31).
		metronome32::new_jr(31),
	}));
	
	// The program ends when the MULTIPLY function returns, so that's where
	// we know to stop the virtual machine.
	while (my_vm.get_context().counter != 4) my_vm.step();
	
	my_vm.reverse();
	
	// The program starts at 0, so that's where we know to stop the virtual
	// machine.
	while (my_vm.get_context().counter != 0) my_vm.step();
}
