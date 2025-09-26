#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINES 10000
#define MAX_ENTRIES 1000
#define MAX_LINE_LEN 1024

typedef struct {
    int oldNum;
    int newNum;
    int lineIdx;
    char *text;
} FullEntry;

typedef struct {
    int oldNum;
    int newNum;
    int lineIdx;
    char *pos;       // pointer to start of citation in line
    FullEntry *ref;
} InTextCitation;

// Find next citation in a line
static int findCitation(const char *line, const char **pos, int *num) {
    const char *p;
    if (!pos || *pos == NULL)
        p = strstr(line, "[^");
    else
        p = strstr(*pos, "[^");

    if (!p) return 0;

    char *end;
    long n = strtol(p+2, &end, 10);
    if (end && *end == ']') {
        *num = (int)n;
        *pos = end + 1;
        return 1;
    }
    *pos = p + 2;
    return findCitation(line, pos, num);
}

// Check if citation is properly after quotes/punctuation
static int hasProperQuoteContext(const char *line, int citeNum) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "[^%d]", citeNum);
    const char *p = strstr(line, pattern);
    if (!p) return 0;

    const char *q = p;
    int quoteCount = 0;

    while (q > line) {
        // Check if we're at the end of another citation like [^12]
        if (q - line >= 4 && q[-1] == ']' && q[-3] == '^' && q[-4] == '[') {
            // Walk backwards past the entire [^number]
            const char *r = q - 2; // position after ^
            while (r > line && isdigit((unsigned char)*r)) r--;
            if (r > line && *r == '^' && r[-1] == '[') {
                q = r - 1; // jump before this citation
                continue;
            }
        }

        if (*(--q) == '"') {
            quoteCount++;
            if (quoteCount == 2) {
                return 1; // found two quotes before this citation
            }
        }
    }

    return 0; // didn't find two quotes before reaching start
}

// Update in-text citations in a line, keeping stacked citations sorted
void updateLineInTexts(char *line, InTextCitation *inTexts, int inCount, int lineIdx) {
    char *p = line;
    while ((p = strstr(p, "[^")) != NULL) {
        // find stacked citations
        char *stackStart = p;
        char *stackEnd = p;
        int count = 0;
        while (stackEnd && strncmp(stackEnd, "[^", 2) == 0) {
            char *endbr = strchr(stackEnd, ']');
            if (!endbr) break;
            stackEnd = endbr + 1;
            count++;
        }

        if (count > 1) {
            // collect newNums for the stack
            int nums[16];
            char *q = stackStart;
            for (int k = 0; k < count; ++k) {
                char *caret = strchr(q, '^');
                int oldNum = atoi(caret+1);
                // find newNum
                int newNum = oldNum;
                for (int j = 0; j < inCount; j++) {
                    if (inTexts[j].lineIdx == lineIdx && inTexts[j].oldNum == oldNum) {
                        newNum = inTexts[j].newNum;
                        break;
                    }
                }
                nums[k] = newNum;
                q = strchr(q, ']') + 1;
            }

            // sort numbers
            for (int a = 0; a < count-1; a++)
                for (int b = a+1; b < count; b++)
                    if (nums[a] > nums[b]) {
                        int tmp = nums[a]; nums[a] = nums[b]; nums[b] = tmp;
                    }

            // write back sorted numbers
            q = stackStart;
            for (int k = 0; k < count; k++) {
                char *caret = strchr(q, '^');
                char *endbr = strchr(q, ']');
                char newStr[16];
                int len = snprintf(newStr, sizeof(newStr), "%d", nums[k]);
                memmove(caret+1 + len, endbr, strlen(endbr)+1);  // shift if needed
                memcpy(caret+1, newStr, len);
                q = endbr + (len - (endbr - caret -1)) + 1;
            }

            p = stackEnd;
        } else {
            // single citation: just replace normally
            char *caret = strchr(stackStart, '^');
            int oldNum = atoi(caret+1);
            int newNum = oldNum;
            for (int j = 0; j < inCount; j++) {
                if (inTexts[j].lineIdx == lineIdx && inTexts[j].oldNum == oldNum) {
                    newNum = inTexts[j].newNum;
                    break;
                }
            }
            if (newNum != oldNum) {
                char newStr[16];
                int len = snprintf(newStr, sizeof(newStr), "%d", newNum);
                char *endbr = strchr(stackStart, ']');
                memmove(caret+1 + len, endbr, strlen(endbr)+1);
                memcpy(caret+1, newStr, len);
            }
            p = stackStart + 3; // move past [^x]
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { 
        fprintf(stderr,"citeorder: missing operand\nUsage: '%s input.md'\n", argv[0]); return 1; }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }

    char *lines[MAX_LINES];
    int lineCount=0;
    char buf[MAX_LINE_LEN];

    while(fgets(buf,sizeof(buf),f)) {
        lines[lineCount] = strdup(buf);
        lineCount++;
    }
    fclose(f);

    FullEntry fullEntries[MAX_ENTRIES];
    int fullCount = 0;
    InTextCitation inTexts[MAX_ENTRIES*2];
    int inCount = 0;

    // Collect full-entry citations
    // ----------------------------
    for(int i = 0; i < lineCount; i++){
        if(strstr(lines[i], "]:")){
            const char *pos = NULL;
            int num;
            if(findCitation(lines[i], &pos, &num)){
                // check duplicates
                for(int j = 0; j < fullCount; j++){
                    if(fullEntries[j].oldNum == num){
                        fprintf(stderr,"ERROR: duplicate [^%d] full-entry citations (line %d and %d)\n",
                                num, fullEntries[j].lineIdx+1, i+1);
                        exit(1);
                    }
                }
                fullEntries[fullCount].oldNum = num;
                fullEntries[fullCount].lineIdx = i;
                fullEntries[fullCount].text = lines[i];
                fullEntries[fullCount].newNum = 0; // assign later
                fullCount++;
            }
        }
    }

    // Collect in-text citations and assign sequential new numbers
    // -----------------------------------------------------------
    int nextNum = 1;
    for(int i=0;i<lineCount;i++){
        if(strstr(lines[i], "]:")) continue; // skip full-entry lines

        const char *pos=NULL;
        int num;
        while(findCitation(lines[i], &pos, &num)){
            // find the corresponding full entry
            FullEntry *entry=NULL;
            for(int j=0;j<fullCount;j++){
                if(fullEntries[j].oldNum==num){ entry=&fullEntries[j]; break; }
            }
            if(!entry) {
                fprintf(stderr,"ERROR: in-text citation [^%d] without full-entry (line %d)\n", num, i+1);
                exit(1);
            }

            if(!hasProperQuoteContext(lines[i], num)) {
                fprintf(stderr,"WARNING: in-text citation [^%d] not properly quoted (line %d)\n", num, i+1);
                exit(1);
            }

            // assign a new number if not already assigned
            if(entry->newNum == 0){
                entry->newNum = nextNum++;
            }

            inTexts[inCount].oldNum=num;
            inTexts[inCount].newNum=entry->newNum;
            inTexts[inCount].lineIdx=i;
            inTexts[inCount].ref=entry;
            inCount++;
        }
    }

    // Ensure unused fullEntries keep their oldNum
    // -------------------------------------------
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
    bool changed = 0;
    for (int i = 0; i < fullCount; i++) {
	if (fullEntries[i].oldNum != fullEntries[i].newNum) {
	    changed = 1;
	    break;
	}
    }
    for (int j = 0; j < inCount; j++) {
	if (inTexts[j].oldNum != inTexts[j].newNum) {
	    changed = 1;
	    break;
	}
    }
    
    // Output to new file
    // ------------------
    if (changed) {
        char outName[512];
        char base[256];
        strncpy(base, argv[1], sizeof(base));
        base[sizeof(base)-1]='\0';
        char *dot = strrchr(base, '.');
        if(dot && strcmp(dot,".md")==0) *dot='\0';
        snprintf(outName,sizeof(outName),"%s-fixed.md", base);
        
        FILE *out=fopen(outName,"w");
        if(!out){ perror("fopen"); return 1; }
        
        // Update lines
	int i = 0;
        while (i < lineCount){
            if (!strstr(lines[i], "]:")) {
	    	// --- in-text line ---
            	char *lineCopy = strdup(lines[i]);
            	updateLineInTexts(lineCopy, inTexts, inCount, i);
            	fputs(lineCopy, out);
            	free(lineCopy);
		i++;
	    } else {
		// --- full entry line ---
                int start = i;
                int end = i;
                while (end + 1 < lineCount && strstr(lines[end + 1], "]:")) {
                    end++;
                }
    
                // Collect block entries
                int blockSize = end - start + 1;
                FullEntry *block[blockSize];
                int k = 0;
                for (int j = start; j <= end; j++) {
                    const char *pos = NULL;
                    int num;
                    if (findCitation(lines[j], &pos, &num)) {
                        for (int fe = 0; fe < fullCount; fe++) {
                            if (fullEntries[fe].oldNum == num) {
                                block[k++] = &fullEntries[fe];
                                break;
                            }
                        }
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
                    char *lineCopy = strdup(block[a]->text);
    
                    // Update marker in this line
                    char oldMarker[16], newMarker[16];
                    snprintf(oldMarker, sizeof(oldMarker), "[^%d]:", block[a]->oldNum);
                    snprintf(newMarker, sizeof(newMarker), "[^%d]:", block[a]->newNum);
                    char *p = strstr(lineCopy, oldMarker);
                    if (p) {
                        memmove(p + strlen(newMarker), p + strlen(oldMarker),
                                strlen(p + strlen(oldMarker)) + 1);
                        memcpy(p, newMarker, strlen(newMarker));
                    }
            	    fputs(lineCopy,out);

		    // Ensure newline after every full-entry
		    size_t len = strlen(lineCopy);
                    if (len == 0 || lineCopy[len - 1] != '\n') {
                        fputc('\n', out);
                    }
            	    free(lineCopy);
		}
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
