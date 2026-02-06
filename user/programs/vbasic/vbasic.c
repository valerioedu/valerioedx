/*
    * AI Generated mock basic interpreter
    * I edited it a bit to make it a line interpreter  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_LINES 100
#define MAX_SRC_LEN 1024
#define MAX_TOKEN_LEN 64

// The 26 variables A-Z
int variables[26];

typedef struct {
    int number;
    char *text;
} Line;

Line program[MAX_LINES];
int line_count = 0;

// Lexer globals
char *ptr;                  // Current position in the code string
char token[MAX_TOKEN_LEN];  // Current token
int token_type;             // Type of current token

enum { DELIMITER = 1, VARIABLE, NUMBER, COMMAND, STRING, UNKNOWN };

const char *commands[] = {
    "PRINT", "INPUT", "IF", "THEN", "GOTO", "LET", "END", 0
};

enum { PRINT = 1, INPUT, IF, THEN, GOTO, LET, END };

void error(const char *s) {
    printf("\nError: %s\n", s);
}

/* --- LEXER (TOKENIZER) --- */
int is_delim(char c) {
    return strchr(" ;,+-*/()=<>", c) != NULL || c == 9 || c == '\r' || c == '\n';
}

int get_token() {
    char *temp = token;
    token_type = 0;

    while (*ptr == ' ' || *ptr == '\t') ptr++;

    if (*ptr == '\0') {
        token[0] = 0;
        return 0;
    }

    if (is_delim(*ptr)) {
        token_type = DELIMITER;
        *temp++ = *ptr++;
        if (*(temp-1) == '<' || *(temp-1) == '>') {
             if (*ptr == '=') *temp++ = *ptr++;
        }
    }
    else if (isdigit(*ptr)) {
        token_type = NUMBER;
        while (isdigit(*ptr)) *temp++ = *ptr++;
    }
    else if (*ptr == '"') {
        ptr++; 
        token_type = STRING;
        while (*ptr != '"' && *ptr != '\0') *temp++ = *ptr++;
        if (*ptr == '"') ptr++;
        *temp = '\0';
    }
    else if (isalpha(*ptr)) {
        while (isalpha(*ptr)) *temp++ = toupper(*ptr++);
        *temp = '\0';
        token_type = VARIABLE;
        
        for (int i = 0; commands[i]; ++i) {
            if (!strcmp(token, commands[i])) {
                token_type = COMMAND;
                break;
            }
        }
    }
    *temp = '\0';
    return token_type;
}

void put_back() {
    char *t = token;
    for (; *t; t++) ptr--;
}

/* --- EXPRESSION PARSER --- */
int expression();

int prim() {
    int temp;
    get_token();
    if (token_type == NUMBER) return atoi(token);
    if (token_type == VARIABLE) return variables[token[0] - 'A'];
    if (*token == '(') {
        temp = expression();
        get_token();
        if (*token != ')') error("Parentheses missing");
        return temp;
    }
    if (*token == '-') return -prim();
    error("Malformed expression");
    return 0;
}

int term() {
    int temp = prim();
    get_token();
    while (*token == '*' || *token == '/') {
        if (*token == '*') {
            temp *= prim();
        } else {
            int divisor = prim();
            if (divisor == 0) error("Divide by zero");
            temp /= divisor;
        }
        get_token();
    }
    put_back();
    return temp;
}

int expression() {
    int temp = term();
    get_token();
    while (*token == '+' || *token == '-') {
        if (*token == '+') {
            temp += term();
        } else {
            temp -= term();
        }
        get_token();
    }
    put_back();
    return temp;
}

/* --- INTERPRETER LOGIC --- */

int find_line(int line_num) {
    for (int i = 0; i < line_count; i++) {
        if (program[i].number == line_num) return i;
    }
    return -1;
}

void exec_line(char *src) {
    ptr = src;
    get_token();

    if (token_type == VARIABLE) {
        char var_name = token[0];
        get_token();
        if (*token != '=') error("Syntax error in assignment");
        int result = expression();
        variables[var_name - 'A'] = result;
        return;
    }

    if (token_type != COMMAND) error("Unknown command");

    int cmd_id = 0;
    for(int i=0; commands[i]; i++) {
        if(!strcmp(token, commands[i])) { cmd_id = i+1; break; }
    }

    switch(cmd_id) {
        case PRINT:
            get_token();
            if (token_type == STRING) {
                printf("%s", token);
            } else {
                put_back();
                printf("%d", expression());
            }
            printf("\n");
            break;

        case INPUT:
            get_token();
            if (token_type != VARIABLE) error("Expected variable for INPUT");
            char var = token[0];
            printf("? ");
            scanf("%d", &variables[var - 'A']);
            // Consume the newline left by scanf to prevent messing up the main loop
            getchar(); 
            break;

        case IF:
            {
                int val1 = expression();
                get_token(); 
                char op[3]; strcpy(op, token);
                int val2 = expression();
                
                int condition = 0;
                if (!strcmp(op, "=")) condition = (val1 == val2);
                else if (!strcmp(op, "<")) condition = (val1 < val2);
                else if (!strcmp(op, ">")) condition = (val1 > val2);
                else if (!strcmp(op, ">=")) condition = (val1 >= val2);
                else if (!strcmp(op, "<=")) condition = (val1 <= val2);
                else error("Unknown operator in IF");

                if (condition) {
                    get_token();
                    if (strcmp(token, "THEN") != 0) error("Expected THEN");
                    exec_line(ptr); 
                }
            }
            break;

        case GOTO:
        case LET: // Handled implicitly or via default, kept for structure
            if (cmd_id == LET) {
                get_token();
                if (token_type != VARIABLE) error("Expected variable in LET");
                char v = token[0];
                get_token();
                if (*token != '=') error("Expected = in LET");
                variables[v - 'A'] = expression();
            }
            break;

        case END:
            _exit(0);
    }
}

// Check GOTO helpers
int check_goto(char *src) {
    char *old_ptr = ptr;
    ptr = src;
    get_token();
    if (!strcmp(token, "IF")) {
       expression(); get_token(); expression(); get_token();
    }
    if (!strcmp(token, "GOTO")) {
        get_token();
        int line = atoi(token);
        ptr = old_ptr;
        return line;
    }
    ptr = old_ptr;
    return -1;
}

int check_if_goto(char *src) {
    char *old_ptr = ptr;
    ptr = src;
    get_token();
    if (strcmp(token, "IF") != 0) { ptr = old_ptr; return -1; }
    int val1 = expression();
    get_token();
    char op[3]; strcpy(op, token);
    int val2 = expression();
    int condition = 0;
    if (!strcmp(op, "=")) condition = (val1 == val2);
    else if (!strcmp(op, "<")) condition = (val1 < val2);
    else if (!strcmp(op, ">")) condition = (val1 > val2);
    else if (!strcmp(op, ">=")) condition = (val1 >= val2);
    else if (!strcmp(op, "<=")) condition = (val1 <= val2);

    if (condition) {
        get_token();
        if (!strcmp(token, "THEN")) {
            get_token();
            if (!strcmp(token, "GOTO")) {
                get_token();
                int target = atoi(token);
                ptr = old_ptr;
                return target;
            }
        }
    }
    ptr = old_ptr;
    return -1;
}

void run_program() {
    int pc = 0; 
    while (pc < line_count) {
        int jump_target = check_goto(program[pc].text);
        if (jump_target == -1) jump_target = check_if_goto(program[pc].text);

        exec_line(program[pc].text);

        if (jump_target != -1) {
            int new_pc = find_line(jump_target);
            if (new_pc == -1) error("GOTO target invalid");
            pc = new_pc;
        } else {
            pc++;
        }
    }
}

/* --- PROGRAM MANAGEMENT --- */

// Comparator for qsort to keep lines ordered
int compare_lines(const void *a, const void *b) {
    Line *l1 = (Line *)a;
    Line *l2 = (Line *)b;
    return (l1->number - l2->number);
}

void swap_bytes(char *a, char *b, size_t size) {
    while (size--) {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (nmemb < 2) return;

    char *arr = (char *)base;
    size_t pivot = 0;
    size_t i, j = 0;

    // Choose the last element as pivot
    for (i = 0; i < nmemb - 1; i++) {
        if (compar(arr + i * size, arr + (nmemb - 1) * size) < 0) {
            if (i != j) swap_bytes(arr + i * size, arr + j * size, size);
            j++;
        }
    }
    swap_bytes(arr + j * size, arr + (nmemb - 1) * size, size);

    qsort(arr, j, size, compar);
    qsort(arr + (j + 1) * size, nmemb - j - 1, size, compar);
}

void add_line(int number, char *text) {
    // Check if line exists and update it
    for (int i = 0; i < line_count; i++) {
        if (program[i].number == number) {
            free(program[i].text);
            program[i].text = strdup(text);
            return;
        }
    }
    // Add new line
    if (line_count < MAX_LINES) {
        program[line_count].number = number;
        program[line_count].text = strdup(text);
        line_count++;
        // Sort the program so line 10 comes before 20 regardless of input order
        qsort(program, line_count, sizeof(Line), compare_lines);
    } else {
        printf("Error: Program memory full.\n");
    }
}

int main() {
    char input_buffer[MAX_SRC_LEN];
    printf("V-BASIC INTERPRETER\n");
    printf("Type lines (e.g. '10 PRINT \"HI\"') or commands (RUN, LIST, CLEAR, EXIT)\n\n");

    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) break;

        input_buffer[strcspn(input_buffer, "\n")] = 0;

        if (strlen(input_buffer) == 0) continue;

        if (isdigit(input_buffer[0])) {
            int line_num = atoi(input_buffer);
            char *text = strchr(input_buffer, ' ');
            if (text)
                add_line(line_num, text + 1);
        } else {
            char cmd_check[MAX_SRC_LEN];
            strcpy(cmd_check, input_buffer);
            for(int i=0; cmd_check[i]; i++)
                cmd_check[i] = toupper(cmd_check[i]);

            if (strcmp(cmd_check, "RUN") == 0)
                run_program();

            else if (strcmp(cmd_check, "LIST") == 0)
                for (int i = 0; i < line_count; i++)
                    printf("%d %s\n", program[i].number, program[i].text);

            else if (strcmp(cmd_check, "CLEAR") == 0) {
                for(int i=0; i<line_count; i++)
                    free(program[i].text);

                line_count = 0;
                printf("Program cleared.\n");
            }
            else if (strcmp(cmd_check, "EXIT") == 0 || strcmp(cmd_check, "QUIT") == 0)
                break;

            else printf("Unknown command.\n");
        }
    }

    return 0;
}