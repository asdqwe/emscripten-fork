llvm's compiler-rt
------------------

These files are from the llvm-project based on release 20.1.8.

We maintain a local fork of llvm-project that contains any emscripten
specific patches:

  https://github.com/emscripten-core/llvm-project

The current patch is based on the emscripten-libs-20 branch.

Update Instructions
-------------------

Run `system/lib/update_compiler_rt.py path/to/llvm-project`

Modifications
-------------

For a list of changes from upstream see the compiler-rt files that are part of:

https://github.com/llvm/llvm-project/compare/llvmorg-20.1.8...emscripten-core:emscripten-libs-20
