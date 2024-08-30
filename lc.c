int F_STRUCT = 1 << 0;
int F_ENUM = 1 << 1;
int F_VOID = 1 << 2;
int F_INT = 1 << 3;
int F_CHAR = 1 << 4;
int F_LONG = 1 << 5;
int F_LONGLONG = 1 << 6;
int F_UNSIGNED = 1 << 7;
int F_SIGNED = 1 << 8;
int F_SHORT = 1 << 9;
int F_FLOAT = 1 << 10;
int F_DOUBLE = 1 << 11;
int F_STATIC = 1 << 12;
int F_TYPEDEF = 1 << 13;
int F_EXTERN = 1 << 14;
int F_CONST = 1 << 15;
int F_VOLATILE = 1 << 16;
int F_TYPEDEFVAR = 1 << 17;
int F_ANY = 1 << 18;

static int VERBOSE = 0;

#define NULL (void *)0

extern int fclose(void *);
extern void *fopen(char *, char *);
extern long fwrite(void *, long, long, void *);
extern int puts(const char *);
extern int printf(const char *, ...);
extern int asprintf(char **, const char *, ...);
extern void *calloc(unsigned long, unsigned long);
extern void *malloc(unsigned long);
extern void *memcpy(void *, void *, unsigned long);
extern void *memset(void *, int, unsigned long);
extern long strtol(const char *, char **, int);
extern void free(void *);
extern void abort(void);
extern long read(int, void *, unsigned long);
extern int isdigit(int);
extern int isspace(int);
extern int strcmp(const char *, const char *);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, unsigned long);
extern char *strcpy(char *, const char *);
extern unsigned long strlen(const char *);
extern char *strdup(const char *);

static int TOKINERR;
static int PARSEINERR;
static int LINENO = 1;
static int COL = 1;
static int LEXERRS;
static int PARSEERRS;
static int LEXWARNS;
static int PARSEWARNS;
static int CHECKERRS;
static int CHECKWARNS;
static char CODE[1000000];
static char *CURCODE = CODE;
static void *GEN;

void _assert(int cond, char *msg) {
  if (!cond) {
    puts(msg);
    puts(NULL);
  }
}

void die(char *ctx, char *msg) {
  printf("%s: %s\n", ctx, msg);
  puts(NULL);
}

int ceilto(int val, int ceil) {
  return ((val + ceil - 1) & ~(ceil - 1));
}

struct tok {
  int kind;
  long i;
  char c;
  char *s;
  int lineno;
  int col;
};

static struct tok TOKS[100000];
static struct tok *CURTOK = TOKS;

struct type {
  int flags;
  int plvl;
  int arrsz;
  char *structname;
  struct type *inner;
};

static struct structdef *STRUCTDEFS[128];

int structdefsize(struct structdef *);
struct structdef *structdeffind(struct structdef **, char *);

int typeatomsize(struct type *ty) {
  /*
   * We're keeping things simple:
   *   - characters are one byte
   *   - structs are whatever their size with some padding are
   *   - everything else including pointers are word-sized
   *
   * Struct alignment and padding? Oh well...
   */
  if (ty->flags & F_STRUCT && ty->plvl == 0) {
    struct structdef *sd;
    sd = structdeffind(STRUCTDEFS, ty->structname);
    _assert(sd != NULL, ty->structname);
    return structdefsize(sd);
  } else if (ty->flags & F_CHAR && ty->plvl == 0) {
    return 1;
  } else {
    return 4;
  }
}

char *gettypeflagsstring(int);

void typedump(struct type *t) {
  printf("<type> flags=%s plvl=%d arrsz=%d", gettypeflagsstring(t->flags),
         t->plvl, t->arrsz);
  if (t->flags & F_STRUCT) {
    printf(" structname=%s", t->structname);
  }
  puts("");
}

int typesize(struct type *ty) {
  int atomsz;

  atomsz = typeatomsize(ty);
  if (ty->arrsz != -1) {
    return ty->arrsz * atomsz;
  }
  return atomsz;
}

struct node {
  int kind;
  int n;
  int i;
  struct tok *tok;
  struct node *left;
  struct node *right;
  struct node *next;   /* some nodes are chained together */
  void *data;          /* data optionally contains node-specific extras */
  struct scope *scope; /* some nodes have a variable scope */
  struct type *type;   /* most expression-like nodes have a type */
};

static struct node *NODES[10000];
static struct node **CURNODE = NODES;
static int NODEN = 0;

struct defpair {
  struct node *def;
  char *name;
};

static struct defpair *TYPEDEFS[1000];
static struct defpair *ENUMS[1000];
static struct defpair *FUNCDEFS[1000];
static struct defpair *FUNCDECLS[1000];

struct defpair *defpairfind(struct defpair **from, char *name) {
  struct defpair **cur = from;
  if (!name) {
    return NULL;
  }
  while (*cur) {
    if (!strcmp((*cur)->name, name)) {
      return *cur;
    }
    ++cur;
  }
  return NULL;
}

struct defpair *defpairappend(struct defpair **to, char *name, struct node *n) {
  struct defpair **cur = to;
  struct defpair *new;
  while (*cur) {
    ++cur;
  }
  new = calloc(1, sizeof(struct defpair));
  new->name = name;
  new->def = n;
  *cur = new;
  return *cur;
}

void nodedump(int, struct node *);

void defpairdump(struct defpair **which) {
  struct defpair **cur = which;
  while (*cur) {
    printf("[defpair] %s\n", (*cur)->name);
    nodedump(0, (*cur)->def);
    ++cur;
  }
}

struct strintpair {
  int val;
  char *name;
};

static struct strintpair *ENUMRS[1000];
static struct strintpair *STRINITS[1000];

struct strintpair *strintpairfindstr(struct strintpair **from, char *name) {
  struct strintpair **cur = from;
  while (*cur) {
    if (!strcmp((*cur)->name, name)) {
      return *cur;
    }
    ++cur;
  }
  return NULL;
}

struct strintpair *strintpairfindint(struct strintpair **from, int val) {
  struct strintpair **cur = from;
  while (*cur) {
    if ((*cur)->val == val) {
      return *cur;
    }
    ++cur;
  }
  return NULL;
}

struct strintpair *strintpairappend(struct strintpair **to, char *name,
                                    int val) {
  struct strintpair **cur = to;
  struct strintpair *new;
  while (*cur) {
    ++cur;
  }
  new = calloc(1, sizeof(struct strintpair));
  new->name = name;
  new->val = val;
  *cur = new;
  return *cur;
}

struct strintpair *strintpairupdateval(struct strintpair **to, char *name,
                                       int val) {
  struct strintpair **cur = to;
  while (*cur) {
    if ((*cur)->name == name) {
      (*cur)->val = val;
      return *cur;
    }
    ++cur;
  }
  return NULL;
}

void nodedump(int, struct node *);

void strintpairdump(struct strintpair **which) {
  struct strintpair **cur = which;
  while (*cur) {
    printf("[strintpair] %s=%d\n", (*cur)->name, (*cur)->val);
    ++cur;
  }
}

struct scope {
  char *id;
  int sshift;
  int pshift;
  int ssize;
  int isfunc;
  struct scope *parent;
  struct scopevar *vars;
};

struct scopevar {
  char *name;
  int instack;
  int size;
  int stacksize;
  int shift;
  int isparam;
  int isextern;
  int n;
  struct node *this;
  struct scopevar *next;
  struct scope *scope;
};

static int SCOPEVARN = 0;

struct scope *newscope(char *id, struct scope *parent, int isfunc) {
  struct scope *ret;
  struct scope *fs;
  int pshift;
  int sshift;

  ret = calloc(1, sizeof(struct scope));
  fs = parent;
  pshift = 4;
  sshift = 0;
  while (fs) {
    if (fs->isfunc) {
      pshift = fs->pshift;
      sshift = fs->sshift;
      break;
    }
    fs = fs->parent;
  }

  ret->id = id;
  ret->parent = parent;
  ret->pshift = pshift;
  ret->sshift = sshift;
  ret->ssize = 0;
  ret->isfunc = isfunc;
  printf("[new scope=%s] parent=%s\n", ret->id, ret->parent->id);
  return ret;
}

int getnodevarsize(struct node *n);

void scopeappendvar(struct scope *s, char *name, int instack, int isparam,
                    int isextern, struct node *n) {
  struct scopevar **sv;
  struct scope *fs;
  int stacksz;
  int sz;

  sv = &(s->vars);
  while (*sv) {
    sv = &((*sv)->next);
  }
  sz = getnodevarsize(n);
  stacksz = ceilto(sz, 4);
  if (VERBOSE) {
    printf("[scope=%s] adding variable: %s (sz=%d, stacksz=%d)\n", s->id, name,
           sz, stacksz);
  }
  *sv = calloc(1, sizeof(struct scopevar));
  (*sv)->name = name;
  (*sv)->this = n;
  (*sv)->instack = instack;
  (*sv)->size = sz;
  (*sv)->stacksize = stacksz;
  (*sv)->isparam = isparam;
  (*sv)->n = ++SCOPEVARN;
  (*sv)->scope = s;
  (*sv)->isextern = isextern;
  if (instack) {
    /*
     * All stack-allocated variables in any nested block will be reserved from
     * the function's call frame. This means we'll first find the current
     * function's scope to `fs` and then track the shift there.
     */
    fs = s;
    while (fs && !fs->isfunc) {
      fs = fs->parent;
    }
    if (isparam) {
      fs->pshift += stacksz;
      (*sv)->shift = fs->pshift;
    } else {
      fs->sshift -= stacksz;
      (*sv)->shift = fs->sshift;
      fs->ssize += stacksz;
    }
  }
}

struct scopevar *scopefindvar(struct scope *s, char *name) {
  struct scopevar *sv;

  while (s) {
    sv = s->vars;
    while (sv) {
      if (!strcmp(name, sv->name)) {
        return sv;
      }
      sv = sv->next;
    }
    s = s->parent;
  }
  return NULL;
}

void scopedump(struct scope *s) {
  struct scopevar *sv;
  char *pid;
  while (s) {
    if (s->parent) {
      pid = s->parent->id;
    } else {
      pid = "<no parent>";
    }
    printf(">> scope=%s (parent=%s)\n", s->id, pid);
    sv = s->vars;
    while (sv) {
      printf("* %s <- %p\n", sv->name, sv->this);
      sv = sv->next;
    }
    s = s->parent;
  }
}

static struct scope tuscope;

struct nodedeclarator {
  int plvl;
  int arrsz;
};

struct structdef {
  char *name;
  /* I know. */
  char *fields[32];
  struct type *types[32];
  int offsets[32];
  int memsz;
};

struct structdef *structdeffind(struct structdef **from, char *name) {
  struct structdef **cur;

  cur = from;
  if (!name) {
    return NULL;
  }
  while (*cur) {
    if (!strcmp((*cur)->name, name)) {
      return *cur;
    }
    ++cur;
  }
  return NULL;
}

struct structdef *structdefappend(struct structdef **to, struct structdef *sd) {
  struct structdef **cur;
  struct structdef *new;

  cur = to;
  while (*cur) {
    ++cur;
  }
  *cur = sd;
  return *cur;
}

void structdefdump(struct structdef *sd) {
  int i;
  printf(">> structdef=%s memsz=%d\n", sd->name, sd->memsz);
  i = 0;
  while (sd->fields[i]) {
    printf("  field=%s, offset=%d\n", sd->fields[i], sd->offsets[i]);
    typedump(sd->types[i]);
    ++i;
  }
}

int structdeffindfield(struct structdef *sd, char *field) {
  int i;
  i = 0;
  while (sd->fields[i]) {
    if (!strcmp(sd->fields[i], field)) {
      return i;
    }
    ++i;
  }
  return -1;
}

struct type *structdeffieldtype(struct structdef *sd, char *field) {
  int i;
  i = structdeffindfield(sd, field);
  if (i == -1) {
    return NULL;
  }
  return sd->types[i];
}

int structdeffieldoffset(struct structdef *sd, char *field) {
  int i;
  i = structdeffindfield(sd, field);
  if (i == -1) {
    return -1;
  }
  return sd->offsets[i];
}

int structdefsize(struct structdef *sd) {
  int atomsz;
  int arrsz;
  int this;
  int sz;
  int i;
  i = 0;
  sz = 0;
  while (sd->fields[i]) {
    atomsz = typeatomsize(sd->types[i]);
    arrsz = sd->types[i]->arrsz;
    if (arrsz > -1) {
      this = atomsz * arrsz;
    } else {
      this = atomsz;
    }
    sz += ceilto(this, 4);
    ++i;
  }
  return sz;
}

/* Generated token stuff begins. */
enum Token {
  ASSIGN_ADD = 128,
  ASSIGN_AND,
  ASSIGN_DIV,
  ASSIGN_LEFT,
  ASSIGN_OR,
  ASSIGN_MOD,
  ASSIGN_MUL,
  ASSIGN_RIGHT,
  ASSIGN_SUB,
  ASSIGN_XOR,
  AUTO,
  BREAK,
  CHAR,
  CONST,
  CONSTANT,
  CONTINUE,
  DEFAULT,
  DOUBLE,
  DO,
  ELLIPSIS,
  ELSE,
  ENUM,
  EXTERN,
  FLOAT,
  FOR,
  GOTO,
  ID,
  IF,
  INT,
  LONG,
  OP_AND,
  OP_DEC,
  OP_EQ,
  OP_LE,
  OP_LEFT,
  OP_GE,
  OP_INC,
  OP_NE,
  OP_OR,
  OP_PTR,
  OP_RIGHT,
  REGISTER,
  RETURN,
  SHORT,
  SIGNED,
  SIZEOF,
  STATIC,
  STRLIT,
  STRUCT,
  SWITCH,
  TYPEDEF,
  UNION,
  UNSIGNED,
  VOID,
  VOLATILE,
  WHILE,
  _END,
};

static char *TOKNAMES[] = {
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       "!",        "\"",         "#",
    NULL,       "%",        "&",          "'",
    "(",        ")",        "*",          "+",
    ",",        "-",        ".",          "/",
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       ":",          ";",
    "<",        "=",        ">",          "?",
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         "[",
    NULL,       "]",        "^",          NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         NULL,
    NULL,       NULL,       NULL,         "{",
    "|",        "}",        "~",          NULL,
    "+=",       "&=",       "/=",         "<<=",
    "|=",       "%=",       "*=",         ">>=",
    "-=",       "^=",       "auto",       "break",
    "char",     "const",    "constant",   "continue",
    "default",  "double",   "do",         "...",
    "else",     "enum",     "extern",     "float",
    "for",      "goto",     "identifier", "if",
    "int",      "long",     "&&",         "--",
    "==",       "<=",       "<<",         ">=",
    "++",       "!=",       "||",         "->",
    ">>",       "register", "return",     "short",
    "signed",   "sizeof",   "static",     "string literal",
    "struct",   "switch",   "typedef",    "union",
    "unsigned", "void",     "volatile",   "while",
    "the end",
};

static char *KEYWORDS[] = {
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    "auto",     "break",    "char",     "const",  NULL,      "continue",
    "default",  "double",   "do",       NULL,     "else",    "enum",
    "extern",   "float",    "for",      "goto",   NULL,      "if",
    "int",      "long",     NULL,       NULL,     NULL,      NULL,
    NULL,       NULL,       NULL,       NULL,     NULL,      NULL,
    NULL,       "register", "return",   "short",  "signed",  "sizeof",
    "static",   NULL,       "struct",   "switch", "typedef", "union",
    "unsigned", "void",     "volatile", "while",  NULL,
};

static int PRECBIN[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 13,
    8,  -1, 15, -1, 13, 12, -1, 12, 15, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 3,  -1, 10, 2,  10, 3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 15, -1, -1, 7,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 6,  -1, -1, -1, 2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 5,  -1, 9,  10, 11, 10, -1, 9,  4,  15, 11, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static int PRECUN[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, -1, -1, -1, -1,
    14, -1, 14, -1, 14, 14, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 14, -1, -1, -1, -1, 14, -1, -1, -1, -1, -1, -1,
    -1, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static int ASSOCBIN[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,
    1,  -1, 1,  -1, 1,  1,  -1, 1,  1,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0,  -1, 1,  0,  1,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,  -1, -1, 1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,  -1, -1, -1, 0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 1,  -1, 1,  1,  1,  1,  -1, 1,  1,  1,  1,  -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};
/* Generated token stuff ends. */

/* Some are truncated, others are long. Aim for consistency. */
enum Node {
  N_CONSTANT,
  N_STRLIT,
  N_CHAR,
  VARREF,
  VARREFADDR,
  ARRAYSUBSCRIPT,
  ARRAYSUBSCRIPTADDR,
  OP_BIN,
  OP_UN,
  OP_UN_POSTFIX,
  ADDROF,
  FUNDECL,
  FUNDEF,
  FUNCALL,
  FUNARG,
  FUNARGS,
  VARDECL,
  ASSIGN,
  N_WHILE,
  TYPECAST,
  DECLARATION,
  DECLARATOR,
  INITIALIZER,
  INITIALIZERARRAY,
  N_ID,
  BLOCK,
  COND,
  N_CONTINUE,
  N_BREAK,
  N_RETURN,
  EXPR,
  N_GOTO,
  LABEL,
  TYPE,
  N_STRUCT,
  N_ENUM,
  ENUMERATOR,
  FUNARGDEF,
  NOP,
  DECLSPECS,
  N_TYPEDEF,
  N_ELLIPSIS,
  ADDRSHIFT,
  ADDRSHIFTVALUE,
  N_SIZEOF_TYPE,
  N_SIZEOF_EXPR,
  ENUMERATORCONSTANT,
  OP_UN_LVALUE,
};

static char *NODENAMES[] = {
    "N_CONSTANT",
    "N_STRLIT",
    "N_CHAR",
    "VARREF",
    "VARREFADDR",
    "ARRAYSUBSCRIPT",
    "ARRAYSUBSCRIPTADDR",
    "OP_BIN",
    "OP_UN",
    "OP_UN_POSTFIX",
    "ADDROF",
    "FUNDECL",
    "FUNDEF",
    "FUNCALL",
    "FUNARG",
    "FUNARGS",
    "VARDECL",
    "ASSIGN",
    "N_WHILE",
    "TYPECAST",
    "DECLARATION",
    "DECLARATOR",
    "INITIALIZER",
    "INITIALIZERARRAY",
    "N_ID",
    "BLOCK",
    "COND",
    "N_CONTINUE",
    "N_BREAK",
    "N_RETURN",
    "EXPR",
    "N_GOTO",
    "LABEL",
    "TYPE",
    "N_STRUCT",
    "N_ENUM",
    "ENUMERATOR",
    "FUNARGDEF",
    "NOP",
    "DECLSPECS",
    "N_TYPEDEF",
    "N_ELLIPSIS",
    "ADDRSHIFT",
    "ADDRSHIFTVALUE",
    "N_SIZEOF_TYPE",
    "N_SIZEOF_EXPR",
    "ENUMERATORCONSTANT",
    "OP_UN_LVALUE",
};

char *gettypeflagsstring(int f) {
  char *ret = calloc(100, 1);
  /* I know. */
  if (f & F_TYPEDEF) {
    strcat(ret, "typedef ");
  }
  if (f & F_TYPEDEFVAR) {
    strcat(ret, "typedefvar ");
  }
  if (f & F_EXTERN) {
    strcat(ret, "extern ");
  }
  if (f & F_CONST) {
    strcat(ret, "const ");
  }
  if (f & F_VOLATILE) {
    strcat(ret, "volatile ");
  }
  if (f & F_STATIC) {
    strcat(ret, "static ");
  }
  if (f & F_STRUCT) {
    strcat(ret, "struct ");
  }
  if (f & F_ENUM) {
    strcat(ret, "enum ");
  }
  if (f & F_VOID) {
    strcat(ret, "void ");
  }
  if (f & F_INT) {
    strcat(ret, "int ");
  }
  if (f & F_CHAR) {
    strcat(ret, "char ");
  }
  if (f & F_LONGLONG) {
    strcat(ret, "long long ");
  } else if (f & F_LONG) {
    strcat(ret, "long ");
  }
  if (f & F_ANY) {
    strcat(ret, "any ");
  }
  if (strlen(ret) == 0) {
    free(ret);
    return NULL;
  }
  ret[strlen(ret) - 1] = 0;
  return ret;
}

static char *indent =
    ". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ";

struct node *getnodedata(struct node *n) {
  return (struct node *)n->data;
}

char *getdecltypedefname(struct node *n) {
  return n->left->left->tok->s;
}

struct node *getdecldecl(struct node *n) {
  return n->left;
}

struct node *getdeclinitializer(struct node *n) {
  if (n->right &&
      (n->right->kind == INITIALIZER || n->right->kind == INITIALIZERARRAY)) {
    return n->right->left;
  }
  return NULL;
}

struct node *getdeclfirstenumerator(struct node *n) {
  return ((struct node *)n->data)->left->right->left;
}

struct node *getdeclfirsttype(struct node *n) {
  return ((struct node *)n->data)->left;
}

char *getdeclfuncname(struct node *n) {
  return n->left->left->tok->s;
}

struct node *getdeclfunargdef(struct node *n) {
  return n->left->right->left;
}

struct node *getdeclstructfirstdecl(struct node *n) {
  if (!n->left->right->right) {
    die("getdeclstructfirstdecl", "not a definition");
  }
  return n->left->right->right;
}

char *getdeclstructname(struct node *n) {
  struct node *d = getnodedata(n);
  if (!d) {
    return NULL;
  }
  d = d->left;
  while (d) {
    if (d->right && d->right->left) {
      return d->right->left->tok->s;
    }
    d = d->next;
  }
  return NULL;
}

char *getsizeoftypestructname(struct node *n) {
  struct node *d;
  d = n->right;
  while (d) {
    if (d->right && d->right->left) {
      return d->right->left->tok->s;
    }
    d = d->next;
  }
  return NULL;
}

char *getdeclenumname(struct node *n) {
  if (n->data) {
    struct node *d = getnodedata(n);
    if (d->left && d->left->right && d->left->right->right) {
      return d->left->right->right->tok->s;
    }
  }
  return NULL;
}

int getspecsflags(struct node *n) {
  return *(int *)n->data;
}

struct node *getfundefblock(struct node *n) {
  return n->left->right->right;
}

struct nodedeclarator *getnodedeclarator(struct node *n) {
  return (struct nodedeclarator *)n->data;
}

char *getdeclaratorstring(struct node *n) {
  struct nodedeclarator *nd;
  char *ret;
  int i;

  nd = getnodedeclarator(n);
  ret = calloc(1, 50);
  i = nd->plvl;
  while (i > 0) {
    strcat(ret, "*");
    --i;
  }
  if (nd->arrsz > 0) {
    char *a;
    asprintf(&a, "[%d]", nd->arrsz);
    strcat(ret, a);
    free(a);
  }
  return ret;
}

char *getdeclstring(struct node *n) {
  char *tf;
  char *dr;
  char *ret;

  tf = gettypeflagsstring(getspecsflags(getnodedata(n)));
  dr = getdeclaratorstring(getdecldecl(n));
  ret = calloc(strlen(tf) + strlen(dr) + 1, 1);
  strcpy(ret, tf);
  strcat(ret, dr);
  return ret;
}

void nodedump(int depth, struct node *n) {
  int k;
  int i;

  if (!n) {
    return;
  }
  i = depth * 4;
  k = n->kind;
  if (k == VARREF) {
    printf("%.*s{%d} VARREF \"%s\"\n", i, indent, n->n, n->tok->s);
  } else if (k == N_CONSTANT) {
    printf("%.*s{%d} N_CONSTANT %ld\n", i, indent, n->n, n->tok->i);
  } else if (k == N_CHAR) {
    printf("%.*s{%d} CHAR %c\n", i, indent, n->n, n->tok->c);
  } else if (k == DECLARATION) {
    char *dd = getdeclstring(n);
    printf("%.*s{%d} DECLARATION: %s\n", i, indent, n->n, dd);
    free(dd);
    printf("%.*s[data]\n", i, indent);
    nodedump(depth + 1, n->data);
  } else if (k == DECLARATOR) {
    struct nodedeclarator *nd = getnodedeclarator(n);
    printf("%.*s{%d} DECLARATOR plvl=%d arrsz=%d\n", i, indent, n->n, nd->plvl,
           nd->arrsz);
  } else if (k == FUNARGDEF) {
    printf("%.*s{%d} FUNARGDEF\n", i, indent, n->n);
    nodedump(depth + 1, n->data);
  } else if (k == INITIALIZER) {
    printf("%.*s{%d} INITIALIZER\n", i, indent, n->n);
  } else if (k == BLOCK) {
    printf("%.*s{%d} BLOCK\n", i, indent, n->n);
  } else if (k == TYPECAST) {
    printf("%.*s{%d} TYPECAST\n", i, indent, n->n);
    printf("%.*s[data]\n", i, indent);
    nodedump(depth + 1, n->data);
  } else if (k == DECLSPECS) {
    printf("%.*s{%d} DECLSPECS=%s\n", i, indent, n->n,
           gettypeflagsstring(getspecsflags(n)));
  } else {
    printf("%.*s{%d} %s %s", i, indent, n->n, NODENAMES[k],
           TOKNAMES[n->tok->kind]);
    if (n->tok->s) {
      printf(" \"%s\"\n", n->tok->s);
    } else {
      puts("");
    }
  }
  if (n->type) {
    printf("%.*s[type] %s plvl=%d arrsz=%d structname=%s\n", i, indent,
           gettypeflagsstring(n->type->flags), n->type->plvl, n->type->arrsz,
           n->type->structname);
  }
  if (n->left) {
    printf("%.*s[left]\n", i, indent);
    nodedump(depth + 1, n->left);
  }
  if (n->right) {
    printf("%.*s[right]\n", i, indent);
    nodedump(depth + 1, n->right);
  }
  if (n->next) {
    printf("%.*s[next]\n", i, indent);
    nodedump(depth, n->next);
  }
}

void nodeemit(struct node *n) {
  *CURNODE = n;
  ++CURNODE;
}

void lexemit(int kind) {
  CURTOK->kind = kind;
  CURTOK->lineno = LINENO;
  CURTOK->col = COL;
  ++CURTOK;
}

void lexemiti(int kind, int i) {
  CURTOK->i = i;
  lexemit(kind);
}

void lexemitc(int kind, char c) {
  CURTOK->c = c;
  lexemit(kind);
}

void lexemits(int kind, char *s) {
  CURTOK->s = s;
  lexemit(kind);
}

int lexpeek(void) {
  return *CURCODE;
}

void lexerr(char *e) {
  ++LEXERRS;
  printf("error: %d:%d (%c): %s\n", LINENO, COL, lexpeek(), e);
}

void lexwarn(char *e) {
  ++LEXWARNS;
  printf("warning: %d:%d (%c): %s\n", LINENO, COL, lexpeek(), e);
}

void lexnext(void) {
  if (*CURCODE == '\n') {
    COL = 1;
    ++LINENO;
  } else {
    ++COL;
  }
  ++CURCODE;
}

int lexexpect(char want) {
  char got;

  got = lexpeek();
  if (got != want) {
    char *msg;
    asprintf(&msg, "found '%c' expecting '%c", got, want);
    lexerr(msg);
    free(msg);
    return 1;
  }
  lexnext();
  return 0;
}

int iskeywordoridstart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

int iskeywordorid(char c) {
  return iskeywordoridstart(c) || (c >= '0' && c <= '9');
}

int lexstrlit(void) {
  int escaped;
  char *s;
  char c;
  int n;

  s = calloc(1024, 1);
  escaped = 0;
  n = 0;
  if (lexexpect('"')) {
    lexerr("string literal must begin with '\"'");
    return 1;
  }
  while ((c = lexpeek()) != 0) {
    /*printf("strlit[%d,%d]=%c (0x%x) -> %s (%lu)\n", n, escaped, c, c, s,
           strlen(s));*/
    if (escaped) {
      escaped = 0;
      if (c == 'n') {
        s[n] = '\n';
      } else if (c == 't') {
        s[n] = '\t';
      } else if (c == 'r') {
        s[n] = '\r';
      } else if (c == 'f') {
        s[n] = '\f';
      } else if (c == 'v') {
        s[n] = '\v';
      } else if (c == '0') {
        s[n] = 0;
      } else if (c == '"') {
        s[n] = '"';
      } else if (c == '\\') {
        s[n] = '\\';
      } else {
        lexerr("unknown escape character");
      }
    } else {
      if (c == '"') {
        lexexpect('"');
        lexemits(STRLIT, s);
        return 0;
      } else if (c == '\\') {
        escaped = 1;
        /* We need to avoid the n++ below when reading the escape as we're not
           printing a character in this branch. */
        lexnext();
        continue;
      } else {
        s[n] = c;
      }
    }
    ++n;
    lexnext();
  }
  lexerr("non-terminated string literal");
  return 1;
}

int lexchar(void) {
  char c;

  if (lexexpect('\'')) {
    lexerr("character literal must begin with '");
    return 1;
  }
  c = lexpeek();
  if (c == '\\') {
    lexnext();
    c = lexpeek();
    if (c == 'n') {
      c = '\n';
    } else if (c == 't') {
      c = '\t';
    } else if (c == 'r') {
      c = '\r';
    } else if (c == 'f') {
      c = '\f';
    } else if (c == 'v') {
      c = '\v';
    } else if (c == '0') {
      c = 0;
    } else if (c == '"') {
      c = '"';
    } else if (c == '\\') {
      c = '\\';
    } else if (c == '\'') {
      c = '\'';
    } else {
      lexerr("unknown escape character");
      return 1;
    }
  }
  lexnext();
  if (lexexpect('\'')) {
    lexerr("non-terminated character literal");
    return 1;
  }
  lexemitc(CHAR, c);
  return 0;
}

int lexkeywordorid(void) {
  char *id;
  char *c;
  int n;

  id = calloc(64, 1);
  c = id;
  n = 0;
  while (iskeywordorid(lexpeek())) {
    *c = lexpeek();
    ++c;
    lexnext();
    ++n;
    if (n == 64) {
      lexerr("keyword or identifier too long");
      free(id);
      return 1;
    }
  }
  /*printf("AAA='%s'\n", id);*/
  n = 0;
  while (n < (sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))) {
    if (KEYWORDS[n] && !strcmp(KEYWORDS[n], id)) {
      /* printf("match[%lu]: id=%s keyword=%s\n", n, id, KEYWORDS[n]);*/
      lexemit(n);
      free(id);
      return 0;
    }
    ++n;
  }
  lexemits(ID, id);
  return 0;
}

int lexnum(void) {
  char *raw;
  int i;
  long long res;

  raw = calloc(22, 1);
  i = 0;
  while (isdigit(lexpeek())) {
    if (i == 21) {
      lexerr("number too long");
      free(raw);
      return 1;
    }
    raw[i] = lexpeek();
    lexnext();
    ++i;
  }
  res = strtol(raw, NULL, 10);
  if (res == 0 && strlen(raw) != 1 && raw[0] != '0') {
    lexerr("bad number");
    free(raw);
    return 1;
  }
  lexemiti(CONSTANT, res);
  free(raw);
  return 0;
}

int lexcomment(void) {
  while (1) {
    if (lexpeek() == 0) {
      return 1;
    } else if (lexpeek() == '*') {
      lexexpect('*');
      if (lexpeek() == '/') {
        lexexpect('/');
        return 0;
      }
    } else {
      lexnext();
    }
  }
  return 0;
}

int lexop(void) {
  char c;

  c = lexpeek();
  /* I know.  */
  if (c == '.') {
    lexexpect('.');
    if (lexpeek() == '.') {
      lexexpect('.');
      if (lexexpect('.')) {
        lexerr("expecting third . for ellipsis");
        return 1;
      }
      lexemit(ELLIPSIS);
      return 0;
    }
    lexemit('.');
    return 0;
  } else if (c == '>') {
    lexexpect('>');
    if (lexpeek() == '>') {
      lexexpect('>');
      if (lexpeek() == '=') {
        lexexpect('=');
        lexemit(ASSIGN_RIGHT);
        return 0;
      }
      lexemit(OP_RIGHT);
      return 0;
    } else if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(OP_GE);
      return 0;
    }
    lexemit('>');
    return 0;
  } else if (c == '<') {
    lexexpect('<');
    if (lexpeek() == '<') {
      lexexpect('<');
      if (lexpeek() == '=') {
        lexexpect('=');
        lexemit(ASSIGN_LEFT);
        return 0;
      }
      lexemit(OP_LEFT);
      return 0;
    } else if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(OP_LE);
      return 0;
    }
    lexemit('<');
    return 0;
  } else if (c == '+') {
    lexexpect('+');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_ADD);
      return 0;
    } else if (lexpeek() == '+') {
      lexexpect('+');
      lexemit(OP_INC);
      return 0;
    }
    lexemit('+');
    return 0;
  } else if (c == '-') {
    lexexpect('-');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_SUB);
      return 0;
    } else if (lexpeek() == '-') {
      lexexpect('-');
      lexemit(OP_DEC);
      return 0;
    } else if (lexpeek() == '>') {
      lexexpect('>');
      lexemit(OP_PTR);
      return 0;
    }
    lexemit('-');
    return 0;
  } else if (c == '*') {
    lexexpect('*');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_MUL);
      return 0;
    }
    lexemit('*');
    return 0;
  } else if (c == '/') {
    lexexpect('/');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_DIV);
      return 0;
    } else if (lexpeek() == '*') {
      lexexpect('*');
      return lexcomment();
    }
    lexemit('/');
    return 0;
  } else if (c == '&') {
    lexexpect('&');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_AND);
      return 0;
    } else if (lexpeek() == '&') {
      lexexpect('&');
      lexemit(OP_AND);
      return 0;
    }
    lexemit('&');
    return 0;
  } else if (c == '^') {
    lexexpect('^');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_XOR);
      return 0;
    }
    lexemit('^');
    return 0;
  } else if (c == '%') {
    lexexpect('%');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_MOD);
      return 0;
    }
    lexemit('%');
    return 0;
  } else if (c == '|') {
    lexexpect('|');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(ASSIGN_OR);
      return 0;
    } else if (lexpeek() == '|') {
      lexexpect('|');
      lexemit(OP_OR);
      return 0;
    }
    lexemit('|');
    return 0;
  } else if (c == '!') {
    lexexpect('!');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(OP_NE);
      return 0;
    }
    lexemit('!');
    return 0;
  } else if (c == '=') {
    lexexpect('=');
    if (lexpeek() == '=') {
      lexexpect('=');
      lexemit(OP_EQ);
      return 0;
    }
    lexemit('=');
    return 0;
  } else if (c == '?' || c == ':' || c == '~' || c == ',' || c == '*') {
    lexexpect(c);
    lexemit(c);
    return 0;
  }
  return 1;
}

int isopstart(char c) {
  return c == '.' || c == '>' || c == '<' || c == '+' || c == '-' || c == '*' ||
         c == '%' || c == '&' || c == '^' || c == '|' || c == '!' || c == '~' ||
         c == '/' || c == '?' || c == ':' || c == '=' || c == ',';
}

int lexff(char w) {
  char c;

  while ((c = lexpeek()) != 0) {
    if (c == w) {
      return 0;
    }
    lexnext();
  }
  return 1;
}

void lex(void) {
  char c;

  while ((c = lexpeek())) {
    int err;

    /*printf("c=%c\n", c);*/
    err = 0;
    if (c == '"') {
      err = lexstrlit();
    } else if (c == '\'') {
      err = lexchar();
    } else if (iskeywordoridstart(c)) {
      err = lexkeywordorid();
    } else if (isspace(c)) {
      lexnext();
    } else if (isopstart(c)) {
      err = lexop();
    } else if (c == ';' || c == '{' || c == '}' || c == '[' || c == ']' ||
               c == '(' || c == ')') {
      lexemit(c);
      lexnext();
    } else if (c == '#') {
      lexwarn("skipping preprocessor directive");
      lexff('\n');
    } else if (isdigit(c)) {
      err = lexnum();
    } else {
      lexerr("unidentified character");
      err = 1;
    }
    if (err) {
      lexff('\n');
    }
  }
  lexemit(_END);
}

void parseerr(char *e) {
  ++PARSEERRS;
  printf("error: %d:%d: %s\n", CURTOK->lineno, CURTOK->col, e);
}

void parsewarn(char *e) {
  ++PARSEWARNS;
  printf("warning: %d:%d: %s\n", CURTOK->lineno, CURTOK->col, e);
}

void checkwarn(struct node *n, char *e) {
  ++CHECKWARNS;
  printf("warning: %d:%d: %s\n", n->tok->lineno, n->tok->col, e);
}

int parsepeek(void) {
  return CURTOK->kind;
}

int parsepeekpeek(void) {
  struct tok *tok;

  tok = CURTOK + 1;
  if (tok) {
    return tok->kind;
  } else {
    return -1;
  }
}

void parsenext(void) {
  ++CURTOK;
}

int parseexpect(int want) {
  int got;

  got = parsepeek();
  if (got != want) {
    char *msg;
    asprintf(&msg, "found %s expecting %s", TOKNAMES[got], TOKNAMES[want]);
    parseerr(msg);
    free(msg);
    return 1;
  }
  parsenext();
  return 0;
}

int parseff(int kind) {
  int tok;

  while ((tok = parsepeek()) != _END) {
    if (tok == kind) {
      return 0;
    }
    parsenext();
  }
  return 1;
}

int parseff2(int kind1, int kind2) {
  int tok;

  while ((tok = parsepeek()) != _END) {
    if (tok == kind1 || tok == kind2) {
      return 0;
    }
    parsenext();
  }
  return 1;
}

void toksdump(void) {
  int kind;

  CURTOK = TOKS;
  while ((kind = parsepeek())) {
    if (kind == ID) {
      printf("[token] id=%s\n", CURTOK->s);
    } else if (kind == STRLIT) {
      printf("[token] strlit=%s\n", CURTOK->s);
    } else if (kind == CONSTANT) {
      printf("[token] constant=%ld\n", CURTOK->i);
    } else if (kind == CHAR) {
      printf("[token] char=%c (%d)\n", CURTOK->c, CURTOK->c);
    } else {
      printf("[token] %s (%d)\n", TOKNAMES[kind], kind);
    }
    parsenext();
    if (kind == _END) {
      break;
    }
  }
}

void parseentry(char *which) {
  char *what;
  int c;

  c = parsepeek();
  if (c == STRLIT || c == ID) {
    printf("%28s:%d:%d: %s\n", which, CURTOK->lineno, CURTOK->col, CURTOK->s);
  } else if (c == CONSTANT) {
    printf("%28s:%d:%d: %ld\n", which, CURTOK->lineno, CURTOK->col, CURTOK->i);
  } else {
    printf("%28s:%d:%d: %s\n", which, CURTOK->lineno, CURTOK->col, TOKNAMES[c]);
  }
}

void checkentry(char *which, struct node *n) {
  printf("%28s:%d:%d: %s\n", which, n->tok->lineno, n->tok->col,
         NODENAMES[n->kind]);
}

int isopbinary(int kind) {
  return PRECBIN[kind] > -1;
}
int isopunary(int kind) {
  return PRECUN[kind] > -1;
}
int isopsimplepostfix(int kind) {
  return kind == OP_INC || kind == OP_DEC;
}

struct tok *copytok(struct tok *orig) {
  struct tok *ret;

  ret = malloc(sizeof(struct tok));
  memcpy(ret, orig, sizeof(struct tok));
  return ret;
}

struct node *node(int kind, struct tok *tok) {
  struct node *n;

  n = calloc(1, sizeof(struct node));
  n->kind = kind;
  if (tok) {
    n->tok = tok;
  } else {
    n->tok = CURTOK;
  }
  n->n = NODEN;
  ++NODEN;
  if (VERBOSE) {
    printf("[new node {%d}] %s (%s)\n", n->n, NODENAMES[kind],
           TOKNAMES[n->tok->kind]);
  }
  return n;
}

struct node *type(int flags, struct node *specs, struct tok *tok) {
  struct node *t;

  t = node(TYPE, tok);
  t->data = malloc(sizeof(int));
  *(int *)t->data = flags;
  t->right = specs;
  return t;
}

struct node *expr(int minprec);
struct node *declarator(void);
struct node *declarationorfundef(void);
struct node *declarationspecifiers(void);
struct node *statement(void);
struct node *typespecorqual(void);
int istypespecifier(struct tok *);

struct node *funcallargs(void) {
  /* To keep things simpler, we don't support abstract declarators. */
  struct node *e;
  struct node **c;
  struct node *ret;

  e = node(FUNARGS, NULL);
  c = &e->left;
  ret = e;
  parseentry("funcallargs");
  while (parsepeek() != ')' && parsepeek() != _END) {
    *c = node(FUNARG, NULL);
    (*c)->left = expr(1);
    if (parsepeek() == ',') {
      parsenext();
    } else {
      break;
    }
    c = &(*c)->next;
  }
  return ret;
}

struct node *atomarray(void) {
  struct node *o = NULL;
  struct node **e;
  while (parsepeek() == '[') {
    parsenext();
    if (!o) {
      o = expr(1);
      e = &o->left;
    } else {
      *e = expr(1);
      e = &(*e)->left;
    }
    if (parsepeek() != ']') {
      parseerr("unbalanced brackets in array subscript");
      return NULL;
    }
    parseexpect(']');
  }
  return o;
}

struct node *declspecs(struct node *specs, int flags, struct tok *tok) {
  struct node *n;
  n = node(DECLSPECS, tok);
  n->left = specs;
  n->data = malloc(sizeof(int));
  *(int *)n->data = flags;
  return n;
}

struct node *atom(void) {
  struct node *e;
  int tmp;
  int c;

  c = parsepeek();
  parseentry("atom");
  if (c == '(') {
    struct tok *otok;
    struct node *t;
    /*
     * Typecast is essentially a prefix operator, but treated separately here
     * with parenthesised expressions.
     */
    parsenext();
    if ((t = typespecorqual())) {
      struct node *tn;
      struct node *ds;
      otok = CURTOK;
      if (!t) {
        parseerr("invalid typecast");
        return NULL;
      }
      e = node(TYPECAST, otok);
      e->left = declarator();
      ds = declspecs(t, *(int *)t->data, otok);
      e->data = ds;
      if (parseexpect(')')) {
        parseerr("unbalanced parentheses in typecast");
        return NULL;
      }
      e->right = expr(PRECUN['(']);
      return e;
    } else {
      e = expr(1);
      if (parseexpect(')')) {
        parseerr("unbalanced parentheses in expression");
        return NULL;
      }
      return e;
    }
  } else if (c == _END) {
    parseerr("abrupt end of expression");
    return NULL;
  } else if (c == ID) {
    e = node(VARREF, NULL);
    parsenext();
    return e;
  } else if (c == CONSTANT) {
    e = node(N_CONSTANT, NULL);
    if (VERBOSE) {
      printf("[number] %ld\n", e->tok->i);
    }
    parsenext();
    return e;
  } else if (c == STRLIT) {
    e = node(N_STRLIT, NULL);
    if (VERBOSE) {
      printf("[string] %s\n", e->tok->s);
    }
    parsenext();
    return e;
  } else if (c == CHAR) {
    e = node(N_CHAR, NULL);
    if (VERBOSE) {
      printf("[character] %c\n", e->tok->c);
    }
    parsenext();
    return e;
  } else if (c == '~' || c == '!' || c == OP_INC || c == OP_DEC || c == '&' ||
             c == '+' || c == '*' || c == '-') {
    e = node(OP_UN, NULL);
    if (VERBOSE) {
      printf("[opun] %s (prec=%d)\n", TOKNAMES[c], PRECUN[c]);
    }
    parsenext();
    e->left = expr(PRECUN[c]);
    return e;
  } else if (c == SIZEOF) {
    /*
     * The separate handling of sizeof is not nice at all. I took a few
     * shortcuts elsewhere with parsing and that makes handling parenthesised
     * expressions a bit painful.
     */
    struct tok *otok = CURTOK;
    parsenext();
    if (parsepeek() == '(') {
      struct node *t;
      parsenext();
      if (!(t = typespecorqual())) {
        e = node(N_SIZEOF_EXPR, otok);
        if (VERBOSE) {
          printf("[opun] %s (prec=%d)\n", TOKNAMES[SIZEOF], PRECUN[SIZEOF]);
        }
        e->left = expr(PRECUN[SIZEOF]);
        if (parseexpect(')')) {
          parseerr("unbalanced parentheses in sizeof expression");
          return NULL;
        }
        return e;
      }
      /* Construct like a declaration so similar type eval also works. */
      e = node(N_SIZEOF_TYPE, otok);
      e->left = declarator();
      e->right = t;
      if (parseexpect(')')) {
        parseerr("unbalanced parentheses in sizeof expression");
        return NULL;
      }
      return e;
    } else {
      e = node(N_SIZEOF_EXPR, otok);
      if (VERBOSE) {
        printf("[opun] %s (prec=%d)\n", TOKNAMES[SIZEOF], PRECUN[SIZEOF]);
      }
      e->left = expr(PRECUN[SIZEOF]);
      return e;
    }
  } else {
    char *msg;
    asprintf(&msg, "found %s when looking for a valid atom", TOKNAMES[c]);
    parseerr(msg);
    free(msg);
    return NULL;
  }
}

struct node *expr(int minprec) {
  struct node *left;

  parseentry("expr");
  if (VERBOSE) {
    printf("minprec=%d\n", minprec);
  }
  if (parsepeek() == ',' || parsepeek() == ';' || parsepeek() == ')' ||
      parsepeek() == ']' || parsepeek() == _END) {
    return NULL;
  }
  left = atom();
  if (!left) {
    return NULL;
  }
  while (parsepeek() != _END && parsepeek() != ';') {
    int c;
    int prec;
    struct node *e;

    c = parsepeek();
    if (VERBOSE) {
      printf("[expr] %s", TOKNAMES[c]);
      if (CURTOK->s) {
        printf("=%s\n", CURTOK->s);
      } else {
        puts("");
      }
    }
    if (c == '(') {
      prec = PRECBIN['('];
      if (prec < minprec) {
        break;
      }
      e = node(FUNCALL, left->tok);
      e->left = left;
      parsenext();
      e->right = funcallargs();
      left = e;
      if (parseexpect(')')) {
        parseerr("unbalanced ')' with function call");
        break;
      }
    } else if (c == '[') {
      prec = PRECBIN['['];
      if (prec < minprec) {
        break;
      }
      e = node(ARRAYSUBSCRIPT, NULL);
      e->left = left;
      parsenext();
      e->right = expr(1);
      left = e;
      if (parseexpect(']')) {
        parseerr("unbalanced ']' with array subscript");
        break;
      }
    } else if (isopbinary(c)) {
      int nextminprec;
      prec = PRECBIN[c];
      if (prec < minprec) {
        break;
      }
      if (ASSOCBIN[c] == 1) {
        nextminprec = prec + 1;
      } else {
        nextminprec = prec;
      }
      if (VERBOSE) {
        printf("[opbin] %s (prec=%d)\n", CURTOK->s, prec);
      }
      e = node(OP_BIN, CURTOK);
      e->left = left;
      parsenext();
      e->right = expr(nextminprec);
      left = e;
    } else if (isopsimplepostfix(c)) {
      /*
       * Array subscripts, function calls, and struct accesses are also
       * treated as postfix operators however we distinguish "[]" and "()" for
       * more explicit parsing logic.. This means we repeat things to keep the
       * cases separate. See the "else" branches below this one.
       *
       * Note that unary postfix operators in C have the highest so "prec <
       * minprec" can never be true.
       *
       * ++ and -- are treated specially because they can be both prefix and
       * postfix, where prefix has the precendece of MAX-1 and postfix is MAX,
       * hence the "+ 1" below.
       */
      prec = PRECUN[c];
      if (c == OP_INC || c == OP_DEC) {
        ++prec;
      }
      if (prec < minprec) {
        break;
      }
      e = node(OP_UN_POSTFIX, CURTOK);
      e->left = left;
      parsenext();
      left = e;
    } else {
      break;
    }
  }
  return left;
}

struct node *storageclassspecifier(void) {
  int t;
  int flags;
  struct tok *otok;

  t = parsepeek();
  flags = 0;
  otok = CURTOK;
  parseentry("storageclassspecifier");
  if (t == AUTO) {
    flags = 0;
  } else if (t == REGISTER) {
    flags = 0;
  } else if (t == STATIC) {
    flags = F_STATIC;
  } else if (t == EXTERN) {
    flags = F_EXTERN;
  } else if (t == TYPEDEF) {
    flags = F_TYPEDEF;
  } else {
    return NULL;
  }
  parsenext();
  return type(flags, NULL, otok);
}

struct node *structdeclaration(void) {
  struct node *sd;

  sd = declarationorfundef();
  if (!sd) {
    parseerr("invalid struct declaration");
    return NULL;
  }
  return sd;
}

struct node *structspecifier(void) {
  struct node *id;
  struct node *ret;
  struct node **cur;

  id = NULL;
  ret = NULL;
  cur = NULL;
  parseentry("structspecifier");
  if (parsepeek() != STRUCT) {
    return NULL;
  }
  parsenext();
  if (parsepeek() == ID) {
    id = node(N_ID, CURTOK);
    parsenext();
  }
  ret = node(N_STRUCT, CURTOK);
  ret->left = id;
  if (parsepeek() != '{') {
    return ret;
  }
  parsenext();
  cur = &ret->right;
  while (parsepeek() != '}' && parsepeek() != _END) {
    *cur = structdeclaration();
    if (!(*cur)) {
      parseerr("broken declaration in struct specifier");
      return NULL;
    }
    cur = &((*cur)->next);
  }
  parsenext();
  return ret;
}

struct node *enumerator(void) {
  struct node *ret;
  struct node *ce;

  ret = NULL;
  parseentry("enumerator");
  if (parsepeek() != ID) {
    parseerr("enumerator does not begin with an identifier");
    return NULL;
  }
  ret = node(ENUMERATOR, CURTOK);
  ret->left = node(N_ID, CURTOK);
  parsenext();
  if (parsepeek() != '=') {
    return ret;
  }
  parsenext();
  ce = expr(1);
  if (!ce) {
    parseerr("bad enumerator expression");
    return NULL;
  }
  ret->right = ce;
  return ret;
}

struct node *enumspecifier(void) {
  struct node *ret;
  struct node **cur;

  cur = NULL;
  parseentry("enumspecifier");
  if (parsepeek() != ENUM) {
    return NULL;
  }
  ret = node(N_ENUM, NULL);
  parsenext();
  if (parsepeek() == ID) {
    ret->right = node(N_ID, NULL);
    parsenext();
  }
  if (parsepeek() != '{') {
    return ret;
  }
  parsenext();
  cur = &ret->left;
  while (parsepeek() != '}' && parsepeek() != _END) {
    *cur = enumerator();
    if (!(*cur)) {
      parseerr("broken enumeration member in enumeration specifier");
      return NULL;
    }
    if (parsepeek() != ',') {
      break;
    }
    parsenext();
    cur = &((*cur)->next);
  }
  if (parseexpect('}')) {
    parseerr("expecting terminating ';' for enumeration specifier");
    return NULL;
  }
  return ret;
}

struct node *typedefname(void) {
  struct defpair *p;

  parseentry("typedefname");
  if (parsepeek() != ID) {
    return NULL;
  }
  if ((p = defpairfind(TYPEDEFS, CURTOK->s))) {
    struct node *ret = node(N_TYPEDEF, NULL);
    ret->left = p->def;
    parsenext();
    return ret;
  }
  return NULL;
}

int evalconstexpr(struct node *n) {
  int k;

  printf("evalconstexpr: %s\n", NODENAMES[n->kind]);
  k = n->kind;
  if (k == N_CONSTANT) {
    return n->tok->i;
  } else if (k == OP_BIN) {
    int tk;

    tk = n->tok->kind;
    if (tk == '+') {
      return evalconstexpr(n->left) + evalconstexpr(n->right);
    } else if (tk == OP_LEFT) {
      return evalconstexpr(n->left) << evalconstexpr(n->right);
    } else {
      die("evalconstexpr", "only know '+'");
    }
  } else if (k == ENUMERATORCONSTANT) {
    return n->tok->i;
  } else if (k == OP_UN) {
    int tk;

    tk = n->tok->kind;
    if (tk == '-') {
      return -evalconstexpr(n->left);
    }
  } else {
    nodedump(0, n);
    die("evalconstexpr knows only constants and binary operators",
        NODENAMES[n->kind]);
  }
  return 0;
}

int constantexpression(void) {
  struct node *n;
  int i;

  /*
   * Zero means failed to parse as constant expression must be greater than
   * zero.
   */
  parseentry("constantexpression");
  n = expr(1);
  i = evalconstexpr(n);
  return i;
}

struct node *block(void) {
  struct node *headdecl;
  struct node *headstmt;
  struct node *prev;
  struct node *cur;
  struct node *bl;

  bl = node(BLOCK, NULL);
  cur = NULL;
  prev = NULL;
  headdecl = NULL;
  headstmt = NULL;
  parseentry("block");
  if (parseexpect('{')) {
    parseerr("expecting '{' to begin block");
    return NULL;
  }
  /* FIXME only recognizes simple types */
  while (istypespecifier(CURTOK)) {
    cur = declarationorfundef();
    if (!cur) {
      parseerr("broken declaration in block");
      parseff(';');
      continue;
    }
    /*
     * The first declaration begins the chain from the `blockdecls` entry. The
     * same logic repeats for statements below.
     */
    if (!headdecl) {
      headdecl = cur;
    }
    if (prev) {
      prev->next = cur;
    }
    prev = cur;
  }
  prev = NULL;
  while (1) {
    int t;

    t = parsepeek();
    if (t == '}') {
      parsenext();
      break;
    }
    if (t == _END) {
      parseerr("abrupt end of block");
      return NULL;
    }
    cur = statement();
    if (!cur) {
      if (!parseff(';')) {
        parsenext();
      }
      continue;
    }
    if (!headstmt) {
      headstmt = cur;
    }
    if (prev) {
      prev->next = cur;
    }
    prev = cur;
  }
  bl->left = headdecl;
  bl->right = headstmt;
  return bl;
}

struct node *statement(void) {
  struct node *cond;
  struct node *cur;
  struct node *ret;
  struct tok *otok;
  struct node *bl;
  int c;

  otok = CURTOK;
  cur = NULL;
  ret = NULL;
  c = parsepeek();
  parseentry("statement");
  if (c == ID && parsepeekpeek() == ':') {
    ret = node(LABEL, otok);
    parsenext();
    parsenext();
  } else if (c == IF) {
    struct node *prev = NULL;
    while (parsepeek() == IF) {
      parsenext();
      if (parseexpect('(')) {
        parseerr("expecting '(' before if condition");
        return NULL;
      }
      cond = expr(1);
      if (!cond) {
        parseerr("if condition does not produce a valid expression");
        return NULL;
      }
      if (parseexpect(')')) {
        parseerr("expecting ')' after if condition");
        printf("got %s\n", TOKNAMES[CURTOK->kind]);
        return NULL;
      }
    final:
      bl = block();
      if (!bl) {
        parseerr("cannot parse if block");
        return NULL;
      }
      cur = node(COND, otok);
      cur->left = cond;
      cur->right = bl;
      if (!ret) {
        ret = cur;
      }
      if (prev) {
        /*
         * COND is a statement node and the block logic uses `next` so we nest
         * it.
         */
        prev->left->next = cur;
      }
      prev = cur;
      if (parsepeek() != ELSE) {
        break;
      }
      parsenext();
      if (parsepeek() == '{') {
        cond = NULL;
        goto final;
      }
    }
  } else if (c == WHILE) {
    parsenext();
    if (parseexpect('(')) {
      parseerr("expecting '(' before while condition");
      return NULL;
    }
    cond = expr(1);
    if (!cond) {
      parseerr("while condition does not produce a valid expression");
      return NULL;
    }
    if (parseexpect(')')) {
      parseerr("expecting ')' after while condition");
      printf("got %s\n", TOKNAMES[CURTOK->kind]);
      return NULL;
    }
    bl = block();
    if (!bl) {
      parseerr("cannot parse while block");
      return NULL;
    }
    cur = node(N_WHILE, otok);
    cur->left = cond;
    cur->right = bl;
    ret = cur;
  } else if (c == CONTINUE) {
    parsenext();
    if (parseexpect(';')) {
      parseerr("expecting ';' to terminate continue");
      return NULL;
    }
    ret = node(N_CONTINUE, otok);
  } else if (c == BREAK) {
    parsenext();
    if (parseexpect(';')) {
      parseerr("expecting ';' to terminate break");
      return NULL;
    }
    ret = node(N_BREAK, otok);
  } else if (c == RETURN) {
    parsenext();
    ret = node(N_RETURN, otok);
    if (parsepeek() == ';') {
      parsenext();
    } else {
      ret->left = expr(1);
      if (!ret->left) {
        parseerr("cannot parse expression in return");
      }
      if (parseexpect(';')) {
        parseerr("expecting ';' to terminate return");
        return NULL;
      }
    }
  } else if (c == GOTO) {
    parsenext();
    if (parsepeek() != ID) {
      parseerr("expecting an identifier after goto");
      return NULL;
    }
    ret = node(N_GOTO, otok);
    ret->left = node(N_ID, CURTOK);
    parsenext();
    if (parseexpect(';')) {
      parseerr("expecting ';' to terminate goto");
      return NULL;
    }
  } else if (c == ';') {
    ret = node(NOP, NULL);
    parsenext();
  } else {
    cur = expr(1);
    if (!cur) {
      parseerr("cannot parse expression in statement");
      return NULL;
    }
    if (parseexpect(';')) {
      parseerr("expecting ';' to terminate expression");
      return NULL;
    }
    ret = node(EXPR, otok);
    ret->left = cur;
  }
  return ret;
}

struct node *fundefordecl(void) {
  struct node *op;
  struct node *n;
  struct node *p;
  struct tok *t;
  int flags;

  t = CURTOK;
  n = NULL;
  p = node(FUNARGDEF, NULL);
  op = p;
  parseentry("fundefordecl");
  while (parsepeek() != _END) {
    struct node *ds = declarationspecifiers();
    flags = *(int *)ds->data;
    if (VERBOSE) {
      printf("flags=0x%x\n", flags);
    }
    if (!flags) {
      parseerr("invalid type-specifier for function argument");
      return NULL;
    }
    p->data = ds;
    if ((p->left = declarator()) == NULL) {
      parseerr("invalid declarator for function argument");
      return NULL;
    }
    if (parsepeek() == ',') {
      parseexpect(',');
      p->next = node(FUNARGDEF, NULL);
      p = p->next;
      if (parsepeek() == ELLIPSIS) {
        struct node *ty = type(F_ANY, NULL, CURTOK);
        /*
         * FIXME very janky: we manually construct a type tree for ellipsis
         */
        p->left = node(DECLARATOR, CURTOK);
        p->left->left = node(N_ELLIPSIS, NULL);
        p->left->data = calloc(1, sizeof(struct nodedeclarator));
        p->data = declspecs(ty, F_ANY, CURTOK);
        parsenext();
        if (parseexpect(')')) {
          parseerr("expecting ')' after ellipsis");
          return NULL;
        }
        break;
      }
      continue;
    } else if (parsepeek() == ')') {
      parsenext();
      break;
    }
  }
  if (parsepeek() == ';') {
    n = node(FUNDECL, t);
    n->left = op;
    return n;
  }
  /*
   * Now we know it's supposed to be a function definition instead of
   * declaration.
   */
  n = node(FUNDEF, t);
  n->left = op;
  n->right = block();
  if (!n->right) {
    parseerr("cannot parse function definition's block");
    return NULL;
  }
  return n;
}

int pointer(void) {
  int plvl;

  plvl = 0;
  parseentry("pointer");
  while (parsepeek() == '*') {
    ++plvl;
    parsenext();
    /* we ignore type qualifiers */
    while (parsepeek() == CONST || parsepeek() == VOLATILE) {
      ;
    }
  }
  return plvl;
}

int istypespecifier(struct tok *tok) {
  int k;

  k = tok->kind;
  parseentry("istypespecifier");
  return k == VOID || k == CHAR || k == SHORT || k == INT || k == LONG ||
         k == FLOAT || k == DOUBLE || k == SIGNED || k == UNSIGNED ||
         k == STRUCT || k == ENUM || k == CONST | k == VOLATILE ||
         defpairfind(TYPEDEFS, tok->s);
}

struct node *declarator(void) {
  struct node *n = NULL;
  struct node *nt = NULL;
  struct node *dd = NULL;
  struct node *nid = NULL;
  struct nodelist *nl = NULL;
  struct tok *t = CURTOK;
  struct nodedeclarator *nd;
  int plvl;
  int arrsz = -1;
  parseentry("declarator");
  plvl = pointer();
  if (parsepeek() == ID) {
    nid = node(N_ID, NULL);
    if (VERBOSE) {
      printf("declarator id=%s\n", CURTOK->s);
    }
    parsenext();
  } else if (parsepeek() == '(') {
    parsenext();
    dd = declarator();
    if (parseexpect(')')) {
      parseerr("parenthesised declarator missing ')");
      return NULL;
    }
  }
  while (1) {
    /* FIXME We don't parse this right as `nt` is overwritten */
    int k;

    k = parsepeek();
    if (k == '(') {
      parsenext();
      /*
       * C grammar is more lenient here, but we look for type-specifiers to
       * separate between function's parameter list and parenthesised
       * declarators.
       */
      if (istypespecifier(CURTOK)) {
        nt = fundefordecl();
      } else {
        nt = declarator();
        parseexpect(')');
      }
    } else if (k == '[') {
      parsenext();
      if (parsepeek() == ']') {
        parsenext();
        /*
         * Constant expression must be greater than zero so we use it to
         * indicate that size is determined by the initializer. The
         * declaration function will figure his out.
         */
        arrsz = 0;
      } else if ((arrsz = constantexpression()) > 0) {
        if (parseexpect(']')) {
          parseerr("mismatched brackets in array");
          parseff(';');
          return NULL;
        }
      } else {
        parseerr("bad constant expression in array declaration");
        parseff(';');
        return NULL;
      }
    } else {
      break;
    }
  }
  n = node(DECLARATOR, t);
  n->left = nid;
  n->right = nt;
  nd = calloc(1, sizeof(struct nodedeclarator));
  nd->plvl = plvl;
  nd->arrsz = arrsz;
  n->data = nd;
  n->next = dd;
  return n;
}

struct node *initializer(void) {
  /*
   * Here we deviate from the grammar and simplify our expression -
   * handling such that we forbid the ',' operator completely.
   *
   * initializer: assignment-expression | "{" initializer % "," ? "}"
   */
  struct tok *t = CURTOK;
  struct node *ret = NULL;
  struct node **cur = NULL;
  parseentry("initializer");
  if (parsepeek() == '{') {
    ret = node(INITIALIZERARRAY, t);
    parsenext();
    cur = &(ret->left);
    while (1) {
      if (parsepeek() == '}' || parsepeek() == _END) {
        break;
      }
      if ((*cur = initializer())) {
        if (parsepeek() == ',') {
          parsenext();
          cur = &((*cur)->next);
        }
      } else {
        parseerr("bad array initializer");
        return NULL;
      }
    }
    if (parseexpect('}')) {
      parseerr("missing '}' to terminate array initializer");
      return NULL;
    }
  } else {
    ret = node(INITIALIZER, t);
    ret->left = expr(1);
  }
  return ret;
}

int typeatomic(void) {
  int ret = 0;
  int k;

  ret = 0;
  k = parsepeek();
  while (1) {
    if (k == VOID) {
      ret |= F_VOID;
    } else if (k == CHAR) {
      ret |= F_CHAR;
    } else if (k == SHORT) {
      ret |= F_SHORT;
    } else if (k == INT) {
      ret |= F_INT;
    } else if (k == LONG) {
      if (ret & F_LONG) {
        ret |= F_LONGLONG;
      }
      ret |= F_LONG;
    } else if (k == FLOAT) {
      ret |= F_FLOAT;
    } else if (k == DOUBLE) {
      ret |= F_DOUBLE;
    } else if (k == SIGNED) {
      ret |= F_SIGNED;
    } else if (k == UNSIGNED) {
      ret |= F_UNSIGNED;
    } else {
      break;
    }
    parsenext();
    k = parsepeek();
  }
  return ret;
}

struct node *typespecorqual(void) {
  struct tok *otok;
  struct node *n;
  int flags;
  int tmp;
  int c;

  n = NULL;
  flags = 0;
  otok = CURTOK;
  c = parsepeek();
  parseentry("typespecorqual");
  if ((tmp = typeatomic()) != 0) {
    flags |= tmp;
  } else if ((n = structspecifier())) {
    flags |= F_STRUCT;
  } else if ((n = enumspecifier())) {
    flags |= F_ENUM;
  } else if ((n = typedefname())) {
    flags |= F_TYPEDEFVAR;
  } else if (c == CONST) {
    flags = F_CONST;
    parsenext();
  } else if (c == VOLATILE) {
    flags = F_VOLATILE;
    parsenext();
  } else {
    return NULL;
  }
  return type(flags, n, otok);
}

int istypeindeclspecs(struct node *, int);

struct node *declarationspecifiers(void) {
  struct node *n = NULL;
  struct node *specs = NULL;
  struct node **cur = NULL;
  struct tok *otok = CURTOK;
  int flags = 0;
  parseentry("declarationspecifiers");
  while (1) {
    if ((n = storageclassspecifier())) {
    } else if ((n = typespecorqual())) {
    } else {
      break;
    }
    if (!specs) {
      specs = n;
    } else {
      *cur = n;
    }
    cur = &n->next;
    flags |= getspecsflags(n);
  }
  return declspecs(specs, flags, CURTOK);
}

char *getdecltypedefname(struct node *n);

struct node *getdeclarator(struct node *n) {
  _assert(n->kind == DECLARATION || n->kind == FUNARGDEF, NODENAMES[n->kind]);
  return n->data;
}

struct node *declarationorfundef(void) {
  struct node *d = NULL;
  struct node *ret = NULL;
  struct node *init = NULL;
  struct tok *t = CURTOK;
  struct node *ds = NULL;
  struct nodedeclarator *nd = NULL;
  int flags;
  parseentry("declarationorfundef");
  ds = declarationspecifiers();
  if (!ds) {
    parseerr("missing type-specifier");
    return NULL;
  }
  flags = *(int *)ds->data;
  if (VERBOSE) {
    printf("flags=0x%x\n", flags);
  }
  /* If we didn't get a type-specifier, it cannot be a valid declaration. */
  if (!flags) {
    parseerr("invalid type-specifier");
    return NULL;
  }
  /* init-declarator  */
  if ((d = declarator()) == NULL) {
    parseerr("invalid declarator");
    return NULL;
  }
  nd = getnodedeclarator(d);
  if (VERBOSE) {
    printf("arrsz=%d, plvl=%d\n", nd->arrsz, nd->plvl);
  }
  if (parsepeek() == '=') {
    parsenext();
    if (!(init = initializer())) {
      parseerr("missing valid initializer after '='");
      return NULL;
    }
    if (nd->arrsz == 0) {
      if (init->kind == INITIALIZERARRAY) {
        struct node *cur;
        int i = 0;
        if (!init) {
          parseerr(
              "array declaration without constant size missing initializer");
          return NULL;
        }
        cur = init->left;
        while (cur) {
          ++i;
          cur = cur->next;
        }
        nd->arrsz = i;
      } else if (init->kind == INITIALIZER && init->tok->kind == STRLIT) {
        nd->arrsz = strlen(init->tok->s) + 1;
        if (VERBOSE) {
          printf("deduced arrsz=%d\n", nd->arrsz);
        }
      } else {
        parseerr("unspecified array size with bad initializer");
        parseerr(TOKNAMES[init->tok->kind]);
        return NULL;
      }
    }
  }
  /* I know. */
  if ((!d->right || d->right->kind != FUNDEF) && parseexpect(';')) {
    parseerr("missing ';' from end of declaration");
    return NULL;
  }
  ret = node(DECLARATION, t);
  ret->data = ds;
  ret->left = d;
  ret->right = init;
  /* typedefs affect parsing of C so we need bookkeeping in parsing order */
  if (istypeindeclspecs(ds, F_TYPEDEF)) {
    char *name = getdecltypedefname(ret);
    printf("- typedef: %s\n", name);
    if (defpairfind(TYPEDEFS, name)) {
      parseerr("typedef with this name already defined");
      return NULL;
    } else {
      defpairappend(TYPEDEFS, name, getdeclarator(ret));
    }
  }
  return ret;
}

int translationunit(void) {
  struct node *n;
  parseentry("translationunit");
  while (1) {
    if (parsepeek() == _END) {
      return 0;
    } else if ((n = declarationorfundef())) {
      *CURNODE = n;
      ++CURNODE;
    } else {
      parseff2(';', '}');
      return 1;
    }
  }
  return 0;
}

void parse(void) {
  CURTOK = TOKS;
  translationunit();
}

void nodesdump(void) {
  struct node **n = NODES;
  while (*n) {
    nodedump(0, *n);
    ++n;
  }
}
void checkerr(struct node *n, char *e) {
  ++CHECKERRS;
  printf("error: %d:%d: %s\n", n->tok->lineno, n->tok->col, e);
}

int isdeclfundef(struct node *n) {
  return n->left && n->left->right && n->left->right->kind == FUNDEF;
}

int isdeclfundecl(struct node *n) {
  return n->left && n->left->right && n->left->right->kind == FUNDECL;
}

int isdeclstruct(struct node *n) {
  return getspecsflags(getnodedata(n)) & F_STRUCT;
}

int issizeoftypestruct(struct node *n) {
  _assert(n != NULL, "issizeoftypestruct: data missing");
  return n->right && n->right->right && n->right->right->kind == N_STRUCT;
}

int isdeclstructdef(struct node *n) {
  struct node *d = getdeclarator(n);
  return d->left->right->right != NULL;
}

int isdeclvar(struct node *n) {
  return n->left && n->left->left && n->left->left->kind == N_ID;
}

char *getdeclvarname(struct node *n) {
  return n->left->left->tok->s;
}

struct node *getfuncallfirstarg(struct node *n) {
  return n->right->left;
}

struct node *getcondblock(struct node *n) {
  return n->right;
}

struct node *getwhilecond(struct node *n) {
  return n->left;
}

struct node *getwhileblock(struct node *n) {
  return n->right;
}

struct node *getcondnext(struct node *n) {
  _assert(n->kind == COND, "getcondnext: not COND");
  if (!n->left) {
    return NULL;
  }
  return n->left->next;
}

struct node *getcondcond(struct node *n) {
  _assert(n->kind == COND, "getcondcond: not COND");
  return n->left;
}

int istypeindeclspecs(struct node *n, int type) {
  struct node **cur;
  _assert(n->kind == DECLSPECS, NODENAMES[n->kind]);
  cur = &n->left;
  while (*cur) {
    if (getspecsflags(*cur) & type) {
      return 1;
    }
    ++cur;
  }
  return 0;
}

int isdecltypedef(struct node *n) {
  struct node *d;
  _assert(n->data != NULL, "isdecltypedef: data missing");
  d = getnodedata(n);
  return istypeindeclspecs(d, F_TYPEDEF);
}

int isdecltypedefvar(struct node *n) {
  struct node *d;
  _assert(n->data != NULL, "isdecltypedefvar: data missing");
  d = getnodedata(n);
  return d->left && d->left->right && d->left->right->kind == N_TYPEDEF;
}

int typecmpflags(int f) {
  /* Would be easier to just negate instead... */
  return f & F_STRUCT | f & F_ENUM | f & F_VOID | f & F_CHAR | f & F_LONG |
         f & F_LONGLONG | f & F_UNSIGNED | f & F_SIGNED | f & F_SHORT |
         f & F_FLOAT | f & F_DOUBLE | f & F_TYPEDEF | f & F_TYPEDEFVAR;
}

int aretypesequal(struct type *t1, struct type *t2) {
  puts("===== aretypesequal =====");
  typedump(t1);
  typedump(t2);
  /* ellipsis involved */
  if (t1->flags & F_ANY || t2->flags & F_ANY) {
    return 1;
  }
  /* void pointers are comparable to any other pointer (mumble mumble) */
  if ((t1->flags & F_VOID || t2->flags & F_VOID) && t1->plvl == t2->plvl &&
      t1->plvl > 0) {
    return 1;
  }
  return typecmpflags(t1->flags) == typecmpflags(t2->flags) &&
         t1->plvl == t2->plvl && t1->arrsz == t2->arrsz;
}

struct type *setnodetype(struct node *n, struct type *ty) {
  n->type = ty;
  return ty;
}

struct type *checkdecltype(struct node *n) {
  struct nodedeclarator *nd;
  struct type *ty;
  struct node *tn;
  struct node *d;
  int i;

  _assert(n != NULL, "evaldecltype: NULL node");
  ty = calloc(1, sizeof(struct type));
  tn = getdeclfirsttype(n);
  d = getdecldecl(n);
  nd = getnodedeclarator(d);
  while (tn) {
    i = *(int *)tn->data;
    ty->flags |= i;
    tn = tn->next;
  }
  if (isdeclstruct(n)) {
    ty->structname = getdeclstructname(n);
    _assert(ty->structname != NULL, "null structname");
  }
  ty->plvl = nd->plvl;
  ty->arrsz = nd->arrsz;
  return ty;
}

void evalstructdef(struct node *n) {
  struct structdef *sd;
  struct node *cursrc;
  struct node *d;
  char *name;
  int cursz;
  int i;

  name = getdeclstructname(n);
  _assert(name != NULL, "no name for struct definition");
  if (structdeffind(STRUCTDEFS, name)) {
    checkwarn(n, "struct already defined");
    return;
  }

  sd = calloc(1, sizeof(struct structdef));
  sd->name = name;
  d = getdeclarator(n);
  cursrc = getdeclstructfirstdecl(d);
  i = 0;
  cursz = 0;
  while (cursrc) {
    sd->fields[i] = strdup(getdeclvarname(cursrc));
    sd->types[i] = checkdecltype(cursrc);
    sd->offsets[i] = cursz;
    /* FIXME Does not pad anything. */
    cursz += ceilto(typesize(sd->types[i]), 4);
    cursrc = cursrc->next;
    ++i;
  }
  sd->memsz = cursz;
  structdefdump(sd);
  structdefappend(STRUCTDEFS, sd);
}

struct type *checksizeoftype(struct node *n) {
  /*
   * Due to jankiness elsewhere, this is an unfortunately close copy of
   * `evaldecltype`.
   */
  struct nodedeclarator *nd;
  struct type *ty;
  struct node *tn;
  struct node *d;
  int i;

  _assert(n != NULL, "evalsizeoftype: NULL node");
  ty = calloc(1, sizeof(struct type));
  tn = n->right;
  d = getdecldecl(n);
  nd = getnodedeclarator(d);
  while (tn) {
    i = *(int *)tn->data;
    ty->flags |= i;
    tn = tn->next;
  }
  if (issizeoftypestruct(n)) {
    ty->structname = getsizeoftypestructname(n);
    _assert(ty->structname != NULL, "evalsizeoftype cannot find structname");
  }
  ty->plvl = nd->plvl;
  ty->arrsz = nd->arrsz;
  return ty;
}

struct type *checkexprtype(struct scope *, struct node *);

struct type *checkfuncalltype(struct scope *s, struct node *n) {
  /* 32 function arguments are enough for everyone */
  struct type *tydefn[32];
  struct type *tycall[32];
  struct node *argdefn;
  struct node *argcall;
  struct defpair *f;
  int foundellipsis;
  struct type *ty;
  int foundvoid;
  char *name;
  int ncall;
  int ndefn;
  int i;

  ncall = 0;
  ndefn = 0;
  foundvoid = -1;
  foundellipsis = -1;
  name = n->left->tok->s;
  /* I know. */
  memset(tydefn, 0, 32 * sizeof(struct type *));
  memset(tycall, 0, 32 * sizeof(struct type *));
  /* FIXME not gonna work with function pointers */
  f = defpairfind(FUNCDEFS, name);
  if (!f) {
    f = defpairfind(FUNCDECLS, name);
    if (!f) {
      checkerr(n, "unrecognized function called");
      return NULL;
    }
  }
  ty = checkdecltype(f->def);
  /* Set the type also to the FUNCALL target. Probably this is a VARREF. */
  setnodetype(n->left, ty);
  i = 0;
  argdefn = getdeclfunargdef(f->def);
  while (argdefn) {
    if (foundellipsis >= 0) {
      checkerr(n, "ellipsis has to be the final function parameter");
      return ty;
    }
    tydefn[i] = checkdecltype(argdefn);
    if (tydefn[i]->flags | F_VOID) {
      foundvoid = i;
    } else if (tydefn[i]->flags | F_ANY) {
      foundellipsis = i;
      break;
    }
    ++i;
    ++ndefn;
    argdefn = argdefn->next;
  }
  i = 0;
  argcall = getfuncallfirstarg(n);
  while (argcall) {
    tycall[i] = checkexprtype(s, argcall->left);
    ++i;
    ++ncall;
    argcall = argcall->next;
  }
  i = 0;
  while (i < ndefn && i < ncall && (foundellipsis == -1 || i < foundellipsis)) {
    if (!aretypesequal(tydefn[i], tycall[i])) {
      char *a;
      asprintf(&a, "type mismatch in function parameter %d", i + 1);
      checkerr(n, a);
      free(a);
    }
    ++i;
  }
  if (ndefn == 1 && foundvoid == 0 && ncall == 0) {
    /* passthrough */
  } else if (ndefn <= ncall + 1 && foundellipsis) {
    /* passthrough  */
  } else if (ndefn < ncall) {
    checkerr(n, "too many arguments in function call");
  } else if (ndefn > ncall) {
    checkerr(n, "not enough arguments in function call");
  }
  return ty;
}

struct type *checkopbintype(struct scope *s, struct node *n) {
  struct type *ty;
  struct type *t1;
  struct type *t2;
  int tk;

  tk = n->tok->kind;
  ty = NULL;
  if (tk == OP_PTR || tk == '.') {
    struct structdef *sd;
    char *sn;
    t1 = checkexprtype(s, n->left);

    /* FIXME Check if t1 is actually a struct with plvl=0 and arrsz=0  */
    if (!(t1->flags & F_STRUCT)) {
      checkerr(n, "trying to access a struct field of a non-struct");
    } else {
      sd = structdeffind(STRUCTDEFS, t1->structname);
      if (n->right->kind != VARREF) {
        checkerr(n->right, "struct access not targeting a field name");
        ty = NULL;
      } else {
        ty = structdeffieldtype(sd, n->right->tok->s);
        if (!ty) {
          char *e;
          asprintf(&e, "struct field missing from '%s': %s", sn,
                   n->right->tok->s);
          checkerr(n->right, e);
          free(e);
        }
      }
    }
  } else {
    t1 = checkexprtype(s, n->left);
    t2 = checkexprtype(s, n->right);
    if (!aretypesequal(t1, t2)) {
      checkerr(n, "binary operation is mistyped");
    }
    ty = t1;
  }
  return ty;
}

struct type *checkopuntype(struct scope *s, struct node *n) {
  struct type *ty;
  struct type *tyr;
  int tk;

  ty = calloc(1, sizeof(struct type));
  tk = n->tok->kind;
  tyr = checkexprtype(s, n->left);
  memcpy(ty, tyr, sizeof(struct type));
  if (tk == '*') {
    --ty->plvl;
  } else if (tk == '&') {
    ++ty->plvl;
  } else if (tk == '+') {
    /* noop */
  } else if (tk == OP_INC) {
    /* noop */
  } else if (tk == OP_DEC) {
    /* noop */
  } else if (tk == '!') {
    /* noop */
  } else if (tk == '-') {
    /* noop */
  } else if (tk == '~') {
    /* noop */
  } else {
    nodedump(0, n);
    die("unhandled OP_UN", TOKNAMES[tk]);
  }
  return ty;
}

struct type *checkinitializertype(struct scope *s, struct node *n) {
  struct type *curtype;
  struct node *cur;
  struct type *ty;

  cur = n;
  ty = NULL;
  while (cur) {
    curtype = checkexprtype(s, cur->left);
    if (!ty) {
      ty = curtype;
    } else {
      if (!aretypesequal(ty, curtype)) {
        checkerr(cur, "initializer type mismatch");
      }
    }
    cur = cur->next;
  }
  return ty;
}

struct type *enumeratortype(void) {
  struct type *ty;

  ty = calloc(1, sizeof(struct type));
  ty->arrsz = -1;
  ty->structname = NULL;
  ty->flags = F_INT;
  ty->plvl = 0;
  return ty;
}

struct type *sizeoftype(void) {
  return enumeratortype();
}

struct type *checkvarreftype(struct scope *s, struct node *n) {
  struct strintpair *sip;
  struct scopevar *v;
  struct type *ty;
  char *name;

  /*
   * For VARREFs we have options:
   *   - it's a variable
   *   - it's an enumerator
   */
  name = n->tok->s;
  v = scopefindvar(s, name);
  if (v) {
    return checkdecltype(v->this);
  }
  sip = strintpairfindstr(ENUMRS, name);
  if (sip) {
    return enumeratortype();
  }
  checkerr(n, "variable not in scope");
}

struct type *checkopunpostfixtype(struct scope *s, struct node *n) {
  struct type *ty;
  int tk;

  ty = checkexprtype(s, n->left);
  tk = n->tok->kind;
  if (tk == OP_INC) {
    die("evalopnupostfixtype", "++ not implemented");
  } else if (tk == OP_DEC) {
    die("evalopnupostfixtype", "-- not implemented");
  } else {
    nodedump(0, n);
    die("unhandled OP_UN_POSTFIX", TOKNAMES[tk]);
  }
  return ty;
}

struct type *checkconstanttype(struct scope *s, struct node *n) {
  struct type *ty;

  ty = calloc(1, sizeof(struct type));
  ty->arrsz = -1;
  ty->flags = F_INT;
  return ty;
}

struct type *checkstrlittype(struct scope *s, struct node *n) {
  struct type *ty;

  ty = calloc(1, sizeof(struct type));
  ty->arrsz = -1;
  strintpairappend(STRINITS, n->tok->s, n->n);
  ty->flags = F_CHAR;
  ty->plvl = 1;
  return ty;
}

struct type *checktypecasttype(struct scope *s, struct node *n) {
  struct type *ty;

  ty = checkdecltype(n);
  checkexprtype(s, n->right);
  setnodetype(n->right, ty);
  return ty;
}

struct type *checkchartype(void) {
  struct type *ty;

  ty = calloc(1, sizeof(struct type));
  ty->arrsz = -1;
  ty->flags = F_CHAR;
  ty->plvl = 0;
  return ty;
}

struct type *checkexprtype(struct scope *s, struct node *n) {
  struct type *ty;
  int k;

  if (!n) {
    return NULL;
  }
  checkentry("evalexprtype", n);
  k = n->kind;
  if (k == N_CONSTANT) {
    ty = checkconstanttype(s, n);
  } else if (k == VARREF) {
    ty = checkvarreftype(s, n);
  } else if (k == N_STRLIT) {
    ty = checkstrlittype(s, n);
  } else if (k == N_CHAR) {
    ty = checkchartype();
  } else if (k == OP_BIN) {
    ty = checkopbintype(s, n);
  } else if (k == OP_UN) {
    ty = checkopuntype(s, n);
  } else if (k == OP_UN_POSTFIX) {
    ty = checkopunpostfixtype(s, n);
  } else if (k == FUNCALL) {
    ty = checkfuncalltype(s, n);
  } else if (k == EXPR) {
    ty = checkexprtype(s, n->left);
  } else if (k == N_SIZEOF_TYPE) {
    ty = sizeoftype();
    ty->inner = checksizeoftype(n);
  } else if (k == N_SIZEOF_EXPR) {
    ty = sizeoftype();
    ty->inner = checkexprtype(s, n->left);
  } else if (k == ARRAYSUBSCRIPT) {
    struct type *tyl;
    /* FIXME index value should evaluate to int-like */
    checkexprtype(s, n->right);
    tyl = checkexprtype(s, n->left);
    ty = calloc(1, sizeof(struct type));
    memcpy(ty, tyl, sizeof(struct type));
    /* XXX this is a bit suspect... */
    if (ty->arrsz > -1) {
      ty->arrsz = -1;
    } else if (ty->plvl > 0) {
      --ty->plvl;
    }
  } else if (k == INITIALIZER) {
    ty = checkinitializertype(s, n);
  } else if (k == TYPECAST) {
    ty = checktypecasttype(s, n);
  } else if (k == ENUMERATORCONSTANT) {
    ty = enumeratortype();
  } else {
    nodedump(0, n);
    die("unhandled evalexprtype", NODENAMES[k]);
  }
  return setnodetype(n, ty);
}

void dumpfuncdecls(void) {
  struct defpair **cur = FUNCDECLS;
  puts("===== FUNCTION DECLARATIONS =====");
  while (*cur) {
    printf("----- [%s] -----\n", (*cur)->name);
    nodedump(0, (*cur)->def);
    ++cur;
  }
}

void dumpfuncdefs(void) {
  struct defpair **cur = FUNCDEFS;
  puts("===== FUNCTION DEFINITIONS =====");
  while (*cur) {
    printf("----- [%s] -----\n", (*cur)->name);
    nodedump(0, (*cur)->def);
    /*scopedump(getfundefblock((*cur)->def)->scope);*/
    ++cur;
  }
}

void dumptypedefs(void) {
  struct defpair **cur = TYPEDEFS;
  puts("===== TYPEDEFS =====");
  while (*cur) {
    printf("----- [%s] -----\n", (*cur)->name);
    nodedump(0, (*cur)->def);
    ++cur;
  }
}

void dumpenums(void) {
  struct defpair **cur = ENUMS;
  puts("===== ENUMERATIONS =====");
  while (*cur) {
    printf("----- [%s] -----\n", (*cur)->name);
    nodedump(0, (*cur)->def);
    ++cur;
  }
}

void dumpenumrs(void) {
  struct strintpair **cur = ENUMRS;
  puts("===== ENUMERATORS =====");
  while (*cur) {
    printf("----- [%s] -----\n", (*cur)->name);
    printf("%d\n", (*cur)->val);
    ++cur;
  }
}

void dumpstructdefs(void) {
  struct structdef **cur = STRUCTDEFS;
  puts("===== STRUCT DEFINITIONS =====");
  while (*cur) {
    printf("----- [%s] -----\n", (*cur)->name);
    ++cur;
  }
}

int isdeclenum(struct node *n) {
  struct node *d = getdeclarator(n);
  if (!d) {
    return 0;
  }
  return d->left && d->left->right && d->left->right->kind == N_ENUM;
}
void checkenum(struct node *n) {
  char *name = getdeclenumname(n);
  struct node *fe = getdeclfirstenumerator(n);
  if (name) {
    printf("- enum: %s\n", name);
  } else {
    puts("- enum: anonymous");
  }
  if (fe) {
    puts("- enum: definition");
    if (defpairfind(ENUMS, name)) {
      checkerr(n, "enumeration with this name already defined");
    } else {
      struct node *ds = getnodedata(n);
      struct node *cur = getdeclfirstenumerator(n);
      int i = 0;
      while (cur) {
        char *ename = cur->left->tok->s;
        if (cur->right) {
          i = evalconstexpr(cur->right);
        }
        printf("  - enumerator: %s (%d)\n", ename, i);
        if (strintpairfindstr(ENUMRS, ename)) {
          checkerr(cur, "enumerator already defined");
        } else {
          strintpairappend(ENUMRS, ename, i);
        }
        cur = cur->next;
        ++i;
      }
      defpairappend(ENUMS, name, ds);
    }

  } else {
    puts("- enum: not defining");
  }
}

void checkstruct(struct node *n) {
  char *name = getdeclstructname(n);
  if (!name) {
    puts(" - struct: anonymous");
  } else {
    printf("- struct: %s\n", name);
    evalstructdef(n);
  }
}

void checkdecltypes(struct scope *scope, struct node *n) {
  struct node *init;
  struct type *tydecl;
  struct type *tyinit;
  checkentry("checkdecltypes", n);
  /*
   * FIXME our grammar permits a lot of nonsensical types for declarations.
   * this would be a good place to check the types. for now, we're lazy and
   * assume that the declarations are well-typed as long as the initializer
   * expression's type matches the target.
   */
  init = getdeclinitializer(n);
  tydecl = checkdecltype(n);
  if (!init) {
    return;
  }
  tyinit = checkexprtype(scope, init);
  if (!aretypesequal(tydecl, tyinit)) {
    checkerr(n, "declaration and initializer types do not match");
  }
}

void checkstmt(struct scope *, struct node *);
void checkdecl(struct scope *, struct node *, int);

void checkblock(char *name, struct scope *s, struct node *bl) {
  struct node *n;
  _assert(bl->kind == BLOCK, NODENAMES[bl->kind]);
  bl->scope = newscope(name, s, 0);
  n = bl->left;
  while (n) {
    checkdecl(bl->scope, n, 0);
    n = n->next;
  }
  n = bl->right;
  while (n) {
    checkstmt(bl->scope, n);
    n = n->next;
  }
}

void checkwhile(struct scope *s, struct node *n) {
  char *sn;
  asprintf(&sn, "while_%d", n->n);
  checkexprtype(s, getwhilecond(n));
  checkblock(sn, s, getwhileblock(n));
}

void checkcond(struct scope *s, struct node *n) {
  struct node *cur;
  struct node *c;
  char *sn;
  cur = n;
  while (cur) {
    asprintf(&sn, "cond_%d", n->n);
    checkexprtype(s, getcondcond(cur));
    c = getcondblock(cur);
    checkblock(sn, s, c);
    cur = getcondnext(cur);
  }
}

void checkstmt(struct scope *s, struct node *n) {
  int k;
  checkentry("checkstmt", n);
  k = n->kind;
  if (k == N_WHILE) {
    checkwhile(s, n);
  } else if (k == COND) {
    checkcond(s, n);
  } else if (k == N_CONTINUE) {
    puts("FIXME check continue");
  } else if (k == N_BREAK) {
    puts("FIXME check break");
  } else if (k == N_RETURN) {
    checkexprtype(s, n->left);
  } else if (k == EXPR) {
    checkexprtype(s, n);
  } else if (k == N_GOTO) {
    /* FIXME check that there is a local-jumpable label */
    ;
  } else if (k == LABEL) {
    ;
  } else if (k == NOP) {
    ;
  } else {
    die("checkstmt", NODENAMES[k]);
  }
}

int istokassign(int);

struct node *rewriteaddrshift(struct scope *s, struct node *n) {
  struct node *newnode;
  struct node *newnode2;
  struct tok *newtok;
  int tk;

  if (n->scope) {
    s = n->scope;
  }
  if (n->left) {
    n->left = rewriteaddrshift(s, n->left);
  }
  if (n->right) {
    n->right = rewriteaddrshift(s, n->right);
  }
  if (n->next) {
    n->next = rewriteaddrshift(s, n->next);
  }

  if (n->kind == OP_BIN) {
    tk = n->tok->kind;
    if (tk == '.' || tk == OP_PTR) {
      /*
       * rewrite `a.b` as `a.{addrshift(b)}`
       * and     `a->b` as `(*a).{addrshift(b)}`
       */
      struct structdef *sd;
      struct node *deref;
      struct node *cur;
      char *fn;
      char *e;
      int i;

      sd = structdeffind(STRUCTDEFS, n->left->type->structname);
      _assert(sd != NULL, "struct rewriting: cannot find struct definition");
      fn = n->right->tok->s;
      i = structdeffieldoffset(sd, fn);
      _assert(i != -1, "struct rewriting: cannot find field offset");

      cur = n->left;
      while (cur && (cur->kind == OP_UN || cur->kind == ADDRSHIFT ||
                     cur->kind == ARRAYSUBSCRIPT || cur->kind == VARREF)) {
        if (cur->kind == VARREF) {
          cur->kind = VARREFADDR;
          break;
        } else if (cur->kind == ARRAYSUBSCRIPT) {
          cur->kind = ARRAYSUBSCRIPTADDR;
          break;
        }
        cur = cur->left;
      }

      newnode2 = node(ADDRSHIFT, n->tok);
      if (tk == OP_PTR) {
        newtok = copytok(n->tok);
        newtok->kind = '*';
        newnode = node(OP_UN, newtok);
        newnode->type = calloc(1, sizeof(struct type));
        memcpy(newnode->type, n->left->type, sizeof(struct type));
        newnode->type->plvl -= 1;
        newnode->left = n->left;
        newnode2->left = newnode;
      } else {
        newnode2->left = n->left;
      }
      newnode2->right = n->right;
      newnode2->i = i;
      newnode2->type = n->type;
      newnode2->next = n->next;

      n = newnode2;
    }
  }
  return n;
}

struct node *rewriteaddrshiftvalue(struct scope *s, struct node *n,
                                   int islvalue) {
  int foundlvalue;
  int tk;

  if (n->next) {
    n->next = rewriteaddrshiftvalue(s, n->next, islvalue);
  }

  tk = n->tok->kind;
  foundlvalue = istokassign(n->tok->kind) ||
                (n->kind == OP_UN && (tk == OP_INC || tk == OP_DEC));
  if (foundlvalue) {
    islvalue = 1;
  }
  if (n->right) {
    if (foundlvalue) {
      n->right = rewriteaddrshiftvalue(s, n->right, 0);
    } else {
      n->right = rewriteaddrshiftvalue(s, n->right, islvalue);
    }
  }
  /*
   * Troubling business. This is due to funcall return values not needing so
   * many redirections.
   */
  if (!islvalue && n->kind == ADDRSHIFT && (n->left->kind != FUNCALL) &&
      (!n->left->left || n->left->left->kind != FUNCALL)) {
    n->kind = ADDRSHIFTVALUE;
    return n;
  }
  if (islvalue || n->kind == ADDRSHIFTVALUE) {
    return n;
  }
  if (n->scope) {
    s = n->scope;
  }
  if (n->left) {
    n->left = rewriteaddrshiftvalue(s, n->left, islvalue);
  }

  return n;
}

struct node *rewritelvalue(struct scope *s, struct node *n, int islvalue) {
  int foundlvalue;

  foundlvalue = istokassign(n->tok->kind);
  if (foundlvalue) {
    islvalue = 1;
  }

  if (foundlvalue && n->kind == OP_BIN) {
    struct node *cur;
    cur = n->left;
    while ((cur->kind == OP_UN && cur->tok->kind == '*') ||
           cur->kind == VARREF || cur->kind == OP_BIN) {
      if (cur->kind == VARREF) {
        cur->kind = VARREFADDR;
        return n;
      }
      cur = cur->left;
    }
  } else if (islvalue && n->kind == ARRAYSUBSCRIPT) {
    n->kind = ARRAYSUBSCRIPTADDR;
    return n;
  } else if (islvalue && n->kind == OP_UN && n->tok->kind == '*') {
    n->kind = OP_UN_LVALUE;
    return n;
  }

  if (n->scope) {
    s = n->scope;
  }
  if (n->left) {
    n->left = rewritelvalue(s, n->left, islvalue);
  }
  if (n->right) {
    if (foundlvalue) {
      n->right = rewritelvalue(s, n->right, 0);
    } else {
      n->right = rewritelvalue(s, n->right, islvalue);
    }
  }
  if (n->next) {
    n->next = rewritelvalue(s, n->next, islvalue);
  }

  return n;
}

struct node *rewriteincdec(struct scope *s, struct node *n, int islvalue) {
  int foundlvalue;

  foundlvalue = istokassign(n->tok->kind);
  if (foundlvalue) {
    islvalue = 1;
  }

  if (n->scope) {
    s = n->scope;
  }
  if (n->left) {
    n->left = rewriteincdec(s, n->left, islvalue);
  }
  if (n->right) {
    n->right = rewriteincdec(s, n->right, islvalue);
  }
  if (n->next) {
    n->next = rewriteincdec(s, n->next, islvalue);
  }

  /* XXXXX POSTFIX TOO AND LVALUE */
  if (n->kind == OP_UN) {
    int tk;

    tk = n->tok->kind;
    if ((tk == OP_INC || tk == OP_DEC) && n->left->kind == VARREF) {
      n->left->kind = VARREFADDR;
    }
  }
  return n;
}

struct node *rewriteenumerators(struct scope *s, struct node *n) {
  if (n->scope) {
    s = n->scope;
  }
  if (n->left) {
    n->left = rewriteenumerators(s, n->left);
  }
  if (n->right) {
    n->right = rewriteenumerators(s, n->right);
  }
  if (n->next) {
    n->next = rewriteenumerators(s, n->next);
  }

  if (n->kind == VARREF) {
    struct scopevar *sv;
    char *name;

    name = n->tok->s;
    sv = scopefindvar(s, name);
    if (!sv) {
      struct strintpair *sip;

      sip = strintpairfindstr(ENUMRS, name);
      if (sip) {
        n->kind = ENUMERATORCONSTANT;
        /* I know. */
        n->tok->i = sip->val;
      }
    }
  }
  return n;
}

struct node *rewritecasts(struct scope *s, struct node *n) {
  if (n->kind == TYPECAST) {
    return n->right;
  }
  if (n->scope) {
    s = n->scope;
  }
  if (n->left) {
    n->left = rewritecasts(s, n->left);
  }
  if (n->right) {
    n->right = rewritecasts(s, n->right);
  }
  if (n->next) {
    n->next = rewritecasts(s, n->next);
  }
  return n;
}

void checkdecl(struct scope *, struct node *, int);

void checkfunc(char *name, struct scope *global, struct node *f) {
  struct node *bl = getfundefblock(f);
  struct node **np;
  struct node *n;
  checkentry("checkfunc", f);
  /*
   * The idea is that the function scope will begin with the ABI-specified
   * shifts for parameters and stack values, respectively. Elsewhere, when we
   * create new scopes for new blocks, we should initialize them with the
   * parent scope's shifts. Thus the function shifts will propagate. Of course
   * only the stack shift `sshift` is really meaningful for nested scopes...
   */
  bl->scope = newscope(name, global, 1);
  puts("----- [function return value] -----");
  if (f->data) {
    nodedump(0, ((struct node *)f->data)->left);
  }
  puts("----- [function arguments] -----");
  n = f->left->right->left;
  while (n) {
    nodedump(0, n);
    checkdecl(bl->scope, n, 1);
    n = n->next;
  }
  puts("----- [function declarations] -----");
  np = &bl->left;
  while (*np) {
    nodedump(0, *np);
    checkdecl(bl->scope, *np, 0);
    *np = rewriteenumerators(bl->scope, *np);
    np = &(*np)->next;
  }
  puts("----- [function statements] -----");
  n = bl->right;
  while (n) {
    printf("---- statement --- %s\n", NODENAMES[n->kind]);
    checkstmt(bl->scope, n);
    n = n->next;
  }
  /*
   * We do some crude lowering here for certain parts of the language.
   */
  np = &bl->right;
  puts("----- [lowering function statements] -----");
  while (*np) {
    *np = rewritelvalue(bl->scope, *np, 0);
    *np = rewriteaddrshift(bl->scope, *np);
    *np = rewriteaddrshiftvalue(bl->scope, *np, 0);
    *np = rewriteincdec(bl->scope, *np, 0);
    *np = rewriteenumerators(bl->scope, *np);
    *np = rewritecasts(bl->scope, *np);
    np = &(*np)->next;
  }
}

void checkdecl(struct scope *scope, struct node *n, int isparam) {
  struct node *t;
  int instack;
  if (!scope->parent) {
    instack = 0;
  } else {
    instack = 1;
  }
  if (n->kind == DECLARATION || n->kind == FUNARGDEF) {
    if (isdeclfundef(n)) {
      char *name = getdeclfuncname(n);
      printf("- function definition: %s\n", name);
      if (defpairfind(FUNCDEFS, name)) {
        checkerr(n, "function with this name already defined");
      } else {
        defpairappend(FUNCDEFS, name, n);
      }
      checkfunc(name, scope, n);
    } else if (isdeclfundecl(n)) {
      if (n->left->next) {
        int plvl = 0;
        struct node *cur = n->left->next;
        struct node *id = NULL;
        while (cur) {
          struct nodedeclarator *nd = getnodedeclarator(cur);
          plvl += nd->plvl;
          if (!id && cur->left && cur->left->kind == N_ID) {
            id = cur->left;
          }
          cur = cur->next;
        }
        printf("- function pointer: %s (plvl=%d)\n", id->tok->s, plvl);
        /* FIXME Bad way of checking whether we're in TU scope */
        scopeappendvar(scope, id->tok->s, instack, isparam, 0, n);
      } else {
        char *name = getdeclfuncname(n);
        printf("- function declaration: %s\n", name);
        if (defpairfind(FUNCDECLS, name)) {
          /*
           * FIXME multiple declarations are fine as long as they don't
           * conflict
           */
          checkerr(n, "function with this name already declared");
        } else {
          defpairappend(FUNCDECLS, name, n);
        }
      }
    } else if (isdeclvar(n) && !isdecltypedef(n)) {
      int isextern;

      puts("- variable");
      if (isdecltypedefvar(n)) {
        puts("  - typedef'd");
      }
      if (isdeclenum(n)) {
        checkenum(n);
      }
      if ((t = getdeclinitializer(n))) {
        struct type *ty;
        puts("  - initialized");
        ty = checkdecltype(n);
        typedump(ty);
        /* FIXME Generalize this with the other place. */
        if (ty->plvl > 0) {
          if (ty->flags & F_CHAR && t->kind == N_STRLIT) {
            strintpairappend(STRINITS, t->tok->s, n->n);
          }
        } else if (ty->arrsz > -1) {
          checkwarn(n, "checkdecl: dunno how to handle array initializer");
        } else {
          /*
           * XXX Should we do something here for non-arrays and non-pointers??
           */
        }
      }

      isextern = getspecsflags(n->data) & F_EXTERN;
      scopeappendvar(scope, getdeclvarname(n), instack, isparam, isextern, n);
    } else if (isdeclstruct(n)) {
      checkstruct(n);
    } else if (isdeclenum(n)) {
      checkenum(n);
    } else if (isdecltypedef(n)) {
      puts("- typedef");
    } else {
      checkerr(n, "unknown declaration");
    }
    checkdecltypes(scope, n);
  }
}

char *escapestrlit(char *s, int ischarlit) {
  char *ret;
  char *src;
  char *dst;

  ret = calloc(1, strlen(s) * 2 + 1);
  src = s;
  dst = ret;
  /* FIXME generalize this */
  while (*src) {
    if (*src == '\n') {
      *dst = '\\';
      ++dst;
      *dst = 'n';
    } else if (*src == '"') {
      *dst = '\\';
      ++dst;
      *dst = '"';
    } else if (*src == '\\') {
      *dst = '\\';
      ++dst;
      *dst = '\\';
    } else if (ischarlit && *src == '\'') {
      *dst = '\\';
      ++dst;
      *dst = '\'';
    } else {
      *dst = *src;
    }
    ++src;
    ++dst;
  }
  puts(ret);
  return ret;
}

void emit(char *str) {
  if (!str) {
    return;
  }
  fwrite(str, 1, strlen(str), GEN);
  printf("emitted: %s\n", str);
}

void emitln(char *str) {
  if (!str) {
    return;
  }
  emit(str);
  fwrite("\n", 1, 1, GEN);
}

void emitpush(char *what) {
  emit("push ");
  emitln(what);
}

void emitpop(char *what) {
  emit("pop ");
  emitln(what);
}

int getnodevarsize(struct node *n) {
  return typesize(checkdecltype(n));
}

void emitexpr(struct scope *, struct node *);

void emitvarsnoinit(void) {
  struct scopevar *var;
  struct node *n;
  int flags;
  char *e;
  /*
   * NB: We don't handle external linkage properly. Same applies to
   * emitvarsinit below.
   */
  var = tuscope.vars;
  while (var) {
    if (var->this) {
      flags = getspecsflags(var->this->data);
      if (flags & F_EXTERN) {
        var = var->next;
        continue;
      }
    }
    n = getdeclinitializer(var->this);
    if (n) {
      var = var->next;
      continue;
    }
    emit("# variable: ");
    emitln(var->name);
    asprintf(&e, ".lcomm __node_%d, %d", var->this->n, var->size);
    emitln(e);
    free(e);
    var = var->next;
  }
}

void emitvarsinit(void) {
  struct strintpair **nodeinit;
  struct scopevar *var;
  struct defpair **func;
  struct node *init;
  struct node *n;
  struct type *ty;
  char *e;

  nodeinit = STRINITS;
  puts(">> emitting string initializers");
  while (*nodeinit) {
    asprintf(&e, "__node_%d: .asciz \"%s\"", (*nodeinit)->val,
             escapestrlit((*nodeinit)->name, 0));
    emitln(e);
    free(e);
    ++nodeinit;
  }

  var = tuscope.vars;
  while (var) {
    init = getdeclinitializer(var->this);
    if (!init) {
      var = var->next;
      continue;
    }
    ty = checkdecltype(var->this);
    if (ty->arrsz > -1) {
      struct node *cur;
      int atomsz;
      char *dve;
      int n = 0;

      emitln("# array initializer");
      atomsz = typeatomsize(ty);
      /* MAYBEFIXME does not support struct initializers */
      if (atomsz == 1) {
        dve = ".byte";
      } else {
        dve = ".int";
      }
      asprintf(&e, "__node_%d: %s ", var->this->n, dve);
      emit(e);
      free(e);
      cur = var->this->right;
      _assert(cur->kind == INITIALIZERARRAY, NODENAMES[cur->kind]);
      cur = cur->left;
      while (cur) {
        int tk;

        tk = cur->left->kind;
        if (tk == N_CONSTANT) {
          asprintf(&e, "%ld", cur->left->tok->i);
        } else if (tk == VARREF) {
          asprintf(&e, "%s", cur->left->tok->s);
        } else if (tk == N_STRLIT) {
          asprintf(&e, "__node_%d", cur->left->n);
        } else if (tk == OP_UN && cur->left->left->kind == N_CONSTANT) {
          asprintf(&e, "-%ld", cur->left->left->tok->i);
        } else if (tk == N_CHAR) {
          char c[2];

          c[0] = cur->left->tok->c;
          c[1] = 0;
          asprintf(&e, "'%s'", escapestrlit(c, 1));
        } else {
          die("emitvarsinit unsupported array initializer",
              NODENAMES[cur->left->kind]);
        }
        emit(e);
        free(e);
        ++n;
        if (n != ty->arrsz) {
          emit(", ");
        }
        cur = cur->next;
      }
      while (n != ty->arrsz) {
        emit("0");
        ++n;
        if (n != ty->arrsz) {
          emit(", ");
        }
      }
      emitln("");
    } else if (ty->plvl > 0 && ty->flags & F_CHAR && init->kind == N_STRLIT) {
      /* has been emitted above from STRINITS */
      var = var->next;
      continue;
    } else if (init->kind == VARREF) {
      struct scopevar *iv;

      iv = scopefindvar(&tuscope, init->tok->s);
      emit("# variable initialized from another variable: ");
      emitln(iv->name);
      asprintf(&e, "__node_%d: .long __node_%d", var->this->n, iv->this->n);
      emitln(e);
      free(e);
    } else {
      asprintf(&e, "__node_%d: .long %d", var->this->n, evalconstexpr(init));
      emitln(e);
      free(e);
    }
    var = var->next;
  }
}

void emitvaraddr(struct scopevar *var) {
  char *e;
  emit("# varaddr=");
  emitln(var->name);
  if (!var->instack) {
    asprintf(&e, "lea __node_%d, %%eax", var->this->n);
  } else {
    asprintf(&e, "lea %d(%%ebp), %%eax", var->shift);
  }
  emitln(e);
  free(e);
  emitpush("%eax");
}

void emitenumeratorconstant(struct node *n) {
  struct strintpair *sip;
  char *name;
  char *e;

  name = n->tok->s;
  sip = strintpairfindstr(ENUMRS, name);
  if (!sip) {
    die("emitenumeratorconstant cannot find", name);
  }
  asprintf(&e, "$%d", sip->val);
  emitpush(e);
  free(e);
}

void emitvarrefaddr(struct scope *s, struct node *n) {
  struct scopevar *var;
  char *e = NULL;
  int i = 0;

  emitln("# varrefaddr");
  while (n->kind == OP_UN && n->tok->kind == '*') {
    ++i;
    n = n->left;
  }
  var = scopefindvar(s, n->tok->s);
  if (!var) {
    die("emitvarref: cannot find scopevar", n->tok->s);
  }
  emitvaraddr(var);
  while (i > 0) {
    emitpop("%eax");
    emitpush("(%eax)");
    --i;
  }
}

void emitvarref(struct scope *s, struct node *n) {
  struct scopevar *var;
  char *nodename;
  char *name;
  char *dst;
  char *op;
  char *e;

  emit("# varref=");
  if (n->type->arrsz > -1) {
    emitvarrefaddr(s, n);
    return;
  }
  name = n->tok->s;
  emitln(name);
  var = scopefindvar(s, name);
  if (!var) {
    puts(n->tok->s);
    nodedump(0, n);
    die("emitvarref", "cannot find scopevar");
  }
  /*
   * This specific piece of jank is a result of treating character pointers
   * into string literals as array-like data in the `data` section. In an
   * earlier phase, we could (should) mark that this specific variable
   * actually is a string literal, but now it's here.
   */
  if (var->isextern) {
    nodename = strdup(name);
  } else {
    asprintf(&nodename, "__node_%d", var->this->n);
  }
  /* This is fine because we only support values sized 1 or 4 bytes. */
  if (typesize(n->type) == 1 && n->type->plvl == 0) {
    op = "movb";
    dst = "%al";
    emitln("xor %eax, %eax");
  } else {
    op = "movl";
    dst = "%eax";
  }
  if (!var->instack && var->this && var->this->right &&
      var->this->right->left && var->this->right->left->kind == N_STRLIT) {
    asprintf(&e, "lea %s, %s", nodename, dst);
  } else if (!var->instack) {
    asprintf(&e, "%s %s, %s", op, nodename, dst);
  } else {
    asprintf(&e, "%s %d(%%ebp), %s", op, var->shift, dst);
  }
  emitln(e);
  free(e);
  free(nodename);
  emitpush("%eax");
}

int istokassign(int k) {
  return k == '=' || k == ASSIGN_ADD || k == ASSIGN_AND || k == ASSIGN_LEFT ||
         k == ASSIGN_OR || k == ASSIGN_MOD || k == ASSIGN_MUL ||
         k == ASSIGN_RIGHT || k == ASSIGN_SUB || k == ASSIGN_XOR;
}

int istokcmp(int k) {
  return k == OP_EQ || k == OP_NE || k == '<' || k == '>' || k == OP_GE ||
         k == OP_LE;
}

void emitaddrof(struct scope *s, struct node *n) {
  struct scopevar *var;
  char *e;
  int tk;

  emitln("# addrof");
  tk = n->left->kind;
  if (tk == VARREF) {
    var = scopefindvar(s, n->left->tok->s);
    if (!var) {
      puts(n->tok->s);
      nodedump(0, n);
      die("emitaddrof", "cannot find scopevar");
    }
    if (!var->instack) {
      asprintf(&e, "lea __node_%d, %%eax", var->this->n);
    } else {
      asprintf(&e, "lea %d(%%ebp), %%eax", var->shift);
    }
    emitln(e);
    free(e);
    emitpush("%eax");
  } else if (tk == ADDRSHIFTVALUE) {
    n->left->kind = ADDRSHIFT;
    emitexpr(s, n->left);
  } else {
    die("addrof does not how to handle", NODENAMES[n->left->kind]);
  }
}

void emitderef(struct scope *s, struct node *n, int lvalue) {
  struct scopevar *var;
  int sz;

  emitln("# deref(*) ->");
  emitexpr(s, n->left);
  emitpop("%eax");
  sz = typesize(n->type);
  emitpush("(%eax)");
  if (!lvalue && sz == 1) {
    emitln("andl $0x000000ff, (%esp)");
  }
  emitln("# deref(*) <-");
}

void emitopbin(struct scope *s, struct node *n) {
  struct scopevar *var;
  char *e;
  int tk;

  puts("emitopbin");
  tk = n->tok->kind;
  if (istokassign(tk)) {
    char *src;
    char *op;
    int sz;

    emitln("# binop (assignment)");
    /* XXX We probably don't verify lvalueness in check stage */
    emitln("# right subtree");
    emitexpr(s, n->right);
    emitln("# left subtree");
    emitexpr(s, n->left);
    emitln("# assigning...");
    emitpop("%eax");
    emitpop("%ebx");
    sz = typesize(n->left->type);
    /* FIXME some repetition here */
    if (tk == '=') {
      if (sz == 1) {
        op = "movb";
        src = "%bl";
      } else {
        op = "movl";
        src = "%ebx";
      }
    } else if (tk == ASSIGN_ADD) {
      if (sz == 1) {
        op = "addb";
        src = "%bl";
      } else {
        op = "addl";
        src = "%ebx";
      }
    } else if (tk == ASSIGN_SUB) {
      if (sz == 1) {
        op = "subb";
        src = "%bl";
      } else {
        op = "subl";
        src = "%ebx";
      }
    } else if (tk == ASSIGN_OR) {
      if (sz == 1) {
        op = "orb";
        src = "%bl";
      } else {
        op = "orl";
        src = "%ebx";
      }
    } else {
      die("emitopbin has an unhandled assignment op", TOKNAMES[tk]);
    }
    asprintf(&e, "%s %s, (%%eax)", op, src);
    emitln(e);
    free(e);
    emitpush("(%eax)");
    if (sz == 1) {
      emitln("andl $0x000000ff, (%esp)");
    }
  } else if (istokcmp(tk)) {
    char *num;

    emitln("# cmp");
    asprintf(&num, "%d", n->n);
    emitexpr(s, n->right);
    emitexpr(s, n->left);
    emitpop("%eax");
    emitpop("%ebx");
    emitln("cmpl %ebx, %eax");
    if (tk == '<') {
      emit("jl __cmp_true_");
    } else if (tk == '>') {
      emit("jg __cmp_true_");
    } else if (tk == OP_EQ) {
      emit("je __cmp_true_");
    } else if (tk == OP_NE) {
      emit("jne __cmp_true_");
    } else if (tk == OP_GE) {
      emit("jge __cmp_true_");
    } else if (tk == OP_LE) {
      emit("jle __cmp_true_");
    } else {
      die("emitexpr", TOKNAMES[tk]);
    }
    emitln(num);
    emitpush("$0");
    emit("jmp __cmp_end_");
    emitln(num);
    emit("__cmp_true_");
    emit(num);
    emitln(":");
    emitpush("$1");
    emit("__cmp_end_");
    emit(num);
    emitln(":");
  } else {
    struct structdef *sd;
    int flags1;
    int flags2;
    int coeff1;
    int coeff2;
    int plvl1;
    int plvl2;

    /*
     * FIXME
     *
     * So since we can have pointer arithmetic here, we should apply the partial
     * rules below for all cases. This is now intentionally mostly unimplemented
     * -- there should be just enough to fulfill our goal. This should work with
     * the pointer cases we have here but will for sure fail with arrays and
     * other non-trivial expressions.
     */
    plvl1 = n->left->type->plvl;
    plvl2 = n->right->type->plvl;
    flags1 = n->left->type->flags;
    flags2 = n->right->type->flags;

    coeff1 = 1;
    coeff2 = 1;
    if ((plvl1 > 0 && plvl2 == 0) && (flags1 & F_STRUCT && flags2 & F_INT)) {
      sd = structdeffind(STRUCTDEFS, n->left->type->structname);
      coeff2 = sd->memsz;
    } else if ((plvl1 == 0 && plvl2 > 0) &&
               (flags1 & F_INT && flags2 & F_STRUCT)) {
      sd = structdeffind(STRUCTDEFS, n->right->type->structname);
      coeff1 = sd->memsz;
    }

    emitln("# binop (non-assigment)");
    typedump(n->left->type);
    typedump(n->right->type);

    emitexpr(s, n->left);
    tk = n->tok->kind;
    if (tk == '-') {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("subl %ebx, %eax");
    } else if (tk == '+') {
      emitexpr(s, n->right);
      emitpop("%ebx");
      if (coeff2 != 1) {
        asprintf(&e, "imul $%d, %%ebx", coeff2);
        emitln(e);
        free(e);
      }
      emitpop("%eax");
      if (coeff1 != 1) {
        asprintf(&e, "imul $%d, %%eax", coeff1);
        emitln(e);
        free(e);
      }
      emitln("addl %ebx, %eax");
    } else if (tk == '*') {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("imul %ebx, %eax");
    } else if (tk == '/') {
      /* I know. */
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("xorl %edx, %edx\nidiv %ebx");
    } else if (tk == '&') {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("andl %ebx, %eax");
    } else if (tk == '|') {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("orl %ebx, %eax");
    } else if (tk == '^') {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("xorl %ebx, %eax");
    } else if (tk == OP_LEFT) {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("movb %bl, %cl\nshl %cl, %eax");
    } else if (tk == OP_RIGHT) {
      emitexpr(s, n->right);
      emitpop("%ebx");
      emitpop("%eax");
      emitln("movb %bl, %cl\nshr %cl, %eax");
    } else if (tk == OP_AND) {
      emitpop("%eax");
      emitln("cmp $0, %eax");
      asprintf(&e, "je __boolean_and_end_%d", n->n);
      emitln(e);
      free(e);
      emitexpr(s, n->right);
      emitpop("%eax");
      emitln("cmp $0, %eax");
      asprintf(&e, "je __boolean_and_end_%d", n->n);
      emitln(e);
      free(e);
      emitln("mov $1, %eax");
      asprintf(&e, "__boolean_and_end_%d:", n->n);
      emitln(e);
      free(e);
    } else if (tk == OP_OR) {
      emitpop("%eax");
      emitln("cmp $0, %eax");
      asprintf(&e, "jne __boolean_or_end_%d", n->n);
      emitln(e);
      free(e);
      emitexpr(s, n->right);
      emitpop("%eax");
      emitln("cmp $0, %eax");
      asprintf(&e, "jne __boolean_or_end_%d", n->n);
      emitln(e);
      free(e);
      emitln("movl $0, %eax");
      asprintf(&e, "__boolean_or_end_%d:", n->n);
      emitln(e);
      free(e);
    } else {
      printf("emitexpr: XXXX UNRECOGNIZED OP_BIN: %s\n", TOKNAMES[tk]);
      abort();
    }
    emitpush("%eax");
  }
}

void emitfuncall(struct scope *s, struct node *n) {
  /* I know. */
  struct node *rev[50];
  struct node *cur;
  struct node *arg;
  int argsz;
  int vars;
  char *e;
  int i;

  emitln("# funcall");
  memset(rev, 0, 50 * sizeof(struct node *));
  emitpush("%eax");
  emitpush("%ecx");
  emitpush("%edx");

  vars = 0;
  arg = getfuncallfirstarg(n);
  argsz = 0;
  while (arg) {
    rev[vars] = arg;
    ++vars;
    argsz += 4;
    arg = arg->next;
  }

  /*
   * Now that we know how much the stack will grow after we push our parameters,
   * we may probe and calculate how misaligned the stack pointer is from the
   * requirement of 16 bytes.
   */
  emitln("# %esp + argsz");
  emitln("mov %esp, %eax");
  asprintf(&e, "sub $%d, %%eax", argsz);
  emitln(e);
  free(e);
  emitln("# %esp remainder");
  emitln("mov $16, %ebx");
  emitln("xorl %edx, %edx");
  emitln("idiv %ebx");
  /*
   * At this point, we have the amount of misalignment in %edx. We will push it
   * to the stack with the four bytes of the push itself subtracted. Note the
   * lazy trick with adding 16 to the remainder so actually rid ourselves of the
   * problem that if the remainder is 0, then we would have to deal with -4...
   * We'll waste 16 bytes of stack space for each call.
   */
  emitln("add $16, %edx");
  emitln("sub $4, %edx");
  emitln("sub %edx, %esp");
  emitpush("%edx");

  if (vars > 0) {
    i = vars - 1;
    while (i >= 0) {
      emitln("# funarg ->");
      emitexpr(s, rev[i]->left);
      emitln("# funarg <-");
      --i;
    }
  }

  emit("call ");
  if (n->left->kind == VARREF) {
    struct defpair *f;
    struct type *ty;
    char *name = n->tok->s;
    emitln(name);
    f = defpairfind(FUNCDEFS, name);
    if (!f) {
      f = defpairfind(FUNCDECLS, name);
      if (!f) {
        printf("emitexpr: cannot find defn/decl for function %s\n", name);
        abort();
      }
    }
    ty = checkdecltype(f->def);
    if (ty->flags & F_STRUCT) {
      if (ty->plvl == 0) {
        die("emitexpr", "we do not know how to handle struct as a return "
                        "value "
                        "yet");
      }
    }
    emitln("mov %eax, %esi");
    /*
     * ANSI C permits returning structs by values, whereas our ABI says that
     * the return value is in %eax. Many ways to solve this, but small
     * structs can then be returned in the register directly, however
     * something else has to be cooked up for larger structs. Alternatively
     * we may decide to not support it.
     */
  } else {
    /* FIXME This is probably function pointer. */
    nodedump(0, n->left);
    die("emitexpr", "cannot handle this kind of a FUNCALL");
  }
  asprintf(&e, "add $%d, %%esp", argsz);
  emitln(e);
  free(e);
  /*
   * Reverse the stack alignment.
   */
  emitpop("%edx");
  emitln("add %edx, %esp");
  emitpop("%edx");
  emitpop("%ecx");
  emitpop("%eax");
  emitpush("%esi");
}

void emitarraysubscript(struct scope *s, struct node *n) {
  struct scopevar *sv;
  int atomsz;
  char *vn;
  char *e;
  int i;

  emit("# array subscript ->");
  if (n->left->kind == VARREF || n->left->kind == VARREFADDR) {
    struct type *ty;

    vn = n->left->tok->s;
    emitln(vn);
    sv = scopefindvar(s, vn);
    _assert(sv != NULL, vn);
    if (n->left->kind == VARREF) {
      emitvaraddr(sv);
    } else {
      emitexpr(s, n->left);
    }
    ty = n->left->type;
    /* Arrays are not pointers in this sense. */
    if (ty->arrsz == -1 && ty->plvl > 0) {
      emitpop("%eax");
      emitpush("(%eax)");
    }
  } else if (n->left->kind == ADDRSHIFT || n->left->kind == ADDRSHIFTVALUE) {
    /*
     * This is a failure of the rewrites for struct accesses. We need the
     * address of the resulting struct access, but it probably is rewritten as
     * ADDRSHIFTVALUE.
     */
    n->left->kind = ADDRSHIFT;
    emitexpr(s, n->left);
  } else {
    die("emitarraysubscript cannot handle", NODENAMES[n->left->kind]);
  }
  atomsz = typeatomsize(n->type);
  emitln("# array subscript index expression ->");
  emitexpr(s, n->right);
  emitln("# array subscript index expression <-");
  emitln("# array subscript index multiply with atomsz ->");
  if (atomsz != 1) {
    emitpop("%eax");
    asprintf(&e, "imul $%d, %%eax", atomsz);
    emitln(e);
    free(e);
    emitpush("%eax");
  }
  emitln("# array subscript index multiply with atomsz <-");
  emitln("# array subscript address + offset + to stack ->");
  emitpop("%ebx");
  emitpop("%eax");
  emitln("addl %ebx, %eax");
  emitpush("%eax");
  if (n->kind == ARRAYSUBSCRIPT) {
    emitpop("%eax");
    emitpush("(%eax)");
  }
  emitln("# array subscript address + offset + to stack <-");
}

void emitsizeoftype(struct scope *s, struct node *n) {
  struct nodedeclarator *nd;
  struct node *d;
  int atomsz;
  int sz;
  char *e;

  emitln("# sizeof type");
  d = n->left;
  _assert(d != NULL, "sizeof lacks declarator");
  nd = getnodedeclarator(d);
  _assert(nd != NULL, "sizeof lacks nodedeclarator");
  atomsz = typeatomsize(n->type->inner);
  if (nd->arrsz < 1) {
    emitln("# sizeof atom");
    sz = atomsz;
  } else {
    emitln("# sizeof array");
    sz = n->type->inner->arrsz * atomsz;
  }
  asprintf(&e, "$%d", sz);
  emitpush(e);
  free(e);
}

void emitsizeofexpr(struct scope *s, struct node *n) {
  char *e;

  emitln("# sizeof expr");
  asprintf(&e, "$%d", typesize(n->type->inner));
  emitpush(e);
  free(e);
}

void emitincdec(struct scope *s, struct node *n) {
  int plvl;
  char *op;
  char *e;
  int sz;
  int tk;
  tk = n->tok->kind;
  /*
   * For prefix ++ and -- things are easy -- we just modify them right
   * now. See nearby for postfix versions which requires something
   * different.
   */
  /* FIXME this will not work for chained inc/dec */

  if (tk == OP_INC) {
    emitln("# prefix ++");
    op = "addl";
  } else {
    emitln("# prefix --");
    op = "subl";
  }
  emitexpr(s, n->left);
  emitpop("%eax");
  /*
   * This sort of works, but it looks like it *barely* works.
   */
  if (n->left->type->plvl == 0 && n->left->type->flags & F_INT) {
    sz = 1;
  } else if (n->left->type->plvl == 1 && n->left->type->flags & F_CHAR) {
    sz = 1;
  } else if (n->left->type->plvl == 1 && n->left->type->flags & F_STRUCT) {
    struct structdef *sd;

    sd = structdeffind(STRUCTDEFS, n->left->type->structname);
    sz = sd->memsz;
  } else {
    sz = 4;
  }
  asprintf(&e, "%s $%d, (%%eax)", op, sz);
  emitln(e);
  free(e);
  emitpush("(%eax)");
}

void emitnot(struct scope *s, struct node *n) {
  emitexpr(s, n->left);
  emitln("# !");
  emitpop("%eax");
  emitln("cmpl $0, %eax");
  emitln("sete %al");
  emitln("andl $0xff, %eax");
  emitpush("%eax");
}

void emitexpr(struct scope *s, struct node *n) {
  struct scopevar *var;
  char *str;
  int k;
  puts("emitexpr");
  k = n->kind;
  /* This could of course be a jump table. */
  if (k == FUNCALL) {
    emitfuncall(s, n);
  } else if (k == VARREF) {
    emitvarref(s, n);
  } else if (k == VARREFADDR) {
    emitvarrefaddr(s, n);
  } else if (k == OP_BIN) {
    emitopbin(s, n);
  } else if (k == N_CONSTANT) {
    emitln("# constant");
    asprintf(&str, "$%ld", n->tok->i);
    emitpush(str);
    free(str);
  } else if (k == N_STRLIT) {
    emitln("# strlit");
    asprintf(&str, "lea __node_%d, %%eax", n->n);
    emitln(str);
    free(str);
    emitpush("%eax");
  } else if (k == N_CHAR) {
    char c[2];

    c[0] = n->tok->c;
    c[1] = 0;
    emitln("# char");
    asprintf(&str, "$'%s", escapestrlit(c, 1));
    emitpush(str);
    free(str);
  } else if (k == OP_UN) {
    int tk;
    tk = n->tok->kind;
    emitln("# unop");
    if (tk == '&') {
      emitaddrof(s, n);
    } else if (tk == '*') {
      emitderef(s, n, 0);
    } else if (tk == '+') {
      /* effectively no op */
      emitexpr(s, n->left);
    } else if (tk == OP_INC || tk == OP_DEC) {
      emitincdec(s, n);
    } else if (tk == '!') {
      emitnot(s, n);
    } else if (tk == '-') {
      emitexpr(s, n->left);
      emitln("# unary -");
      emitln("negl (%esp)");
    } else if (tk == '~') {
      emitexpr(s, n->left);
      emitln("# ~");
      emitln("notl (%esp)");
    } else {
      die("emitexpr unop", TOKNAMES[tk]);
    }
  } else if (k == OP_UN_LVALUE) {
    int tk;
    tk = n->tok->kind;
    emitln("# unop lvalue");
    if (tk == '*') {
      emitderef(s, n, 1);
    } else {
      die("emitexpr unop lvalue", TOKNAMES[tk]);
    }
  } else if (k == OP_UN_POSTFIX) {
    int tk;
    tk = n->tok->kind;
    emitln("# unop postfix");
    if (tk == OP_INC) {
      emitln("# postfix ++");
      die("postfix ++", "not implemented");
    } else if (tk == OP_DEC) {
      emitln("# postfix --");
      die("postfix --", "not implemented");
    } else {
      die("unrecognized unary postfix op", TOKNAMES[tk]);
    }
  } else if (k == ADDRSHIFT || k == ADDRSHIFTVALUE) {
    emitln("# addrshift address ->");
    emitexpr(s, n->left);
    emitln("# addrshift address <-");
    if (n->i > 0) {
      emitln("# addrshift offset ->");
      emitpop("%eax");
      asprintf(&str, "add $%d, %%eax", n->i);
      emitln(str);
      free(str);
      emitpush("%eax");
      emitln("# addrshift offset <-");
    }
    if (k == ADDRSHIFTVALUE) {
      emitln("# addrshift value ->");
      emitpop("%eax");
      emitpush("(%eax)");
      emitln("# addrshift value <-");
    }
  } else if (k == N_SIZEOF_TYPE) {
    emitsizeoftype(s, n);
  } else if (k == N_SIZEOF_EXPR) {
    emitsizeofexpr(s, n);
  } else if (k == ARRAYSUBSCRIPT || k == ARRAYSUBSCRIPTADDR) {
    emitarraysubscript(s, n);
  } else if (k == TYPECAST) {
    emitexpr(s, n->right);
  } else if (k == ENUMERATORCONSTANT) {
    emitln("# enumeratorconstant");
    asprintf(&str, "$%d", n->tok->i);
    emitpush(str);
    free(str);
  } else {
    die("emitexpr op", NODENAMES[k]);
  }
}

void emitblock(char *, char *, char *, struct node *);

void emitwhile(char *func, struct scope *s, struct node *n) {
  char *l;
  char *lblstart;
  char *lblend;

  asprintf(&lblstart, "__while_start_%d", n->n);
  asprintf(&lblend, "__while_end_%d", n->n);

  emitln("# while");
  emit(lblstart);
  emitln(":");

  emitexpr(s, getwhilecond(n));
  emitpop("%eax");
  emitln("cmp $0, %eax");
  asprintf(&l, "je __while_end_%d", n->n);
  emitln(l);
  free(l);
  emitblock(func, lblstart, lblend, getwhileblock(n));

  emit("jmp ");
  emitln(lblstart);

  emit(lblend);
  emitln(":");
}

void emitcond(char *func, char *lblstart, char *lblend, struct scope *s,
              struct node *n) {
  struct node *cur;
  char *l;
  int i;

  emitln("# cond");
  i = 1;
  cur = n;
  while (cur) {
    asprintf(&l, "__cond_%d_%d:", n->n, i);
    emitln(l);
    free(l);
    if (getcondcond(cur)) {
      emitexpr(s, getcondcond(cur));
      emitpop("%eax");
      emitln("cmp $0, %eax");
      asprintf(&l, "je __cond_%d_%d", n->n, i + 1);
      emitln(l);
      free(l);
    }
    emitblock(func, lblstart, lblend, cur->right);
    asprintf(&l, "jmp __cond_end_%d", n->n);
    emitln(l);
    free(l);
    ++i;
    cur = getcondnext(cur);
  }
  asprintf(&l, "__cond_%d_%d:", n->n, i);
  emitln(l);
  free(l);

  asprintf(&l, "__cond_end_%d:", n->n);
  emitln(l);
  free(l);
}

void emitstmt(char *func, char *lblstart, char *lblend, struct scope *s,
              struct node *n) {
  int stackd;
  int stack0;
  char *e;
  int k;

  k = n->kind;
  asprintf(&e, "# ------------------------- line=%d", n->tok->lineno);
  emitln(e);
  free(e);
  if (k == N_WHILE) {
    emitwhile(func, s, n);
  } else if (k == COND) {
    emitcond(func, lblstart, lblend, s, n);
  } else if (k == N_CONTINUE) {
    _assert(lblstart != NULL, "emitstmt: missing lblstart");
    emitln("# continue");
    emit("jmp ");
    emitln(lblstart);
  } else if (k == N_BREAK) {
    _assert(lblend != NULL, "emitstmt: missing lblend");
    emitln("# break");
    emit("jmp ");
    emitln(lblend);
  } else if (k == N_RETURN) {
    char *e;
    if (n->left) {
      emitexpr(s, n->left);
      emitpop("%eax");
    }
    asprintf(&e, "jmp %s_end", func);
    emitln(e);
    free(e);
  } else if (k == EXPR) {
    emitexpr(s, n->left);
    /*
     * All expressions push something on top of the stack, so get rid of it in
     * the end.
     */
  } else if (k == N_GOTO) {
    struct scope *sf;

    sf = s;
    while (sf && !sf->isfunc) {
      sf = sf->parent;
    }
    emitln("# goto");
    emit("jmp __goto_");
    emit(sf->id);
    emit("_");
    emitln(n->left->tok->s);
  } else if (k == LABEL) {
    struct scope *sf;

    sf = s;
    while (sf && !sf->isfunc) {
      sf = sf->parent;
    }
    emitln("# label");
    emit("__goto_");
    emit(sf->id);
    emit("_");
    emit(n->tok->s);
    emitln(":");
  } else if (k == NOP) {
    ;
  } else {
    die("emitstmt statement node", NODENAMES[k]);
  }
}

void emitblock(char *func, char *lblstart, char *lblend, struct node *bl) {
  struct scopevar *var;
  struct node *n;
  struct node *t;
  char *e;

  _assert(bl->kind == BLOCK, NODENAMES[bl->kind]);
  _assert(bl->scope != NULL, "emitblock: scope NULL");
  emitln("# block variables");
  var = bl->scope->vars;
  while (var) {
    if (!var->isparam) {
      asprintf(&e, "# %s %d", var->name, var->shift);
      emitln(e);
      free(e);
      if ((t = getdeclinitializer(var->this))) {
        struct type *ty;

        ty = checkdecltype(var->this);
        if (ty->plvl > 0) {
          if (ty->flags & F_CHAR && t->kind == N_STRLIT) {
            asprintf(&e, "lea __node_%d, %%eax\nmov %%eax, %d(%%ebp)",
                     var->this->n, var->shift);
          } else {
            emitexpr(bl->scope, t);
            emitpop("%eax");
            asprintf(&e, "mov %%eax, %d(%%ebp)", var->shift);
          }
          emitln(e);
          free(e);
        } else if (ty->arrsz > -1) {
          struct node *cur;
          struct type *ty;
          int atomsz;
          char *iz;
          int n;
          int i;

          cur = var->this->right;
          _assert(cur->kind == INITIALIZERARRAY, NODENAMES[cur->kind]);
          cur = cur->left;

          ty = checkdecltype(var->this);
          atomsz = typeatomsize(ty);
          i = var->shift;
          n = 0;
          while (cur) {
            _assert(cur->kind == INITIALIZER, NODENAMES[cur->kind]);
            _assert(cur->left->kind == N_CONSTANT, NODENAMES[cur->left->kind]);
            asprintf(&iz, "movl $%ld, %d(%%ebp)", cur->tok->i, i);
            emitln(iz);
            free(iz);
            i += atomsz;
            cur = cur->next;
            ++n;
          }
          while (n != ty->arrsz) {
            asprintf(&iz, "movl $0, %d(%%ebp)", i);
            emitln(iz);
            free(iz);
            i += atomsz;
            ++n;
          }
        } else {
          if (ty->flags & F_INT) {
            asprintf(&e, "movl $%d, %d(%%ebp)",
                     evalconstexpr(getdeclinitializer(var->this)), var->shift);
            emitln(e);
            free(e);
          } else {
            die("emitblock", "dunno how to handle plain variable");
          }
        }
      }
    }
    var = var->next;
  }
  emitln("# block statements");
  n = bl->right;
  while (n) {
    emitstmt(func, lblstart, lblend, bl->scope, n);
    n = n->next;
  }
  emitln("# block ends");
}

void emitfuncs(void) {
  struct defpair **cur;
  struct node *bl;
  char *name;
  int ssize;
  int stack;
  int stack0;
  char *e;

  cur = FUNCDEFS;
  while (*cur) {
    name = (*cur)->name;
    bl = getfundefblock((*cur)->def);
    ssize = bl->scope->ssize;
    emit("# emitting function: ");
    emitln(name);
    /* FIXME We expose all functions by default. */
    emit(".globl ");
    emitln(name);
    emit(name);
    emitln(":");
    emitln("push %ebp");
    emitln("mov %esp, %ebp");
    emitln("# function locals");
    if (ssize) {
      asprintf(&e, "sub $%d, %%esp", ssize);
      emitln(e);
      free(e);
    }
    emitln("push %edx");
    emitln("push %edi");
    emitln("push %esi");
    /*
     * Modern GNU/Linux is pretty strict with stack alignment, and seems like
     * this extends to 32-bit mode, too. Before calling our `main`, the C
     * runtime probably set us up with a stack which was aligned to 16 bytes.
     * The `call` pushed 4 bytes of return address, and after that we push
     * `ebp`, the function locals, and caller-save registers, which means we
     * probably have to realign again once we do further function calls beyond
     * our translation unit, namely back to libc. So all in all our stack size
     * begins at ret + ebp + locals + edx + edi + esi = (5 * 4) + ssize;
     */
    stack = 20 + ssize;
    stack0 = stack;
    asprintf(&e, "# stack before=%d", stack0);
    emitln(e);
    free(e);
    emitblock(name, NULL, NULL, getfundefblock((*cur)->def));
    asprintf(&e, "# stack after=%d (%d)", stack, stack0 - stack);
    emitln(e);
    free(e);
    emit(name);
    emitln("_end:");
    emitln("nop");
    emitln("pop %esi");
    emitln("pop %edi");
    emitln("pop %edx");
    if (ssize) {
      asprintf(&e, "add $%d, %%esp", ssize);
      emitln(e);
      free(e);
    }
    emitln("mov %ebp, %esp");
    emitln("pop %ebp");
    emitln("ret");
    emit("# end of function: ");
    emitln(name);
    ++cur;
  }
}

void codegen(void) {
  GEN = fopen("out.s", "w");
  _assert(GEN != NULL, "unable to open output file");
  emitln("# lc generated this");
  emitln(".set NULL, 0");
  emitln(".bss");
  emitvarsnoinit();
  emitln(".data");
  emitvarsinit();
  emitln(".text");
  emitfuncs();
  emitln("# lc is done");
  fclose(GEN);
}

int main(int argc, char *argv[]) {
  struct node **n;
  char line[1024];
  int inerr;
  int rv;
  int r;
  int i;

  inerr = 0;
  i = 1;
  n = NODES;
  strcat(CODE, "void *NULL = 0;\n");
  r = strlen(CODE);
  while (1) {
    memset(line, 0, 1024);
    rv = read(0, line, 1023);
    if (rv == -1) {
      die("read", "it errored");
    } else if (rv == 0) {
      break;
    }
    strncat(CODE, line, sizeof(CODE) - r);
    r += strlen(line);
    if (r >= sizeof(CODE) - 1) {
      die("main", "too much input");
    }
  }
  puts(CODE);
  CODE[sizeof(CODE) - 1] = 0;
  puts("Tokenizing...");
  lex();
  if (VERBOSE) {
    toksdump();
  }
  parse();
  if (VERBOSE) {
    nodesdump();
    printf("[tuscope=%p]\n", &tuscope);
  }
  tuscope.id = "translation-unit";
  while (*n) {
    printf("----- [%04d] -----\n", i);
    nodedump(0, *n);
    checkdecl(&tuscope, *n, 0);
    ++i;
    ++n;
  }
  if (VERBOSE) {
    scopedump(&tuscope);
    dumpfuncdecls();
    dumpfuncdefs();
    dumpstructdefs();
    dumptypedefs();
    dumpenums();
    dumpenumrs();
    dumpstructdefs();
  }
  codegen();

  return 0;
}
