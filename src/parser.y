%debug
%{
    #include <stdio.h>
    #include <Arduino.h>
    
    void yyerror(const char *s);
    int yylex(void);
    extern void handleLedOn();
    extern void handleLedOff();
    extern void handleHelp();
    extern void handleReset();
    extern void handleSet(ulong value);
    extern void handleGet();
    extern void handleTime();
    extern void handleClear();
    extern void handleLog();
%}

%union {
    char *string;
    unsigned int value;
    unsigned long lvalue;
}

%token <string> HEXNUMBER
%token <lvalue> NUMBER 
%token LED ON OFF HELP NEWLINE RESET SET GET TIME LOG CLEAR
%token reset

%%
program:
      command NEWLINE    { }
    | command           { }  /* Allow commands without newline */
    ;

command:
      led_command  
    | help_command   
    | reset_command  
    | set_command
    | get_command
    | time_command
    | clear_command
    | log_command
    ;

led_command:
    LED ON     { handleLedOn();  }
    | LED OFF  { handleLedOff(); }
    ;

help_command:
    HELP    { handleHelp(); }
    ;

reset_command:
    RESET { handleReset(); }
    ;

set_command:
    SET NUMBER { handleSet($2); }
    ;

get_command:
    GET { handleGet(); }
    ;

time_command:
    TIME { handleTime(); }
    ;

clear_command:
    CLEAR { handleClear(); }
    ;

log_command:
    LOG { handleLog(); }
    ;


%%

static const char *parseError = NULL;

void yyerror(const char *s) {
    parseError = s;
}

const char *getParseError() {
    return parseError;
}

void clearParseError() {
    parseError = NULL;  /* Use NULL instead of empty string */
}