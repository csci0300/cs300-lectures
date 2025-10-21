#include "kernel.hh"
#include "lib.hh"
#include "elf.h"
#include "k-apic.hh"
#include "k-pci.hh"
#include "k-vmiter.hh"
#include <atomic>


// k-hardware.cc
//
//    Functions for interacting with x86 hardware.


// init_hardware
//    Initialize hardware. Calls other functions below.

pcistate pcistate::state;

static void init_kernel_memory();
static void init_interrupts();
static void init_constructors();
static void init_cpu_state();
static void stash_kernel_data(bool restore);
extern "C" { extern void exception_entry(); }
extern "C" { extern void syscall_entry(); }

void init_hardware() {
    // initialize kernel virtual memory structures
    init_kernel_memory();

    // initialize console position
    cursorpos = 3 * CONSOLE_COLUMNS;

    // initialize interrupt descriptors and controller
    init_interrupts();

    // call C++ constructors for global objects
    // (NB none of these constructors may allocate memory)
    init_constructors();

    // initialize this CPU
    init_cpu_state();
}


// init_kernel_memory
//    Set up early-stage segment registers and kernel page table.
//
//    The early-stage segment registers and global descriptors are
//    used during hardware initialization. The kernel page table is
//    used whenever no appropriate process page table exists.
//
//    The interrupt descriptor table tells the processor where to jump
//    when an interrupt or exception happens. See k-exception.S.
//
//    The layouts of these types are defined by the hardware.

static void set_app_segment(uint64_t* segment, uint64_t type, int dpl) {
    *segment = type
        | X86SEG_S                    // code/data segment
        | ((uint64_t) dpl << 45)
        | X86SEG_P;                   // segment present
}

static void set_sys_segment(uint64_t* segment, uintptr_t addr, size_t size,
                            uint64_t type, int dpl) {
    segment[0] = ((addr & 0x0000000000FFFFFFUL) << 16)
        | ((addr & 0x00000000FF000000UL) << 32)
        | ((size - 1) & 0x0FFFFUL)
        | (((size - 1) & 0xF0000UL) << 48)
        | type
        | ((uint64_t) dpl << 45)
        | X86SEG_P;                   // segment present
    segment[1] = addr >> 32;
}

static void set_gate(x86_64_gatedescriptor* gate, uintptr_t addr,
                     int type, int dpl, int ist) {
    assert(unsigned(type) < 16 && unsigned(dpl) < 4 && unsigned(ist) < 8);
    gate->gd_low = (addr & 0x000000000000FFFFUL)
        | (SEGSEL_KERN_CODE << 16)
        | (uint64_t(ist) << 32)
        | (uint64_t(type) << 40)
        | (uint64_t(dpl) << 45)
        | X86SEG_P
        | ((addr & 0x00000000FFFF0000UL) << 32);
    gate->gd_high = addr >> 32;
}

x86_64_pagetable kernel_pagetable[5];
uint64_t kernel_gdt_segments[7];
static x86_64_taskstate kernel_taskstate;

void init_kernel_memory() {
    stash_kernel_data(false);

    // initialize segments
    kernel_gdt_segments[0] = 0;
    set_app_segment(&kernel_gdt_segments[SEGSEL_KERN_CODE >> 3],
                    X86SEG_X | X86SEG_L, 0);
    set_app_segment(&kernel_gdt_segments[SEGSEL_KERN_DATA >> 3],
                    X86SEG_W, 0);
    set_app_segment(&kernel_gdt_segments[SEGSEL_APP_CODE >> 3],
                    X86SEG_X | X86SEG_L, 3);
    set_app_segment(&kernel_gdt_segments[SEGSEL_APP_DATA >> 3],
                    X86SEG_W, 3);
    set_sys_segment(&kernel_gdt_segments[SEGSEL_TASKSTATE >> 3],
                    (uintptr_t) &kernel_taskstate, sizeof(kernel_taskstate),
                    X86SEG_TSS, 0);
    x86_64_pseudodescriptor gdt;
    gdt.limit = (sizeof(uint64_t) * 3) - 1;
    gdt.base = (uint64_t) kernel_gdt_segments;

    asm volatile("lgdt %0" : : "m" (gdt.limit));


    // initialize kernel page table
    memset(kernel_pagetable, 0, sizeof(kernel_pagetable));
    kernel_pagetable[0].entry[0] =
        (x86_64_pageentry_t) &kernel_pagetable[1] | PTE_P | PTE_W | PTE_U;
    kernel_pagetable[1].entry[0] =
        (x86_64_pageentry_t) &kernel_pagetable[2] | PTE_P | PTE_W | PTE_U;
    kernel_pagetable[2].entry[0] =
        (x86_64_pageentry_t) &kernel_pagetable[3] | PTE_P | PTE_W | PTE_U;
    kernel_pagetable[2].entry[1] =
        (x86_64_pageentry_t) &kernel_pagetable[4] | PTE_P | PTE_W | PTE_U;

    // the kernel can access [1GiB,4GiB) of physical memory,
    // which includes important memory-mapped I/O devices
    kernel_pagetable[1].entry[1] =
        (1UL << 30) | PTE_P | PTE_W | PTE_PS;
    kernel_pagetable[1].entry[2] =
        (2UL << 30) | PTE_P | PTE_W | PTE_PS;
    kernel_pagetable[1].entry[3] =
        (3UL << 30) | PTE_P | PTE_W | PTE_PS;

    // user-accessible mappings for physical memory,
    // except that (for debuggability) nullptr is totally inaccessible
    for (vmiter it(kernel_pagetable);
         it.va() < MEMSIZE_PHYSICAL;
         it += PAGESIZE) {
        if (it.va() != 0) {
            it.map(it.va(), PTE_P | PTE_W | PTE_U);
        }
    }

    wrcr3((uintptr_t) kernel_pagetable);


    // Now that boot-time structures (pagetable and global descriptor
    // table) have been replaced, we can reuse boot-time memory.
}


void init_constructors() {
    typedef void (*constructor_function)();
    extern constructor_function __init_array_start[];
    extern constructor_function __init_array_end[];
    for (auto fp = __init_array_start; fp != __init_array_end; ++fp) {
        (*fp)();
    }
}


// init_interrupts

// processor state for taking an interrupt
extern x86_64_gatedescriptor interrupt_descriptors[256];

void init_interrupts() {
    // initialize interrupt descriptors
    // Macros in `k-exception.S` initialized `interrupt_descriptors[]`
    // with function pointers in the `gd_low` members. We must change
    // them to the weird format x86-64 expects.
    for (int i = 0; i < 256; ++i) {
        uintptr_t addr = interrupt_descriptors[i].gd_low;
        set_gate(&interrupt_descriptors[i], addr,
                 X86GATE_INTERRUPT, i == INT_BP ? 3 : 0, 0);
    }

    // ensure machine has an enabled APIC
    assert(cpuid(1).edx & (1 << 9));
    uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);
    assert(apic_base & IA32_APIC_BASE_ENABLED);
    assert((apic_base & 0xFFFFFFFFF000) == lapicstate::lapic_pa);

    // disable the old programmable interrupt controller
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)
    outb(IO_PIC1 + 1, 0xFF);
    outb(IO_PIC2 + 1, 0xFF);
}


void init_cpu_state() {
    // taskstate lets the kernel receive interrupts
    memset(&kernel_taskstate, 0, sizeof(kernel_taskstate));
    kernel_taskstate.ts_rsp[0] = KERNEL_STACK_TOP;

    x86_64_pseudodescriptor gdt;
    gdt.limit = sizeof(kernel_gdt_segments) - 1;
    gdt.base = (uint64_t) kernel_gdt_segments;

    x86_64_pseudodescriptor idt;
    idt.limit = sizeof(interrupt_descriptors) - 1;
    idt.base = (uint64_t) interrupt_descriptors;

    // load segment descriptor tables
    asm volatile("lgdt %0; ltr %1; lidt %2"
                 :
                 : "m" (gdt.limit),
                   "r" ((uint16_t) SEGSEL_TASKSTATE),
                   "m" (idt.limit)
                 : "memory", "cc");

    // initialize segments
    asm volatile("movw %%ax, %%fs; movw %%ax, %%gs"
                 : : "a" ((uint16_t) SEGSEL_KERN_DATA));


    // set up control registers
    uint32_t cr0 = rdcr0();
    cr0 |= CR0_PE | CR0_PG | CR0_WP | CR0_AM | CR0_MP | CR0_NE;
    wrcr0(cr0);


    // set up syscall/sysret
    wrmsr(MSR_IA32_STAR, (uintptr_t(SEGSEL_KERN_CODE) << 32)
          | (uintptr_t(SEGSEL_APP_CODE) << 48));
    wrmsr(MSR_IA32_LSTAR, reinterpret_cast<uint64_t>(syscall_entry));
    wrmsr(MSR_IA32_FMASK, EFLAGS_TF | EFLAGS_DF | EFLAGS_IF
          | EFLAGS_IOPL_MASK | EFLAGS_AC | EFLAGS_NT);


    // initialize local APIC (interrupt controller)
    auto& lapic = lapicstate::get();
    lapic.enable_lapic(INT_IRQ + IRQ_SPURIOUS);

    // timer is in periodic mode
    lapic.write(lapic.reg_timer_divide, lapic.timer_divide_1);
    lapic.write(lapic.reg_lvt_timer,
                lapic.timer_periodic | (INT_IRQ + IRQ_TIMER));
    lapic.write(lapic.reg_timer_initial_count, 0);

    // disable logical interrupt lines
    lapic.write(lapic.reg_lvt_lint0, lapic.lvt_masked);
    lapic.write(lapic.reg_lvt_lint1, lapic.lvt_masked);

    // set LVT error handling entry
    lapic.write(lapic.reg_lvt_error, INT_IRQ + IRQ_ERROR);

    // clear error status by reading the error;
    // acknowledge any outstanding interrupts
    lapic.error();
    lapic.ack();
}


// init_timer(rate)
//    Set the timer interrupt to fire `rate` times a second. Disables the
//    timer interrupt if `rate <= 0`.

void init_timer(int rate) {
    auto& lapic = lapicstate::get();
    if (rate > 0) {
        lapic.write(lapic.reg_timer_initial_count, 1000000000 / rate);
    } else {
        lapic.write(lapic.reg_timer_initial_count, 0);
    }
}


// check_pagetable
//    Validate a page table by checking that important kernel procedures
//    are mapped at the expected addresses.

void check_pagetable(x86_64_pagetable* pagetable) {
    assert(((uintptr_t) pagetable & PAGEOFFMASK) == 0); // must be page aligned
    assert(vmiter(pagetable, (uintptr_t) exception_entry).pa()
           == (uintptr_t) exception_entry);
    assert(vmiter(kernel_pagetable, (uintptr_t) pagetable).pa()
           == (uintptr_t) pagetable);
    assert(vmiter(pagetable, (uintptr_t) kernel_pagetable).pa()
           == (uintptr_t) kernel_pagetable);
}


// set_pagetable
//    Change page table after checking it.

void set_pagetable(x86_64_pagetable* pagetable) {
    check_pagetable(pagetable);
    wrcr3((uintptr_t) pagetable);
}


// reserved_physical_address(pa)
//    Returns true iff `pa` is a reserved physical address.

#define IOPHYSMEM       0x000A0000
#define EXTPHYSMEM      0x00100000

bool reserved_physical_address(uintptr_t pa) {
    return pa < PAGESIZE || (pa >= IOPHYSMEM && pa < EXTPHYSMEM);
}


// allocatable_physical_address(pa)
//    Returns true iff `pa` is an allocatable physical address, i.e.,
//    not reserved or holding kernel data.

bool allocatable_physical_address(uintptr_t pa) {
    extern uint8_t _kernel_end;
    return !reserved_physical_address(pa)
        && (pa < KERNEL_START_ADDR
            || pa >= round_up((uintptr_t)&_kernel_end, PAGESIZE))
        && (pa < KERNEL_STACK_TOP - PAGESIZE
            || pa >= KERNEL_STACK_TOP)
        && pa < MEMSIZE_PHYSICAL;
}


// pcistate::next(addr)
//    Return the next valid PCI function after `addr`, or -1 if there
//    is none.
int pcistate::next(int addr) const {
    uint32_t x = readl(addr + config_lthb);
    while (true) {
        if (addr_func(addr) == 0
            && (x == uint32_t(-1) || !(x & 0x800000))) {
            addr += make_addr(0, 1, 0);
        } else {
            addr += make_addr(0, 0, 1);
        }
        if (addr >= addr_end) {
            return -1;
        }
        x = readl(addr + config_lthb);
        if (x != uint32_t(-1)) {
            return addr;
        }
    }
}

void pcistate::enable(int addr) {
    // enable I/O (0x01), memory (0x02), and bus master (0x04)
    writew(addr + config_command, 0x0007);
}


// poweroff
//    Turn off the virtual machine. This requires finding a PCI device
//    that speaks ACPI.

void poweroff() {
    auto& pci = pcistate::get();
    int addr = pci.find([&] (int a) {
            uint32_t vd = pci.readl(a + pci.config_vendor);
            return vd == 0x71138086U /* PIIX4 Power Management Controller */
                || vd == 0x29188086U /* ICH9 LPC Interface Controller */;
        });
    if (addr >= 0) {
        // Read I/O base register from controller's PCI configuration space.
        int pm_io_base = pci.readl(addr + 0x40) & 0xFFC0;
        // Write `suspend enable` to the power management control register.
        outw(pm_io_base + 4, 0x2000);
    }
    // No known ACPI controller; spin.
    console_printf(CPOS(24, 0), 0xC000, "Cannot power off!\n");
    while (true) {
    }
}


// reboot
//    Reboot the virtual machine.

void reboot() {
    outb(0x92, 3); // does not return
    while (true) {
    }
}


// init_process(p, flags)
//    Initialize special-purpose registers for process `p`.

void init_process(proc* p, int flags) {
    memset(&p->regs, 0, sizeof(p->regs));
    p->regs.reg_cs = SEGSEL_APP_CODE | 3;
    p->regs.reg_fs = SEGSEL_APP_DATA | 3;
    p->regs.reg_gs = SEGSEL_APP_DATA | 3;
    p->regs.reg_ss = SEGSEL_APP_DATA | 3;
    p->regs.reg_rflags = EFLAGS_IF;

    if (flags & PROCINIT_ALLOW_PROGRAMMED_IO) {
        p->regs.reg_rflags |= EFLAGS_IOPL_3;
    }
    if (flags & PROCINIT_DISABLE_INTERRUPTS) {
        p->regs.reg_rflags &= ~EFLAGS_IF;
    }
}


// console_show_cursor(cpos)
//    Move the console cursor to position `cpos`, which should be between 0
//    and 80 * 25.

void console_show_cursor(int cpos) {
    if (cpos < 0 || cpos > CONSOLE_ROWS * CONSOLE_COLUMNS) {
        cpos = 0;
    }
    outb(0x3D4, 14);
    outb(0x3D5, cpos / 256);
    outb(0x3D4, 15);
    outb(0x3D5, cpos % 256);
}



// keyboard_readc
//    Read a character from the keyboard. Returns -1 if there is no character
//    to read, and 0 if no real key press was registered but you should call
//    keyboard_readc() again (e.g. the user pressed a SHIFT key). Otherwise
//    returns either an ASCII character code or one of the special characters
//    listed in kernel.hh.

// Unfortunately mapping PC key codes to ASCII takes a lot of work.

#define MOD_SHIFT       (1 << 0)
#define MOD_CONTROL     (1 << 1)
#define MOD_CAPSLOCK    (1 << 3)

#define KEY_SHIFT       0372
#define KEY_CONTROL     0373
#define KEY_ALT         0374
#define KEY_CAPSLOCK    0375
#define KEY_NUMLOCK     0376
#define KEY_SCROLLLOCK  0377

#define CKEY(cn)        0x80 + cn

static const uint8_t keymap[256] = {
    /*0x00*/ 0, 033, CKEY(0), CKEY(1), CKEY(2), CKEY(3), CKEY(4), CKEY(5),
        CKEY(6), CKEY(7), CKEY(8), CKEY(9), CKEY(10), CKEY(11), '\b', '\t',
    /*0x10*/ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', CKEY(12), CKEY(13), CKEY(14), KEY_CONTROL, 'a', 's',
    /*0x20*/ 'd', 'f', 'g', 'h', 'j', 'k', 'l', CKEY(15),
        CKEY(16), CKEY(17), KEY_SHIFT, CKEY(18), 'z', 'x', 'c', 'v',
    /*0x30*/ 'b', 'n', 'm', CKEY(19), CKEY(20), CKEY(21), KEY_SHIFT, '*',
        KEY_ALT, ' ', KEY_CAPSLOCK, 0, 0, 0, 0, 0,
    /*0x40*/ 0, 0, 0, 0, 0, KEY_NUMLOCK, KEY_SCROLLLOCK, '7',
        '8', '9', '-', '4', '5', '6', '+', '1',
    /*0x50*/ '2', '3', '0', '.', 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x60*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x70*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x80*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0x90*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, CKEY(14), KEY_CONTROL, 0, 0,
    /*0xA0*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0xB0*/ 0, 0, 0, 0, 0, '/', 0, 0,  KEY_ALT, 0, 0, 0, 0, 0, 0, 0,
    /*0xC0*/ 0, 0, 0, 0, 0, 0, 0, KEY_HOME,
        KEY_UP, KEY_PAGEUP, 0, KEY_LEFT, 0, KEY_RIGHT, 0, KEY_END,
    /*0xD0*/ KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
    /*0xE0*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
    /*0xF0*/ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
};

static const struct keyboard_key {
    uint8_t map[4];
} complex_keymap[] = {
    /*CKEY(0)*/ {{'1', '!', 0, 0}},  /*CKEY(1)*/ {{'2', '@', 0, 0}},
    /*CKEY(2)*/ {{'3', '#', 0, 0}},  /*CKEY(3)*/ {{'4', '$', 0, 0}},
    /*CKEY(4)*/ {{'5', '%', 0, 0}},  /*CKEY(5)*/ {{'6', '^', 0, 036}},
    /*CKEY(6)*/ {{'7', '&', 0, 0}},  /*CKEY(7)*/ {{'8', '*', 0, 0}},
    /*CKEY(8)*/ {{'9', '(', 0, 0}},  /*CKEY(9)*/ {{'0', ')', 0, 0}},
    /*CKEY(10)*/ {{'-', '_', 0, 037}},  /*CKEY(11)*/ {{'=', '+', 0, 0}},
    /*CKEY(12)*/ {{'[', '{', 033, 0}},  /*CKEY(13)*/ {{']', '}', 035, 0}},
    /*CKEY(14)*/ {{'\n', '\n', '\r', '\r'}},
    /*CKEY(15)*/ {{';', ':', 0, 0}},
    /*CKEY(16)*/ {{'\'', '"', 0, 0}},  /*CKEY(17)*/ {{'`', '~', 0, 0}},
    /*CKEY(18)*/ {{'\\', '|', 034, 0}},  /*CKEY(19)*/ {{',', '<', 0, 0}},
    /*CKEY(20)*/ {{'.', '>', 0, 0}},  /*CKEY(21)*/ {{'/', '?', 0, 0}}
};

int keyboard_readc() {
    static uint8_t modifiers;
    static uint8_t last_escape;

    if ((inb(KEYBOARD_STATUSREG) & KEYBOARD_STATUS_READY) == 0) {
        return -1;
    }

    uint8_t data = inb(KEYBOARD_DATAREG);
    uint8_t escape = last_escape;
    last_escape = 0;

    if (data == 0xE0) {         // mode shift
        last_escape = 0x80;
        return 0;
    } else if (data & 0x80) {   // key release: matters only for modifier keys
        int ch = keymap[(data & 0x7F) | escape];
        if (ch >= KEY_SHIFT && ch < KEY_CAPSLOCK) {
            modifiers &= ~(1 << (ch - KEY_SHIFT));
        }
        return 0;
    }

    int ch = (unsigned char) keymap[data | escape];

    if (ch >= 'a' && ch <= 'z') {
        if (modifiers & MOD_CONTROL) {
            ch -= 0x60;
        } else if (!(modifiers & MOD_SHIFT) != !(modifiers & MOD_CAPSLOCK)) {
            ch -= 0x20;
        }
    } else if (ch >= KEY_CAPSLOCK) {
        modifiers ^= 1 << (ch - KEY_SHIFT);
        ch = 0;
    } else if (ch >= KEY_SHIFT) {
        modifiers |= 1 << (ch - KEY_SHIFT);
        ch = 0;
    } else if (ch >= CKEY(0) && ch <= CKEY(21)) {
        ch = complex_keymap[ch - CKEY(0)].map[modifiers & 3];
    } else if (ch < 0x80 && (modifiers & MOD_CONTROL)) {
        ch = 0;
    }

    return ch;
}


// symtab: reference to kernel symbol table; useful for debugging.
// The `mkchickadeesymtab` function fills this structure in.
#define SYMTAB_ADDR 0x1000000
elf_symtabref symtab = {
    reinterpret_cast<elf_symbol*>(SYMTAB_ADDR), 0, nullptr, 0
};

// lookup_symbol(addr, name, start)
//    Use the debugging symbol table to look up `addr`. Return the
//    corresponding symbol name (usually a function name) in `*name`
//    and the first address in that function in `*start`.

__no_asan
bool lookup_symbol(uintptr_t addr, const char** name, uintptr_t* start) {
    if (rdcr3() != (uintptr_t) kernel_pagetable) {
        return false;
    } else if (!kernel_pagetable[2].entry[SYMTAB_ADDR / 0x200000]) {
        kernel_pagetable[2].entry[SYMTAB_ADDR / 0x200000] =
            SYMTAB_ADDR | PTE_P | PTE_W | PTE_PS;
    }

    size_t l = 0;
    size_t r = symtab.nsym;
    while (l < r) {
        size_t m = l + ((r - l) >> 1);
        auto& sym = symtab.sym[m];
        if (sym.st_value <= addr
            && (sym.st_size != 0
                ? addr < sym.st_value + sym.st_size
                : m + 1 == symtab.nsym || addr < (&sym)[1].st_value)) {
            if (name) {
                *name = symtab.strtab + symtab.sym[m].st_name;
            }
            if (start) {
                *start = symtab.sym[m].st_value;
            }
            return true;
        } else if (symtab.sym[m].st_value < addr) {
            l = m + 1;
        } else {
            r = m;
        }
    }
    return false;
}


// log_printf, log_vprintf
//    Print debugging messages to the host's `log.txt` file. We run QEMU
//    so that messages written to the QEMU "parallel port" end up in `log.txt`.

#define IO_PARALLEL1_DATA       0x378
#define IO_PARALLEL1_STATUS     0x379
# define IO_PARALLEL_STATUS_BUSY        0x80
#define IO_PARALLEL1_CONTROL    0x37A
# define IO_PARALLEL_CONTROL_SELECT     0x08
# define IO_PARALLEL_CONTROL_INIT       0x04
# define IO_PARALLEL_CONTROL_STROBE     0x01

static void delay() {
    (void) inb(0x84);
    (void) inb(0x84);
    (void) inb(0x84);
    (void) inb(0x84);
}

static void parallel_port_putc(unsigned char c) {
    static int initialized;
    if (!initialized) {
        outb(IO_PARALLEL1_CONTROL, 0);
        initialized = 1;
    }

    for (int i = 0;
         i < 12800 && (inb(IO_PARALLEL1_STATUS) & IO_PARALLEL_STATUS_BUSY) == 0;
         ++i) {
        delay();
    }
    outb(IO_PARALLEL1_DATA, c);
    outb(IO_PARALLEL1_CONTROL, IO_PARALLEL_CONTROL_SELECT
         | IO_PARALLEL_CONTROL_INIT | IO_PARALLEL_CONTROL_STROBE);
    outb(IO_PARALLEL1_CONTROL, IO_PARALLEL_CONTROL_SELECT
         | IO_PARALLEL_CONTROL_INIT);
}

namespace {
struct log_printer : public printer {
    void putc(unsigned char c, int) override {
        parallel_port_putc(c);
    }
};
}

void log_vprintf(const char* format, va_list val) {
    log_printer p;
    p.vprintf(0, format, val);
}

void log_printf(const char* format, ...) {
    va_list val;
    va_start(val, format);
    log_vprintf(format, val);
    va_end(val);
}


namespace {
struct backtracer {
    uintptr_t rbp_;
    uintptr_t rsp_;
    uintptr_t stack_top_;

    backtracer(uintptr_t rbp, uintptr_t rsp, uintptr_t stack_top)
        : rbp_(rbp), rsp_(rsp), stack_top_(stack_top) {
    }
    bool ok() const {
        return rbp_ >= rsp_ && stack_top_ - rbp_ >= 16;
    }
    uintptr_t ret_rip() const {
        uintptr_t* rbpx = reinterpret_cast<uintptr_t*>(rbp_);
        return rbpx[1];
    }
    void step() {
        uintptr_t* rbpx = reinterpret_cast<uintptr_t*>(rbp_);
        rsp_ = rbp_ + 16;
        rbp_ = rbpx[0];
    }
};
}

// log_backtrace(prefix[, rsp, rbp])
//    Print a backtrace to `log.txt`, each line prefixed by `prefix`.

void log_backtrace(const char* prefix) {
    log_backtrace(prefix, rdrsp(), rdrbp());
}

void log_backtrace(const char* prefix, uintptr_t rsp, uintptr_t rbp) {
    if (rsp != rbp && round_up(rsp, PAGESIZE) == round_down(rbp, PAGESIZE)) {
        log_printf("%s  warning: possible stack overflow (rsp %p, rbp %p)\n",
                   rsp, rbp);
    }
    int frame = 1;
    for (backtracer bt(rbp, rsp, round_up(rsp, PAGESIZE));
         bt.ok();
         bt.step(), ++frame) {
        uintptr_t ret_rip = bt.ret_rip();
        const char* name;
        if (lookup_symbol(ret_rip, &name, nullptr)) {
            log_printf("%s  #%d  %p  <%s>\n", prefix, frame, ret_rip, name);
        } else if (ret_rip) {
            log_printf("%s  #%d  %p\n", prefix, frame, ret_rip);
        }
    }
}


// error_vprintf, error_printf
//    Print debugging messages to the console and to the host's
//    `log.txt` file via `log_printf`.

__noinline
int error_vprintf(int cpos, int color, const char* format, va_list val) {
    va_list val2;
    __builtin_va_copy(val2, val);
    log_vprintf(format, val2);
    va_end(val2);
    return console_vprintf(cpos, color, format, val);
}

__noinline
int error_printf(int cpos, int color, const char* format, ...) {
    va_list val;
    va_start(val, format);
    cpos = error_vprintf(cpos, color, format, val);
    va_end(val);
    return cpos;
}

__noinline
void error_printf(int color, const char* format, ...) {
    va_list val;
    va_start(val, format);
    error_vprintf(-1, color, format, val);
    va_end(val);
}

__noinline
void error_printf(const char* format, ...) {
    va_list val;
    va_start(val, format);
    error_vprintf(-1, COLOR_ERROR, format, val);
    va_end(val);
}


// check_keyboard
//    Check for the user typing a control key. 'a', 'e', 'r', and 'x'
//    cause a soft where the kernel runs alice, eve, recurse, or alice+eve,
//    respectively. Control-C or 'q' exit the virtual machine.
//    Returns key typed or -1 for no key.

int check_keyboard() {
    int c = keyboard_readc();
    if (c == 'a' || c == 'e' || c == 'r' || c == 'x') {
        // Turn off the timer interrupt.
        init_timer(-1);
        // Install a temporary page table to carry us through the
        // process of reinitializing memory. This replicates work the
        // bootloader does.
        x86_64_pagetable* pt = (x86_64_pagetable*) 0x8000;
        memset(pt, 0, PAGESIZE * 2);
        pt[0].entry[0] = 0x9000 | PTE_P | PTE_W;
        pt[1].entry[0] = PTE_P | PTE_W | PTE_PS;
        wrcr3((uintptr_t) pt);
        // The soft reboot process doesn't modify memory, so it's
        // safe to pass `multiboot_info` on the kernel stack, even
        // though it will get overwritten as the kernel runs.
        uint32_t multiboot_info[5];
        multiboot_info[0] = 4;
        const char* argument = "aliceandeve";
        if (c == 'a') {
            argument = "alice";
        } else if (c == 'e') {
            argument = "eve";
        } else if (c == 'r') {
            argument = "recurse";
        }
        uintptr_t argument_ptr = (uintptr_t) argument;
        assert(argument_ptr < 0x100000000L);
        multiboot_info[4] = (uint32_t) argument_ptr;
        // restore initial value of data segment for reboot support
        stash_kernel_data(true);
        extern uint8_t _data_start, _edata, _kernel_end;
        uintptr_t data_size = (uintptr_t) &_edata - (uintptr_t) &_data_start;
        uintptr_t zero_size = (uintptr_t) &_kernel_end - (uintptr_t) &_edata;
        uint8_t* data_stash = (uint8_t*) (SYMTAB_ADDR - data_size);
        memcpy(&_data_start, data_stash, data_size);
        memset(&_edata, 0, zero_size);
        // restart kernel
        asm volatile("movl $0x2BADB002, %%eax; jmp kernel_entry"
                     : : "b" (multiboot_info) : "memory");
    } else if (c == 0x03 || c == 'q') {
        poweroff();
    }
    return c;
}


// fail
//    Loop until user presses Control-C, then poweroff.

[[noreturn]] void fail() {
    while (true) {
        check_keyboard();
    }
}


// panic, assert_fail
//    Use console_printf() to print a failure message and then wait for
//    control-C. Also write the failure message to the log.

bool panicking;

void panic(const char* format, ...) {
    va_list val;
    va_start(val, format);
    panicking = true;

    cursorpos = CPOS(24, 80);
    if (format) {
        // Print panic message to both the screen and the log
        error_printf(-1, COLOR_ERROR, "PANIC: ");
        error_vprintf(-1, COLOR_ERROR, format, val);
        if (CCOL(cursorpos)) {
            error_printf(-1, COLOR_ERROR, "\n");
        }
    } else {
        error_printf(-1, COLOR_ERROR, "PANIC");
        log_printf("\n");
    }

    va_end(val);
    fail();
}

void assert_fail(const char* file, int line, const char* msg) {
    cursorpos = CPOS(23, 0);
    error_printf("%s:%d: kernel assertion '%s' failed\n", file, line, msg);

    uintptr_t rsp = rdrsp();
    int frame = 1;
    for (backtracer bt(rdrbp(), rsp, round_up(rsp, PAGESIZE));
         bt.ok();
         bt.step(), ++frame) {
        uintptr_t ret_rip = bt.ret_rip();
        const char* name;
        if (lookup_symbol(ret_rip, &name, nullptr)) {
            error_printf("  #%d  %p  <%s>\n", frame, ret_rip, name);
        } else if (ret_rip) {
            error_printf("  #%d  %p\n", frame, ret_rip);
        }
    }
    fail();
}


// program_loader
//    This type iterates over the loadable segments in a binary.

extern uint8_t _binary_obj_p_eve_start[];
extern uint8_t _binary_obj_p_eve_end[];
extern uint8_t _binary_obj_p_alice_start[];
extern uint8_t _binary_obj_p_alice_end[];
extern uint8_t _binary_obj_p_recurse_start[];
extern uint8_t _binary_obj_p_recurse_end[];

struct ramimage {
    const char* name;
    void* begin;
    void* end;
} ramimages[] = {
    { "eve", _binary_obj_p_eve_start, _binary_obj_p_eve_end },
    { "alice", _binary_obj_p_alice_start, _binary_obj_p_alice_end },
    { "recurse", _binary_obj_p_recurse_start, _binary_obj_p_recurse_end }
};

program_loader::program_loader(int program_number) {
    elf_ = nullptr;
    if (program_number >= 0
        && size_t(program_number) < sizeof(ramimages) / sizeof(ramimages[0])) {
        elf_ = (elf_header*) ramimages[program_number].begin;
    }
    reset();
}
int program_loader::program_number(const char* program_name) {
    for (size_t i = 0; i != sizeof(ramimages) / sizeof(ramimages[0]); ++i) {
        if (strcmp(program_name, ramimages[i].name) == 0) {
            return i;
        }
    }
    return -1;
}
program_loader::program_loader(const char* program_name)
    : program_loader(program_number(program_name)) {
}
void program_loader::fix() {
    while (ph_ && ph_ != endph_ && ph_->p_type != ELF_PTYPE_LOAD) {
        ++ph_;
    }
}
uintptr_t program_loader::va() const {
    return ph_ != endph_ ? ph_->p_va : 0;
}
size_t program_loader::size() const {
    return ph_ != endph_ ? ph_->p_memsz : 0;
}
const char* program_loader::data() const {
    return ph_ != endph_ ? (const char*) elf_ + ph_->p_offset : nullptr;
}
size_t program_loader::data_size() const {
    return ph_ != endph_ ? ph_->p_filesz : 0;
}
bool program_loader::present() const {
    return ph_ != endph_;
}
bool program_loader::writable() const {
    return ph_ != endph_ && (ph_->p_flags & ELF_PFLAG_WRITE);
}
uintptr_t program_loader::entry() const {
    return elf_ ? elf_->e_entry : 0;
}
void program_loader::operator++() {
    if (ph_ != endph_) {
        ++ph_;
        fix();
    }
}
void program_loader::reset() {
    if (elf_) {
        assert(elf_->e_magic == ELF_MAGIC);
        // XXX should check that no ELF pointers go beyond the data!
        ph_ = (elf_program*) ((uint8_t*) elf_ + elf_->e_phoff);
        endph_ = ph_ + elf_->e_phnum;
        fix();
    } else {
        ph_ = endph_ = nullptr;
    }
}


// Functions required by the C++ compiler and calling convention

namespace std {
const nothrow_t nothrow;
}

extern "C" {
// The __cxa_guard functions control the initialization of static variables.

// __cxa_guard_acquire(guard)
//    Return 0 if the static variables guarded by `*guard` are already
//    initialized. Otherwise lock `*guard` and return 1. The compiler
//    will initialize the statics, then call `__cxa_guard_release`.
int __cxa_guard_acquire(long long* arg) {
    std::atomic<char>* guard = reinterpret_cast<std::atomic<char>*>(arg);
    if (guard->load(std::memory_order_relaxed) == 2) {
        return 0;
    }
    while (true) {
        char old_value = guard->exchange(1);
        if (old_value == 2) {
            guard->exchange(2);
            return 0;
        } else if (old_value == 1) {
            pause();
        } else {
            return 1;
        }
    }
}

// __cxa_guard_release(guard)
//    Mark `guard` to indicate that the static variables it guards are
//    initialized.
void __cxa_guard_release(long long* arg) {
    std::atomic<char>* guard = reinterpret_cast<std::atomic<char>*>(arg);
    guard->store(2);
}

// __cxa_pure_virtual()
//    Used as a placeholder for pure virtual functions.
void __cxa_pure_virtual() {
    panic("pure virtual function called in kernel!\n");
}

// __dso_handle, __cxa_atexit
//    Used to destroy global objects at "program exit". We don't bother.
void* __dso_handle;
void __cxa_atexit(...) {
}

}

// stash_kernel_data
//    Soft reboot requires that we restore kernel data memory to
//    its initial state, so we store its initial state in unused
//    physical memory.
static void stash_kernel_data(bool reboot) {
    // stash initial value of data segment for soft-reboot support
    extern uint8_t _data_start, _edata, _kernel_end;
    uintptr_t data_size = (uintptr_t) &_edata - (uintptr_t) &_data_start;
    uint8_t* data_stash = (uint8_t*) (SYMTAB_ADDR - data_size);
    if (reboot) {
        memcpy(&_data_start, data_stash, data_size);
        memset(&_edata, 0, &_kernel_end - &_edata);
    } else {
        memcpy(data_stash, &_data_start, data_size);
    }
}


// `proc` members have fixed offsets
static_assert(offsetof(proc, pagetable) == 0, "proc::pagetable has bad offset");
static_assert(offsetof(proc, state) == 12, "proc::state has bad offset");
static_assert(offsetof(proc, regs) == 16, "proc::refs has bad offset");
