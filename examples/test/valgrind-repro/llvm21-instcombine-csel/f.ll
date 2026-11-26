; Copyright (C) 2026 Qore Technologies, s.r.o.
;
; a select as the first operand of a binary operator whose second operand is not a select: InstCombine of LLVM 21.1 on
; aarch64 reports "Conditional jump or move depends on uninitialised value(s)" under valgrind (see run.sh)
define i64 @f(i1 %c, i64 %x) {
  %s = select i1 %c, i64 64, i64 32
  %r = and i64 %s, %x
  ret i64 %r
}
