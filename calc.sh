#!/bin/bash

# RISCV目录
RISCV=~/rvcc/riscv

# 声明一个函数
calculate() {
  # 输入值 为参数1
  input="$*"

  # echo "$input"

  # 将生成结果写入tmp.s汇编文件。
  # 如果运行不成功，则会执行exit退出。成功时会短路exit操作
  ./rvcc "$input" > tmp.s || exit
  # 编译rvcc产生的汇编文件
  # gcc -o tmp tmp.s
  $RISCV/bin/riscv64-unknown-linux-gnu-gcc -static -o tmp tmp.s

  # 运行生成出来目标文件
  # ./tmp
  $RISCV/bin/qemu-riscv64 -L $RISCV/sysroot ./tmp
  # $RISCV/bin/spike --isa=rv64gc $RISCV/riscv64-unknown-linux-gnu/bin/pk ./tmp

  # 获取程序返回值，存入 实际值
  actual="$?"
  # 输出
  echo $actual
}

calculate $*