default: $(TARGETS)

define BUILD_TARGET
.PHONY: S(1)
$(1): $(1).bin $(1).srec $(1).txt
	@size $(1).elf

$(1).elf: $$($(1)_OBJS) $$($(1)_LKR)
	m68hc11-elf-ld -T $$($(1)_LKR) -o $(1).elf -Map $(1).map $$($(1)_OBJS)

$(1).bin: $(1).elf
	m68hc11-elf-objcopy -j .text -j .vectors --gap-fill=0xFF -O binary $(1).elf $(1).bin

$(1).srec: $(1).elf
	m68hc11-elf-objcopy -O srec $(1).elf $(1).srec

$(1).txt: $(1).elf
	m68hc11-elf-objdump -S $(1).elf > $(1).txt

$(1)_clean:
	rm -f $(1).bin $(1).srec $(1).elf $(1).txt $(1).map $($(1)_OBJS)
endef

$(foreach TARGET, $(TARGETS), $(eval $(call BUILD_TARGET,$(TARGET))))

%.o:%.asm
	m68hc11-elf-as -I../inc -o $@ $<

.PHONY: clean
clean: $(addsuffix _clean, $(TARGETS))

