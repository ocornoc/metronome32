# Metronome32 Version 1.0.0-alpha

(using SemVer 2.0.0)

[![Build Status](https://travis-ci.com/ocornoc/metronome32.svg?branch=master)](https://travis-ci.com/ocornoc/metronome32)
[![codecov](https://codecov.io/gh/ocornoc/metronome32/branch/master/graph/badge.svg)](https://codecov.io/gh/ocornoc/metronome32)

Credit for the Pendulum 32-bit Instruction Set Architecture goes to Carlin James
Vieri.

Metronome32 is made by Grayson Burton and is hosted at
[its Github repo](https://github.com/ocornoc/metronome32).

## What is Pendulum?

Pendulum is a 32-bit RISC Instruction Set Architecture that is completely,
fully, 100% **reversible**. Any instruction executed can be reversed. This has
advantages such as potentially "rewinding" a computer if a crash happens.

## What is Metronome?

Metronome is a C++14 virtual machine made to execute Pendulum 32-bit bytecode.
It also has a library for creating programs or instructions straight from C++,
though this capability is intentionally barebones.

## Beware!

Make sure to read through the [Pendulum 32-bit Instruction Set Architecture specifications](https://dspace.mit.edu/bitstream/handle/1721.1/36039/33342527-MIT.pdf).
It contains a lot of valuable information, not to mention that it's the sole
source use in Metronome32's production.

One *extremely* easy mistake to make is to forget (or never know) that all
branch and jump instructions must point to a `CF` (come from) instruction.

Metronome tries its best to stick to pure mandatory-only ISO C++14.
Unfortunately, the use of `std::uint32_t` was practically inevitable. On top of
that, Metronome assumes that the conversion of negative signed integers to
unsigned numbers uses two's complement. These are thought to be relatively
reasonable.

It uses SD-6 feature tests to optionally enable some attributes such as
`[[fallthrough]]`. It does not test for `[[maybe_unused]]` support, instead
presuming the compiler doesn't actually treat it as an error if it cannot be
used.

## Routine Testing

Currently, Metronome32's master branch is tested on a per pull request basis.
It uses [Travis CI](https://travis-ci.com/) to test on:
 * Ubuntu Xenial Xerus 16.04 LTS using GCC 5.4.0
 * Ubuntu Xenial Xerus 16.04 LTS using Clang/LLVM 7.0
 * macOS 10.13 (Xcode 10.1) using Clang/LLVM 9.1
