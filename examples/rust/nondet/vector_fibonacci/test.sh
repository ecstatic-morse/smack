#! /bin/bash

rustc -A unused-imports -C opt-level=0 -C no-prepopulate-passes -g --emit=llvm-ir --cfg 'verifier="smack"' fibonacci.rs
clang -S -emit-llvm fibonacci_c.c
llvm-link fibonacci.ll fibonacci_c.ll -o fibonacci.bc
smack fibonacci.bc --unroll=15 --no-memory-splitting -bpl out.bpl
