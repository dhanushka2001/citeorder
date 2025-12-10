#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#define MAX_LINES 5000
#define MAX_ENTRIES 200
#define MAX_LINE_LEN 1024

typedef struct {
    char *label;
    int newNum;
    int lineIdx;
    const char *text;
} FullEntry;

typedef struct {
    char *label;
    int newNum;
    int lineIdx;
    char *pos;       // pointer to start of citation in line '['
    FullEntry *ref;
} InText;

/* Define my own strdup and strndup functions */
#ifndef HAVE_STRDUP
static char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy; // caller will have to free
}
#define strdup my_strdup
#endif

#ifndef HAVE_STRNDUP
static char *my_strndup(const char *s, size_t n) {
    char *copy = malloc(n);
    if (copy) {
        memcpy(copy, s, n);
        copy[n] = '\0';
    }
    return copy; // caller will have to free
}
#define strndup my_strndup
#endif

void markCodeBlocks(const char **lines, int lineCount, int *isCodeLine) {
    int insideFence = 0;
    for (int i = 0; i < lineCount; i++) {
	    const char *line = lines[i];
    
	    // skip leading spaces
	    while (*line && isspace((unsigned char)*line)) {
	        line++;
	    }

        if (strncmp(line, "```", 3) == 0) {
            insideFence = insideFence == 0 ? 1 : 0; // toggle
            isCodeLine[i] = 1; // mark fence line as code
        } else {
            isCodeLine[i] = insideFence;
        }	
    }
}

// Check if a given portion of a line (e.g. '[^footnote]') is inside inline code: start='[', end=']'
int isInsideInlineCode(const char *line, const char *start, const char *end) {
    bool inCode = 0;
    int cite_idx = (int)(start - line); // index of '[' for this citation [^citeNum]
    // scan left-to-right from beginning of line to just before '[' in [^citeNum]
    for (int i = 0; i < cite_idx - 1; i++) {
	    if (line[i] == '`' && line[i+1] == '`') {
	        inCode = !inCode; // toggle
	        i += 1; // skip next iteration
	    }
    }
    const char *p = strstr(end + 1, "``"); // find first occurrence of '``' after ']' in [^citeNum]
    if (inCode && p) return 1;
    else return 0;
}

int findInText(const char *line, const char **pos, char **label) {
    const char *p;
    if (!pos || *pos == NULL)
        p = strstr(line, "[^");
    else
        p = strstr(*pos, "[^");

    if (!p) return 0;

    const char *end = strchr(p + 2, ']');
    if (!end) return 0; // no closing bracket anywhere → no citation in this line
    
    // check if footnote is inside inline code
    if (isInsideInlineCode(line, p, end)) return 0;

    // Extract raw label (trim spaces inside)
    const char *start = p + 2;
    while (*start && isspace((unsigned char)*start)) start++; // skip leading spaces

    const char *finish = end - 1;
    while (finish >= start && isspace((unsigned char)*finish)) finish--; // trim trailing spaces

    size_t len = finish - start + 1; // main() will handle case where len=0

    *label = strndup(start, len); // caller must free
    *pos = end + 1; // citation [^something] was found, move one space past ']'

    return 1; // return 1 to keep while loop searching for single/stacked in-text footnotes
}

// returns 1 if a valid full-entry footnote is found, else 0
int findFullEntry(const char *line, char **label, const char **body) {
    // must start with '[^', thus don't need to worry about being inside inline code

    // skip leading spaces
    while (*line && isspace((unsigned char)*line)) {
        line++;
    }

    if (strncmp(line, "[^", 2) != 0) return 0;

    const char *p = line + 2; // skip "[^"

    // find closing bracket
    const char *end = strchr(p, ']');
    if (!end) return 0;

    // next character must be ':'
    if (*(end + 1) != ':') return 0;

    // extract label
    size_t len = end - p;

    // allocate label string
    *label = strndup(p, len);  // caller must free
    *body = end + 2;           // after "]:"

    return 1;
}

// scan line right-to-left looking for a '"' or a ']'
int backScanForQuote(const char *line, int pos) {
    for (int i = pos - 1; i >= 0; i--) {
        // check if hit a previous in-text citation before reaching a quote
   	    if (i > 2 && line[i] == ']') {
   	        if (isdigit((unsigned char)line[i - 1]) &&
                line[i - 2] == '^' && line[i - 3] == '[') {
   	    	    return -2;
   	        }
   	    }
        if (line[i] == '"') {
	        return i;
        }
    }
    // did not find '"' or '[^n]' in line
    return -1;
}

// Check if citation is properly after quotes/punctuation
int hasProperQuoteContext(const char **lines, int lineNum, const char *pos) {
    const char *line = lines[lineNum];
    
    // pos given from findInText(), the position after ']' in [^citeNum]
    int q = (int)(pos - line);
    // search backwards from ']' to have q point to '['
    while (q > 0) {
        if (line[q-1] == '[' && line[q] == '^') {
            q--;
            break;
        }
        q--;
    }

    int cite_idx = q; //(int)(p - line);   // index of '[' for this exact unique citation [^citeNum]

    q--; // q pointing to the left of cite_idx '['
    
    // check if cite_idx '[' is to the left of a closing footnote ']'
    if (line[q] == ']') {
        // scan backwards for '[^', leaving enough room for at least "A"
        while (q > 3) {
            // if to the left of a stack, return true (already checked footnotes to the left) 
            if (line[q-1] == '[' && line[q] == '^') {
                return 1;
            }
            q--;
        }
        // could not find opening '[^' for ']' to its left, therefore not proper quote context
        return 0;
    }

    int end_quote = 0;
    char c = line[cite_idx - 1];
    // check if cite_idx '[' is to the left of an end quote '"'
    if (c == '"') {
	    end_quote = cite_idx - 1;
    }
    // allow at most 1 punctuation directly after the end quote
    if (c == ',' || c == '.' || c == ';' || c == ':' || c == '?' || c == '!' || c == ')') {
	    if (line[cite_idx - 2] == '"') {
	        end_quote = cite_idx - 2;
	    }
    }
    
    // If there is no closing quote before the citation, it's invalid.
    if (end_quote == 0) return 0;

    // Scan right-to-left to before closing quote to find opening quote, must be before a previous in-text citation, could be in a line prior.
    int var = backScanForQuote(line, end_quote);
    if (var == -2) return 0; // backScanForQuote() found an in-text citation before an opening quote
    if (var >= 0) return 1; // backScanForQuote() found the opening quote


    // did not find opening quote in the same line, loop through all previous lines to find it
    for (int i = 0; i < lineNum; i++) {
	    const char *pline = lines[lineNum - 1 - i];
	    size_t len = strlen(pline);
	    int var = backScanForQuote(pline, len);
	    if (var == -2) return 0;
	    if (var >= 0) return 1;
    }
    // could not find an opening quote in any of the lines
    return 0;
}

// Update in-text citations in a line, keeping stacked citations sorted
void updateLineInTexts(char *line, InText *inTexts, int inCount, int lineIdx) {
    char *p = line;
    // recursively search for start of footnotes (single or stacked)
    while ((p = strstr(p, "[^")) != NULL) {
        char *stackStart = p;
        char *stackEnd = p;
        int count = 0;

        // count stacked [^...]
        while (stackEnd && strncmp(stackEnd, "[^", 2) == 0) {
            char *endbr = strchr(stackEnd, ']');
            if (!endbr) break;
            stackEnd = endbr + 1;
            count++;
        }

        if (count > 1) {
            // --- stacked citation ---
            const int MAX_STACK = 16;
            int nums[MAX_STACK];
            char *q = stackStart;
            int k = 0;
            int any_missing = 0;
        
            // Parse each [^ ... ] within the stack without mutating the line
            while (k < count) {
                // find next "[^"
                char *footnoteStart = strstr(q, "[^");
                if (!footnoteStart || footnoteStart >= stackEnd) break;
        
                char *footnoteEnd = strchr(footnoteStart, ']');
                if (!footnoteEnd || footnoteEnd >= stackEnd) break;
        
                // label bounds (don't mutate)
                char *label_s = footnoteStart + 2;        // first char after "[^"
                char *label_e = footnoteEnd - 1;          // last char before ']'
        
                // trim by scanning (no memmove)
                while (label_s <= label_e && isspace((unsigned char)*label_s)) label_s++;
                while (label_e >= label_s && isspace((unsigned char)*label_e)) label_e--;
        
                // copy trimmed label
                char labelbuf[128];
                if (label_s > label_e) {
                    labelbuf[0] = '\0';
                } else {
                    int lablen = (int)(label_e - label_s + 1);
                    if (lablen >= (int)sizeof(labelbuf)) lablen = (int)sizeof(labelbuf) - 1;
                    memcpy(labelbuf, label_s, (size_t)lablen);
                    labelbuf[lablen] = '\0';
                }
        
                // lookup newNum
                int newNum = -1;
                for (int j = 0; j < inCount; j++) {
                    if (inTexts[j].lineIdx == lineIdx && strcmp(inTexts[j].label, labelbuf) == 0) {
                        newNum = inTexts[j].newNum;
                        break;
                    }
                }
                if (newNum == -1) any_missing = 1;
                nums[k++] = newNum;
        
                // advance q to just after this footnote's closing ']'
                q = footnoteEnd + 1;
            }
        
            // If something went wrong (missing mapping or parsed fewer tokens), skip modifying this stack
            if (k != count || any_missing) {
                // safe fallback: do not touch this stack
                p = stackEnd;
                continue;
            }
        
            // sort nums ascending
            for (int a = 0; a < k - 1; a++)
                for (int b = a + 1; b < k; b++)
                    if (nums[a] > nums[b]) {
                        int tmp = nums[a]; nums[a] = nums[b]; nums[b] = tmp;
                    }
        
            // build replacement stack string
            char newStack[512];
            size_t np = 0;
            for (int i = 0; i < k; i++) {
                int written = snprintf(newStack + np, sizeof(newStack) - np, "[^%d]", nums[i]);
                if (written < 0 || (size_t)written >= sizeof(newStack) - np) break; // overflow safe-guard
                np += (size_t)written;
            }
            newStack[np] = '\0';
        
            // replace the entire original stack (stackStart .. stackEnd-1) with newStack
            size_t tail_len = strlen(stackEnd);
            memmove(stackStart + np, stackEnd, tail_len + 1); // shift tail
            memcpy(stackStart, newStack, np);                 // write new stack
        
            // continue scanning after replaced stack
            p = stackStart + np;
        } else {
            // --- single citation ---
            char *caret = strchr(stackStart, '^');   // points at '^'
            if (!caret) { p = stackStart + 2; continue; }

            char *endbr = strchr(caret, ']');        // points at ']'
            if (!endbr) { p = caret + 1; continue; }

            // compute trimmed label boundaries (do NOT mutate the original string)
            char *label_s = caret + 1;   // first char after '^'
            char *label_e = endbr - 1;   // last char before ']'
            while (label_s <= label_e && isspace((unsigned char)*label_s)) label_s++;
            while (label_e >= label_s && isspace((unsigned char)*label_e)) label_e--;

            // copy trimmed label to a buffer for strcmp
            char labelbuf[128];
            int lablen = (label_s > label_e) ? 0 : (int)(label_e - label_s + 1);
            if (lablen >= (int)sizeof(labelbuf)) lablen = (int)sizeof(labelbuf) - 1;
            if (lablen > 0) memcpy(labelbuf, label_s, (size_t)lablen);
            labelbuf[lablen] = '\0';

            // look up the new number
            int newNum = -1;
            for (int j = 0; j < inCount; j++) {
                if (inTexts[j].lineIdx == lineIdx && strcmp(inTexts[j].label, labelbuf) == 0) {
                    newNum = inTexts[j].newNum;
                    break;
                }
            }

            if (newNum != -1) {
                // write the numeric label immediately after '^' (caret+1),
                // then move the ']' and tail to be immediately after the number
                char newStr[16];
                int nlen = snprintf(newStr, sizeof(newStr), "%d", newNum);

                // move tail (from ']' onward) to its new position
                memmove(caret + 1 + nlen, endbr, strlen(endbr) + 1); // includes '\0'

                // copy the digits into place (overwriting any leading spaces)
                memcpy(caret + 1, newStr, (size_t)nlen);

                // advance p to just after the closing ']'
                p = caret + 1 + nlen + 1;
            } else {
                // no mapping found; leave it alone and continue after the ']'
                p = endbr + 1;
            }
        }
    }
}

void print_version(void) {
    printf("  citeorder 1.2.1 (GPL-3.0-or-later)\n");
    printf("  Copyright (c) 2025 Dhanushka Jayagoda\n");
#if defined(__clang__)
    printf("  Built with clang %s\n", __clang_version__);
#elif defined(__GNUC__)
    printf("  Built with gcc %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    printf("  Built with MSVC %d\n", _MSC_VER);
#else
    printf("  Built with unknown compiler\n");
#endif
    printf("  Build date: %s, %s\n", __DATE__, __TIME__);
    printf("  Homepage: https://github.com/dhanushka2001/citeorder\n");
}

void print_help(void) {
    printf("citeorder - reorder Markdown footnotes\n\n");
    printf("Usage:\n");
    printf("  citeorder [options] input.md\n\n");
    printf("Description:\n");
    printf("  Processes a Markdown file and reorders its footnotes.\n");
    printf("  The result is written to 'input-fixed.md'.\n\n");
    printf("Options:\n");
    printf("  -q, --relaxed-quotes       Relaxed handling of quotation marks\n");
    printf("  -d, --relaxed-duplicates   Relaxed handling of duplicate footnotes (auto-increment)\n");
    printf("  -h, --help                 Show this help message\n");
    printf("  -v, --version              Show program version\n\n");
    printf("Version:\n");
    print_version();
}

// helper: check if string is purely digits
bool isNumeric(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; ++p) {
        if (!isdigit((unsigned char)*p))
            return false;
    }
    return true;
}

// helper: check if a citation changed
bool entryChanged(const char *label, int newNum) {
    char numStr[16];
    snprintf(numStr, sizeof(numStr), "%d", newNum);
    return !isNumeric(label) || strcmp(label, numStr) != 0;
}

void cleanup(const char **lines, int lineCount, FullEntry *fullEntries, InText *inTexts, int fullCount, int inCount) {
    for (int i = 0; i < lineCount; i++) {
        if(lines[i] != NULL) {
            // Cast away the 'const' qualifier to satisfy free() function signature
            free((char *)lines[i]);
            lines[i] = NULL; // optional, good practice
        }
    }
    // No need to free(lines) because the array itself is on the stack.
    
    for (int i = 0; i < fullCount; i++) {
        if (fullEntries[i].label != NULL) {
            free(fullEntries[i].label);
            fullEntries[i].label == NULL;
        }
    }
    for (int i = 0; i < inCount; i++) {
        if (inTexts[i].label != NULL) {
            free(inTexts[i].label);
            inTexts[i].label == NULL;
        }
    }
}

int main(int argc, char **argv) {
    int relaxedQuotes = 0;
    int incrementDuplicates = 0;
    int num_dup_full_entry = 0;
    const char *dup_full_entry = NULL;
    int num_dup_in_text = 0;
    const char *filename = NULL;

    if (argc < 2) { 
	    printf("citeorder: missing operand\nUsage: 'citeorder [options] input.md'\nHelp: 'citeorder [-h|--help]'\n");
	    return 1;
    }


    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--relaxed-quotes") == 0) {
	        relaxedQuotes = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--relaxed-duplicates") == 0) {
            incrementDuplicates = 1;
	    } else {
	        filename = argv[i];
	    }
    }
    
    FILE *f = fopen(filename, "r");
    if (!f) { 
	    // perror("fopen");
	    fprintf(stderr,
		        "citeorder: file '%s' does not exist\nUsage: 'citeorder [options] input.md'\nHelp: 'citeorder [-h|--help]'\n",
		        filename);
	    return 1;
    }

    const char *lines[MAX_LINES]; // "lines" is a pointer to an array of const char pointers
    int lineCount=0;
    char buf[MAX_LINE_LEN];

    while(fgets(buf,sizeof(buf),f)) {
        lines[lineCount] = strdup(buf);
        lineCount++;
    }
    fclose(f);

    int isCodeLine[lineCount];
    markCodeBlocks(lines, lineCount, isCodeLine);

    FullEntry fullEntries[MAX_ENTRIES];
    // FullEntry *fullEntries = malloc(MAX_ENTRIES * sizeof(FullEntry));
    int fullCount = 0;
    InText inTexts[MAX_ENTRIES*2];
    // InText *inTexts = malloc(MAX_ENTRIES * sizeof(InTexts));
    int inCount = 0;

    // Collect full-entry citations
    // ----------------------------
    for (int i = 0; i < lineCount; i++) {
        if (strstr(lines[i], "]:") && !isCodeLine[i]) {
            char *label = NULL;
            const char *body;
            if (findFullEntry(lines[i], &label, &body)) {
                // check if label has length=0
                if (strlen(label) == 0) {
                    fprintf(stderr, "ERROR: [^%s] full-entry citation missing label (line %d)\n",
                            label, i+1);
                    cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                    return 1;
                }
                // check if label contains any spaces
                for (int k = 0; k < strlen(label); k++) {
                    if (isspace(label[k])) {
                        fprintf(stderr, "ERROR: [^%s] full-entry citation contains a space (line %d)\n",
                                label, i+1);
                        cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                        return 1;
                    }
                }
                // loop through current list of full-entries to check if duplicate
                for (int j = 0; j < fullCount; j++) {
                    if (strcmp(fullEntries[j].label, label) == 0) {
                        // ONE duplicate allowed
                        if (incrementDuplicates) {
                            // first duplicate found
                            if (dup_full_entry == NULL) {
                                dup_full_entry = strdup(label);
                                num_dup_full_entry = 2;
                                // printf("First full-entry dupe found, line %d\n",
                                //         i+1);
                                // printf("num_dup_full_entry = %d\n",
                                //         num_dup_full_entry);
                            // first duplicate previously found already
                            } else {
                                // this duplicate is DIFFERENT from first duplicate found
                                if (strcmp(label, dup_full_entry) != 0) {
                                    fprintf(stderr,"ERROR: relaxed-duplicates (-d) mode allows only ONE full-entry duplicate (found: [^%s] and [^%s] duplicates)\n",
                                            dup_full_entry, label);
                                    cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                                    return 1;
                                // this duplicate is the SAME as first duplicate found
                                } else {
                                    num_dup_full_entry++;
                                    // printf("Dupe found, line %d\n",
                                    //         i+1);
                                    // printf("num_dup_full_entry = %d\n",
                                    //         num_dup_full_entry);
                                    break;
                                }
                            }
                        // NO duplicates allowed
                        } else {
                            fprintf(stderr,"ERROR: duplicate [^%s] full-entry citations (line %d and %d)\n",
                                    label, fullEntries[j].lineIdx+1, i+1);
                            printf("Help: Use the '-d' flag to relax duplicate handling. Run 'citeorder -h' for more info\n");
                            cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                            return 1;
                        }
                    }
                }

                fullEntries[fullCount].label    = label;    // store strndup'd label
                fullEntries[fullCount].lineIdx  = i;
                fullEntries[fullCount].text     = lines[i];
                fullEntries[fullCount].newNum   = 0;        // assign later
                fullCount++;
                
                // printf("Line %d: New full_entry: {label: %s, line: %d, newNum: %d}\n",
                //         i+1,
                //         fullEntries[fullCount-1].label,
                //         fullEntries[fullCount-1].lineIdx,
                //         fullEntries[fullCount-1].newNum);
            }
        }
    }
    // printf("FINISHED COLLECTING FULL-ENTRIES.\n");

    // Collect in-text citations and assign sequential new numbers
    // -----------------------------------------------------------
    int nextNum = 1;
    for (int i = 0; i < lineCount; i++){
        if (strstr(lines[i], "]:") || isCodeLine[i]) continue; // skip full-entry lines or code blocks

        const char *pos=NULL;
        char *label = NULL;
        
        // recursively check lines[i] for in-text footnotes
        while (findInText(lines[i], &pos, &label)){
            // check if label has length<=0
            if (strlen(label) == 0) {
                fprintf(stderr, "ERROR: in-text citation [^%s] missing label (line %d)\n",
                        label, i+1);
                cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                return 1;
            }
            // check if label contains any spaces
            for (int k = 0; k < strlen(label); k++) {
                if (isspace(label[k])) {
                    fprintf(stderr, "ERROR: in-text citation [^%s] contains a space (line %d)\n",
                            label, i+1);
                    cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                    return 1;
                }
            }
            // check if in-text matches duplicate full-entry
            if (incrementDuplicates) {
                if (strcmp(label, dup_full_entry) == 0) {
                    num_dup_in_text++;
                    // printf("num_dup_in_text = %d, line: %d\n",
                    //         num_dup_in_text, i+1);
                }
                // check if number of duplicate in-texts > number of duplicate full-entries
                if (num_dup_in_text > num_dup_full_entry) {
                    fprintf(stderr,"ERROR: relaxed-duplicates (-d) mode expects EQUAL number of full-entry and in-text duplicates (found: %d [^%s] full-entries, %d [^%s] in-texts)\n",
                            num_dup_full_entry, label, num_dup_in_text, label);
                    cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                    return 1;
                }
            }
            // find the corresponding full entry
            FullEntry *entry=NULL;
            for (int j = 0; j < fullCount; j++) {
                if(strcmp(fullEntries[j].label, label) == 0) {
                    entry = &fullEntries[j];
                    // skip if duplicates allowed and matched full-entry already assigned
                    if (incrementDuplicates && entry->newNum != 0) {
                        // printf("Full-entry on line %d already assigned\n",
                        //         entry->lineIdx + 1);
                        continue;
                    } else {
                        break;
                    }
                }
            }
            if(!entry) {
                fprintf(stderr,"ERROR: in-text citation [^%s] without full-entry (line %d)\n", label, i+1);
                // cleanup
                cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                return 1;
            }
	        if (!relaxedQuotes) {
                if(!hasProperQuoteContext(lines, i, pos)) {
                    fprintf(stderr,"ERROR: in-text citation [^%s] not properly quoted (line %d)\n", label, i+1);
                    printf("Help: Use the '-q' flag to relax quote handling. Run 'citeorder -h' for more info\n");
                    // cleanup
                    cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
                    return 1;
		        }
            }

            // assign matching full-entry the next number if not already assigned
            if(entry->newNum == 0){
                entry->newNum = nextNum++;
            }

            inTexts[inCount].label      = label;
            inTexts[inCount].newNum     = entry->newNum;
            inTexts[inCount].lineIdx    = i;
            inTexts[inCount].ref        = entry;
            inCount++;
            
            // printf("Line %d: New in_text: {label: %s, line: %d, newNum: %d}\n",
            //         i+1,
            //         inTexts[inCount-1].label,
            //         inTexts[inCount-1].lineIdx,
            //         inTexts[inCount-1].newNum);
            // printf("Matching full-entry: {label: %s, line: %d, newNum: %d}\n",
            //         entry->label,
            //         entry->lineIdx,
            //         entry->newNum);
        }
    }
    // printf("FINISHED COLLECTING IN-TEXTS.\n");
    // check if number of duplicate in-texts < number of duplicate full-entries
    if (num_dup_in_text < num_dup_full_entry) {
        fprintf(stderr,"ERROR: relaxed-duplicates (-d) mode expects EQUAL number of full-entry and in-text duplicates (found: %d [^%s] full-entries, %d [^%s] in-texts)\n",
                num_dup_full_entry, dup_full_entry, num_dup_in_text, dup_full_entry);
        cleanup(lines, lineCount, fullEntries, inTexts, fullCount, inCount);
        return 1;
    }

    // Unused fullEntries get bubbled to the top
    // -----------------------------------------
    int numUnusedFullEntry = 0;
    for (int i = 0; i < fullCount; i++) {
	    if (fullEntries[i].newNum == 0) {
	        numUnusedFullEntry++;
	    }
    }
    int k = 1;
    for (int j = 0; j < fullCount; j++) {
        if (fullEntries[j].newNum == 0) {
            fullEntries[j].newNum = fullCount - numUnusedFullEntry + k;
	        k++;
        }
    }

    // Check if anything changed
    // -------------------------
    bool changed = false;
    for (int i = 0; i < fullCount; i++) {
        if (entryChanged(fullEntries[i].label, fullEntries[i].newNum)) {
            changed = true;
            break;
        }
    }
    for (int j = 0; j < inCount; j++) {
        if (entryChanged(inTexts[j].label, inTexts[j].newNum)) {
            changed = true;
            break;
        }
    }
    // printf("Ready for outputting...\n");
    // Output to new file
    // ------------------
    if (changed) {
        char outName[512];
        char base[256];
        strncpy(base, filename, sizeof(base));
        base[sizeof(base)-1]='\0';
        char *dot = strrchr(base, '.');
        if(dot && strcmp(dot,".md")==0) *dot='\0';
        snprintf(outName,sizeof(outName),"%s-fixed.md", base);
        
        FILE *out=fopen(outName,"w");
        if(!out){ perror("fopen"); return 1; }
        
        // Update lines
	    int i = 0;
        int dup_count = 0;
        while (i < lineCount){
            // inside code block?
            if (isCodeLine[i]) {
           	    // char *lineCopy = strdup(lines[i]);
                // fputs(lineCopy, out);
                // free(lineCopy);
                fputs(lines[i], out);
                i++;
                continue; 
            } else if (!strstr(lines[i], "]:")) {
    	    	// --- in-text line ---
           	    char *lineCopy = strdup(lines[i]);
           	    updateLineInTexts(lineCopy, inTexts, inCount, i);
           	    fputs(lineCopy, out);
           	    free(lineCopy);
    		    i++;
            } else {
                // --- full entry line ---
                // inside code block?
                // if (isCodeLine[i]) {
                //     fputs(lines[i], out);
                //     i++;
                //     continue; 
                // }
                const char *p = strstr(lines[i], "[^");
                if (!p) {
                    fputs(lines[i], out);
                    i++;
                    continue;
                }
                const char *q = strstr(p + 2, "]");
                if (!q) {
                    fputs(lines[i], out);
                    i++;
                    continue;
                }
                if (isInsideInlineCode(lines[i], p, q)) {
                    fputs(lines[i], out);
                    i++;
                    continue;
                }

                int start = i;
                int end = i;
                char *label;
                const char *body;
                while (end < lineCount - 1 && findFullEntry(lines[end+1], &label, &body)) {
                    end++;
                }
            
                // Collect block entries
                int blockSize = end - start + 1;
                // printf("blockSize = %d\n", blockSize);
                FullEntry **block = malloc(blockSize * sizeof(*block));
                int k = 0;
                int dup_skip = 0;
                for (int j = start; j <= end; j++) {
                    // find full-entry
                    if (findFullEntry(lines[j], &label, &body)) {
                        // find matching full-entry in fullEntries
                        for (int fe = 0; fe < fullCount; fe++) {
                            if (strcmp(fullEntries[fe].label, label) == 0) {
                                if (incrementDuplicates && strcmp(label, dup_full_entry) == 0) {
                                    if (dup_skip == 0) {
                                        dup_count++;
                                        dup_skip = dup_count - 1;
                                    } else {
                                        dup_skip--;
                                    }
                                }
                                if (!incrementDuplicates || dup_skip == 0) {
                                    block[k++] = &fullEntries[fe];
                                    break;
                                }
                            }
                        }
                        free(label);
                    }
                }
            
                // Sort block by newNum
                for (int a = 0; a < k - 1; a++) {
                    for (int b = a + 1; b < k; b++) {
                        if (block[a]->newNum > block[b]->newNum) {
                            FullEntry *tmp = block[a];
                            block[a] = block[b];
                            block[b] = tmp;
                        }
                    }
                }
            
                // Print block in order
                for (int a = 0; a < k; a++) {
                    const FullEntry *fe = block[a];
            
                    // Construct updated line
                    const char *orig = fe->text;
                    const char *colon = strchr(orig, ':');  // should always exist
                    if (!colon) continue;
            
                    char newMarker[32];
                    snprintf(newMarker, sizeof(newMarker), "[^%d]:", fe->newNum);
            
                    // add leading spaces
                    int piv=0;
                    while (isspace((unsigned char)*(orig+piv))) {
                        piv++;
                        fputs(" ", out);
                    }

                    // Print new marker + remainder of original line (after the ':')
                    fputs(newMarker, out);
                    fputs(colon + 1, out);
            
                    // Ensure newline
                    size_t len = strlen(orig);
                    if (len == 0 || orig[len - 1] != '\n') {
                        fputc('\n', out);
                    }
                }
            
                free(block);
                i = end + 1;
            }
        }
        fclose(out);
	    printf("Output written to %s\n", outName);
    } else {
	    printf("No changes required.\n");
    }
    return 0;
}
