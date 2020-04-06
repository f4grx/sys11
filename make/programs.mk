default: $(TARGETS)

define BUILD_TARGET
.PHONY: $(1)
$(1): $(1).bin $(1).srec $(1).dis
	@size $(1).elf

$(1).elf: $$($(1)_OBJS) $$($(1)_LKR)
	m68hc11-elf-ld -T $$($(1)_LKR) -o $(1).elf -Map $(1).map $$($(1)_OBJS)

$(1).bin: $(1).elf
	m68hc11-elf-objcopy -j .text -j .rodata -j .vectors --gap-fill=0xFF -O binary $(1).elf $(1).bin

$(1).srec: $(1).elf
	m68hc11-elf-objcopy -j .text -j .rodata -j .vectors -O srec $(1).elf $(1).srec

$(1).dis: $(1).elf
	m68hc11-elf-objdump -ds $(1).elf > $(1).dis

$(1)_clean:
	rm -f $(1).bin $(1).srec $(1).elf $(1).dis $(1).map $($(1)_OBJS)
endef

$(foreach TARGET, $(TARGETS), $(eval $(call BUILD_TARGET,$(TARGET))))

%.o:%.asm
	m68hc11-elf-as -g -I../inc -o $@ $<

.PHONY: clean
clean: $(addsuffix _clean, $(TARGETS))

