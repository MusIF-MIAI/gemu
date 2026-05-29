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
 *   use:    ./gec prog.c -o prog.s   &&   gasm -o prog.bin prog.s
 *           ge prog.bin            # main()'s return value ends up in __rv
 *
 * Not a conforming C compiler; see docs/ABI.md §7 for limits.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */
/* ------------------------------------------------------------------ */
static const char *g_src;     /* whole source, for line reporting     */
static const char *g_file = "<stdin>";
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
        case ' ': return 0x50; case '[': return 0x4A; case '#': return 0x4B;
        case '@': return 0x4C; case ':': return 0x4D; case '>': return 0x4E;
        case '?': return 0x4F; case '&': return 0x5A; case '.': return 0x5B;
        case ']': return 0x5C; case '(': return 0x5D; case '<': return 0x5E;
        case '\\':return 0x5F; case '$': return 0xAB; case ')': return 0xAD;
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
    if (lv.kind == LV_TEMP) EM("LR 1, %d(5)\n", lv.taddr);   /* R1 = address */
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
    if (lv.kind == LV_TEMP) EM("LR 1, %d(5)\n", lv.taddr);
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
        if (n->sym->is_global) { EM("LA 1, _%s\n", n->sym->name); EM("STR 1, %d(5)\n", dst); }
        else { EM("LA 1, %d(5)\n", n->sym->off); EM("STR 1, %d(5)\n", dst); }
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
        EM("LA 1, __str%d\n", n->slabel); EM("STR 1, %d(5)\n", dst); return;
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
    fprintf(o, "__modu:\n\tSTR 7, __lr\n\tJRT 0xF0, __divu\n\tLR 7, __lr\n"
               "\tMVC 2, __rv, __acc\n\tJU 0x000(7)\n");

    /* __shl: __rv = __a << __b  (repeated doubling; leaf) */
    fprintf(o, "__shl:\n\tMVC 2, __rv, __a\n"
               "Lsl:\tCMC 2, __b, __zero\n\tJC 0x20, Lsld\n"
               "\tAB 2,2, __rv+1, __rv+1\n\tSB 2,2, __b+1, __one+1\n\tJU Lsl\n"
               "Lsld:\tJU 0x000(7)\n");

    /* __shru: __rv = __a >> __b  (unsigned: divide by 2^b; saves link) */
    fprintf(o, "__shru:\n\tSTR 7, __lr\n\tMVI 0, __pw\n\tMVI 1, __pw+1\n"
               "Lsr:\tCMC 2, __b, __zero\n\tJC 0x20, Lsrd\n"
               "\tAB 2,2, __pw+1, __pw+1\n\tSB 2,2, __b+1, __one+1\n\tJU Lsr\n"
               "Lsrd:\tMVC 2, __b, __pw\n\tJRT 0xF0, __divu\n\tLR 7, __lr\n\tJU 0x000(7)\n");
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
    fprintf(o, "\tSTR 5, %d(6)\n", f->args_size);
    fprintf(o, "\tSTR 7, %d(6)\n", f->args_size + 2);
    fprintf(o, "\tLA 5, 0x000(6)\n");
    fprintf(o, "\tLA 6, %d(6)\n", frame);
    fputs(body, o);
    fprintf(o, "L%s_ret:\n", f->name);
    fprintf(o, "\tLR 7, %d(5)\n", f->args_size + 2);
    fprintf(o, "\tLA 6, 0x000(5)\n");
    fprintf(o, "\tLR 5, %d(5)\n", f->args_size);
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
}

int main(int argc, char **argv) {
    const char *inpath = 0, *outpath = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) outpath = argv[++i];
        else inpath = argv[i];
    }
    if (!inpath) { fprintf(stderr, "usage: gec input.c [-o out.s]\n"); return 1; }
    g_file = inpath;
    FILE *in = fopen(inpath, "rb");
    if (!in) { perror(inpath); return 1; }
    fseek(in, 0, SEEK_END); long sz = ftell(in); fseek(in, 0, SEEK_SET);
    char *src = malloc(sz + 1); fread(src, 1, sz, in); src[sz] = 0; fclose(in);
    g_src = src; lp = src;

    parse_program();

    FILE *o = outpath ? fopen(outpath, "w") : stdout;
    if (!o) { perror(outpath); return 1; }
    fprintf(o, "; generated by gec from %s\n", inpath);
    emit_runtime(o);
    for (Func *f = g_funcs; f; f = f->next) gen_function(f, o);
    gen_data(o);
    if (o != stdout) fclose(o);
    return 0;
}
