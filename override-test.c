#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
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


static volatile sig_atomic_t interrupted;

static void stop_test(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static bool send_smm(uint32_t command, uint32_t argument)
{
    struct smm_result result = read_smm(command, argument);
    return !result.carry && result.eax != command &&
           (result.eax & 0xffff) != 0xffff;
}

int main(int argc, char **argv)
{
    bool automatic_only = argc == 2 && strcmp(argv[1], "--automatic") == 0;
    bool hold = argc == 2 && (strcmp(argv[1], "--hold-medium") == 0 ||
                              strcmp(argv[1], "--hold-full") == 0);
    bool medium = argc == 2 && (strcmp(argv[1], "--medium") == 0 ||
                                strcmp(argv[1], "--hold-medium") == 0);
    if (argc != 1 && !automatic_only && !medium && !hold) {
        fputs("Usage: override-test [--automatic | --medium | --hold-medium | --hold-full]\n", stderr);
        return 1;
    }
    const uint32_t restore = 0x31a3, manual = 0x30a3;
    if (ioperm(0xb2, 1, 1) || ioperm(0x84, 1, 1)) {
        perror("Cannot access Dell firmware ports");
        return 1;
    }

    struct smm_result signature = read_smm(0xffa3, 0);
    if (signature.eax != 0x44494147 || signature.edx != 0x44454c4c)
        signature = read_smm(0xfea3, 0);
    if (signature.eax != 0x44494147 || signature.edx != 0x44454c4c) {
        fputs("Dell firmware signature unavailable. No override attempted.\n", stderr);
        return 1;
    }

    struct sigaction handler = { .sa_handler = stop_test };
    sigemptyset(&handler.sa_mask);
    if (sigaction(SIGINT, &handler, NULL) || sigaction(SIGTERM, &handler, NULL) ||
        sigaction(SIGHUP, &handler, NULL)) {
        perror("Cannot install cleanup handlers");
        return 1;
    }

    if (!send_smm(restore, 0)) {
        fputs("Firmware rejected the restore command. No override attempted.\n", stderr);
        return 1;
    }
    puts("Firmware accepted the restore command.");
    if (automatic_only)
        return 0;
    if (interrupted)
        return 1;

    int status = 1;
    if (!send_smm(manual, 0)) {
        fputs("Firmware rejected the manual override.\n", stderr);
    } else if (!interrupted && send_smm(0x01a3, medium ? 0x0100 : 0x0200)) {
        if (hold && !interrupted) {
            printf("%s speed requested. Manual override remains active.\n",
                   medium ? "Medium" : "Full");
            return 0;
        }
        printf("%s speed requested for up to eight seconds.\n", medium ? "Medium" : "Full");
        fflush(stdout);
        for (int second = 0; second < 8 && !interrupted; ++second) {
            sleep(1);
            struct smm_result rpm = read_smm(0x02a3, 0);
            if (!rpm.carry && rpm.eax != 0x02a3 && (rpm.eax & 0xffff) != 0xffff)
                printf("Raw tach: %u\n", rpm.eax & 0xffff);
            else
                puts("Raw tach unavailable.");
            fflush(stdout);
        }
        status = 0;
    } else {
        fputs("Fan speed request did not complete.\n", stderr);
    }

    if (!send_smm(restore, 0)) {
        fputs("Firmware rejected restoration. Reboot to reinitialize fan control.\n", stderr);
        return 2;
    }
    puts("Firmware accepted restoration of automatic control.");
    return status;
}
