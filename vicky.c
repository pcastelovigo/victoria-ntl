#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//
// Original patterns
//
unsigned char START_YEAR[] = {0x2B, 0x07, 0x88, 0x5C};             // 1835
unsigned char END_YEAR_1[] = {0x91, 0x07, 0x88, 0x5C};             // 1937
unsigned char END_YEAR_2[] = {0x90, 0x07, 0x00, 0x00, 0xF4, 0x01}; // 1936

#define LEN_START (sizeof(START_YEAR))
#define LEN1 (sizeof(END_YEAR_1))
#define LEN2 (sizeof(END_YEAR_2))

//
// Find pattern in a binary buffer
//
long find_pattern(unsigned char *buf, long size,
                  unsigned char *pat, long pat_len)
{
    for (long i = 0; i <= size - pat_len; i++)
        if (memcmp(buf + i, pat, pat_len) == 0)
            return i;
    return -1;
}

//
// Build replacement blocks
//
void make_start_block(uint16_t year, unsigned char block[LEN_START])
{
    block[0] = year & 0xFF;
    block[1] = (year >> 8) & 0xFF;
    block[2] = 0x88;
    block[3] = 0x5C;
}

void make_end1_block(uint16_t year, unsigned char block[LEN1])
{
    block[0] = year & 0xFF;
    block[1] = (year >> 8) & 0xFF;
    block[2] = 0x88;
    block[3] = 0x5C;
}

void make_end2_block(uint16_t year, unsigned char block[LEN2])
{
    block[0] = year & 0xFF;
    block[1] = (year >> 8) & 0xFF;
    block[2] = 0x00;
    block[3] = 0x00;
    block[4] = 0xF4;
    block[5] = 0x01;
}

//
// Print usage instructions
//
void print_usage(const char *prog)
{
    printf("Usage: %s [file] [--end-year=YYYY] [--start-year=YYYY]\n", prog);
    printf("\nIf no file is provided, 'Victoria.exe' will be used by default.\n");
    printf("\nOptions:\n");
    printf("  --end-year=YYYY     Set END YEAR 1 & END YEAR 2 (default: 2038)\n");
    printf("  --start-year=YYYY   Also patch START YEAR (optional)\n");
    printf("\nA backup is always saved as <file>.back\n");
}

//
// Create backup file
//
int create_backup(const char *filename)
{
    char backup_name[512];
    snprintf(backup_name, sizeof(backup_name), "%s.back", filename);

    FILE *src = fopen(filename, "rb");
    if (!src) {
        perror("Error opening file for backup");
        return 0;
    }

    FILE *dst = fopen(backup_name, "wb");
    if (!dst) {
        perror("Error creating backup file");
        fclose(src);
        return 0;
    }

    unsigned char buffer[4096];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, n, dst);

    fclose(src);
    fclose(dst);

    printf("[*] Backup created: %s\n", backup_name);
    return 1;
}

//
// Main program
//
int main(int argc, char *argv[])
{
    char *filename = NULL;
    uint16_t end_year = 2038;
    uint16_t start_year = 0;
    int patch_start = 0;

    // Help command
    if (argc >= 2 && 
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(argv[0]);
        return 0;
    }

    // Default file if none provided OR first argument is an option
    if (argc >= 2 && strncmp(argv[1], "--", 2) != 0) {
        filename = argv[1];
    } else {
        filename = "Victoria.exe";
        printf("[*] No file parameter detected - using default: %s\n", filename);
    }

    // Parse remaining arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--end-year=", 11) == 0) {
            end_year = atoi(argv[i] + 11);
        }
        else if (strncmp(argv[i], "--start-year=", 13) == 0) {
            start_year = atoi(argv[i] + 13);
            patch_start = 1;
        }
    }

    printf("[*] Target file: %s\n", filename);
    printf("[*] END YEAR => %u\n", end_year);
    printf("[*] START YEAR => %s\n", patch_start ? "will be patched" : "unchanged");

    //
    // Create backup first
    //
    if (!create_backup(filename)) {
        printf("[!] Backup failed. Aborting.\n");
        return 1;
    }

    //
    // Load file
    //
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Error opening input file");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *buf = malloc(size);
    if (!buf) {
        printf("Error: insufficient memory.\n");
        fclose(f);
        return 1;
    }

    fread(buf, 1, size, f);
    fclose(f);

    int patches = 0;

    //
    // Patch START YEAR
    //
    if (patch_start) {
        long off = find_pattern(buf, size, START_YEAR, LEN_START);
        if (off >= 0) {
            unsigned char block[LEN_START];
            make_start_block(start_year, block);
            memcpy(buf + off, block, LEN_START);
            printf("[+] START YEAR patched at offset 0x%lX\n", off);
            patches++;
        } else {
            printf("[-] START YEAR pattern not found\n");
        }
    }

    //
    // Patch END YEAR 1
    //
    long off1 = find_pattern(buf, size, END_YEAR_1, LEN1);
    if (off1 >= 0) {
        unsigned char block1[LEN1];
        make_end1_block(end_year, block1);
        memcpy(buf + off1, block1, LEN1);
        printf("[+] END YEAR 1 patched at offset 0x%lX\n", off1);
        patches++;
    } else {
        printf("[-] END YEAR 1 pattern not found\n");
    }

    //
    // Patch END YEAR 2
    //
    long off2 = find_pattern(buf, size, END_YEAR_2, LEN2);
    if (off2 >= 0) {
        unsigned char block2[LEN2];
        make_end2_block(end_year, block2);
        memcpy(buf + off2, block2, LEN2);
        printf("[+] END YEAR 2 patched at offset 0x%lX\n", off2);
        patches++;
    } else {
        printf("[-] END YEAR 2 pattern not found\n");
    }

    //
    // Write patched file
    //
    FILE *out = fopen(filename, "wb");
    if (!out) {
        perror("Error writing patched file");
        free(buf);
        return 1;
    }

    fwrite(buf, 1, size, out);
    fclose(out);

    if (patches > 0) {
        printf("[✓] Patch completed (%d modifications).\n", patches);
        printf("    Patched file saved as: %s\n", filename);
        printf("    Backup saved as: %s.back\n", filename);
    } else {
        printf("[!] No matching patterns found. Backup remains unchanged.\n");
    }

    free(buf);
    return 0;
}

