default: out.bin out.s19 out.txt

out.bin: out.elf
	m68hc11-elf-objcopy -O binary out.elf out.bin

out.s19: out.elf
	m68hc11-elf-objcopy -O srec out.elf out.s19

out.txt: out.elf
	m68hc11-elf-objdump -S out.elf > out.txt

out.elf: loader.o bootstrap.ld
	m68hc11-elf-ld -T bootstrap.ld -o out.elf -Map map.txt loader.o

loader.o: loader.asm
	m68hc11-elf-as -I../inc -o loader.o loader.asm

clean:
	rm -f out.* *.o map.txt 

