#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum
{
    TOK_EOF, TOK_ID, TOK_NUM, TOK_BOOL_CONST,
    TOK_INT, TOK_BOOL_TYPE, TOK_BEGIN, TOK_END,
    TOK_IF, TOK_THEN, TOK_ELSE,
    TOK_NOT, TOK_ANY, TOK_ALL,
    TOK_AND, TOK_OR, TOK_XOR,
    TOK_ASSIGN, TOK_EQ, TOK_NEQ,
    TOK_COLON, TOK_SEMICOLON, TOK_LPAREN, TOK_RPAREN,
    TOK_AMP,
} TokenType;

typedef struct
{
    TokenType type;
    char lexeme[64];
    int line, col;
    int value;
} Token;

typedef enum { LEX, SYNTAX, SEMANT } ErrLevel;

void error(ErrLevel lvl, const char *msg, int l, int c)
{
    const char *s = (lvl == LEX) ? "Лексическая" :
                    (lvl == SYNTAX) ? "Синтаксическая" : "Семантическая";
    printf("%s ошибка (строка %d, колонка %d): %s\n", s, l, c, msg);
    exit(1);
}


typedef enum { TYPE_INT, TYPE_BOOL } VarType;
typedef struct { char name[64]; int declared; int initialized; VarType type; int line, col; } TIDRec;

#define MAX_TID 1000
TIDRec TID[MAX_TID];
int tid_count = 0;

int tid_lookup_or_insert(const char *name, int l, int c)
{
    for (int i = 0; i < tid_count; i++)
        if (!strcmp(TID[i].name, name)) return i;
    if (tid_count >= MAX_TID) error(SEMANT, "Переполнение TID", l, c);
    strcpy(TID[tid_count].name, name);
    TID[tid_count].declared = 0;
    TID[tid_count].initialized = 0;
    TID[tid_count].line = l;
    TID[tid_count].col  = c;
    return tid_count++;
}

#define ISTACK_MAX 1024
int istack[ISTACK_MAX];
int isp = 0;
void ipush(int i) { if (isp >= ISTACK_MAX) error(SEMANT,"Переполнение стека int",0,0); istack[isp++] = i; }
int  ipop(void)   { return istack[--isp]; }

#define SSTACK_MAX 1024
#define STR_MAX 16
char sstack[SSTACK_MAX][STR_MAX];
int ssp = 0;
void spush(const char *s)
{
    if (ssp >= SSTACK_MAX) error(SEMANT,"Переполнение стека строк",0,0);
    strncpy(sstack[ssp], s, STR_MAX-1);
    sstack[ssp][STR_MAX-1] = '\0';
    ssp++;
}
char *spop(void) { return sstack[--ssp]; }


#define MAX_TOKENS 4096
Token tokens[MAX_TOKENS];
int tokenCount = 0;
int tokenPos   = 0;

char lexem[64];
int lexemLen = 0;
void add(char sym) { lexem[lexemLen++] = sym; }
void clear(void) { for (int i = 0; i < lexemLen; i++) lexem[i] = 0; lexemLen = 0; }

void put_token(TokenType type, int line, int col)
{
    if (tokenCount >= MAX_TOKENS) error(LEX, "Слишком много токенов", line, col);
    tokens[tokenCount].type = type;
    strncpy(tokens[tokenCount].lexeme, lexem, 63);
    tokens[tokenCount].lexeme[63] = '\0';
    tokens[tokenCount].line = line;
    tokens[tokenCount].col  = col;
    tokens[tokenCount].value = -1;
    tokenCount++;
    clear();
}

TokenType keyword_type(void)
{
    if (!strcmp(lexem, "int"))   return TOK_INT;
    if (!strcmp(lexem, "bool"))  return TOK_BOOL_TYPE;
    if (!strcmp(lexem, "begin")) return TOK_BEGIN;
    if (!strcmp(lexem, "end"))   return TOK_END;
    if (!strcmp(lexem, "if"))    return TOK_IF;
    if (!strcmp(lexem, "then"))  return TOK_THEN;
    if (!strcmp(lexem, "else"))  return TOK_ELSE;
    if (!strcmp(lexem, "not"))   return TOK_NOT;
    if (!strcmp(lexem, "any"))   return TOK_ANY;
    if (!strcmp(lexem, "all"))   return TOK_ALL;
    if (!strcmp(lexem, "and"))   return TOK_AND;
    if (!strcmp(lexem, "or"))    return TOK_OR;
    if (!strcmp(lexem, "xor"))   return TOK_XOR;
    if (!strcmp(lexem, "True"))  return TOK_BOOL_CONST;
    if (!strcmp(lexem, "False")) return TOK_BOOL_CONST;
    return TOK_ID;
}

int lexParsing(void)
{
    FILE *input = fopen("input.txt", "r");
    enum { START, ID, NUM0, NUMX, NUMBIN, DLM, FIN } cond = START;
    int line = 1, col = 1;
    int sym = getc(input);

    while (sym != EOF)
    {
        switch (cond) {
        case START:
            if (sym == ' ' || sym == '\t' || sym == '\n') {
                if (sym == '\n') { line++; col = 1; } else col++;
                sym = getc(input);
            } else if ((sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z')) {
                add(sym); col++; sym = getc(input); cond = ID;
            } else if (sym == '0') {
                add(sym); col++; sym = getc(input); cond = NUM0;
            } else if (sym == '=' || sym == '!' || sym == ':' || sym == ';' ||
                       sym == '(' || sym == ')' || sym == '^') {
                add(sym); col++; sym = getc(input); cond = DLM;
            } else if (sym == '&') {
                col++; cond = FIN;
            } else {
                char buf[64];
                snprintf(buf, sizeof buf, "Недопустимый символ '%c'", sym);
                error(LEX, buf, line, col);
            }
            break;
        case ID:
            if ((sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z') ||
                (sym >= '0' && sym <= '9')) {
                add(sym); col++; sym = getc(input); cond = ID;
            } else {
                TokenType t = keyword_type();
                int isId = (t == TOK_ID);
                put_token(t, line, col - lexemLen);
                if (isId)
                    tokens[tokenCount - 1].value =
                        tid_lookup_or_insert(tokens[tokenCount - 1].lexeme,
                                             tokens[tokenCount - 1].line,
                                             tokens[tokenCount - 1].col);
                cond = START;
            }
            break;
        case NUM0:
            if (sym == 'x') { add(sym); col++; sym = getc(input); cond = NUMX; }
            else error(LEX, "Ожидался 'x' после '0'", line, col);
            break;
        case NUMX:
            if (sym == '0' || sym == '1') { add(sym); col++; sym = getc(input); cond = NUMBIN; }
            else error(LEX, "Ожидалась двоичная цифра после '0x'", line, col);
            break;
        case NUMBIN:
            if (sym == '0' || sym == '1') { add(sym); col++; sym = getc(input); cond = NUMBIN; }
            else { put_token(TOK_NUM, line, col - lexemLen); cond = START; }
            break;
        case DLM:
            if (sym == '=' && !strcmp(lexem, "=")) {
                add(sym); col++; sym = getc(input);
                put_token(TOK_EQ, line, col - 2); cond = START;
            } else if (sym == '=' && !strcmp(lexem, "!")) {
                add(sym); col++; sym = getc(input);
                put_token(TOK_NEQ, line, col - 2); cond = START;
            } else {
                if      (!strcmp(lexem, "=")) put_token(TOK_ASSIGN,    line, col - 1);
                else if (!strcmp(lexem, ":")) put_token(TOK_COLON,     line, col - 1);
                else if (!strcmp(lexem, ";")) put_token(TOK_SEMICOLON, line, col - 1);
                else if (!strcmp(lexem, "(")) put_token(TOK_LPAREN,    line, col - 1);
                else if (!strcmp(lexem, ")")) put_token(TOK_RPAREN,    line, col - 1);
                else if (!strcmp(lexem, "^")) put_token(TOK_XOR,       line, col - 1);
                else error(LEX, "Неизвестный разделитель", line, col);
                cond = START;
            }
            break;
        case FIN:
            sym = getc(input);
            put_token(TOK_EOF, line, col);
            break;
        }
    }
    fclose(input);
    return 0;
}

Token cur;
void nextToken(void)
{
    if (tokenPos < tokenCount) cur = tokens[tokenPos++];
    else cur = tokens[tokenCount - 1];
}


const char* gettype(const char *op, const char *t1, const char *t2)
{
    if (!strcmp(op,"and") || !strcmp(op,"or") || !strcmp(op,"xor")) {
        if (!strcmp(t1,"int") && !strcmp(t2,"int")) return "int";
        return "no";
    }
    if (!strcmp(op,"==") || !strcmp(op,"!=")) {
        if (!strcmp(t1,"int")  && !strcmp(t2,"int"))  return "int";
        if (!strcmp(t1,"bool") && !strcmp(t2,"bool")) return "bool";
        return "no";
    }
    return "no";
}

void eqtype(int l, int c)
{
    char t2[STR_MAX], t1[STR_MAX];
    strcpy(t2, spop()); strcpy(t1, spop());
    if (strcmp(t1,t2)) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Несовпадение типов при присваивании: %s и %s", t1, t2);
        error(SEMANT, buf, l, c);
    }
}

void eqbool(int l, int c)
{
    char t[STR_MAX]; strcpy(t, spop());
    if (strcmp(t,"bool")) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Условие должно иметь тип bool (получен %s)", t);
        error(SEMANT, buf, l, c);
    }
}

void decid(int i, const char *t)
{
    if (TID[i].declared) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Повторное объявление переменной '%s'", TID[i].name);
        error(SEMANT, buf, TID[i].line, TID[i].col);
    }
    TID[i].declared = 1;
    TID[i].type = !strcmp(t,"int") ? TYPE_INT : TYPE_BOOL;
}
void dec(const char *t) { int i; while ((i = ipop()) != -1) decid(i, t); }

void checkid_expr(int idx, int l, int c)
{
    if (!TID[idx].declared) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Необъявленная переменная '%s'", TID[idx].name);
        error(SEMANT, buf, l, c);
    }
    if (!TID[idx].initialized) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Переменная '%s' не инициализирована", TID[idx].name);
        error(SEMANT, buf, l, c);
    }
    spush(TID[idx].type == TYPE_INT ? "int" : "bool");
}

void checkid_lhs(int idx, int l, int c)
{
    if (!TID[idx].declared) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Необъявленная переменная '%s'", TID[idx].name);
        error(SEMANT, buf, l, c);
    }
    spush(TID[idx].type == TYPE_INT ? "int" : "bool");
}


typedef enum 
{
    OP_AND,
    OP_OR,
    OP_XOR, 
    OP_NOT,
    OP_EQ,  
    OP_NEQ,
    OP_ANY, 
    OP_ALL,
    OP_ASSIGN,
    OP_JF,
    OP_JMP,
} OpCode;

static const char *op_names[] = {
    "and", 
    "or", 
    "xor", 
    "not",
    "==", 
    "!=",
    "any", 
    "all",
    ":=",
    "!F",
    "!"
};

typedef struct {
    int  class_;
    int  value;
    char str[64];
} PLex;

#define MAXLEN_P 10000
static PLex P[MAXLEN_P];
static int  freeP = 0;

static void put_lex(PLex l)
{
    if (freeP >= MAXLEN_P) error(SEMANT, "Переполнение ПОЛИЗ", 0, 0);
    P[freeP++] = l;
}
static void put_lex5(PLex l)
{
    l.class_ = 5;
    put_lex(l);
}

static PLex make_op(OpCode op)
{
    PLex l;
    l.class_ = 2;
    l.value  = op;
    strncpy(l.str, op_names[op], sizeof l.str - 1);
    return l;
}

static PLex make_labl(int k)
{
    PLex l;
    l.class_ = 0;
    l.value  = k;
    snprintf(l.str, sizeof l.str, "%d", k);
    return l;
}

static PLex make_var(int tid_idx)
{
    PLex l;
    l.class_ = 4;
    l.value  = tid_idx;
    strncpy(l.str, TID[tid_idx].name, sizeof l.str - 1);
    return l;
}

static PLex make_num(const char *text)
{
    PLex l;
    l.class_ = 4;
    int v = 0;
    if (text[0] == '0' && text[1] == 'x')
        for (int i = 2; text[i]; i++) v = v*2 + (text[i]-'0');
    l.value = v;
    strncpy(l.str, text, sizeof l.str - 1);
    return l;
}

static PLex make_bool(int isTrue)
{
    PLex l;
    l.class_ = 4;
    l.value  = isTrue;
    strcpy(l.str, isTrue ? "True" : "False");
    return l;
}

static OpCode str_to_op(const char *s)
{
    if (!strcmp(s, "and")) return OP_AND;
    if (!strcmp(s, "or"))  return OP_OR;
    if (!strcmp(s, "xor")) return OP_XOR;
    if (!strcmp(s, "not")) return OP_NOT;
    if (!strcmp(s, "=="))  return OP_EQ;
    if (!strcmp(s, "!="))  return OP_NEQ;
    if (!strcmp(s, "any")) return OP_ANY;
    if (!strcmp(s, "all")) return OP_ALL;
    error(SEMANT, "Неизвестная операция в str_to_op", 0, 0);
    return OP_AND;
}

void checkop(int l, int c)
{
    char t2[STR_MAX], op[STR_MAX], t1[STR_MAX];
    strcpy(t2, spop());
    strcpy(op, spop());
    strcpy(t1, spop());
    const char *res = gettype(op, t1, t2);
    if (!strcmp(res,"no")) {
        char buf[256];
        snprintf(buf,sizeof buf,
                 "Операция '%s' недопустима для типов %s и %s", op, t1, t2);
        error(SEMANT, buf, l, c);
    }
    spush(res);
    put_lex(make_op(str_to_op(op)));
}

void print_poliz(void)
{
    printf("\nСгенерированный ПОЛИЗ (%d элементов):\n", freeP);
    for (int i = 0; i < freeP; i++) {
        printf("  %4d: ", i);
        switch (P[i].class_) {
            case 0: printf("(0, %d)\t\t[метка]\n",       P[i].value); break;
            case 2: printf("(2, %d)\t\t[оп: %s]\n",     P[i].value, P[i].str); break;
            case 4: printf("(4, %d)\t\t[значение: %s]\n", P[i].value, P[i].str); break;
            case 5: printf("(5, %d)\t\t[адрес: %s]\n",  P[i].value, P[i].str); break;
            default: printf("(?, %d)\n", P[i].value);
        }
    }
}

void parseVarDecl();
void parseVarDecls();
void parseFactor();
void parseAndExpr();
void parseXorExpr();
void parseOrExpr();
void parseComp();
void parseStatement();
void parseBody();
void parseProgram();

void parseVarDecl(void)
{
    ipush(-1);
    if (cur.type != TOK_ID)
        error(SYNTAX, "Ожидался идентификатор в объявлении", cur.line, cur.col);
    int idx = cur.value;
    nextToken();
    ipush(idx);
    if (cur.type != TOK_COLON)
        error(SYNTAX, "Ожидался ':' после идентификатора", cur.line, cur.col);
    nextToken();
    if      (cur.type == TOK_INT)        { nextToken(); dec("int");  }
    else if (cur.type == TOK_BOOL_TYPE)  { nextToken(); dec("bool"); }
    else error(SYNTAX, "Ожидался тип 'int' или 'bool'", cur.line, cur.col);
    if (cur.type != TOK_SEMICOLON)
        error(SYNTAX, "Ожидался ';' после объявления", cur.line, cur.col);
    nextToken();
}
void parseVarDecls(void) { while (cur.type == TOK_ID) parseVarDecl(); }

void parseFactor(void)
{
    if (cur.type == TOK_ID) {
        int idx = cur.value, l = cur.line, c = cur.col;
        nextToken();
        checkid_expr(idx, l, c);
        put_lex(make_var(idx));
    }
    else if (cur.type == TOK_NUM) {
        char txt[64]; strcpy(txt, cur.lexeme);
        nextToken();
        spush("int");
        put_lex(make_num(txt));
    }
    else if (cur.type == TOK_BOOL_CONST) {
        int isTrue = !strcmp(cur.lexeme, "True");
        nextToken();
        spush("bool");
        put_lex(make_bool(isTrue));
    }
    else if (cur.type == TOK_NOT) {
        int l = cur.line, c = cur.col;
        nextToken();
        parseFactor();
        char t[STR_MAX]; strcpy(t, spop());
        if (strcmp(t,"int")) {
            char buf[128];
            snprintf(buf,sizeof buf,
                     "Оператор 'not' применим только к int (получен %s)", t);
            error(SEMANT, buf, l, c);
        }
        spush("int");
        put_lex(make_op(OP_NOT));
    }
    else if (cur.type == TOK_LPAREN) {
        nextToken();
        parseComp();
        if (cur.type != TOK_RPAREN)
            error(SYNTAX, "Ожидалась ')'", cur.line, cur.col);
        nextToken();
    }
    else if (cur.type == TOK_ANY || cur.type == TOK_ALL) {
        int l = cur.line, c = cur.col;
        int is_any = (cur.type == TOK_ANY);
        nextToken();
        parseFactor();
        char t[STR_MAX]; strcpy(t, spop());
        if (strcmp(t,"int")) {
            char buf[128];
            snprintf(buf,sizeof buf,
                     "Оператор '%s' применим только к int (получен %s)",
                     is_any ? "any" : "all", t);
            error(SEMANT, buf, l, c);
        }
        spush("bool");
        put_lex(make_op(is_any ? OP_ANY : OP_ALL));
    }
    else error(SYNTAX, "Ожидался операнд", cur.line, cur.col);
}

void parseAndExpr(void)
{
    parseFactor();
    while (cur.type == TOK_AND) {
        int l = cur.line, c = cur.col;
        spush("and"); nextToken();
        parseFactor(); checkop(l, c);
    }
}
void parseXorExpr(void)
{
    parseAndExpr();
    while (cur.type == TOK_XOR) {
        int l = cur.line, c = cur.col;
        spush("xor"); nextToken();
        parseAndExpr(); checkop(l, c);
    }
}
void parseOrExpr(void)
{
    parseXorExpr();
    while (cur.type == TOK_OR) {
        int l = cur.line, c = cur.col;
        spush("or"); nextToken();
        parseXorExpr(); checkop(l, c);
    }
}
void parseComp(void)
{
    parseOrExpr();
    while (cur.type == TOK_EQ || cur.type == TOK_NEQ) {
        int l = cur.line, c = cur.col;
        spush(cur.type == TOK_EQ ? "==" : "!=");
        nextToken();
        parseOrExpr(); checkop(l, c);
    }
}

void parseStatement(void)
{
    if (cur.type == TOK_ID) {
        int idx = cur.value, l = cur.line, c = cur.col;
        nextToken();
        checkid_lhs(idx, l, c);
        put_lex5(make_var(idx));
        if (cur.type != TOK_ASSIGN)
            error(SYNTAX, "Ожидался '=' в операторе присваивания", cur.line, cur.col);
        nextToken();
        parseComp();
        eqtype(l, c);
        put_lex(make_op(OP_ASSIGN));
        TID[idx].initialized = 1;
    }
    else if (cur.type == TOK_IF) {
        nextToken();
        int l = cur.line, c = cur.col;
        parseComp();
        eqbool(l, c);
        int pl2 = freeP++;
        put_lex(make_op(OP_JF));
        if (cur.type != TOK_THEN)
            error(SYNTAX, "Ожидался 'then'", cur.line, cur.col);
        nextToken();
        parseBody();
        if (cur.type == TOK_ELSE) {
            int pl3 = freeP++;
            put_lex(make_op(OP_JMP));
            P[pl2] = make_labl(freeP);
            nextToken();
            parseBody();
            P[pl3] = make_labl(freeP);
        } else {
            P[pl2] = make_labl(freeP);
        }
    }
    else error(SYNTAX, "Ожидался оператор", cur.line, cur.col);
}

void parseBody(void)
{
    if (cur.type != TOK_BEGIN)
        error(SYNTAX, "Ожидался 'begin'", cur.line, cur.col);
    nextToken();
    while (cur.type != TOK_END && cur.type != TOK_EOF) {
        parseStatement();
        if (cur.type != TOK_SEMICOLON)
            error(SYNTAX, "Ожидался ';' после оператора", cur.line, cur.col);
        nextToken();
    }
    if (cur.type != TOK_END)
        error(SYNTAX, "Ожидался 'end'", cur.line, cur.col);
    nextToken();
}

void parseProgram(void)
{
    parseVarDecls();
    if (cur.type != TOK_BEGIN)
        error(SYNTAX, "Ожидался 'begin' (тело программы)", cur.line, cur.col);
    parseBody();
    if (cur.type != TOK_EOF)
        error(SYNTAX, "Ожидался '&' в конце программы", cur.line, cur.col);
    nextToken();
}

int main(void)
{
    if (lexParsing()) return 1;
    tokenPos = 0;
    nextToken();
    parseProgram();
    if (cur.type != TOK_EOF)
        error(SYNTAX, "Лишние символы после '&'", cur.line, cur.col);

    printf("Синтаксический и семантический анализ завершён успешно.\n");
    print_poliz();
    return 0;
}