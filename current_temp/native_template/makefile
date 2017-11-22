# The toolchain to use. arm-none-eabi works, but there does exist 
# arm-bcm2708-linux-gnueabi. ARMGNU ?= arm-none-eabi

# The intermediate directory for compiled object files.
BUILD = build/

# The directory in which source files are stored.
SOURCE = source/

# The name of the output file to generate.
TARGET = kernel.img

# The name of the assembler listing file to generate.
LIST = kernel.list

# The name of the map file to generate.
MAP = kernel.map

# The name of the linker script to use.
LINKER = kernel.ld

# The names of all object files that must be generated. Deduced from the 
# c and assembly code files in source.

OBJECTS_AS := $(patsubst $(SOURCE)%.s,$(BUILD)%.o,$(wildcard $(SOURCE)*.s))
OBJECTS_GCC := $(patsubst $(SOURCE)%.c,$(BUILD)%.o,$(wildcard $(SOURCE)*.c))
OBJECTS := $(OBJECTS_AS) $(OBJECTS_GCC)

# Rule to make everything.
all: $(TARGET) $(LIST) $(OBJECTS_GCC)

# Rule to remake everything. Does not include clean.
rebuild: all

# Rule to make the object files from C source.
$(BUILD)%.o: $(SOURCE)%.c
	gcc -I $(SOURCE) $< -c -std=gnu99 -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -ffast-math -o $@

# Rule to make the listing file.
$(LIST) : $(BUILD)output.elf
	objdump -d $(BUILD)output.elf > $(LIST)

# Rule to make the image file.
$(TARGET) : $(BUILD)output.elf
	objcopy $(BUILD)output.elf -O  binary $(TARGET) 

# Rule to make the elf file.
$(BUILD)output.elf : $(OBJECTS) $(LINKER)
	ld --no-undefined $(OBJECTS) -Map $(MAP) -o $(BUILD)output.elf -T $(LINKER)

# Rule to make the object files from Assembly sources.
$(BUILD)%.o: $(SOURCE)%.s
	as -I $(SOURCE) $< -mfpu=vfpv2 -o $@

# Rule to clean files.
clean : 
	-rm -f $(BUILD)*.o 
	-rm -f $(BUILD)output.elf
	-rm -f $(TARGET)
	-rm -f $(LIST)
	-rm -f $(MAP)
