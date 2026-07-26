/*
 * gec — a small C compiler for the GE-120 / GE-130.
 *
 * Emits gasm assembly per software/gemu/docs/ABI.md. Memory-to-memory code
 * generation: every scalar value is computed in a 2-byte frame temporary;
 * the eight change registers are used only to form effective addresses and
 * to hold the stack pointer (R6), frame pointer (R5) and link register (R7).
 *
 * Subset: functions, int/char/pointer/1-D arrays, globals & locals,
 * + - * / %, comparisons, && || !, unary -, & * [], assignment, calls
 * (incl. recursion), if/else, while, for, return, blocks, string/char
 * literals (translated to the GE-100 internal graphic set).
 *
 * Type model: char=1 (unsigned), int/short=2, pointer=2, big-endian.
 * Multiply/divide/shift are emitted as calls to runtime helpers (the ISA
 * has no binary mul/div and no shift). crt0 + helpers are emitted as a
 * preamble so the whole program assembles in one gasm pass.
 *
 *   build:  cc -std=c99 -Wall -o gec gec.c
 *   use:    ./gec prog.c -o prog.bin        # drives gasm (gcc-style)
 *           ./gec prog.c --boot -o prog.cap # ready boot DECK for the real
 *                                           #   machine (gasm --boot; feed
 *                                           #   with 'arm prog.cap')
 *           ./gec prog.c -S -o prog.s       # stop at gasm assembly
 *           ge prog.bin            # main()'s return value ends up in __rv
 *
 * The driver finds gasm next to itself (../assembler/gasm) or in PATH.
 * crt0 (__start) is emitted FIRST at the origin, so image entry == origin
 * -- the exact contract of the gasm --boot card.
 *
 * Not a conforming C compiler; see docs/ABI.md §7 for limits.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */
/* ------------------------------------------------------------------ */
static const char *g_src;     /* whole source, for line reporting     */
static const char *g_file = "<stdin>";
static int g_use_stdio;
static int line_of(const char *p) {
    int n = 1; for (const char *q = g_src; q && q < p; q++) if (*q == '\n') n++;
    return n;
}
static const char *g_errpos;
static void die(const char *fmt, ...) __attribute__((noreturn));
static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "%s:%d: error: ", g_file, g_errpos ? line_of(g_errpos) : 0);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap); exit(1);
}

/* ------------------------------------------------------------------ */
/* lexer                                                               */
/* ------------------------------------------------------------------ */
enum {
    T_EOF, T_NUM, T_ID, T_STR, T_CHARK,
    T_LP, T_RP, T_LB, T_RB, T_LBRK, T_RBRK,
    T_SEMI, T_COMMA, T_ASSIGN,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PCT,
    T_AMP, T_NOT, T_SHL, T_SHR, T_OR, T_XOR, T_TILDE,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_ANDAND, T_OROR,
    /* keywords */
    T_INT, T_CHAR, T_VOID, T_SHORT,
    T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN
};
typedef struct { int kind; long num; char *text; const char *pos; } Token;
static const char *lp;        /* lexer cursor */
static Token tok, ntok;       /* current + 1-lookahead */
static int have_ntok = 0;

static char *xstrndup(const char *s, int n) {
    char *r = malloc(n + 1); memcpy(r, s, n); r[n] = 0; return r;
}

static int kw(const char *s, int n) {
    struct { const char *w; int t; } K[] = {
        {"int",T_INT},{"char",T_CHAR},{"void",T_VOID},{"short",T_SHORT},
        {"if",T_IF},{"else",T_ELSE},{"while",T_WHILE},{"for",T_FOR},
        {"return",T_RETURN},{0,0}
    };
    for (int i = 0; K[i].w; i++)
        if ((int)strlen(K[i].w) == n && !strncmp(K[i].w, s, n)) return K[i].t;
    return 0;
}

/* GE-100 internal graphic code for an ASCII character (ISA.md §2.1). */
static int ge100(int c) {
    if (c >= '0' && c <= '9') return 0x40 + (c - '0');
    if (c >= 'A' && c <= 'I') return 0x51 + (c - 'A');
    if (c >= 'J' && c <= 'R') return 0xA1 + (c - 'J');
    if (c >= 'S' && c <= 'Z') return 0xB2 + (c - 'S');
    /* lower-case folds to upper (the set has no lower case) */
    if (c >= 'a' && c <= 'z') return ge100(c - 'a' + 'A');
    switch (c) {
        case '\n':return 0x00; case '\r':return 0x00; case '\t': return 0x50;
        case ' ': return 0x50; case '[': return 0x4A; case '#': return 0x4B;
        case '@': return 0x4C; case ':': return 0x4D; case '>': return 0x4E;
        case '?': return 0x4F; case '&': return 0x5A; case '.': return 0x5B;
        case ']': return 0x5C; case '(': return 0x5D; case '<': return 0x5E;
        case '\\':return 0x5F; case '$': return 0xAB; case '-': return 0xAA;
        case ')': return 0xAD;
        case ';': return 0xAE; case '\'':return 0xAF; case '+': return 0xB0;
        case '/': return 0xB1; case ',': return 0xBB; case '%': return 0xBC;
        case '=': return 0xBD; case '"': return 0xBE; case '!': return 0xBF;
        case 0:   return 0x00;
    }
    return 0x50; /* unknown -> space */
}

static int esc(int c) { /* C escape after backslash */
    switch (c) { case 'n': return '\n'; case 't': return '\t';
                 case '0': return 0; case '\\': return '\\';
                 case '\'': return '\''; case '"': return '"'; }
    return c;
}

static char *preprocess_source(const char *src)
{
    size_t cap = strlen(src) + 1;
    char *out = malloc(cap);
    size_t oi = 0;

    g_use_stdio = 0;
    while (*src) {
        const char *line = src;
        while (*src && *src != '\n')
            src++;
        const char *end = src;
        const char *p = line;
        while (p < end && isspace((unsigned char)*p))
            p++;
        if (p < end && *p == '#') {
            p++;
            while (p < end && isspace((unsigned char)*p))
                p++;
            if (end - p >= 7 && !strncmp(p, "include", 7)) {
                p += 7;
                while (p < end && isspace((unsigned char)*p))
                    p++;
                if ((end - p == 9 && !strncmp(p, "<stdio.h>", 9)) ||
                    (end - p == 9 && !strncmp(p, "\"stdio.h\"", 9))) {
                    g_use_stdio = 1;
                } else {
                    g_errpos = line;
                    die("unsupported include directive");
                }
            } else {
                g_errpos = line;
                die("unsupported preprocessor directive");
            }
        } else {
            size_t n = (size_t)(end - line);
            memcpy(out + oi, line, n);
            oi += n;
            if (*src == '\n')
                out[oi++] = '\n';
        }
        if (*src == '\n')
            src++;
    }
    out[oi] = 0;
    return out;
}

static Token lex(void) {
    Token t; memset(&t, 0, sizeof t);
    for (;;) {
        while (*lp && isspace((unsigned char)*lp)) lp++;
        if (lp[0] == '/' && lp[1] == '/') { while (*lp && *lp != '\n') lp++; continue; }
        if (lp[0] == '/' && lp[1] == '*') { lp += 2; while (*lp && !(lp[0]=='*'&&lp[1]=='/')) lp++; if (*lp) lp += 2; continue; }
        break;
    }
    t.pos = lp;
    if (!*lp) { t.kind = T_EOF; return t; }
    if (isdigit((unsigned char)*lp)) {
        char *end; long v;
        if (lp[0]=='0' && (lp[1]=='x'||lp[1]=='X')) v = strtol(lp, &end, 16);
        else v = strtol(lp, &end, 10);
        t.kind = T_NUM; t.num = v; lp = end; return t;
    }
    if (isalpha((unsigned char)*lp) || *lp == '_') {
        const char *s = lp; while (isalnum((unsigned char)*lp) || *lp == '_') lp++;
        int n = (int)(lp - s); int k = kw(s, n);
        if (k) { t.kind = k; } else { t.kind = T_ID; t.text = xstrndup(s, n); }
        return t;
    }
    if (*lp == '\'') {
        lp++; int c = *lp;
        if (c == '\\') { lp++; c = esc(*lp); }
        lp++; if (*lp == '\'') lp++;
        t.kind = T_CHARK; t.num = ge100(c); return t;
    }
    if (*lp == '"') {
        lp++; char buf[1024]; int n = 0;
        while (*lp && *lp != '"') {
            int c = *lp++;
            if (c == '\\') c = esc(*lp++);
            buf[n++] = (char)ge100(c);
        }
        if (*lp == '"') lp++;
        buf[n] = 0;
        t.kind = T_STR; t.text = malloc(n + 1); memcpy(t.text, buf, n + 1); t.num = n;
        return t;
    }
    int c = *lp++;
    switch (c) {
        case '(': t.kind = T_LP; return t;   case ')': t.kind = T_RP; return t;
        case '{': t.kind = T_LB; return t;   case '}': t.kind = T_RB; return t;
        case '[': t.kind = T_LBRK; return t; case ']': t.kind = T_RBRK; return t;
        case ';': t.kind = T_SEMI; return t; case ',': t.kind = T_COMMA; return t;
        case '+': t.kind = T_PLUS; return t; case '-': t.kind = T_MINUS; return t;
        case '*': t.kind = T_STAR; return t; case '/': t.kind = T_SLASH; return t;
        case '%': t.kind = T_PCT; return t;  case '~': t.kind = T_TILDE; return t;
        case '^': t.kind = T_XOR; return t;
        case '=': if (*lp=='='){lp++;t.kind=T_EQ;} else t.kind=T_ASSIGN; return t;
        case '!': if (*lp=='='){lp++;t.kind=T_NE;} else t.kind=T_NOT; return t;
        case '<': if (*lp=='='){lp++;t.kind=T_LE;} else if(*lp=='<'){lp++;t.kind=T_SHL;} else t.kind=T_LT; return t;
        case '>': if (*lp=='='){lp++;t.kind=T_GE;} else if(*lp=='>'){lp++;t.kind=T_SHR;} else t.kind=T_GT; return t;
        case '&': if (*lp=='&'){lp++;t.kind=T_ANDAND;} else t.kind=T_AMP; return t;
        case '|': if (*lp=='|'){lp++;t.kind=T_OROR;} else t.kind=T_OR; return t;
    }
    g_errpos = t.pos; die("stray character '%c'", c);
    return t;
}
static void advance(void) {
    if (have_ntok) { tok = ntok; have_ntok = 0; } else tok = lex();
}
static int accept(int k) { if (tok.kind == k) { advance(); return 1; } return 0; }
static void expect(int k, const char *what) {
    if (tok.kind != k) { g_errpos = tok.pos; die("expected %s", what); }
    advance();
}

/* ------------------------------------------------------------------ */
/* types                                                               */
/* ------------------------------------------------------------------ */
enum { TY_INT, TY_CHAR, TY_PTR, TY_ARRAY, TY_VOID };
typedef struct Type {
    int kind;
    struct Type *base;   /* for PTR / ARRAY */
    int len;             /* for ARRAY */
} Type;
static Type ty_int = { TY_INT, 0, 0 };
static Type ty_char = { TY_CHAR, 0, 0 };
static Type ty_void = { TY_VOID, 0, 0 };
static Type *ptr_to(Type *b) { Type *t = calloc(1, sizeof *t); t->kind = TY_PTR; t->base = b; return t; }
static Type *array_of(Type *b, int n) { Type *t = calloc(1, sizeof *t); t->kind = TY_ARRAY; t->base = b; t->len = n; return t; }
static int ty_size(Type *t) {
    switch (t->kind) {
        case TY_CHAR: return 1;
        case TY_INT: case TY_PTR: return 2;
        case TY_ARRAY: return t->len * ty_size(t->base);
        default: return 0;
    }
}
static int is_ptrish(Type *t) { return t->kind == TY_PTR || t->kind == TY_ARRAY; }
static Type *elem_of(Type *t) { return t->base; }

/* ------------------------------------------------------------------ */
/* symbols & AST                                                       */
/* ------------------------------------------------------------------ */
typedef struct Sym {
    char *name; Type *type;
    int is_global; int off;   /* local: frame offset; global: uses label _name */
    struct Sym *next;
} Sym;

enum {
    E_NUM, E_VAR, E_STR, E_CALL, E_BIN, E_ASSIGN, E_INDEX,
    E_ADDR, E_DEREF, E_LOGAND, E_LOGOR, E_NOT, E_NEG, E_CVT
};
enum {
    S_EXPR, S_IF, S_WHILE, S_FOR, S_RETURN, S_BLOCK, S_DECL, S_EMPTY
};
typedef struct Node {
    int kind; int op;          /* expr kind / stmt kind; op = token for E_BIN */
    Type *type;
    long num;                  /* E_NUM */
    char *str; int slabel;     /* E_STR: bytes + assigned data label index */
    Sym *sym;                  /* E_VAR */
    struct Node *a, *b, *c, *d;/* generic children */
    struct Node **args; int nargs; /* E_CALL */
    char *name;                /* call target */
    struct Node *next;         /* statement / decl list */
} Node;
static Node *newn(int k) { Node *n = calloc(1, sizeof *n); n->kind = k; return n; }

/* string literals collected for the .data section */
typedef struct Str { char *bytes; int len; int label; struct Str *next; } Str;
static Str *g_strs; static int g_strn;

/* ------------------------------------------------------------------ */
/* parser                                                              */
/* ------------------------------------------------------------------ */
static Sym *g_globals;             /* global symbol list */
static Sym *l_locals;              /* current function locals (params+locals) */
static int  l_args_size;           /* bytes of params */
static int  l_locals_size;         /* bytes of locals (after params) */
static Type *cur_ret;              /* return type of current function */

static Sym *find_sym(const char *name) {
    for (Sym *s = l_locals; s; s = s->next) if (!strcmp(s->name, name)) return s;
    for (Sym *s = g_globals; s; s = s->next) if (!strcmp(s->name, name)) return s;
    return 0;
}

static Type *parse_base_type(void) {
    if (accept(T_INT) || accept(T_SHORT)) return &ty_int;
    if (accept(T_CHAR)) return &ty_char;
    if (accept(T_VOID)) return &ty_void;
    return 0;
}
/* after a base type, consume any '*'s */
static Type *parse_ptr_suffix(Type *t) { while (accept(T_STAR)) t = ptr_to(t); return t; }

static Node *parse_expr(void);
static Node *parse_assign(void);

static Node *mk_num(long v) { Node *n = newn(E_NUM); n->num = v; n->type = &ty_int; return n; }

static Node *parse_primary(void) {
    if (accept(T_LP)) { Node *n = parse_expr(); expect(T_RP, "')'"); return n; }
    if (tok.kind == T_NUM)  { Node *n = mk_num(tok.num); advance(); return n; }
    if (tok.kind == T_CHARK){ Node *n = mk_num(tok.num); n->type=&ty_int; advance(); return n; }
    if (tok.kind == T_STR) {
        Node *n = newn(E_STR); n->str = tok.text; n->num = tok.num;
        Str *s = calloc(1, sizeof *s); s->bytes = tok.text; s->len = (int)tok.num;
        s->label = g_strn; n->slabel = g_strn++; s->next = g_strs; g_strs = s;
        n->type = ptr_to(&ty_char); advance(); return n;
    }
    if (tok.kind == T_ID) {
        char *name = tok.text; const char *pos = tok.pos; advance();
        if (tok.kind == T_LP) {           /* function call */
            advance();
            Node *n = newn(E_CALL); n->name = name; n->type = &ty_int;
            Node *args[32]; int na = 0;
            if (tok.kind != T_RP) {
                do { args[na++] = parse_assign(); } while (accept(T_COMMA));
            }
            expect(T_RP, "')'");
            n->nargs = na; n->args = malloc(sizeof(Node*) * (na ? na : 1));
            for (int i = 0; i < na; i++) n->args[i] = args[i];
            return n;
        }
        Sym *s = find_sym(name);
        if (!s) { g_errpos = pos; die("undeclared identifier '%s'", name); }
        Node *n = newn(E_VAR); n->sym = s; n->type = s->type; return n;
    }
    g_errpos = tok.pos; die("expected expression");
    return 0;
}

static Node *parse_postfix(void) {
    Node *n = parse_primary();
    for (;;) {
        if (accept(T_LBRK)) {              /* a[i]  ==  *(a + i) */
            Node *idx = parse_expr(); expect(T_RBRK, "']'");
            Node *e = newn(E_INDEX); e->a = n; e->b = idx;
            if (!is_ptrish(n->type)) { g_errpos = tok.pos; die("subscript of non-array"); }
            e->type = elem_of(n->type); n = e;
        } else break;
    }
    return n;
}

static Node *parse_unary(void) {
    if (accept(T_MINUS)) { Node *n = newn(E_NEG); n->a = parse_unary(); n->type = &ty_int; return n; }
    if (accept(T_NOT))   { Node *n = newn(E_NOT); n->a = parse_unary(); n->type = &ty_int; return n; }
    if (accept(T_STAR))  { Node *n = newn(E_DEREF); n->a = parse_unary();
                           if (!is_ptrish(n->a->type)) die("cannot dereference non-pointer");
                           n->type = elem_of(n->a->type); return n; }
    if (accept(T_AMP))   { Node *n = newn(E_ADDR); n->a = parse_unary(); n->type = ptr_to(n->a->type); return n; }
    return parse_postfix();
}

/* operator-precedence climbing */
static int prec(int t) {
    switch (t) {
        case T_STAR: case T_SLASH: case T_PCT: return 10;
        case T_PLUS: case T_MINUS: return 9;
        case T_SHL: case T_SHR: return 8;
        case T_LT: case T_GT: case T_LE: case T_GE: return 7;
        case T_EQ: case T_NE: return 6;
        case T_AMP: return 5; case T_XOR: return 4; case T_OR: return 3;
        case T_ANDAND: return 2; case T_OROR: return 1;
        default: return 0;
    }
}
static Node *parse_binop(int minp) {
    Node *lhs = parse_unary();
    for (;;) {
        int t = tok.kind, p = prec(t);
        if (p == 0 || p < minp) break;
        advance();
        Node *rhs = parse_binop(p + 1);
        Node *n;
        if (t == T_ANDAND) { n = newn(E_LOGAND); }
        else if (t == T_OROR) { n = newn(E_LOGOR); }
        else { n = newn(E_BIN); n->op = t; }
        n->a = lhs; n->b = rhs;
        /* result type: pointer +/- int keeps pointer type; else int */
        if ((t == T_PLUS || t == T_MINUS) && is_ptrish(lhs->type)) n->type = lhs->type;
        else n->type = &ty_int;
        lhs = n;
    }
    return lhs;
}
static Node *parse_assign(void) {
    Node *lhs = parse_binop(1);
    if (accept(T_ASSIGN)) {
        Node *rhs = parse_assign();
        Node *n = newn(E_ASSIGN); n->a = lhs; n->b = rhs; n->type = lhs->type;
        return n;
    }
    return lhs;
}
static Node *parse_expr(void) { return parse_assign(); }

/* declarations inside a function: returns a chain of S_DECL nodes */
static Node *parse_local_decl(void) {
    Type *base = parse_base_type();
    Node *head = 0, **tail = &head;
    do {
        Type *t = parse_ptr_suffix(base);
        if (tok.kind != T_ID) { g_errpos = tok.pos; die("expected declarator"); }
        char *name = tok.text; advance();
        if (accept(T_LBRK)) {            /* array */
            if (tok.kind != T_NUM) die("array size must be a constant");
            int n = (int)tok.num; advance(); expect(T_RBRK, "']'");
            t = array_of(t, n);
        }
        Sym *s = calloc(1, sizeof *s); s->name = name; s->type = t;
        s->off = l_args_size + 4 + l_locals_size;   /* after params + savedFP + savedLR */
        l_locals_size += ty_size(t);
        s->next = l_locals; l_locals = s;
        Node *d = newn(S_DECL); d->sym = s;
        if (accept(T_ASSIGN)) d->a = parse_assign();   /* initializer */
        *tail = d; tail = &d->next;
    } while (accept(T_COMMA));
    expect(T_SEMI, "';'");
    return head;
}

static Node *parse_stmt(void);
static Node *parse_block(void) {
    expect(T_LB, "'{'");
    Node *head = 0, **tail = &head;
    while (tok.kind != T_RB && tok.kind != T_EOF) {
        Node *s;
        if (tok.kind==T_INT||tok.kind==T_CHAR||tok.kind==T_VOID||tok.kind==T_SHORT)
            s = parse_local_decl();
        else s = parse_stmt();
        if (s) { *tail = s; while (*tail) tail = &(*tail)->next; }
    }
    expect(T_RB, "'}'");
    Node *blk = newn(S_BLOCK); blk->a = head; return blk;
}
static Node *parse_stmt(void) {
    if (tok.kind == T_LB) return parse_block();
    if (accept(T_SEMI)) { return newn(S_EMPTY); }
    if (accept(T_IF)) {
        Node *n = newn(S_IF); expect(T_LP, "'('"); n->a = parse_expr(); expect(T_RP, "')'");
        n->b = parse_stmt(); if (accept(T_ELSE)) n->c = parse_stmt(); return n;
    }
    if (accept(T_WHILE)) {
        Node *n = newn(S_WHILE); expect(T_LP, "'('"); n->a = parse_expr(); expect(T_RP, "')'");
        n->b = parse_stmt(); return n;
    }
    if (accept(T_FOR)) {
        Node *n = newn(S_FOR); expect(T_LP, "'('");
        if (!accept(T_SEMI)) { n->a = parse_expr(); expect(T_SEMI, "';'"); }
        if (!accept(T_SEMI)) { n->b = parse_expr(); expect(T_SEMI, "';'"); }
        if (tok.kind != T_RP) n->c = parse_expr();
        expect(T_RP, "')'"); n->d = parse_stmt(); return n;
    }
    if (accept(T_RETURN)) {
        Node *n = newn(S_RETURN);
        if (tok.kind != T_SEMI) n->a = parse_expr();
        expect(T_SEMI, "';'"); return n;
    }
    Node *n = newn(S_EXPR); n->a = parse_expr(); expect(T_SEMI, "';'"); return n;
}

/* ------------------------------------------------------------------ */
/* top level: functions & globals                                      */
/* ------------------------------------------------------------------ */
typedef struct Func {
    char *name; Type *ret; Node *body;
    Sym *locals; int args_size, locals_size;
    struct Func *next;
} Func;
static Func *g_funcs, **g_functail = &g_funcs;

static void parse_program(void) {
    advance();
    while (tok.kind != T_EOF) {
        Type *base = parse_base_type();
        if (!base) { g_errpos = tok.pos; die("expected a type at top level"); }
        Type *t = parse_ptr_suffix(base);
        if (tok.kind != T_ID) { g_errpos = tok.pos; die("expected name"); }
        char *name = tok.text; advance();
        if (tok.kind == T_LP) {                 /* function definition */
            advance();
            l_locals = 0; l_args_size = 0; l_locals_size = 0; cur_ret = base;
            /* parameters */
            if (tok.kind != T_RP) {
                do {
                    Type *pb = parse_base_type();
                    if (!pb) die("expected parameter type");
                    if (pb->kind == TY_VOID && tok.kind == T_RP) break;  /* (void) */
                    Type *pt = parse_ptr_suffix(pb);
                    if (tok.kind != T_ID) die("expected parameter name");
                    char *pn = tok.text; advance();
                    if (accept(T_LBRK)) { /* array param decays to pointer */
                        if (tok.kind==T_NUM) advance();
                        expect(T_RBRK,"']'"); pt = ptr_to(pt);
                    }
                    Sym *s = calloc(1, sizeof *s); s->name = pn; s->type = pt;
                    s->off = l_args_size; l_args_size += ty_size(pt);
                    s->next = l_locals; l_locals = s;
                } while (accept(T_COMMA));
            }
            expect(T_RP, "')'");
            Func *f = calloc(1, sizeof *f);
            f->name = name; f->ret = base;
            f->body = parse_block();
            f->locals = l_locals; f->args_size = l_args_size; f->locals_size = l_locals_size;
            *g_functail = f; g_functail = &f->next;
        } else {                                 /* global variable */
            if (accept(T_LBRK)) {
                if (tok.kind != T_NUM) die("array size must be constant");
                int n = (int)tok.num; advance(); expect(T_RBRK, "']'");
                t = array_of(t, n);
            }
            Sym *s = calloc(1, sizeof *s); s->name = name; s->type = t; s->is_global = 1;
            if (accept(T_ASSIGN)) { if (tok.kind != T_NUM) die("global init must be constant");
                                    s->off = (int)tok.num; s->next = 0; advance(); s->off |= 0x10000; }
            s->next = g_globals; g_globals = s;
            expect(T_SEMI, "';'");
        }
    }
}

/* ------------------------------------------------------------------ */
/* code generation                                                     */
/* ------------------------------------------------------------------ */
static FILE *out;             /* current output stream (body buffer)   */
static int temp_base, temp_off, temp_max;
static int g_lbl;
static int lbl(void) { return g_lbl++; }
static int push2(void) { int t = temp_off; temp_off += 2; if (temp_off > temp_max) temp_max = temp_off; return t; }

#define EM(...) fprintf(out, "\t" __VA_ARGS__)

/* an lvalue: where a value lives so we can load/store it */
enum { LV_GLOBAL, LV_LOCAL, LV_TEMP };
typedef struct { int kind; const char *gname; int off; int taddr; Type *type; } LV;

static void gen_expr(Node *n, int dst);     /* leaves 2-byte value at dst(5) */
static LV   gen_lval(Node *n);

/* address string of a byte for instructions that take an address.
   For TEMP lvalues the address must first be loaded into R1 (caller does it). */
static void load_to(LV lv, int dst);
static void store_from(LV lv, int src);

/* ---- load/store through an lvalue ---- */
static void load_to(LV lv, int dst) {
    int sz = ty_size(lv.type);
    /* A 2-byte word at frame offset N occupies mem[N..N+1] (SS-op convention);
     * a register op addresses it by its RIGHTMOST byte, so read the saved
     * address with N+1.  See ISA.md §0.5 row 4. */
    if (lv.kind == LV_TEMP) EM("LR 1, %d(5)\n", lv.taddr + 1);   /* R1 = address */
    if (sz == 2) {
        switch (lv.kind) {
            case LV_GLOBAL: EM("MVC 2, %d(5), _%s\n", dst, lv.gname); break;
            case LV_LOCAL:  EM("MVC 2, %d(5), %d(5)\n", dst, lv.off); break;
            case LV_TEMP:   EM("MVC 2, %d(5), 0x000(1)\n", dst); break;
        }
    } else { /* char: zero-extend into 2-byte temp */
        EM("MVI 0, %d(5)\n", dst);
        switch (lv.kind) {
            case LV_GLOBAL: EM("MVC 1, %d(5), _%s\n", dst + 1, lv.gname); break;
            case LV_LOCAL:  EM("MVC 1, %d(5), %d(5)\n", dst + 1, lv.off); break;
            case LV_TEMP:   EM("MVC 1, %d(5), 0x000(1)\n", dst + 1); break;
        }
    }
}
static void store_from(LV lv, int src) {
    int sz = ty_size(lv.type);
    if (lv.kind == LV_TEMP) EM("LR 1, %d(5)\n", lv.taddr + 1);   /* rightmost byte */
    if (sz == 2) {
        switch (lv.kind) {
            case LV_GLOBAL: EM("MVC 2, _%s, %d(5)\n", lv.gname, src); break;
            case LV_LOCAL:  EM("MVC 2, %d(5), %d(5)\n", lv.off, src); break;
            case LV_TEMP:   EM("MVC 2, 0x000(1), %d(5)\n", src); break;
        }
    } else {
        switch (lv.kind) {
            case LV_GLOBAL: EM("MVC 1, _%s, %d(5)\n", lv.gname, src + 1); break;
            case LV_LOCAL:  EM("MVC 1, %d(5), %d(5)\n", lv.off, src + 1); break;
            case LV_TEMP:   EM("MVC 1, 0x000(1), %d(5)\n", src + 1); break;
        }
    }
}

/* compute the *address* of an lvalue node into a 2-byte temp dst */
static void gen_addr(Node *n, int dst) {
    if (n->kind == E_VAR) {
        /* Store the address into the 2-byte temp mem[dst..dst+1]: a register
         * op addresses the word by its rightmost byte (dst+1). */
        if (n->sym->is_global) { EM("LA 1, _%s\n", n->sym->name); EM("STR 1, %d(5)\n", dst + 1); }
        else { EM("LA 1, %d(5)\n", n->sym->off); EM("STR 1, %d(5)\n", dst + 1); }
        return;
    }
    if (n->kind == E_DEREF) { gen_expr(n->a, dst); return; }   /* &*p == p */
    if (n->kind == E_INDEX) {                                   /* &a[i] */
        int base = push2(), idx = push2();
        /* base address of a */
        if (is_ptrish(n->a->type) && n->a->type->kind == TY_ARRAY) gen_addr(n->a, base);
        else gen_expr(n->a, base);                              /* pointer value */
        gen_expr(n->b, idx);
        int es = ty_size(n->type);
        if (es == 2) EM("AB 2,2, %d(5), %d(5)\n", idx + 1, idx + 1);   /* idx *= 2 */
        else if (es != 1) { /* general: __mul */
            EM("MVC 2, __a, %d(5)\n", idx);
            EM("MVI 0, __b\n"); EM("MVI %d, __b+1\n", es & 0xff);
            EM("JRT 0xF0, __mul\n"); EM("MVC 2, %d(5), __rv\n", idx);
        }
        EM("MVC 2, %d(5), %d(5)\n", dst, base);
        EM("AB 2,2, %d(5), %d(5)\n", dst + 1, idx + 1);          /* dst = base + idx */
        return;
    }
    die("not an lvalue");
}

static LV gen_lval(Node *n) {
    LV lv; lv.type = n->type;
    if (n->kind == E_VAR) {
        if (n->sym->is_global) { lv.kind = LV_GLOBAL; lv.gname = n->sym->name; }
        else { lv.kind = LV_LOCAL; lv.off = n->sym->off; }
        return lv;
    }
    /* *p and a[i] -> address in a temp */
    int t = push2();
    gen_addr(n, t);
    lv.kind = LV_TEMP; lv.taddr = t; return lv;
}

/* binary arithmetic helpers operating on 2-byte temps */
static void emit_add(int dst, int l, int r) { EM("MVC 2, %d(5), %d(5)\n", dst, l); EM("AB 2,2, %d(5), %d(5)\n", dst + 1, r + 1); }
static void emit_sub(int dst, int l, int r) { EM("MVC 2, %d(5), %d(5)\n", dst, l); EM("SB 2,2, %d(5), %d(5)\n", dst + 1, r + 1); }
/* multiply a temp in place by a (compile-time) element size, for ptr arithmetic */
static void emit_scale(int t, int es) {
    if (es == 2) EM("AB 2,2, %d(5), %d(5)\n", t + 1, t + 1);
    else if (es != 1) {
        EM("MVC 2, __a, %d(5)\n", t); EM("MVI 0, __b\n"); EM("MVI %d, __b+1\n", es & 0xff);
        EM("JRT 0xF0, __mul\n"); EM("MVC 2, %d(5), __rv\n", t);
    }
}
static void emit_helper(const char *fn, int dst, int l, int r) {
    EM("MVC 2, __a, %d(5)\n", l); EM("MVC 2, __b, %d(5)\n", r);
    EM("JRT 0xF0, %s\n", fn); EM("MVC 2, %d(5), __rv\n", dst);
}
/* relational: leave boolean (0/1) in dst, using CC of (l - r) */
static void emit_cmp(int dst, int l, int r, int mask) {
    int tc = push2();
    EM("MVC 2, %d(5), %d(5)\n", tc, l);
    EM("SB 2,2, %d(5), %d(5)\n", tc + 1, r + 1);     /* CC = sign(l-r) */
    int Lt = lbl(), Le = lbl();
    EM("MVI 0, %d(5)\n", dst); EM("MVI 0, %d(5)\n", dst + 1);
    EM("JC 0x%02X, L%d\n", mask, Lt);
    EM("JU L%d\n", Le);
    fprintf(out, "L%d:\t", Lt); EM("MVI 1, %d(5)\n", dst + 1);
    fprintf(out, "L%d:\n", Le);
}

static void emit_word_imm(int dst, int value)
{
    EM("MVI 0x%02X, %d(5)\n", (value >> 8) & 0xff, dst);
    EM("MVI 0x%02X, %d(5)\n", value & 0xff, dst + 1);
}

static void emit_call0(const char *fn)
{
    EM("JRT 0xF0, %s\n", fn);
}

static void emit_call1_temp(const char *fn, int arg)
{
    EM("MVC 2, 0x000(6), %d(5)\n", arg);
    EM("JRT 0xF0, %s\n", fn);
}

static void emit_check_io_ok(int Lfail)
{
    EM("CMC 2, __io_ok, __zero\n");
    EM("JC 0x20, L%d\n", Lfail);
}

static int gen_builtin_call(Node *n, int dst)
{
    /* Console / lamp / timing / RNG built-ins. These need no peripheral channel
     * (they ride on real CPU instructions only), so they are handled before the
     * stdio gate below and work without #include <stdio.h>. gec has no `long`
     * or `bool` type, so counts and returns are plain `int`. */
    if (!strcmp(n->name, "lon")) {            /* light the OPER. CALL lamp     */
        EM("LON\n");                          /* 02 80: CI87 sets ALAM         */
        emit_word_imm(dst, 0);
        return 1;
    }
    if (!strcmp(n->name, "loff")) {           /* extinguish it                 */
        EM("LOFF\n");                         /* 02 40: CI88 resets ALAM       */
        emit_word_imm(dst, 0);
        return 1;
    }
    if (!strcmp(n->name, "sleep")) {          /* busy-wait n milliseconds      */
        /* The emulator clock is 4 us/cycle (250 cycles = 1 ms). Spend ~250
         * cycles per ms with an outer (ms) loop around a fixed inner spin; the
         * inner count below is calibrated so each outer pass ~= 1 ms. */
        if (n->nargs != 1)
            die("sleep() expects 1 argument");
        int t = push2();
        gen_expr(n->args[0], t);
        int Lo = lbl(), Ld = lbl(), Li = lbl(), Lie = lbl();
        fprintf(out, "L%d:\tCMC 2, %d(5), __zero\n", Lo, t);
        EM("JC 0x20, L%d\n", Ld);             /* ms == 0 -> done               */
        EM("MVI 0, __acc\n");
        EM("MVI 9, __acc+1\n");               /* inner spins per ms (calibrated)*/
        fprintf(out, "L%d:\tCMC 2, __acc, __zero\n", Li);
        EM("JC 0x20, L%d\n", Lie);
        EM("SB 2,2, __acc+1, __one+1\n");
        EM("JU L%d\n", Li);
        fprintf(out, "L%d:\tSB 2,2, %d(5), __one+1\n", Lie, t + 1); /* ms -= 1  */
        EM("JU L%d\n", Lo);
        fprintf(out, "L%d:\n", Ld);
        emit_word_imm(dst, 0);
        return 1;
    }
    if (!strcmp(n->name, "rand")) {           /* full-period 16-bit LCG        */
        /* s = (s * 25173 + 13849) mod 2^16. No hardware shift, so a Lehmer LCG
         * (a-1 mult. of 4, c odd) gives a full 65536 period via __mul + AB. */
        EM("MVC 2, __a, __rseed\n");
        EM("MVI 0x62, __b\n");
        EM("MVI 0x55, __b+1\n");              /* 25173 = 0x6255                */
        EM("JRT 0xF0, __mul\n");              /* __rv = (s * 25173) mod 2^16   */
        EM("MVI 0x36, __acc\n");
        EM("MVI 0x19, __acc+1\n");            /* 13849 = 0x3619                */
        EM("MVC 2, __rseed, __rv\n");
        EM("AB 2,2, __rseed+1, __acc+1\n");   /* s += 13849                    */
        EM("MVC 2, %d(5), __rseed\n", dst);
        return 1;
    }
    if (!strcmp(n->name, "srand")) {          /* seed rand()                   */
        if (n->nargs != 1)
            die("srand() expects 1 argument");
        int t = push2();
        gen_expr(n->args[0], t);
        EM("MVC 2, __rseed, %d(5)\n", t);
        emit_word_imm(dst, 0);
        return 1;
    }

    int pct = ge100('%');
    int spc = ge100(' ');
    int ch_c = ge100('c');
    int ch_d = ge100('d');
    int ch_s = ge100('s');
    int ch_u = ge100('u');

    if (!g_use_stdio)
        return 0;

    if (!strcmp(n->name, "printf")) {
        if (n->nargs < 1 || n->args[0]->kind != E_STR)
            die("printf requires a literal format string when stdio.h is used");
        unsigned char *fmt = (unsigned char *)n->args[0]->str;
        int flen = (int)n->args[0]->num;
        int ai = 1;
        for (int i = 0; i < flen; i++) {
            int t;
            if (fmt[i] != pct) {
                t = push2();
                emit_word_imm(t, fmt[i]);
                emit_call1_temp("_putchar", t);
                continue;
            }
            if (++i >= flen)
                die("dangling %% in printf format");
            if (fmt[i] == pct) {
                t = push2();
                emit_word_imm(t, pct);
                emit_call1_temp("_putchar", t);
                continue;
            }
            if (ai >= n->nargs)
                die("printf format/argument count mismatch");
            t = push2();
            gen_expr(n->args[ai++], t);
            if (fmt[i] == ch_c) emit_call1_temp("_putchar", t);
            else if (fmt[i] == ch_s) emit_call1_temp("__io_putstr", t);
            else if (fmt[i] == ch_d) emit_call1_temp("__io_printi", t);
            else if (fmt[i] == ch_u) emit_call1_temp("__io_printu", t);
            else die("unsupported printf format specifier");
        }
        emit_word_imm(dst, 0);
        return 1;
    }

    if (!strcmp(n->name, "scanf")) {
        if (n->nargs < 1 || n->args[0]->kind != E_STR)
            die("scanf requires a literal format string when stdio.h is used");
        unsigned char *fmt = (unsigned char *)n->args[0]->str;
        int flen = (int)n->args[0]->num;
        int ai = 1;
        int assigned = push2();
        int Lend = lbl();
        emit_word_imm(assigned, 0);
        emit_call0("__io_readline");
        for (int i = 0; i < flen; i++) {
            int pt;
            if (fmt[i] == spc) {
                emit_call0("__io_skip_space");
                while (i + 1 < flen && fmt[i + 1] == spc)
                    i++;
                continue;
            }
            if (fmt[i] != pct) {
                int ch = push2();
                emit_word_imm(ch, fmt[i]);
                emit_call1_temp("__io_expect", ch);
                emit_check_io_ok(Lend);
                continue;
            }
            if (++i >= flen)
                die("dangling %% in scanf format");
            if (ai >= n->nargs)
                die("scanf format/argument count mismatch");
            pt = push2();
            gen_expr(n->args[ai++], pt);
            if (fmt[i] == ch_d || fmt[i] == ch_u) {
                EM("JRT 0xF0, __io_skip_space\n");
                EM("JRT 0xF0, __io_parse_int\n");
                emit_check_io_ok(Lend);
                EM("LR 1, %d(5)\n", pt + 1);
                EM("MVC 2, 0x000(1), __rv\n");
            } else if (fmt[i] == ch_c) {
                EM("JRT 0xF0, __io_parse_char\n");
                emit_check_io_ok(Lend);
                EM("LR 1, %d(5)\n", pt + 1);
                EM("MVC 1, 0x000(1), __rv+1\n");
            } else if (fmt[i] == ch_s) {
                EM("JRT 0xF0, __io_skip_space\n");
                emit_call1_temp("__io_parse_word", pt);
                emit_check_io_ok(Lend);
            } else {
                die("unsupported scanf format specifier");
            }
            EM("AB 2,2, %d(5), __one+1\n", assigned + 1);
        }
        fprintf(out, "L%d:\n", Lend);
        EM("MVC 2, %d(5), %d(5)\n", dst, assigned);
        return 1;
    }

    return 0;
}

static const char *cur_fn;   /* for the return label */

static void gen_expr(Node *n, int dst) {
    /* Array-to-pointer decay: an array used as a value yields its address. */
    if (n->type && n->type->kind == TY_ARRAY && n->kind != E_ADDR) {
        gen_addr(n, dst);
        return;
    }
    switch (n->kind) {
    case E_NUM:
        EM("MVI %ld, %d(5)\n", (n->num >> 8) & 0xff, dst);
        EM("MVI %ld, %d(5)\n", n->num & 0xff, dst + 1);
        return;
    case E_STR:
        EM("LA 1, __str%d\n", n->slabel); EM("STR 1, %d(5)\n", dst + 1); return;
    case E_VAR: case E_DEREF: case E_INDEX: {
        LV lv = gen_lval(n); load_to(lv, dst); return;
    }
    case E_ADDR: gen_addr(n->a, dst); return;
    case E_ASSIGN: {
        int rv = push2(); gen_expr(n->b, rv);
        LV lv = gen_lval(n->a); store_from(lv, rv);
        EM("MVC 2, %d(5), %d(5)\n", dst, rv);    /* assignment yields the value */
        return;
    }
    case E_NEG: {
        int t = push2(); gen_expr(n->a, t);
        EM("MVI 0, %d(5)\n", dst); EM("MVI 0, %d(5)\n", dst + 1);
        EM("SB 2,2, %d(5), %d(5)\n", dst + 1, t + 1); return;
    }
    case E_NOT: {
        int t = push2(); gen_expr(n->a, t);
        int L1 = lbl(), L2 = lbl();
        EM("MVI 0, %d(5)\n", dst); EM("MVI 0, %d(5)\n", dst + 1);
        EM("CMC 2, %d(5), __zero\n", t);
        EM("JC 0x20, L%d\n", L1);                 /* t == 0 -> result 1 */
        EM("JU L%d\n", L2);
        fprintf(out, "L%d:\t", L1); EM("MVI 1, %d(5)\n", dst + 1);
        fprintf(out, "L%d:\n", L2);
        return;
    }
    case E_LOGAND: {
        int Lf = lbl(); int t = push2();
        EM("MVI 0, %d(5)\n", dst); EM("MVI 0, %d(5)\n", dst + 1);
        gen_expr(n->a, t); EM("CMC 2, %d(5), __zero\n", t); EM("JC 0x20, L%d\n", Lf);
        gen_expr(n->b, t); EM("CMC 2, %d(5), __zero\n", t); EM("JC 0x20, L%d\n", Lf);
        EM("MVI 1, %d(5)\n", dst + 1);
        fprintf(out, "L%d:\n", Lf);
        return;
    }
    case E_LOGOR: {
        int Lt = lbl(); int t = push2();
        EM("MVI 0, %d(5)\n", dst); EM("MVI 1, %d(5)\n", dst + 1);
        gen_expr(n->a, t); EM("CMC 2, %d(5), __zero\n", t); EM("JC 0x50, L%d\n", Lt);
        gen_expr(n->b, t); EM("CMC 2, %d(5), __zero\n", t); EM("JC 0x50, L%d\n", Lt);
        EM("MVI 0, %d(5)\n", dst + 1);
        fprintf(out, "L%d:\n", Lt);
        return;
    }
    case E_BIN: {
        int l = push2(), r = push2();
        gen_expr(n->a, l); gen_expr(n->b, r);
        switch (n->op) {
        case T_PLUS:
            if (is_ptrish(n->a->type))      emit_scale(r, ty_size(elem_of(n->a->type)));
            else if (is_ptrish(n->b->type)) emit_scale(l, ty_size(elem_of(n->b->type)));
            emit_add(dst, l, r); return;
        case T_MINUS:
            if (is_ptrish(n->a->type) && !is_ptrish(n->b->type))
                emit_scale(r, ty_size(elem_of(n->a->type)));
            emit_sub(dst, l, r); return;
        case T_STAR:  emit_helper("__mul", dst, l, r); return;
        case T_SLASH: emit_helper("__divu", dst, l, r); return;
        case T_PCT:   emit_helper("__modu", dst, l, r); return;
        case T_SHL:   emit_helper("__shl", dst, l, r); return;
        case T_SHR:   emit_helper("__shru", dst, l, r); return;
        case T_AMP:   EM("MVC 2, %d(5), %d(5)\n", dst, l); EM("NC 2, %d(5), %d(5)\n", dst, r); return;
        case T_OR:    EM("MVC 2, %d(5), %d(5)\n", dst, l); EM("OC 2, %d(5), %d(5)\n", dst, r); return;
        case T_XOR:   EM("MVC 2, %d(5), %d(5)\n", dst, l); EM("XC 2, %d(5), %d(5)\n", dst, r); return;
        case T_LT: emit_cmp(dst, l, r, 0x40); return;
        case T_GT: emit_cmp(dst, l, r, 0x10); return;
        case T_LE: emit_cmp(dst, l, r, 0x60); return;
        case T_GE: emit_cmp(dst, l, r, 0x30); return;
        case T_EQ: emit_cmp(dst, l, r, 0x20); return;
        case T_NE: emit_cmp(dst, l, r, 0x50); return;
        }
        die("bad binop");
    }
    case E_CALL: {
        if (gen_builtin_call(n, dst))
            return;
        int tmps[32];
        for (int i = 0; i < n->nargs; i++) { tmps[i] = push2(); gen_expr(n->args[i], tmps[i]); }
        /* write args to the outgoing area (relative to SP = R6) */
        int aoff = 0;
        for (int i = 0; i < n->nargs; i++) {
            EM("MVC 2, %d(6), %d(5)\n", aoff, tmps[i]); aoff += 2;
        }
        EM("JRT 0xF0, _%s\n", n->name);
        EM("MVC 2, %d(5), __rv\n", dst);      /* collect return value */
        return;
    }
    }
    die("cannot generate expression");
}

static void gen_stmt(Node *n);
static void gen_stmts(Node *list) { for (Node *s = list; s; s = s->next) gen_stmt(s); }

static void gen_cond_branch(Node *cond, int target_false) {
    temp_off = temp_base;
    int t = push2(); gen_expr(cond, t);
    EM("CMC 2, %d(5), __zero\n", t);    /* compare full 2-byte value to 0 */
    EM("JC 0x20, L%d\n", target_false); /* cc2 == equal == zero == false */
}

static void gen_stmt(Node *n) {
    temp_off = temp_base;
    switch (n->kind) {
    case S_EMPTY: return;
    case S_BLOCK: gen_stmts(n->a); return;
    case S_DECL:
        if (n->a) { int t = push2(); gen_expr(n->a, t);
                    LV lv; lv.kind = LV_LOCAL; lv.off = n->sym->off; lv.type = n->sym->type;
                    store_from(lv, t); }
        return;
    case S_EXPR: { int t = push2(); gen_expr(n->a, t); return; }
    case S_RETURN:
        if (n->a) { int t = push2(); gen_expr(n->a, t); EM("MVC 2, __rv, %d(5)\n", t); }
        EM("JU L%s_ret\n", cur_fn); return;
    case S_IF: {
        int Lelse = lbl(), Lend = lbl();
        gen_cond_branch(n->a, Lelse);
        gen_stmt(n->b);
        EM("JU L%d\n", Lend);
        fprintf(out, "L%d:\n", Lelse);
        if (n->c) gen_stmt(n->c);
        fprintf(out, "L%d:\n", Lend);
        return;
    }
    case S_WHILE: {
        int Ltop = lbl(), Lend = lbl();
        fprintf(out, "L%d:\n", Ltop);
        gen_cond_branch(n->a, Lend);
        gen_stmt(n->b);
        EM("JU L%d\n", Ltop);
        fprintf(out, "L%d:\n", Lend);
        return;
    }
    case S_FOR: {
        int Ltop = lbl(), Lend = lbl();
        if (n->a) { temp_off = temp_base; int t = push2(); gen_expr(n->a, t); }
        fprintf(out, "L%d:\n", Ltop);
        if (n->b) gen_cond_branch(n->b, Lend);
        gen_stmt(n->d);
        if (n->c) { temp_off = temp_base; int t = push2(); gen_expr(n->c, t); }
        EM("JU L%d\n", Ltop);
        fprintf(out, "L%d:\n", Lend);
        return;
    }
    }
    die("cannot generate statement");
}

/* ------------------------------------------------------------------ */
/* runtime preamble: EQUs, crt0, helpers                               */
/* ------------------------------------------------------------------ */
static void emit_runtime(FILE *o) {
    fprintf(o,
        "; ---- GE-120 C runtime (gec) ----\n"
        "__rv   EQU 0x0010\n"           /* return value (16 bytes)            */
        "__a    EQU 0x0020\n"           /* helper operand A                   */
        "__b    EQU 0x0022\n"           /* helper operand B                   */
        "__acc  EQU 0x0024\n"           /* helper accumulator / remainder     */
        "__one  EQU 0x0026\n"           /* constant 1 (set by crt0)           */
        "__qb   EQU 0x0028\n"           /* scratch quotient-bit byte          */
        "__zero EQU 0x002A\n"           /* constant 0 (set by crt0)           */
        "__lr   EQU 0x002C\n"           /* saved link register for non-leaf helpers */
        "__pw   EQU 0x002E\n"           /* scratch power-of-two               */
        "__io_status EQU 0x0030\n"
        "__io_count  EQU 0x0032\n"
        "__io_ok     EQU 0x0034\n"
        "__io_inp    EQU 0x0036\n"
        "__io_neg    EQU 0x0038\n"
        "__io_chr    EQU 0x003A\n"
        "__io_tmp    EQU 0x003C\n"
        "__io_tmp2   EQU 0x003E\n"
        "__lr1  EQU 0x0040\n"
        "__lr2  EQU 0x0042\n"
        "__lr3  EQU 0x0044\n"
        "__lr4  EQU 0x0046\n"
        "__io_len EQU 0x0048\n"
        "__io_ptr EQU 0x004A\n"        /* saved pointer across putchar calls    */
        "__rseed  EQU 0x004C\n"        /* rand() LCG state (16-bit, seeded crt0) */
        /* Code + globals live above 0x1000. With bit-15 absolute/modified
         * addressing honored (gemu indexing micro-cycle), absolute code/data
         * references are used verbatim and no longer alias the reloaded base
         * registers (R5/R6 = 0x6000) — so the layout is no longer confined to
         * low memory. Frame/stack live at 0x6000 via disp(5)/disp(6) (modified). */
        "       ORG 0x1100\n"
        "__start:\n"
        "\tLA 5, 0x000(6)\n"            /* FP = SP (R6 = 0x6000 by reset identity) */
        "\tMVI 0, __one\n\tMVI 1, __one+1\n"
        "\tMVI 0, __zero\n\tMVI 0, __zero+1\n"
        "\tMVI 0xAC, __rseed\n\tMVI 0xE1, __rseed+1\n");  /* nonzero rand() seed */
    /* Seed the channel-2 output PER order block's buffer address. Only emit
     * this when stdio.h is in use — the __io_* order block and addrmask only
     * exist in that case (otherwise these are undefined symbols). */
    if (g_use_stdio)
        fprintf(o,
            "\tLA 1, __io_chrbuf\n\tSTR 1, __io_per_out_char+5\n"
            "\tNC 2, __io_per_out_char+4, __io_addrmask\n");
    fprintf(o,
        "\tJRT 0xF0, _main\n"
        "__halt:\tHLT\n\tJU __halt\n");

    /* __mul: __rv = __a * __b  (unsigned 16x16 -> low 16, shift-add, no shift op) */
    fprintf(o, "__mul:\n\tMVI 0, __rv\n\tMVI 0, __rv+1\n\tMVC 2, __acc, __a\n");
    for (int i = 0; i < 16; i++) {
        int byte = (i < 8) ? 1 : 0;          /* big-endian: low byte = bits 0..7 */
        int mask = 1 << (i & 7);
        fprintf(o, "\tTM 0x%02X, __b+%d\n", mask, byte);
        fprintf(o, "\tJC 0x20, M%d\n", i);   /* cc2 = selected bits zero -> skip add */
        fprintf(o, "\tAB 2,2, __rv+1, __acc+1\n");
        fprintf(o, "M%d:\tAB 2,2, __acc+1, __acc+1\n", i);   /* acc <<= 1 */
    }
    fprintf(o, "\tJU 0x000(7)\n");

    /* __divu: __rv = __a / __b ; remainder left in __acc (restoring division) */
    fprintf(o, "__divu:\n\tMVI 0, __rv\n\tMVI 0, __rv+1\n\tMVI 0, __acc\n\tMVI 0, __acc+1\n");
    for (int i = 15; i >= 0; i--) {
        int byte = (i < 8) ? 1 : 0; int mask = 1 << (i & 7);
        fprintf(o, "\tAB 2,2, __acc+1, __acc+1\n");          /* acc <<= 1 */
        fprintf(o, "\tTM 0x%02X, __a+%d\n", mask, byte);     /* bring in bit i of a */
        fprintf(o, "\tJC 0x20, DS%d\n", i);
        fprintf(o, "\tOC 1, __acc+1, __one+1\n");            /* acc |= 1 */
        fprintf(o, "DS%d:\tCMC 2, __acc, __b\n", i);         /* acc vs b (unsigned) */
        fprintf(o, "\tJC 0x40, DN%d\n", i);                  /* cc1 = acc<b -> no subtract */
        fprintf(o, "\tSB 2,2, __acc+1, __b+1\n");            /* acc -= b */
        fprintf(o, "\tMVI 0x%02X, __qb\n", mask);
        fprintf(o, "\tOC 1, __rv+%d, __qb\n", byte);         /* set quotient bit i */
        fprintf(o, "DN%d:\n", i);
    }
    fprintf(o, "\tJU 0x000(7)\n");

    /* __modu: __rv = __a %% __b  (calls __divu; saves link) */
    fprintf(o, "__modu:\n\tSTR 7, __lr+1\n\tJRT 0xF0, __divu\n\tLR 7, __lr+1\n"
               "\tMVC 2, __rv, __acc\n\tJU 0x000(7)\n");

    /* __shl: __rv = __a << __b  (repeated doubling; leaf) */
    fprintf(o, "__shl:\n\tMVC 2, __rv, __a\n"
               "Lsl:\tCMC 2, __b, __zero\n\tJC 0x20, Lsld\n"
               "\tAB 2,2, __rv+1, __rv+1\n\tSB 2,2, __b+1, __one+1\n\tJU Lsl\n"
               "Lsld:\tJU 0x000(7)\n");

    /* __shru: __rv = __a >> __b  (unsigned: divide by 2^b; saves link) */
    fprintf(o, "__shru:\n\tSTR 7, __lr+1\n\tMVI 0, __pw\n\tMVI 1, __pw+1\n"
               "Lsr:\tCMC 2, __b, __zero\n\tJC 0x20, Lsrd\n"
               "\tAB 2,2, __pw+1, __pw+1\n\tSB 2,2, __b+1, __one+1\n\tJU Lsr\n"
               "Lsrd:\tMVC 2, __b, __pw\n\tJRT 0xF0, __divu\n\tLR 7, __lr+1\n\tJU 0x000(7)\n");
}

static void emit_stdio_runtime(FILE *o)
{
    /*
     * Calling/addressing convention for these hand-written helpers
     * (see ISA.md §0.5 rows 3-4):
     *   - Register ops (LR/STR/AMR/SMR) address a 2-byte word by its RIGHTMOST
     *     byte: `LR n, L+1` / `STR n, L+1` read/write mem[L..L+1], matching the
     *     SS-op convention (`MVC 2, L, ...` -> mem[L..L+1]) and the binary
     *     arithmetic convention (`AB/SB 2,2, L+1, ...`).  So every absolute
     *     register operand below is a `+1` (or +5 for a PER order address field
     *     at offsets 4-5), never the bare label.
     *   - An incoming pointer argument sits in the frame word mem[0(6)..1(6)];
     *     load it straight into a change register with `LR n, 0x001(6)`.
     *   - A modified displacement is unsigned 0..0xFFF (EA = chgreg + disp, full
     *     16-bit add): there is no negative displacement, so a pointer is
     *     decremented with `SMR n, __one+1`, never `LA n, 0xFFF(n)`.
     *   - Link register (R7) saves use the private slots __lr1..__lr4 at L+1;
     *     R1 saved across a _putchar call uses __io_ptr+1.  Each helper uses a
     *     distinct __lrN so a caller's save survives the callee.
     */
    fprintf(o,
        "; ---- stdio runtime (channel 2 integrated typewriter) ----\n"
        "__strlen:\n"
        "\tLR 1, 0x001(6)\n"
        "\tMVI 0, __rv\n\tMVI 0, __rv+1\n"
        "Lstrlen:\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_zero\n"
        "\tJC 0x20, Lstrlen_done\n"
        "\tAB 2,2, __rv+1, __one+1\n"
        "\tLA 1, 0x001(1)\n"
        "\tJU Lstrlen\n"
        "Lstrlen_done:\tJU 0x000(7)\n"

        "_putchar:\n"
        "\tMVC 1, __io_chrbuf, 0x001(6)\n"
        "\tMVI 0, __io_status\n\tMVI 0, __io_status+1\n"
        "\tMVI 0, __io_count\n\tMVI 0, __io_count+1\n"
        "\tPER 0x80, __io_per_out_char\n"
        "Lio_wait_char:\tCMC 2, __io_status, __zero\n"
        "\tJC 0x20, Lio_wait_char\n"
        "\tMVI 0, __rv\n\tMVC 1, __rv+1, __io_chrbuf\n"
        "\tJU 0x000(7)\n"

        "__io_putstr:\n"
        "\tSTR 7, __lr1+1\n"
        "\tLR 1, 0x001(6)\n"
        "Lputstr_loop:\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_zero\n"
        "\tJC 0x20, Lputstr_done\n"
        "\tSTR 1, __io_ptr+1\n"
        "\tMVI 0, 0x000(6)\n\tMVC 1, 0x001(6), __io_chr+1\n"
        "\tJRT 0xF0, _putchar\n"
        "\tLR 1, __io_ptr+1\n"
        "\tLA 1, 0x001(1)\n"
        "\tJU Lputstr_loop\n"
        "Lputstr_done:\tLR 7, __lr1+1\n"
        "\tJU 0x000(7)\n"

        "_puts:\n"
        "\tSTR 7, __lr2+1\n"
        "\tJRT 0xF0, __io_putstr\n"
        "\tMVC 2, 0x000(6), __io_nl_word\n"
        "\tJRT 0xF0, _putchar\n"
        "\tLR 7, __lr2+1\n"
        "\tMVI 0, __rv\n\tMVI 0, __rv+1\n"
        "\tJU 0x000(7)\n"

        "__io_printu:\n"
        "\tSTR 7, __lr2+1\n"
        "\tMVC 2, __a, 0x000(6)\n"
        "\tMVI 0, __io_len\n\tMVI 0, __io_len+1\n"
        "\tCMC 2, __a, __zero\n"
        "\tJC 0x20, Lprintu_zero\n"
        "\tLA 1, __io_digits+7\n"
        "Lprintu_loop:\tMVI 0, __b\n\tMVI 10, __b+1\n"
        "\tJRT 0xF0, __divu\n"
        "\tMVI 0, __io_tmp\n\tMVC 1, __io_tmp+1, __acc+1\n"
        "\tAB 2,2, __io_tmp+1, __io_wdigit0+1\n"
        "\tMVC 1, 0x000(1), __io_tmp+1\n"
        "\tAB 2,2, __io_len+1, __one+1\n"
        "\tCMC 2, __rv, __zero\n"
        "\tMVC 2, __a, __rv\n"
        "\tJC 0x20, Lprintu_emit\n"
        "\tSMR 1, __one+1\n"
        "\tJU Lprintu_loop\n"
        "Lprintu_zero:\tLA 1, __io_digits+7\n"
        "\tMVC 1, 0x000(1), __io_c0\n"
        "\tMVI 0, __io_len\n\tMVI 1, __io_len+1\n"
        /* R1 already points at the most-significant digit; emit forward. */
        "Lprintu_emit:\tCMC 2, __io_len, __zero\n"
        "\tJC 0x20, Lprintu_emit_done\n"
        "\tSTR 1, __io_ptr+1\n"
        "\tMVI 0, 0x000(6)\n\tMVC 1, 0x001(6), 0x000(1)\n"
        "\tJRT 0xF0, _putchar\n"
        "\tLR 1, __io_ptr+1\n"
        "\tLA 1, 0x001(1)\n"
        "\tSB 2,2, __io_len+1, __one+1\n"
        "\tJU Lprintu_emit\n"
        "Lprintu_emit_done:\tLR 7, __lr2+1\n"
        "\tJU 0x000(7)\n"

        "__io_printi:\n"
        "\tSTR 7, __lr3+1\n"
        "\tTM 0x80, 0x000(6)\n"
        "\tJC 0x20, Lprinti_pos\n"
        /* Negative: form the magnitude (0 - arg) into __io_tmp BEFORE touching
         * the argument word at 0(6), then print '-' and the magnitude. */
        "\tMVI 0, __io_tmp\n\tMVI 0, __io_tmp+1\n"
        "\tSB 2,2, __io_tmp+1, 0x001(6)\n"
        "\tMVC 2, 0x000(6), __io_minus_word\n"
        "\tJRT 0xF0, _putchar\n"
        "\tMVC 2, 0x000(6), __io_tmp\n"
        "\tJRT 0xF0, __io_printu\n"
        "\tLR 7, __lr3+1\n"
        "\tJU 0x000(7)\n"
        "Lprinti_pos:\tMVC 2, 0x000(6), 0x000(6)\n"
        "\tJRT 0xF0, __io_printu\n"
        "\tLR 7, __lr3+1\n"
        "\tJU 0x000(7)\n"

        "__io_readline:\n"
        "\tMVI 0, __io_status\n\tMVI 0, __io_status+1\n"
        "\tMVI 0, __io_count\n\tMVI 0, __io_count+1\n"
        "\tMVI 0, __io_per_in_line+2\n\tMVI 255, __io_per_in_line+3\n"
        "\tLA 1, __io_line\n\tSTR 1, __io_per_in_line+5\n\tNC 2, __io_per_in_line+4, __io_addrmask\n"
        "\tPER 0x80, __io_per_in_line\n"
        "Lio_wait_in:\tCMC 2, __io_status, __zero\n"
        "\tJC 0x20, Lio_wait_in\n"
        "\tLA 1, __io_line\n\tSTR 1, __io_inp+1\n\tSTR 1, __rv+1\n"
        "\tNC 2, __io_inp, __io_addrmask\n\tNC 2, __rv, __io_addrmask\n"
        "\tJU 0x000(7)\n"

        "_getchar:\n"
        "\tMVI 0, __io_status\n\tMVI 0, __io_status+1\n"
        "\tMVI 0, __io_count\n\tMVI 0, __io_count+1\n"
        "\tMVI 0, __io_per_in_char+2\n\tMVI 1, __io_per_in_char+3\n"
        "\tLA 1, __io_chrbuf\n\tSTR 1, __io_per_in_char+5\n\tNC 2, __io_per_in_char+4, __io_addrmask\n"
        "\tPER 0x80, __io_per_in_char\n"
        "Lio_wait_chr:\tCMC 2, __io_status, __zero\n"
        "\tJC 0x20, Lio_wait_chr\n"
        "\tMVI 0, __rv\n\tMVC 1, __rv+1, __io_chrbuf\n"
        "\tJU 0x000(7)\n"

        "_gets:\n"
        "\tMVI 0, __io_status\n\tMVI 0, __io_status+1\n"
        "\tMVI 0, __io_count\n\tMVI 0, __io_count+1\n"
        "\tMVI 0, __io_per_in_line+2\n\tMVI 255, __io_per_in_line+3\n"
        "\tMVC 2, __io_per_in_line+4, 0x000(6)\n\tNC 2, __io_per_in_line+4, __io_addrmask\n"
        "\tPER 0x80, __io_per_in_line\n"
        "Lio_wait_gets:\tCMC 2, __io_status, __zero\n"
        "\tJC 0x20, Lio_wait_gets\n"
        "\tMVC 2, __rv, 0x000(6)\n"
        "\tJU 0x000(7)\n"

        "__io_skip_space:\n"
        "\tLR 1, __io_inp+1\n"
        "Lskip_space:\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_space\n"
        "\tJC 0x20, Lskip_advance\n"
        "\tJU 0x000(7)\n"
        "Lskip_advance:\tLA 1, 0x001(1)\n\tSTR 1, __io_inp+1\n\tJU Lskip_space\n"

        "__io_expect:\n"
        "\tMVI 0, __io_ok\n\tMVI 0, __io_ok+1\n"
        "\tLR 1, __io_inp+1\n\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, 0x001(6)\n"
        "\tJC 0x20, Lexpect_yes\n"
        "\tJU 0x000(7)\n"
        "Lexpect_yes:\tLA 1, 0x001(1)\n\tSTR 1, __io_inp+1\n"
        "\tMVI 0, __io_ok\n\tMVI 1, __io_ok+1\n"
        "\tJU 0x000(7)\n"

        "__io_parse_char:\n"
        "\tMVI 0, __io_ok\n\tMVI 0, __io_ok+1\n"
        "\tLR 1, __io_inp+1\n\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_zero\n"
        "\tJC 0x20, Lparse_char_fail\n"
        "\tMVI 0, __rv\n\tMVC 1, __rv+1, __io_chr+1\n"
        "\tLA 1, 0x001(1)\n\tSTR 1, __io_inp+1\n"
        "\tMVI 0, __io_ok\n\tMVI 1, __io_ok+1\n"
        "Lparse_char_fail:\tJU 0x000(7)\n"

        "__io_parse_word:\n"
        "\tMVI 0, __io_ok\n\tMVI 0, __io_ok+1\n"
        "\tLR 1, __io_inp+1\n\tLR 2, 0x001(6)\n"
        "\tMVI 0, __io_count\n\tMVI 0, __io_count+1\n"
        "Lparse_word:\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_zero\n"
        "\tJC 0x20, Lparse_word_done\n"
        "\tCMC 1, __io_chr+1, __io_space\n"
        "\tJC 0x20, Lparse_word_done\n"
        "\tMVC 1, 0x000(2), __io_chr+1\n"
        "\tLA 1, 0x001(1)\n\tLA 2, 0x001(2)\n\tSTR 1, __io_inp+1\n"
        "\tAB 2,2, __io_count+1, __one+1\n"
        "\tJU Lparse_word\n"
        "Lparse_word_done:\tMVI 0, 0x000(2)\n"
        "\tCMC 2, __io_count, __zero\n"
        "\tJC 0x20, Lparse_word_fail\n"
        "\tMVI 0, __io_ok\n\tMVI 1, __io_ok+1\n"
        "Lparse_word_fail:\tJU 0x000(7)\n"

        "__io_parse_int:\n"
        "\tSTR 7, __lr4+1\n"
        "\tMVI 0, __io_ok\n\tMVI 0, __io_ok+1\n"
        "\tMVI 0, __io_neg\n\tMVI 0, __io_neg+1\n"
        "\tMVI 0, __rv\n\tMVI 0, __rv+1\n"
        "\tLR 1, __io_inp+1\n\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_minus\n"
        "\tJC 0x20, Lparse_int_neg\n"
        "\tJU Lparse_int_loop\n"
        "Lparse_int_neg:\tMVI 0, __io_neg\n\tMVI 1, __io_neg+1\n"
        "\tLA 1, 0x001(1)\n\tSTR 1, __io_inp+1\n"
        "Lparse_int_loop:\tLR 1, __io_inp+1\n\tMVC 1, __io_chr+1, 0x000(1)\n"
        "\tCMC 1, __io_chr+1, __io_c0\n\tJC 0x40, Lparse_int_done\n"
        "\tCMC 1, __io_chr+1, __io_c9\n\tJC 0x10, Lparse_int_done\n"
        "\tMVC 2, __a, __rv\n\tMVC 2, __b, __io_w10\n\tJRT 0xF0, __mul\n"
        "\tMVI 0, __io_tmp\n\tMVC 1, __io_tmp+1, __io_chr+1\n"
        "\tSB 2,2, __io_tmp+1, __io_wdigit0+1\n"
        "\tAB 2,2, __rv+1, __io_tmp+1\n"
        "\tLA 1, 0x001(1)\n\tSTR 1, __io_inp+1\n"
        "\tMVI 0, __io_ok\n\tMVI 1, __io_ok+1\n"
        "\tJU Lparse_int_loop\n"
        "Lparse_int_done:\tCMC 2, __io_ok, __zero\n\tJC 0x20, Lparse_int_fail\n"
        "\tCMC 2, __io_neg, __zero\n\tJC 0x20, Lparse_int_exit\n"
        "\tMVI 0, __io_tmp\n\tMVI 0, __io_tmp+1\n"
        "\tSB 2,2, __io_tmp+1, __rv+1\n\tMVC 2, __rv, __io_tmp\n"
        "Lparse_int_exit:\tLR 7, __lr4+1\n\tJU 0x000(7)\n"
        "Lparse_int_fail:\tLR 7, __lr4+1\n\tJU 0x000(7)\n");
}

/* ------------------------------------------------------------------ */
/* driver                                                              */
/* ------------------------------------------------------------------ */
static void gen_function(Func *f, FILE *o) {
    l_locals = f->locals; l_args_size = f->args_size; l_locals_size = f->locals_size;
    cur_ret = f->ret; cur_fn = f->name;
    temp_base = f->args_size + 4 + f->locals_size;
    temp_off = temp_base; temp_max = temp_base;

    char *body; size_t bodysz;
    out = open_memstream(&body, &bodysz);
    gen_stmts(f->body->a);
    fflush(out); fclose(out);

    int frame = temp_max;
    fprintf(o, "_%s:\n", f->name);
    /* Saved-register slots: caller R5 at frame[args_size..+1], caller R7 at
     * frame[args_size+2..+3].  Register ops address a word by its rightmost
     * byte, so use args_size+1 / args_size+3 (ISA.md §0.5 row 4). */
    fprintf(o, "\tSTR 5, %d(6)\n", f->args_size + 1);
    fprintf(o, "\tSTR 7, %d(6)\n", f->args_size + 3);
    fprintf(o, "\tLA 5, 0x000(6)\n");
    fprintf(o, "\tLA 6, %d(6)\n", frame);
    fputs(body, o);
    fprintf(o, "L%s_ret:\n", f->name);
    fprintf(o, "\tLR 7, %d(5)\n", f->args_size + 3);
    fprintf(o, "\tLA 6, 0x000(5)\n");
    fprintf(o, "\tLR 5, %d(5)\n", f->args_size + 1);
    fprintf(o, "\tJU 0x000(7)\n");
    free(body);
}

static void gen_data(FILE *o) {
    fprintf(o, "; ---- data ----\n");
    for (Sym *s = g_globals; s; s = s->next) {
        fprintf(o, "_%s:", s->name);
        if (s->off & 0x10000) { int v = s->off & 0xffff;
            if (ty_size(s->type) == 1) fprintf(o, "\tDB 0x%02X\n", v & 0xff);
            else fprintf(o, "\tDB 0x%02X, 0x%02X\n", (v >> 8) & 0xff, v & 0xff);
        } else fprintf(o, "\tDS %d\n", ty_size(s->type));
    }
    for (Str *s = g_strs; s; s = s->next) {
        fprintf(o, "__str%d:\tDB ", s->label);
        for (int i = 0; i <= s->len; i++) fprintf(o, "%s0x%02X", i ? ", " : "", (unsigned char)s->bytes[i]);
        fprintf(o, "\n");
    }
    if (g_use_stdio) {
        fprintf(o,
            "__io_per_out_char:\tDB 0x80, 0x85, 0x00, 0x01, 0x00, 0x00\n"
            "__io_per_in_line:\tDB 0x00, 0x40, 0x00, 0x00, 0x00, 0x00\n"
            "__io_per_in_char:\tDB 0x00, 0x41, 0x00, 0x00, 0x00, 0x00\n"
            "__io_chrbuf:\tDB 0x00\n"
            "__io_zero:\tDB 0x00\n"
            "__io_space:\tDB 0x50\n"
            "__io_minus:\tDB 0xAA\n"
            "__io_c0:\tDB 0x40\n"
            "__io_c9:\tDB 0x49\n"
            "__io_nl_word:\tDB 0x00, 0x00\n"
            "__io_minus_word:\tDB 0x00, 0xAA\n"
            "__io_addrmask:\tDB 0x7F, 0xFF\n"
            "__io_w10:\tDB 0x00, 0x0A\n"
            "__io_wdigit0:\tDB 0x00, 0x40\n"
            "__io_digits:\tDS 8\n"
            "__io_line:\tDS 256\n");
    }
}

/* Locate gasm: next to this binary's tree (../assembler/gasm, then a
 * sibling), else fall back to PATH. Returns a static buffer or "gasm". */
static const char *find_gasm(const char *argv0)
{
    static char path[1024];
    const char *slash = strrchr(argv0, '/');
    if (slash) {
        int dlen = (int)(slash - argv0);
        snprintf(path, sizeof(path), "%.*s/../assembler/gasm", dlen, argv0);
        if (access(path, X_OK) == 0) return path;
        snprintf(path, sizeof(path), "%.*s/gasm", dlen, argv0);
        if (access(path, X_OK) == 0) return path;
    }
    return "gasm";
}

static int run_gasm(const char *gasm, const char *asmpath,
                    const char *outpath, int bootflag)
{
    const char *args[8];
    int n = 0;
    args[n++] = gasm;
    if (bootflag) args[n++] = "--boot";
    args[n++] = "-o";
    args[n++] = outpath;
    args[n++] = asmpath;
    args[n]   = NULL;
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        execvp(gasm, (char *const *)args);
        perror(gasm);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

int main(int argc, char **argv) {
    const char *inpath = 0, *outpath = 0;
    int sflag = 0, bootflag = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "-S")) sflag = 1;
        else if (!strcmp(argv[i], "--boot")) bootflag = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "gec: unknown option '%s'\n", argv[i]);
            return 1;
        }
        else inpath = argv[i];
    }
    if (!inpath) {
        fprintf(stderr,
            "usage: gec input.c [-S] [--boot] [-o out]\n"
            "  default : compile + assemble via gasm -> out (a.bin)\n"
            "  --boot  : link with the boot card, emit a .cap deck (a.cap)\n"
            "  -S      : stop after compilation, emit gasm assembly (a.s)\n"
            "  (-o ending in .s implies -S, matching the old behaviour)\n");
        return 1;
    }
    /* Back-compat: 'gec prog.c -o prog.s' keeps emitting assembly. */
    if (outpath) {
        size_t ol = strlen(outpath);
        if (ol > 2 && !strcmp(outpath + ol - 2, ".s")) sflag = 1;
    }
    if (sflag && bootflag) {
        fprintf(stderr, "gec: -S and --boot are mutually exclusive\n");
        return 1;
    }
    if (!outpath) outpath = sflag ? "a.s" : (bootflag ? "a.cap" : "a.bin");

    g_file = inpath;
    FILE *in = fopen(inpath, "rb");
    if (!in) { perror(inpath); return 1; }
    fseek(in, 0, SEEK_END); long sz = ftell(in); fseek(in, 0, SEEK_SET);
    char *raw = malloc(sz + 1); fread(raw, 1, sz, in); raw[sz] = 0; fclose(in);
    char *src = preprocess_source(raw);
    free(raw);
    g_src = src; lp = src;

    parse_program();

    char tmppath[] = "/tmp/gecXXXXXX";
    FILE *o;
    if (sflag) {
        o = fopen(outpath, "w");
        if (!o) { perror(outpath); return 1; }
    } else {
        int fd = mkstemp(tmppath);
        if (fd < 0) { perror("mkstemp"); return 1; }
        o = fdopen(fd, "w");
    }
    fprintf(o, "; generated by gec from %s\n", inpath);
    emit_runtime(o);
    if (g_use_stdio)
        emit_stdio_runtime(o);
    for (Func *f = g_funcs; f; f = f->next) gen_function(f, o);
    gen_data(o);
    fclose(o);
    if (sflag)
        return 0;

    int rc = run_gasm(find_gasm(argv[0]), tmppath, outpath, bootflag);
    unlink(tmppath);
    if (rc) {
        fprintf(stderr, "gec: gasm failed\n");
        return 1;
    }
    return 0;
}
