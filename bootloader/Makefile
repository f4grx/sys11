stage2_OBJS=stage2.o
stage2_LKR=../link/bootstrap.ld

ramtest_OBJS=ramtest.o
ramtest_LKR=../link/bootstrap.ld

xramtest_OBJS=ramtest.o
xramtest_LKR=../link/bootstrap.ld

romtest_OBJS=romtest.o
romtest_LKR=../link/bootstrap.ld

dump_OBJS=dump.o
dump_LKR=../link/bootstrap.ld

dumpeee2_OBJS=dumpee_e2.o
dumpeee2_LKR=../link/bootstrap.ld

TARGETS=stage2 ramtest xramtest romtest dump dumpeee2

include ../make/bootstrap.mk

.PHONY: mon
mon:
	../tools/upload.py --reset=rts --term --fast --srec ../monitor/monitor.srec

.PHONY: memtest
test:
	../tools/upload.py --reset=rts --fast ramtest.bin

.PHONY: xmemtest
test:
	../tools/upload.py --reset=rts --fast xramtest.bin

.PHONY: romtest
test:
	../tools/upload.py --reset=rts --fast romtest.bin

.PHONY: read
read:
	../tools/upload.py --reset=rts --term --fast dump.bin

