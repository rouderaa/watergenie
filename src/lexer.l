%{
#include "parser.tab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// External declarations
extern YYSTYPE yylval;
extern int yyparse(void);
extern char *getParseError(void);
extern void clearParseError(void);

// Function to report lexical errors
void yyerror(const char *s);
%}

/* Options */
%option noyywrap
%option nounput
%option noinput

%%

"led"       { return LED; }
"on"        { return ON; }
"off"       { return OFF; }
"help"      { return HELP; }
"reset"     { return RESET; }
"set"       { return SET; }
"get"       { return GET; }
"time"      { return TIME; }
"log"       { return LOG; }
"clear"     { return CLEAR; }

\%[A-Fa-f0-9]{8}   { 
                    yylval.string = strdup(yytext + 1);
                    if (!yylval.string) {
                        fprintf(stderr, "Memory allocation failed\n");
                        return -1;
                    }
                    return HEXNUMBER; 
                }

[-+]?[0-9]+ {
               long val = atol(yytext);
               yylval.value = (int)val;
               return NUMBER;
            }

[ \t]+      ; /* Skip whitespace */
\n          { return NEWLINE; }
#.*         ; /* Skip comments starting with # */
.           { 
               // Skip FF character at startup of telnet
               if ((yytext[0] != (unsigned char) 0xff) ||
                   (yytext[0] != (unsigned char) 0xfd)
                ) {
                   fprintf(stderr, "Unexpected character: '%c' (0x%02X)\n", 
                       yytext[0], (unsigned char)yytext[0]);
                   return -1;
               }
            }

%%

/* ----------------------------------------------------------------------
 * set_input_string
 *
 *  Parses the supplied input string with the Bison/Yacc parser.
 *
 *  Parameters
 *      str : C‑string containing the text to be parsed.  May be NULL.
 *
 *  Returns
 *      const char * :  NULL on success, otherwise a pointer to a
 *                      constant error description owned by the parser.
 *
 *  Notes
 *      • The function no longer returns a mutable `char *`; returning a
 *        mutable pointer to a string literal (or to internal parser data)
 *        would violate the const‑correctness contract and triggers the
 *        “return discards ‘const’ qualifier” warning.
 *      • All early‑exit paths clean up any resources that were allocated.
 * ---------------------------------------------------------------------- */
const char *set_input_string(const char *str)
{
    /* --------------------------------------------------------------
     * 1️⃣  Guard against a NULL input string.
     * -------------------------------------------------------------- */
    if (!str) {
        /* Return a constant error message – the caller must not try to
         * modify it. */
        return "Input string is NULL";
    }

    /* --------------------------------------------------------------
     * 2️⃣  Reset any previous parse error state.
     * -------------------------------------------------------------- */
    clearParseError();                 /* implementation‑specific */

    /* --------------------------------------------------------------
     * 3️⃣  Create a Flex buffer from the input string.
     * -------------------------------------------------------------- */
    YY_BUFFER_STATE buffer = yy_scan_string(str);
    if (!buffer) {
        return "Failed to create input buffer";
    }

    /* --------------------------------------------------------------
     * 4️⃣  Run the Bison parser.
     * -------------------------------------------------------------- */
    int result = yyparse();            /* returns 0 on success */

    /* --------------------------------------------------------------
     * 5️⃣  Clean up the Flex buffer – this must happen regardless of
     *     the parse outcome.
     * -------------------------------------------------------------- */
    yy_delete_buffer(buffer);

    /* --------------------------------------------------------------
     * 6️⃣  Examine the parser result.
     * -------------------------------------------------------------- */
    if (result != 0) {
        /* An error occurred; fetch the stored error message (if any). */
        const char *parseError = getParseError();   /* returns const char * */
        return parseError ? parseError : "Parse error occurred";
    }

    /* --------------------------------------------------------------
     * 7️⃣  Success path – there is no error message to report.
     * -------------------------------------------------------------- */
    return NULL;                       /* indicates success */
}
