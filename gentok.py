LOW_TOKS = {t for t in """!"#%&'()*+,-./:;<=>?[]^{|}~"""}
HIGH_TOKS_I = 128
HIGH_TOKS = [
    # (Name, Printable)
    ("ASSIGN_ADD", "+="),
    ("ASSIGN_AND", "&="),
    ("ASSIGN_DIV", "/="),
    ("ASSIGN_LEFT", "<<="),
    ("ASSIGN_OR", "|="),
    ("ASSIGN_MOD", "%="),
    ("ASSIGN_MUL", "*="),
    ("ASSIGN_RIGHT", ">>="),
    ("ASSIGN_SUB", "-="),
    ("ASSIGN_XOR", "^="),
    ("AUTO", ""),
    ("BREAK", ""),
    ("CHAR", ""),
    ("CONST", ""),
    ("CONSTANT", ""),
    ("CONTINUE", ""),
    ("DEFAULT", ""),
    ("DOUBLE", ""),
    ("DO", ""),
    ("ELLIPSIS", "..."),
    ("ELSE", ""),
    ("ENUM", ""),
    ("EXTERN", ""),
    ("FLOAT", ""),
    ("FOR", ""),
    ("GOTO", ""),
    ("ID", "identifier"),
    ("IF", ""),
    ("INT", ""),
    ("LONG", ""),
    ("OP_AND", "&&"),
    ("OP_DEC", "--"),
    ("OP_EQ", "=="),
    ("OP_LE", "<="),
    ("OP_LEFT", "<<"),
    ("OP_GE", ">="),
    ("OP_INC", "++"),
    ("OP_NE", "!="),
    ("OP_OR", "||"),
    ("OP_PTR", "->"),
    ("OP_RIGHT", ">>"),
    ("REGISTER", ""),
    ("RETURN", ""),
    ("SHORT", ""),
    ("SIGNED", ""),
    ("SIZEOF", "sizeof"),
    ("STATIC", ""),
    ("STRLIT", "string literal"),
    ("STRUCT", ""),
    ("SWITCH", ""),
    ("TYPEDEF", ""),
    ("UNION", ""),
    ("UNSIGNED", ""),
    ("VOID", ""),
    ("VOLATILE", ""),
    ("WHILE", ""),
    ("_END", "the end"),
]
OPS_BIN = {
    # CharOrName: (Precedence, Associativity)
    # Comma is treated on grammar level.
    # ",": (1, "left"),
    "=": (2, "right"),
    "+=": (2, "right"),
    "-=": (2, "right"),
    "*=": (2, "right"),
    "/=": (2, "right"),
    "%=": (2, "right"),
    "&=": (2, "right"),
    "^=": (2, "right"),
    "|=": (2, "right"),
    "<<=": (2, "right"),
    ">>=": (2, "right"),
    "?": (3, "right"),
    ":": (3, "right"),
    "||": (4, "left"),
    "&&": (5, "left"),
    "|": (6, "left"),
    "^": (7, "left"),
    "&": (8, "left"),
    "==": (9, "left"),
    "!=": (9, "left"),
    "<": (10, "left"),
    ">": (10, "left"),
    "<=": (10, "left"),
    ">=": (10, "left"),
    "<<": (11, "left"),
    ">>": (11, "left"),
    "+": (12, "left"),
    "-": (12, "left"),
    "*": (13, "left"),
    "/": (13, "left"),
    "%": (13, "left"),
    ".": (15, "left"),
    "->": (15, "left"),
    "(": (15, "left"),
    "[": (15, "left"),
}
OPS_UN = {
    "*": 14,
    "&": 14,
    "+": 14,
    "-": 14,
    "!": 14,
    "~": 14,
    "++": 14,
    "--": 14,
    # postfix "++" and "--" are treated in the parser
    "sizeof": 14,
    "(": 14,
}
KEYWORDS = [
    "auto",
    "break",
    "char",
    "const",
    "continue",
    "default",
    "double",
    "do",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "int",
    "long",
    "register",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
]


def tokname(tok):
    if tok[1] == "":
        return tok[0].lower()
    else:
        return tok[1]


def genenums(prefix="enum Token {", postfix="};"):
    r = prefix
    for i, tok in enumerate(HIGH_TOKS):
        if i == 0:
            r += f"{tok[0]} = {HIGH_TOKS_I},"
        else:
            r += f"{tok[0]},"
    r += postfix
    return r


def gennames(prefix="static char *TOKNAMES[] = {", postfix="};"):
    toks = []
    for i in range(0, 128):
        c = chr(i)
        if c in LOW_TOKS:
            if c == '"':
                toks.append(r'"\""')
            else:
                toks.append(f'"{c}"')
        else:
            toks.append("NULL")
    for h in HIGH_TOKS:
        toks.append(f'"{tokname(h)}"')
    r = prefix
    for tok in toks:
        r += f"{tok},"
    r += postfix
    return r


def genkeywords(prefix="static char *KEYWORDS[] = {", postfix="};"):
    toks = []
    toks = ["NULL"] * (HIGH_TOKS_I)
    for h in HIGH_TOKS:
        tn = h[0].lower()
        if tn in KEYWORDS:
            toks.append(f'"{tn}"')
        else:
            toks.append("NULL")
    r = prefix
    for tok in toks:
        r += f"{tok},"
    r += postfix
    return r


def genprec():
    r = "static int PRECBIN[] = {"
    for i in range(128):
        c = chr(i)
        if c in OPS_BIN:
            r += str(OPS_BIN[c][0]) + ","
        else:
            r += "-1" + ","
    for h in HIGH_TOKS:
        tn = h[1]
        if not tn or tn not in OPS_BIN:
            r += "-1" + ","
            continue
        r += str(OPS_BIN[tn][0]) + ","
    r += "};"
    r += "\n\n"
    r += "static int PRECUN[] = {"
    for i in range(128):
        c = chr(i)
        if c in OPS_UN:
            r += str(OPS_UN[c]) + ","
        else:
            r += "-1" + ","
    for h in HIGH_TOKS:
        tn = h[1]
        if not tn or tn not in OPS_UN:
            r += "-1" + ","
            continue
        r += str(OPS_UN[tn]) + ","
    r += "};"
    r += "\n\n"
    r += "static int ASSOCBIN[] = {"
    for i in range(128):
        c = chr(i)
        if c in OPS_BIN:
            if OPS_BIN[c][1] == "left":
                r += "1,"
            else:
                r += "0,"
        else:
            r += "-1,"
    for h in HIGH_TOKS:
        tn = h[1]
        if not tn or tn not in OPS_BIN:
            r += "-1,"
            continue
        if OPS_BIN[tn][1] == "left":
            r += "1,"
        else:
            r += "0,"
    r += "};"
    return r


if __name__ == "__main__":
    print(genenums())
    print()
    print(gennames())
    print()
    print(genkeywords())
    print()
    print(genprec())
