#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ================== Токены ================== */
typedef enum {
    TOK_EOF, TOK_ID, TOK_NUM, TOK_BOOL_CONST,
    TOK_INT, TOK_BOOL_TYPE, TOK_BEGIN, TOK_END,
    TOK_IF, TOK_THEN, TOK_ELSE,
    TOK_NOT, TOK_ANY, TOK_ALL,
    TOK_AND, TOK_OR, TOK_XOR,
    TOK_ASSIGN, TOK_EQ, TOK_NEQ,
    TOK_COLON, TOK_SEMICOLON, TOK_LPAREN, TOK_RPAREN,
    TOK_AMP
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[64];
    int line, col;
    int value;            /* индекс в TID (для TOK_ID) */
} Token;

/* ================== Уровни ошибок ================== */
typedef enum { LEX, SYNTAX, SEMANT } ErrLevel;

static void error(ErrLevel lvl, const char *msg, int l, int c) {
    const char *s = (lvl == LEX)    ? "Лексическая"   :
                    (lvl == SYNTAX) ? "Синтаксическая" : "Семантическая";
    fprintf(stderr, "%s ошибка (строка %d, колонка %d): %s\n", s, l, c, msg);
    exit(1);
}

/* ================== TID ================== */
typedef enum { TYPE_INT, TYPE_BOOL } VarType;

typedef struct {
    char name[64];
    int  declared;
    int  initialized;
    VarType type;
    int  line, col;       /* первое появление в тексте */
} TIDRec;

#define MAX_TID 1000
static TIDRec TID[MAX_TID];
static int tid_count = 0;

static int tid_lookup_or_insert(const char *name, int l, int c) {
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

/* ============ Стек целых (для объявлений) ============ */
#define ISTACK_MAX 1024
static int istack[ISTACK_MAX];
static int isp = 0;
static void ipush(int i) { if (isp >= ISTACK_MAX) error(SEMANT,"Переполнение стека int",0,0); istack[isp++] = i; }
static int  ipop(void)   { return istack[--isp]; }

/* ============ Стек строк (для типов/операций) ============ */
#define SSTACK_MAX 1024
#define STR_MAX 16
static char sstack[SSTACK_MAX][STR_MAX];
static int ssp = 0;
static void spush(const char *s) {
    if (ssp >= SSTACK_MAX) error(SEMANT,"Переполнение стека строк",0,0);
    strncpy(sstack[ssp], s, STR_MAX-1);
    sstack[ssp][STR_MAX-1] = '\0';
    ssp++;
}
static char *spop(void) { return sstack[--ssp]; }

/* ================== Лексер ================== */
static const char *src;
static int pos = 0, line = 1, col = 1;
static Token cur;

static void skip_whitespace(void) {
    while (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r') {
        if (src[pos] == '\n') { line++; col = 1; }
        else col++;
        pos++;
    }
}

static void next_token(void) {
    skip_whitespace();
    int sl = line, sc = col;
    char c = src[pos];

    if (c == '\0') {
        cur.type = TOK_EOF; strcpy(cur.lexeme, "EOF");
        cur.line = sl; cur.col = sc; cur.value = -1;
        return;
    }

    if (c >= 'a' && c <= 'z') {
        int len = 0;
        while ((src[pos] >= 'a' && src[pos] <= 'z') ||
               (src[pos] >= 'A' && src[pos] <= 'Z') ||
               (src[pos] >= '0' && src[pos] <= '9')) {
            cur.lexeme[len++] = src[pos++];
            col++;
        }
        cur.lexeme[len] = '\0';
        cur.line = sl; cur.col = sc; cur.value = -1;

        if      (!strcmp(cur.lexeme, "int"))   cur.type = TOK_INT;
        else if (!strcmp(cur.lexeme, "bool"))  cur.type = TOK_BOOL_TYPE;
        else if (!strcmp(cur.lexeme, "begin")) cur.type = TOK_BEGIN;
        else if (!strcmp(cur.lexeme, "end"))   cur.type = TOK_END;
        else if (!strcmp(cur.lexeme, "if"))    cur.type = TOK_IF;
        else if (!strcmp(cur.lexeme, "then"))  cur.type = TOK_THEN;
        else if (!strcmp(cur.lexeme, "else"))  cur.type = TOK_ELSE;
        else if (!strcmp(cur.lexeme, "not"))   cur.type = TOK_NOT;
        else if (!strcmp(cur.lexeme, "any"))   cur.type = TOK_ANY;
        else if (!strcmp(cur.lexeme, "all"))   cur.type = TOK_ALL;
        else if (!strcmp(cur.lexeme, "and"))   cur.type = TOK_AND;
        else if (!strcmp(cur.lexeme, "or"))    cur.type = TOK_OR;
        else if (!strcmp(cur.lexeme, "xor"))   cur.type = TOK_XOR;
        else if (!strcmp(cur.lexeme, "True"))  cur.type = TOK_BOOL_CONST;
        else if (!strcmp(cur.lexeme, "False")) cur.type = TOK_BOOL_CONST;
        else {
            cur.type = TOK_ID;
            cur.value = tid_lookup_or_insert(cur.lexeme, sl, sc);
        }
        return;
    }

    if (c == '0' && src[pos+1] == 'x') {
        pos += 2; col += 2;
        int len = 0;
        cur.lexeme[len++] = '0';
        cur.lexeme[len++] = 'x';
        if (src[pos] != '0' && src[pos] != '1')
            error(LEX, "Ожидалась двоичная цифра после '0x'", line, col);
        while (src[pos] == '0' || src[pos] == '1') {
            cur.lexeme[len++] = src[pos++];
            col++;
        }
        cur.lexeme[len] = '\0';
        cur.type = TOK_NUM;
        cur.line = sl; cur.col = sc; cur.value = -1;
        return;
    }

    if (c == '=' && src[pos+1] == '=') {
        strcpy(cur.lexeme,"=="); pos += 2; col += 2;
        cur.type = TOK_EQ; cur.line=sl; cur.col=sc; cur.value=-1; return;
    }
    if (c == '!' && src[pos+1] == '=') {
        strcpy(cur.lexeme,"!="); pos += 2; col += 2;
        cur.type = TOK_NEQ; cur.line=sl; cur.col=sc; cur.value=-1; return;
    }
    if (c == '=') { strcpy(cur.lexeme,"="); pos++; col++; cur.type=TOK_ASSIGN; }
    else if (c == ':') { strcpy(cur.lexeme,":"); pos++; col++; cur.type=TOK_COLON; }
    else if (c == ';') { strcpy(cur.lexeme,";"); pos++; col++; cur.type=TOK_SEMICOLON; }
    else if (c == '(') { strcpy(cur.lexeme,"("); pos++; col++; cur.type=TOK_LPAREN; }
    else if (c == ')') { strcpy(cur.lexeme,")"); pos++; col++; cur.type=TOK_RPAREN; }
    else if (c == '&') { strcpy(cur.lexeme,"&"); pos++; col++; cur.type=TOK_AMP; }
    else if (c == '^') { strcpy(cur.lexeme,"^"); pos++; col++; cur.type=TOK_XOR; }
    else {
        char buf[64];
        snprintf(buf, sizeof buf, "Недопустимый символ '%c'", c);
        error(LEX, buf, line, col);
    }
    cur.line = sl; cur.col = sc; cur.value = -1;
}

/* ================ Семантические действия ================ */

/* Проверка допустимости операции op над типами t1, t2.
   Для int — только побитовые операции; результат сравнения int×int — int.
   Для bool — только сравнение.  */
static const char* gettype(const char *op, const char *t1, const char *t2) {
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

/* checkop: снимает t2, op, t1, проверяет gettype, кладёт результат */
static void checkop(int l, int c) {
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
}

/* eqtype: проверяет равенство двух типов, снятых со стека */
static void eqtype(int l, int c) {
    char t2[STR_MAX], t1[STR_MAX];
    strcpy(t2, spop());
    strcpy(t1, spop());
    if (strcmp(t1,t2)) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Несовпадение типов при присваивании: %s и %s", t1, t2);
        error(SEMANT, buf, l, c);
    }
}

/* eqbool: тип условия должен быть bool */
static void eqbool(int l, int c) {
    char t[STR_MAX];
    strcpy(t, spop());
    if (strcmp(t,"bool")) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Условие должно иметь тип bool (получен %s)", t);
        error(SEMANT, buf, l, c);
    }
}

/* decid / dec — заполнение TID после разбора типа (с. 165) */
static void decid(int i, const char *t) {
    if (TID[i].declared) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Повторное объявление переменной '%s'", TID[i].name);
        error(SEMANT, buf, TID[i].line, TID[i].col);
    }
    TID[i].declared = 1;
    TID[i].type = !strcmp(t,"int") ? TYPE_INT : TYPE_BOOL;
}
static void dec(const char *t) {
    int i;
    while ((i = ipop()) != -1) decid(i, t);
}

/* checkid для выражения: требует declared && initialized */
static void checkid_expr(int idx, int l, int c) {
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

/* checkid для левой части присваивания: достаточно declared */
static void checkid_lhs(int idx, int l, int c) {
    if (!TID[idx].declared) {
        char buf[128];
        snprintf(buf,sizeof buf,
                 "Необъявленная переменная '%s'", TID[idx].name);
        error(SEMANT, buf, l, c);
    }
    spush(TID[idx].type == TYPE_INT ? "int" : "bool");
}

/* ============ Рекурсивный спуск с действиями ============ */

/* VarDecl → Id : Type ;  (с ipush/dec) */
static void parse_VarDecl(void) {
    ipush(-1);                                          /* дно стека */
    if (cur.type != TOK_ID)
        error(SYNTAX, "Ожидался идентификатор в объявлении", cur.line, cur.col);
    int idx = cur.value;
    next_token();
    ipush(idx);
    if (cur.type != TOK_COLON)
        error(SYNTAX, "Ожидался ':' после идентификатора", cur.line, cur.col);
    next_token();
    if (cur.type == TOK_INT)            { next_token(); dec("int");  }
    else if (cur.type == TOK_BOOL_TYPE) { next_token(); dec("bool"); }
    else error(SYNTAX, "Ожидался тип 'int' или 'bool'", cur.line, cur.col);
    if (cur.type != TOK_SEMICOLON)
        error(SYNTAX, "Ожидался ';' после объявления", cur.line, cur.col);
    next_token();
}

static void parse_VarDecls(void) {
    while (cur.type == TOK_ID) parse_VarDecl();
}

/* Factor → Id <checkid_expr> | Num <spush("int")> | BoolNum <spush("bool")>
          | not Factor <...>  | ( Comp ) | any Factor <...> | all Factor <...> */
static void parse_Factor(void) {
    if (cur.type == TOK_ID) {
        int idx = cur.value, l = cur.line, c = cur.col;
        next_token();
        checkid_expr(idx, l, c);
    } else if (cur.type == TOK_NUM) {
        next_token(); spush("int");
    } else if (cur.type == TOK_BOOL_CONST) {
        next_token(); spush("bool");
    } else if (cur.type == TOK_NOT) {
        int l = cur.line, c = cur.col;
        next_token();
        parse_Factor();
        char t[STR_MAX]; strcpy(t, spop());
        if (strcmp(t,"int")) {
            char buf[128];
            snprintf(buf,sizeof buf,
                     "Оператор 'not' применим только к int (получен %s)", t);
            error(SEMANT, buf, l, c);
        }
        spush("int");
    } else if (cur.type == TOK_LPAREN) {
        next_token();
        parse_Comp();
        if (cur.type != TOK_RPAREN)
            error(SYNTAX, "Ожидалась ')'", cur.line, cur.col);
        next_token();
    } else if (cur.type == TOK_ANY || cur.type == TOK_ALL) {
        int l = cur.line, c = cur.col;
        int is_any = (cur.type == TOK_ANY);
        next_token();
        parse_Factor();
        char t[STR_MAX]; strcpy(t, spop());
        if (strcmp(t,"int")) {
            char buf[128];
            snprintf(buf,sizeof buf,
                     "Оператор '%s' применим только к int (получен %s)",
                     is_any ? "any" : "all", t);
            error(SEMANT, buf, l, c);
        }
        spush("bool");
    } else {
        error(SYNTAX, "Ожидался операнд", cur.line, cur.col);
    }
}

static void parse_AndExpr(void) {
    parse_Factor();
    while (cur.type == TOK_AND) {
        int l = cur.line, c = cur.col;
        spush("and"); next_token();
        parse_Factor(); checkop(l, c);
    }
}
static void parse_XorExpr(void) {
    parse_AndExpr();
    while (cur.type == TOK_XOR) {
        int l = cur.line, c = cur.col;
        spush("xor"); next_token();
        parse_AndExpr(); checkop(l, c);
    }
}
static void parse_OrExpr(void) {
    parse_XorExpr();
    while (cur.type == TOK_OR) {
        int l = cur.line, c = cur.col;
        spush("or"); next_token();
        parse_XorExpr(); checkop(l, c);
    }
}
static void parse_Comp(void) {
    parse_OrExpr();
    while (cur.type == TOK_EQ || cur.type == TOK_NEQ) {
        int l = cur.line, c = cur.col;
        spush(cur.type == TOK_EQ ? "==" : "!=");
        next_token();
        parse_OrExpr();
        checkop(l, c);
    }
}

/* Statement → Id <checkid_lhs> = Comp <eqtype>  (с последующей пометкой initialized)
             | if Comp <eqbool> then Body [else Body]                              */
static void parse_Statement(void) {
    if (cur.type == TOK_ID) {
        int idx = cur.value, l = cur.line, c = cur.col;
        next_token();
        checkid_lhs(idx, l, c);
        if (cur.type != TOK_ASSIGN)
            error(SYNTAX, "Ожидался '=' в операторе присваивания", cur.line, cur.col);
        next_token();
        parse_Comp();
        eqtype(l, c);
        TID[idx].initialized = 1;
    } else if (cur.type == TOK_IF) {
        next_token();
        int l = cur.line, c = cur.col;
        parse_Comp();
        eqbool(l, c);
        if (cur.type != TOK_THEN)
            error(SYNTAX, "Ожидался 'then'", cur.line, cur.col);
        next_token();
        parse_Body();
        if (cur.type == TOK_ELSE) {
            next_token();
            parse_Body();
        }
    } else {
        error(SYNTAX, "Ожидался оператор", cur.line, cur.col);
    }
}

static void parse_Body(void) {
    if (cur.type != TOK_BEGIN)
        error(SYNTAX, "Ожидался 'begin'", cur.line, cur.col);
    next_token();
    while (cur.type != TOK_END && cur.type != TOK_EOF) {
        parse_Statement();
        if (cur.type != TOK_SEMICOLON)
            error(SYNTAX, "Ожидался ';' после оператора", cur.line, cur.col);
        next_token();
    }
    if (cur.type != TOK_END)
        error(SYNTAX, "Ожидался 'end'", cur.line, cur.col);
    next_token();
}

static void parse_Program(void) {
    parse_VarDecls();
    if (cur.type != TOK_BEGIN)
        error(SYNTAX, "Ожидался 'begin' (тело программы)", cur.line, cur.col);
    parse_Body();
    if (cur.type != TOK_AMP)
        error(SYNTAX, "Ожидался '&' в конце программы", cur.line, cur.col);
    next_token();
}

/* ================== main ================== */
int main(void) {
    FILE *f = fopen("input.txt", "r");
    if (!f) { perror("input.txt"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    src = buf;
    next_token();
    parse_Program();
    if (cur.type != TOK_EOF)
        error(SYNTAX, "Лишние символы после '&'", cur.line, cur.col);
    printf("Синтаксический и семантический анализ завершён успешно.\n");
    free(buf);
    return 0;
}