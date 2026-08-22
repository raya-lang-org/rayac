#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

#define MAX_OUTPUT 4096
#define MAX_LINE 1024

/* Normalize an error line by stripping [E####] codes */
static const char *normalize_error(const char *s) {
    if (strncmp(s, "error[", 6) == 0) {
        const char *p = strchr(s, ']');
        if (p && p[1] == ':') return p + 3;
    }
    if (strncmp(s, "error: ", 7) == 0) return s + 7;
    return s;
}

static int is_error_line(const char *s) {
    return strncmp(s, "error:", 6) == 0 || strncmp(s, "error[", 6) == 0;
}

static int output_contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static int run_test(const char *raya, const char *test_dir, const char *name) {
    char cmd[MAX_LINE];
    char output[MAX_OUTPUT];
    char expected_path[MAX_LINE];

    snprintf(cmd, sizeof(cmd), "%s --check \"%s/%s.raya\" 2>&1",
         raya, test_dir, name);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("FAIL: %s (cannot run compiler)\n", name); return 0; }

    size_t n = fread(output, 1, sizeof(output)-1, fp);
    output[n] = '\0';
    pclose(fp);

    snprintf(expected_path, sizeof(expected_path), "%s/%s.expected", test_dir, name);
    FILE *ef = fopen(expected_path, "r");
    if (!ef) { printf("SKIP: %s (no .expected)\n", name); return 1; }

    char exp_line[MAX_LINE];
    int expect_errors = 0;
    char expected_buf[MAX_OUTPUT] = {0};
    while (fgets(exp_line, sizeof(exp_line), ef)) {
        strcat(expected_buf, exp_line);
        if (is_error_line(exp_line)) expect_errors = 1;
    }
    fclose(ef);

    if (expect_errors) {
        /* Check each expected error line appears in output */
        int ok = 1;
        char *copy = strdup(expected_buf);
        char *line = strtok(copy, "\n");
        while (line) {
            if (strlen(line) > 0 && !output_contains(output, line)
                && !output_contains(output, normalize_error(line))) {
                printf("FAIL: %s\n", name);
                printf("  Missing expected error: %s\n", line);
                printf("  Output: %s\n", output);
                ok = 0;
                break;
            }
            line = strtok(NULL, "\n");
        }
        free(copy);
        if (ok) { printf("PASS: %s\n", name); return 1; }
        return 0;
    } else {
        if (strstr(output, "error:")) {
            printf("FAIL: %s\n", name);
            printf("  Expected: ok\n");
            printf("  Got: %s\n", output);
            return 0;
        }
        printf("PASS: %s\n", name);
        return 1;
    }
}

int main(int argc, char **argv) {
    const char *raya = argc > 1 ? argv[1] : "bin\\raya.exe";
    const char *test_dir = argc > 2 ? argv[2] : "tests/sema";

    printf("=== Raya Sema Tests ===\n");
    printf("Compiler: %s\n", raya);
    printf("Test dir: %s\n\n", test_dir);

    DIR *d = opendir(test_dir);
    if (!d) { printf("Cannot open %s\n", test_dir); return 1; }

    int passed = 0, failed = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 5 && strcmp(ent->d_name + len - 5, ".raya") == 0) {
            char name[MAX_LINE];
            snprintf(name, sizeof(name), "%.*s", (int)(len - 5), ent->d_name);
            if (run_test(raya, test_dir, name)) passed++; else failed++;
        }
    }
    closedir(d);

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    return failed > 0 ? 1 : 0;
}
