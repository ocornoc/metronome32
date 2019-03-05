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
#include "memory.h"
namespace p32 = metronome32;
namespace p32mem = p32::memory;

typedef p32::register_value reg_val;
typedef p32::memory_value mem_val;

// Read 1 word from a memory context.
mem_val p32mem::read_word(const mem_t& memory, const reg_val& address) noexcept
{
	if (memory.count(address) == 0) {
		return p32::memory_default;
	} else {
		return memory.at(address);
	}
}

// Write 1 word to a memory context.
void p32mem::write_word(mem_t& memory, const reg_val& address, const mem_val& val) noexcept
{
	memory.erase(address);
	memory[address] = val;
}
