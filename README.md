# hardbound-llvm

LLVM 11 [compiler pass][llvm compiler pass] for [hardbound][hardbound doi] instrumentation for [hardbound-vp][hardbound-vp].

## Installation

Copy the code in this repository to `llvm/lib/Transform/Hardbound` in
the LLVM Monorepo. Also add `add_subdirectory(Hardbound)` to
`llvm/lib/Transform/CMakeLists.txt`.

Refer to the [upstream documentation][llvm build env] to setup the build
environment. It is not necessary to build the entire LLVM tree as long
as the configuration you employ for the LLVM source repository is ABI
compatible with the binary LLVM package provided by your Linux
distribution. Running `make` in `llvm/build/lib/Transform/Hardbound`
should result in `llvm/build/lib/LLVMHardbound.so` being generated.

## Usage

After generating `LLVMHardbound.so` run:

	$ clang -Xclang -load -Xclang LLVMHardbound.so code.c

## Tests

Several tiny test programs are available, these must be compiled using:

	$ make -C tests

Afterwards, tests can be run using

	$ ./tests/run_tests.sh

## Idea

[Hardbound][hardbound doi] requires a compiler pass to insert `setbound`
instructions for local and global variables. C pointer variables should
be identifiable through LLVM IR [store][llvm store instr] an
[getelementptr][llvm getelementptr instr] instructions. For each of
these instructions, append a corresponding `setbound` call.

## Resources

* https://llvm.org/docs/WritingAnLLVMPass.html
* https://llvm.org/docs/ProgrammersManual.html
* https://llvm.org/docs/LangRef.html
* https://llvm.org/devmtg/2019-10/slides/Warzynski-WritingAnLLVMPass.pdf

[llvm build env]: https://releases.llvm.org/11.0.0/docs/WritingAnLLVMPass.html#setting-up-the-build-environment
[llvm compiler pass]: https://releases.llvm.org/11.0.0/docs/WritingAnLLVMPass.html
[hardbound doi]: https://doi.org/10.1145/1353535.1346295
[llvm store instr]: https://llvm.org/docs/LangRef.html#store-instruction
[llvm getelementptr instr]: https://llvm.org/docs/LangRef.html#getelementptr-instruction
[hardbound-vp]: https://github.com/agra-uni-bremen/hardbound-vp
