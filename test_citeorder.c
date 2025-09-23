#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

FILE *junit;
int failures = 0;

// Helper to write a test file
// ---------------------------
void write_file(const char *filename, const char *content) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("fopen"); exit(1); }
    fputs(content, f);
    fclose(f);
}

// Helper to read entire file into string
// --------------------------------------
char* read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");   // binary mode, avoids CRLF surprises
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t nread = fread(buf, 1, len, f);
    buf[nread] = '\0';   // terminate where we *actually* stopped reading

    fclose(f);
    return buf;
}

// Helper to run citeorder and capture output
// ------------------------------------------
int run_citeorder(const char *inputFile, const char *outputFile,
		  const char *stdoutFile, const char *stderrFile)
{
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd),
	     "citeorder.exe \"%s\" \"%s\" 1>\"%s\" 2>\"%s\"",
	     inputFile, outputFile, stdoutFile, stderrFile);
#else
    snprintf(cmd, sizeof(cmd),
	     "./citeorder \"%s\" \"%s\" 1>\"%s\" 2>\"%s\"",
	     inputFile, outputFile, stdoutFile, stderrFile);
#endif
    return system(cmd);
}

// Check if two files match
// ------------------------
bool files_match(const char *actual_file, const char *expected_file, bool ignore_path) {
    char *actual = read_file(actual_file);
    char *expected = read_file(expected_file);

    if (!actual || !expected) {
        free(actual);
        free(expected);
        return false;
    }

    // Trim trailing newlines/CR
    size_t alen = strlen(actual);
    while (alen > 0 && (actual[alen-1] == '\n' || actual[alen-1] == '\r')) {
        actual[--alen] = '\0';
    }

    size_t elen = strlen(expected);
    while (elen > 0 && (expected[elen-1] == '\n' || expected[elen-1] == '\r')) {
        expected[--elen] = '\0';
    }

    bool match = false;
    if (ignore_path) {
        const char *prefix = "Output written to ";
        size_t prefix_len = strlen(prefix);
    
        if (strncmp(actual, prefix, prefix_len) == 0 &&
            strncmp(expected, prefix, prefix_len) == 0) {
    
            // Point to the path part only
            const char *a_path = actual + prefix_len;
            const char *e_path = expected + prefix_len;
    
            // Find the last slash/backslash in the path
            const char *a_base = strrchr(a_path, '/');
    #ifdef _WIN32
            const char *b_base = strrchr(a_path, '\\');
            if (!a_base || (b_base && b_base > a_base)) a_base = b_base;
    #endif
            if (a_base) a_base++; else a_base = a_path;
    
            const char *e_base = strrchr(e_path, '/');
    #ifdef _WIN32
            const char *f_base = strrchr(e_path, '\\');
            if (!e_base || (f_base && f_base > e_base)) e_base = f_base;
    #endif
            if (e_base) e_base++; else e_base = e_path;
    
            // Compare only the basename parts
            match = strcmp(a_base, e_base) == 0;
        } else {
            // Normal exact compare for lines without the prefix
            match = strcmp(actual, expected) == 0;
        }
    } else {
        match = strcmp(actual, expected) == 0;
    }
    
    if (!match) {
        printf("DEBUG expected: \n[%s]\n", expected);
        printf("DEBUG actual:   \n[%s]\n", actual);

        // for (size_t i = 0; i < alen; i++) printf("%02X ", (unsigned char)actual[i]);
        // printf("\n");
        // for (size_t i = 0; i < elen; i++) printf("%02X ", (unsigned char)expected[i]);
        // printf("\n");
    }

    free(actual);
    free(expected);
    return match;
}

// Run a single test case
// ----------------------
void run_test_case(const char *test_name,
		   const char *inputFile,
		   const char *expectedOutputFile,
                   const char *expectedStdoutFile,
		   const char *expectedStderrFile)
{

    printf("\nRunning test: %s\n", test_name);

    char outFile[128], outStd[128], outErr[128];
    const char *outDir = "tests/output/";
    snprintf(outFile, sizeof(outFile), "tests/%s-fixed.md",       test_name);
    snprintf(outStd,  sizeof(outStd),  "%s%s_stdout.txt", outDir, test_name);
    snprintf(outErr,  sizeof(outErr),  "%s%s_stderr.txt", outDir, test_name);

    int ret = run_citeorder(inputFile, outFile, outStd, outErr);
    if (ret != 0) {
        printf("citeorder returned non-zero exit code: %d\n", ret);
    }
    
    int test_case = -1;
    bool pass = true;
    char *error_message;
    if (expectedOutputFile && expectedStdoutFile) {
        if (!files_match(outFile, expectedOutputFile, 0)) {
	    error_message = "FAIL: output file mismatch";
	    printf("%s\n", error_message);
            pass = false;
	}
	if (!files_match(outStd, expectedStdoutFile, 1)) {
	    error_message = "FAIL: stdout file mismatch";
	    printf("%s\n", error_message);
            pass = false;
	}
	test_case = 0;
    }  
    else if (expectedStdoutFile && !files_match(outStd, expectedStdoutFile, 0)) {
        error_message = "FAIL: stdout mismatch";
	printf("%s\n", error_message);
        pass = false;
	test_case = 1;
    }
    else if (expectedStderrFile && !files_match(outErr, expectedStderrFile, 0)) {
        error_message = "FAIL: stderr mismatch";
	printf("%s\n", error_message);
	pass = false;
	test_case = 2;
    }

    if (pass) {
        printf("PASS\n");
	fprintf(junit, "  <testcase classname=\"citeorder\" name=\"%s\"/>\n", test_name);
    } else {
        printf("Input file: %s\n", inputFile);
	if (test_case == 0) { printf("Check %s, %s for details\n", outFile, outStd); }
	if (test_case == 1) { printf("Check %s for details\n", outStd); }
	if (test_case == 2) { printf("Check %s for details\n", outErr); }
	fprintf(junit, "  <testcase classname=\"citeorder\" name=\"%s\">\n", test_name);
    	fprintf(junit, "    <failure message=\"%s\">TBA</failure>\n", error_message);
	fprintf(junit, "  </testcase>\n");
	failures++;
    }
}

// Example test cases
int main() {
    int total_tests = 5;
    junit = fopen("results.xml", "w");
    if (!junit) return 1;
    long headerPos = ftell(junit);
    fprintf(junit, "<testsuite name=\"citeorder\" tests=\"%d\" failures=\"%s\">\n",
	    total_tests, "F");

    // 1. No change required test
    run_test_case("no-change",
        	  "tests/no-change.md", 		    // input file
        	  NULL,				            // expected output file
                  "tests/expected/no-change_stdout.txt",    // expected stdout
        	  NULL                                      // expected stderr
    );
    // 2. Stacked renumbering test
    run_test_case("stacked",
        	  "tests/stacked.md", 	                    // input file
        	  "tests/expected/stacked-fixed.md",        // expected output file
                  "tests/expected/stacked_stdout.txt",	    // expected stdout
        	  NULL                                      // expected stderr
    );
    // 3. Missing quote test
    run_test_case("missing-quote",
                  "tests/missing-quote.md",                 // input file
                  NULL,                                     // expected output file
                  NULL,                                     // expected stdout
                  "tests/expected/missing-quote_stderr.txt" // expected stderr
    );
    // 4. Missing full-entry test
    run_test_case("missing-full",
                  "tests/missing-full.md",                  // input file
                  NULL,                                     // expected output file
                  NULL,                                     // expected stdout
                  "tests/expected/missing-full_stderr.txt"  // expected stderr
    );
    // 5. Real example
    run_test_case("test",
                  "tests/test.md",                          // input file
                  "tests/expected/test-fixed.md",           // expected output file
                  "tests/expected/test_stdout.txt",         // expected stdout
                  NULL                                      // expected stderr
    );

    fprintf(junit, "</testsuite>\n");
    
    // --- go back and patch failures ---
    fseek(junit, headerPos, SEEK_SET);
    fprintf(junit, "<testsuite name=\"citeorder\" tests=\"%d\" failures=\"%d\">",
	    total_tests, failures);
    
    fclose(junit);

    return 0;
}

