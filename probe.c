#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/io.h>

struct smm_result {
    uint32_t eax;
    uint32_t ebx;
    uint32_t edx;
    bool carry;
};

static struct smm_result read_smm(uint32_t command, uint32_t argument)
{
    struct smm_result result = { .eax = command, .ebx = argument };
    uint32_t ecx = 0, esi = 0, edi = 0;
    asm volatile(
        "outb %%al, $0xb2\n\t"
        "outb %%al, $0x84"
        : "+a"(result.eax), "+b"(result.ebx), "+d"(result.edx),
          "+c"(ecx), "+S"(esi), "+D"(edi), "=@ccc"(result.carry)
        :
        : "memory");
    return result;
}

int main(void)
{
    if (ioperm(0xb2, 1, 1) || ioperm(0x84, 1, 1)) {
        perror("Cannot access Dell firmware ports");
        return 1;
    }

    struct smm_result signature = read_smm(0xffa3, 0);
    if (signature.eax != 0x44494147 || signature.edx != 0x44454c4c) {
        signature = read_smm(0xfea3, 0);
        if (signature.eax != 0x44494147 || signature.edx != 0x44454c4c) {
            fputs("Dell firmware signature unavailable through direct I/O.\n", stderr);
            return 1;
        }
    }

    puts("Dell direct firmware interface responded.");
    for (uint32_t fan = 0; fan < 2; ++fan) {
        struct smm_result state = read_smm(0x00a3, fan);
        struct smm_result rpm = read_smm(0x02a3, fan);
        if (!state.carry && (state.eax & 0xffff) <= 3 && !rpm.carry &&
            (rpm.eax & 0xffff) != 0xffff && rpm.eax != 0x02a3)
            printf("Firmware fan %u: state %u, raw tach %u\n",
                   fan, state.eax & 0xffff, rpm.eax & 0xffff);
        else
            printf("Firmware fan %u: no valid response\n", fan);
    }
}
