stage2_OBJS=stage2.o
stage2_LKR=../link/bootstrap.ld

ramtest_OBJS=ramtest.o
ramtest_LKR=../link/bootstrap.ld

TARGETS=stage2 ramtest

include ../make/programs.mk

.PHONY: mon
mon:
	../tools/upload.py --reset=rts --term --fast --srec ../monitor/monitor.srec

.PHONY: run
run:
	../tools/upload.py --reset=rts --term --fast ramtest.bin

