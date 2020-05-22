#runtime
forth_OBJS=main.o vectors.o

forth_LKR=hc11.ld

TARGETS=forth

include ../make/programs.mk

.PHONY: boot
boot:
	../tools/upload.py \
		--reset=rts \
		--fast \
		--srec forth.srec \
		--term=9600

.PHONY: dic
dic:
	../tools/forthdic.py -dot forth.elf > dic.txt

.PHONY: sim
sim:
	../sim/sim --bin 0xE000:forth.bin

