#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct _STRING
{
    const char *buf;
    int len;
} STRING, *PSTRING;

typedef struct
{
    STRING strName;
    STRING strTarget;
    int nCallingConvention;
    int nOrdinal;
    int nStackBytes;
    int nArgCount;
    int anArgs[30];
    unsigned int uFlags;
    int nNumber;
} EXPORT;

enum _ARCH
{
    ARCH_X86,
    ARCH_AMD64,
    ARCH_IA64,
    ARCH_ARM,
    ARCH_PPC
};

typedef int (*PFNOUTLINE)(FILE *, EXPORT *);
int gbMSComp = 0;
int gbImportLib = 0;
int gbNotPrivateNoWarn = 0;
int gbTracing = 0;
int giArch = ARCH_X86;
char *pszArchString = "i386";
char *pszArchString2;
char *pszSourceFileName = NULL;
char *pszDllName = NULL;
char *gpszUnderscore = "";
int gbDebug;
unsigned guOsVersion = 0x502;
#define DbgPrint(...) (!gbDebug || fprintf(stderr, __VA_ARGS__))

enum
{
    FL_PRIVATE = 1,
    FL_STUB = 2,
    FL_NONAME = 4,
    FL_ORDINAL = 8,
    FL_NORELAY = 16,
    FL_RET64 = 32,
    FL_REGISTER = 64,
};

enum
{
    CC_STDCALL,
    CC_CDECL,
    CC_FASTCALL,
    CC_THISCALL,
    CC_EXTERN,
    CC_STUB,
};

enum
{
    ARG_LONG,
    ARG_PTR,
    ARG_STR,
    ARG_WSTR,
    ARG_DBL,
    ARG_INT64,
    ARG_INT128,
    ARG_FLOAT
};

const char* astrCallingConventions[] =
{
    "STDCALL",
    "CDECL",
    "FASTCALL",
    "THISCALL",
    "EXTERN"
};

static const char* astrShouldBePrivate[] =
{
    "DllCanUnloadNow",
    "DllGetClassObject",
    "DllGetClassFactoryFromClassString",
    "DllGetDocumentation",
    "DllInitialize",
    "DllInstall",
    "DllRegisterServer",
    "DllRegisterServerEx",
    "DllRegisterServerExW",
    "DllUnload",
    "DllUnregisterServer",
    "RasCustomDeleteEntryNotify",
    "RasCustomDial",
    "RasCustomDialDlg",
    "RasCustomEntryDlg",
};

static
int
IsSeparator(char chr)
{
    return ((chr <= ',' && chr != '$' && chr != '#') ||
            (chr >= ':' && chr < '?') );
}

int
CompareToken(const char *token, const char *comparand)
{
    while (*comparand)
    {
        if (*token != *comparand) return 0;
        token++;
        comparand++;
    }
    if (IsSeparator(comparand[-1])) return 1;
    if (!IsSeparator(*token)) return 0;
    return 1;
}

const char *
ScanToken(const char *token, char chr)
{
    while (!IsSeparator(*token))
    {
        if (*token == chr) return token;
        token++;
    }
    return 0;
}

char *
NextLine(char *pc)
{
    while (*pc != 0)
    {
        if (pc[0] == '\n' && pc[1] == '\r') return pc + 2;
        else if (pc[0] == '\n') return pc + 1;
        pc++;
    }
    return pc;
}

int
TokenLength(char *pc)
{
    int length = 0;

    while (!IsSeparator(*pc++)) length++;

    return length;
}

char *
NextToken(char *pc)
{
    /* Skip token */
    while (!IsSeparator(*pc)) pc++;

    /* Skip white spaces */
    while (*pc == ' ' || *pc == '\t') pc++;

    /* Check for end of line */
    if (*pc == '\n' || *pc == '\r' || *pc == 0) return 0;

    /* Check for comment */
    if (*pc == '#' || *pc == ';') return 0;

    return pc;
}

void
OutputHeader_stub(FILE *file)
{
    fprintf(file, "/* This file is autogenerated, do not edit. */\n\n"
            "#include <stubs.h>\n");

    if (gbTracing)
    {
        fprintf(file, "#include <wine/debug.h>\n");
        fprintf(file, "#include <inttypes.h>\n");
        fprintf(file, "WINE_DECLARE_DEBUG_CHANNEL(relay);\n");
    }

    fprintf(file, "\n");
}

int
OutputLine_stub(FILE *file, EXPORT *pexp)
{
    int i;
    int bRelay = 0;
    int bInPrototype = 0;

    if (pexp->nCallingConvention != CC_STUB &&
        (pexp->uFlags & FL_STUB) == 0)
    {
        /* Only relay trace stdcall C functions */
        if (!gbTracing || (pexp->nCallingConvention != CC_STDCALL)
                || (pexp->uFlags & FL_NORELAY)
                || (pexp->strName.buf[0] == '?'))
        {
            return 0;
        }
        bRelay = 1;
    }

    /* Declare the "real" function */
    if (bRelay)
    {
        fprintf(file, "extern ");
        bInPrototype = 1;
    }

    do
    {
        if (pexp->uFlags & FL_REGISTER)
        {
            /* FIXME: Not sure this is right */
            fprintf(file, "void ");
        }
        else if (pexp->uFlags & FL_RET64)
        {
            fprintf(file, "__int64 ");
        }
        else
        {
            fprintf(file, "int ");
        }

        if ((giArch == ARCH_X86) &&
            pexp->nCallingConvention == CC_STDCALL)
        {
            fprintf(file, "__stdcall ");
        }

        /* Check for C++ */
        if (pexp->strName.buf[0] == '?')
        {
            fprintf(file, "stub_function%d(", pexp->nNumber);
        }
        else
        {
            if (!bRelay || bInPrototype)
                fprintf(file, "%.*s(", pexp->strName.len, pexp->strName.buf);
            else
                fprintf(file, "$relaytrace$%.*s(", pexp->strName.len, pexp->strName.buf);
        }

        for (i = 0; i < pexp->nArgCount; i++)
        {
            if (i != 0) fprintf(file, ", ");
            switch (pexp->anArgs[i])
            {
                case ARG_LONG: fprintf(file, "long"); break;
                case ARG_PTR:  fprintf(file, "void*"); break;
                case ARG_STR:  fprintf(file, "char*"); break;
                case ARG_WSTR: fprintf(file, "wchar_t*"); break;
                case ARG_DBL:  fprintf(file, "double"); break;
                case ARG_INT64 :  fprintf(file, "__int64"); break;
                /* __int128 is not supported on x86, and int128 in spec files most often represents a GUID */
                case ARG_INT128 :  fprintf(file, "GUID"); break;
                case ARG_FLOAT: fprintf(file, "float"); break;
            }
            fprintf(file, " a%d", i);
        }

        if (bInPrototype)
        {
            fprintf(file, ");\n\n");
        }
    } while (bInPrototype--);

    if (!bRelay)
    {
        fprintf(file, ")\n{\n\tDbgPrint(\"WARNING: calling stub %.*s(",
                pexp->strName.len, pexp->strName.buf);
    }
    else
    {
        fprintf(file, ")\n{\n");
        if (pexp->uFlags & FL_REGISTER)
        {
            /* No return value */
        }
        else if (pexp->uFlags & FL_RET64)
        {
            fprintf(file, "\t__int64 retval;\n");
        }
        else
        {
            fprintf(file, "\tint retval;\n");
        }
        fprintf(file, "\tif (TRACE_ON(relay))\n\t\tDPRINTF(\"%s: %.*s(",
                pszDllName, pexp->strName.len, pexp->strName.buf);
    }

    for (i = 0; i < pexp->nArgCount; i++)
    {
        if (i != 0) fprintf(file, ",");
        switch (pexp->anArgs[i])
        {
            case ARG_LONG: fprintf(file, "0x%%lx"); break;
            case ARG_PTR:  fprintf(file, "0x%%p"); break;
            case ARG_STR:  fprintf(file, "'%%s'"); break;
            case ARG_WSTR: fprintf(file, "'%%ws'"); break;
            case ARG_DBL:  fprintf(file, "%%f"); break;
            case ARG_INT64: fprintf(file, "%%\"PRIx64\""); break;
            case ARG_INT128: fprintf(file, "'%%s'"); break;
            case ARG_FLOAT: fprintf(file, "%%f"); break;
        }
    }
    fprintf(file, ")\\n\"");

    for (i = 0; i < pexp->nArgCount; i++)
    {
        fprintf(file, ", ");
        switch (pexp->anArgs[i])
        {
            case ARG_LONG: fprintf(file, "(long)a%d", i); break;
            case ARG_PTR:  fprintf(file, "(void*)a%d", i); break;
            case ARG_STR:  fprintf(file, "(char*)a%d", i); break;
            case ARG_WSTR: fprintf(file, "(wchar_t*)a%d", i); break;
            case ARG_DBL:  fprintf(file, "(double)a%d", i); break;
            case ARG_INT64: fprintf(file, "(__int64)a%d", i); break;
            case ARG_INT128: fprintf(file, "wine_dbgstr_guid(&a%d)", i); break;
            case ARG_FLOAT: fprintf(file, "(float)a%d", i); break;
        }
    }
    fprintf(file, ");\n");

    if (pexp->nCallingConvention == CC_STUB)
    {
        fprintf(file, "\t__wine_spec_unimplemented_stub(\"%s\", __FUNCTION__);\n", pszDllName);
    }
    else if (bRelay)
    {
        if (pexp->uFlags & FL_REGISTER)
        {
            fprintf(file,"\t");
        }
        else
        {
            fprintf(file, "\tretval = ");
        }
        fprintf(file, "%.*s(", pexp->strName.len, pexp->strName.buf);

        for (i = 0; i < pexp->nArgCount; i++)
        {
            if (i != 0) fprintf(file, ", ");
            fprintf(file, "a%d", i);
        }
        fprintf(file, ");\n");
    }

    if (!bRelay)
        fprintf(file, "\treturn 0;\n}\n\n");
    else if ((pexp->uFlags & FL_REGISTER) == 0)
    {
        if (pexp->uFlags & FL_RET64)
        {
            fprintf(file, "\tif (TRACE_ON(relay))\n\t\tDPRINTF(\"%s: %.*s: retval = %%\"PRIx64\"\\n\", retval);\n",
                    pszDllName, pexp->strName.len, pexp->strName.buf);
        }
        else
        {
            fprintf(file, "\tif (TRACE_ON(relay))\n\t\tDPRINTF(\"%s: %.*s: retval = 0x%%lx\\n\", retval);\n",
                    pszDllName, pexp->strName.len, pexp->strName.buf);
        }
        fprintf(file, "\treturn retval;\n}\n\n");
    }

    return 1;
}

void
OutputHeader_asmstub(FILE *file, char *libname)
{
    fprintf(file, "; File generated automatically, do not edit! \n\n");

    if (giArch == ARCH_X86)
    {
        fprintf(file, ".586\n.model flat\n.code\n");
    }
    else if (giArch == ARCH_AMD64)
    {
        fprintf(file, ".code\n");
    }
    else if (giArch == ARCH_ARM)
    {
        fprintf(file, "    AREA |.text|,ALIGN=2,CODE,READONLY\n\n");
    }
}

void
Output_stublabel(FILE *fileDest, char* pszSymbolName)
{
    if (giArch == ARCH_ARM)
    {
        fprintf(fileDest,
                "\tEXPORT |%s| [FUNC]\n|%s|\n",
                pszSymbolName,
                pszSymbolName);
    }
    else
    {
        fprintf(fileDest,
                "PUBLIC %s\n%s: nop\n",
                pszSymbolName,
                pszSymbolName);
    }
}

int
OutputLine_asmstub(FILE *fileDest, EXPORT *pexp)
{
    char szNameBuffer[128];

    /* Handle autoname */
    if (pexp->strName.len == 1 && pexp->strName.buf[0] == '@')
    {
        sprintf(szNameBuffer, "%sordinal%d\n%sordinal%d: nop\n",
                gpszUnderscore, pexp->nOrdinal, gpszUnderscore, pexp->nOrdinal);
    }
    else if (giArch != ARCH_X86)
    {
        sprintf(szNameBuffer, "_stub_%.*s",
                pexp->strName.len, pexp->strName.buf);
    }
    else if (pexp->nCallingConvention == CC_STDCALL)
    {
        sprintf(szNameBuffer, "__stub_%.*s@%d",
                pexp->strName.len, pexp->strName.buf, pexp->nStackBytes);
    }
    else if (pexp->nCallingConvention == CC_FASTCALL)
    {
        sprintf(szNameBuffer, "@_stub_%.*s@%d",
                pexp->strName.len, pexp->strName.buf, pexp->nStackBytes);
    }
    else if ((pexp->nCallingConvention == CC_CDECL) ||
             (pexp->nCallingConvention == CC_THISCALL) ||
             (pexp->nCallingConvention == CC_EXTERN) ||
             (pexp->nCallingConvention == CC_STUB))
    {
        sprintf(szNameBuffer, "__stub_%.*s",
                pexp->strName.len, pexp->strName.buf);
    }
    else
    {
        fprintf(stderr, "Invalid calling convention");
        return 0;
    }

    Output_stublabel(fileDest, szNameBuffer);

    return 1;
}

void
OutputHeader_def(FILE *file, char *libname)
{
    fprintf(file,
            "; File generated automatically, do not edit!\n\n"
            "NAME %s\n\n"
            "EXPORTS\n",
            libname);
}

void
PrintName(FILE *fileDest, EXPORT *pexp, PSTRING pstr, int fDeco)
{
    const char *pcName = pstr->buf;
    int nNameLength = pstr->len;
    const char* pcDot, *pcAt;

    /* Check for non-x86 first */
    if (giArch != ARCH_X86)
    {
        /* Does the string already have stdcall decoration? */
        pcAt = ScanToken(pcName, '@');
        if (pcAt && (pcAt < (pcName + nNameLength)) && (pcName[0] == '_'))
        {
            /* Skip leading underscore and remove trailing decoration */
            pcName++;
            nNameLength = (int)(pcAt - pcName);
        }

        /* Print the undecorated function name */
        fprintf(fileDest, "%.*s", nNameLength, pcName);
    }
    else if (fDeco &&
        ((pexp->nCallingConvention == CC_STDCALL) ||
         (pexp->nCallingConvention == CC_FASTCALL)))
    {
        /* Scan for a dll forwarding dot */
        pcDot = ScanToken(pcName, '.');
        if (pcDot)
        {
            /* First print the dll name, followed by a dot */
            nNameLength = (int)(pcDot - pcName);
            fprintf(fileDest, "%.*s.", nNameLength, pcName);

            /* Now the actual function name */
            pcName = pcDot + 1;
            nNameLength = pexp->strTarget.len - nNameLength - 1;
        }

        /* Does the string already have decoration? */
        pcAt = ScanToken(pcName, '@');
        if (pcAt && (pcAt < (pcName + nNameLength)))
        {
            /* On GCC, we need to remove the leading stdcall underscore */
            if (!gbMSComp && (pexp->nCallingConvention == CC_STDCALL))
            {
                pcName++;
                nNameLength--;
            }

            /* Print the already decorated function name */
            fprintf(fileDest, "%.*s", nNameLength, pcName);
        }
        else
        {
            /* Print the prefix, but skip it for (GCC && stdcall) */
            if (gbMSComp || (pexp->nCallingConvention != CC_STDCALL))
            {
                fprintf(fileDest, "%c", pexp->nCallingConvention == CC_FASTCALL ? '@' : '_');
            }

            /* Print the name with trailing decoration */
            fprintf(fileDest, "%.*s@%d", nNameLength, pcName, pexp->nStackBytes);
        }
    }
    else
    {
        /* Print the undecorated function name */
        fprintf(fileDest, "%.*s", nNameLength, pcName);
    }
}

void
OutputLine_def_MS(FILE *fileDest, EXPORT *pexp)
{
    PrintName(fileDest, pexp, &pexp->strName, 0);

    if (gbImportLib)
    {
        /* Redirect to a stub function, to get the right decoration in the lib */
        fprintf(fileDest, "=_stub_%.*s", pexp->strName.len, pexp->strName.buf);
    }
    else if (pexp->strTarget.buf)
    {
        if (pexp->strName.buf[0] == '?')
        {
            //fprintf(stderr, "warning: ignoring C++ redirection %.*s -> %.*s\n",
            //        pexp->strName.len, pexp->strName.buf, pexp->strTarget.len, pexp->strTarget.buf);
        }
        else
        {
            fprintf(fileDest, "=");

            /* If the original name was decorated, use decoration in the forwarder as well */
            if ((giArch == ARCH_X86) && ScanToken(pexp->strName.buf, '@') &&
                !ScanToken(pexp->strTarget.buf, '@') &&
                ((pexp->nCallingConvention == CC_STDCALL) ||
                (pexp->nCallingConvention == CC_FASTCALL)) )
            {
                PrintName(fileDest, pexp, &pexp->strTarget, 1);
            }
            else
            {
                /* Write the undecorated redirection name */
                fprintf(fileDest, "%.*s", pexp->strTarget.len, pexp->strTarget.buf);
            }
        }
    }
    else if (((pexp->uFlags & FL_STUB) || (pexp->nCallingConvention == CC_STUB)) &&
             (pexp->strName.buf[0] == '?'))
    {
        /* C++ stubs are forwarded to C stubs */
        fprintf(fileDest, "=stub_function%d", pexp->nNumber);
    }
    else if (gbTracing && ((pexp->uFlags & FL_NORELAY) == 0) && (pexp->nCallingConvention == CC_STDCALL) &&
            (pexp->strName.buf[0] != '?'))
    {
        /* Redirect it to the relay-tracing trampoline */
        fprintf(fileDest, "=$relaytrace$%.*s", pexp->strName.len, pexp->strName.buf);
    }
}

void
OutputLine_def_GCC(FILE *fileDest, EXPORT *pexp)
{
    int bTracing = 0;
    /* Print the function name, with decoration for export libs */
    PrintName(fileDest, pexp, &pexp->strName, gbImportLib);
    DbgPrint("Generating def line for '%.*s'\n", pexp->strName.len, pexp->strName.buf);

    /* Check if this is a forwarded export */
    if (pexp->strTarget.buf)
    {
        int fIsExternal = !!ScanToken(pexp->strTarget.buf, '.');
        DbgPrint("Got redirect '%.*s'\n", pexp->strTarget.len, pexp->strTarget.buf);

        /* print the target name, don't decorate if it is external */
        fprintf(fileDest, "=");
        PrintName(fileDest, pexp, &pexp->strTarget, !fIsExternal);
    }
    else if (((pexp->uFlags & FL_STUB) || (pexp->nCallingConvention == CC_STUB)) &&
             (pexp->strName.buf[0] == '?'))
    {
        /* C++ stubs are forwarded to C stubs */
        fprintf(fileDest, "=stub_function%d", pexp->nNumber);
    }
    else if (gbTracing && ((pexp->uFlags & FL_NORELAY) == 0) &&
             (pexp->nCallingConvention == CC_STDCALL) &&
            (pexp->strName.buf[0] != '?'))
    {
        /* Redirect it to the relay-tracing trampoline */
        char buf[256];
        STRING strTarget;
        fprintf(fileDest, "=");
        sprintf(buf, "$relaytrace$%.*s", pexp->strName.len, pexp->strName.buf);
        strTarget.buf = buf;
        strTarget.len = pexp->strName.len + 12;
        PrintName(fileDest, pexp, &strTarget, 1);
        bTracing = 1;
    }

    /* Special handling for stdcall and fastcall */
    if ((giArch == ARCH_X86) &&
        ((pexp->nCallingConvention == CC_STDCALL) ||
         (pexp->nCallingConvention == CC_FASTCALL)))
    {
        /* Is this the import lib? */
        if (gbImportLib)
        {
            /* Is the name in the spec file decorated? */
            const char* pcDeco = ScanToken(pexp->strName.buf, '@');
            if (pcDeco && (pcDeco < pexp->strName.buf + pexp->strName.len))
            {
                /* Write the name including the leading @  */
                fprintf(fileDest, "==%.*s", pexp->strName.len, pexp->strName.buf);
            }
        }
        else if ((!pexp->strTarget.buf) && !(bTracing))
        {
            /* Write a forwarder to the actual decorated symbol */
            fprintf(fileDest, "=");
            PrintName(fileDest, pexp, &pexp->strName, 1);
        }
    }
}

int
OutputLine_def(FILE *fileDest, EXPORT *pexp)
{
    DbgPrint("OutputLine_def: '%.*s'...\n", pexp->strName.len, pexp->strName.buf);
    fprintf(fileDest, " ");

    if (gbMSComp)
        OutputLine_def_MS(fileDest, pexp);
    else
        OutputLine_def_GCC(fileDest, pexp);

    if (pexp->uFlags & FL_ORDINAL)
    {
        fprintf(fileDest, " @%d", pexp->nOrdinal);
    }

    if (pexp->uFlags & FL_NONAME)
    {
        fprintf(fileDest, " NONAME");
    }

    /* Either PRIVATE or DATA */
    if (pexp->uFlags & FL_PRIVATE)
    {
        fprintf(fileDest, " PRIVATE");
    }
    else if (pexp->nCallingConvention == CC_EXTERN)
    {
        fprintf(fileDest, " DATA");
    }

    fprintf(fileDest, "\n");

    return 1;
}

int
ParseFile(char* pcStart, FILE *fileDest, PFNOUTLINE OutputLine)
{
    char *pc, *pcLine;
    int nLine;
    EXPORT exp;
    int included, version_included;
    char namebuffer[16];
    unsigned int i;

    //fprintf(stderr, "info: line %d, pcStart:'%.30s'\n", nLine, pcStart);

    /* Loop all lines */
    nLine = 1;
    exp.nNumber = 0;
    for (pcLine = pcStart; *pcLine; pcLine = NextLine(pcLine), nLine++)
    {
        pc = pcLine;

        exp.nArgCount = 0;
        exp.uFlags = 0;
        exp.nNumber++;

        /* Skip white spaces */
        while (*pc == ' ' || *pc == '\t') pc++;

        /* Skip empty lines, stop at EOF */
        if (*pc == ';' || *pc <= '#') continue;
        if (*pc == 0) return 0;

        /* Now we should get either an ordinal or @ */
        if (*pc == '@')
            exp.nOrdinal = -1;
        else
        {
            exp.nOrdinal = atol(pc);
            /* The import lib should contain the ordinal only if -ordinal was specified */
            if (!gbImportLib)
                exp.uFlags |= FL_ORDINAL;
        }

        /* Go to next token (type) */
        if (!(pc = NextToken(pc)))
        {
            fprintf(stderr, "%s line %d: error: unexpected end of line\n", pszSourceFileName, nLine);
            return -10;
        }

        //fprintf(stderr, "info: Token:'%.*s'\n", TokenLength(pc), pc);

        /* Now we should get the type */
        if (CompareToken(pc, "stdcall"))
        {
            exp.nCallingConvention = CC_STDCALL;
        }
        else if (CompareToken(pc, "cdecl") ||
                 CompareToken(pc, "varargs"))
        {
            exp.nCallingConvention = CC_CDECL;
        }
        else if (CompareToken(pc, "fastcall"))
        {
            exp.nCallingConvention = CC_FASTCALL;
        }
        else if (CompareToken(pc, "thiscall"))
        {
            exp.nCallingConvention = CC_THISCALL;
        }
        else if (CompareToken(pc, "extern"))
        {
            exp.nCallingConvention = CC_EXTERN;
        }
        else if (CompareToken(pc, "stub"))
        {
            exp.nCallingConvention = CC_STUB;
        }
        else
        {
            fprintf(stderr, "%s line %d: error: expected callconv, got '%.*s' %d\n",
                    pszSourceFileName, nLine, TokenLength(pc), pc, *pc);
            return -11;
        }

        /* Go to next token (options or name) */
        if (!(pc = NextToken(pc)))
        {
            fprintf(stderr, "fail2\n");
            return -12;
        }

        /* Handle options */
        included = 1;
        version_included = 1;
        while (*pc == '-')
        {
            if (CompareToken(pc, "-arch="))
            {
                /* Default to not included */
                included = 0;
                pc += 5;

                /* Look if we are included */
                do
                {
                    pc++;
                    if (CompareToken(pc, pszArchString) ||
                        CompareToken(pc, pszArchString2))
                    {
                        included = 1;
                    }

                    /* Skip to next arch or end */
                    while (*pc > ',') pc++;
                } while (*pc == ',');
            }
            else if (CompareToken(pc, "-i386"))
            {
                if (giArch != ARCH_X86) included = 0;
            }
            else if (CompareToken(pc, "-version="))
            {
                /* Default to not included */
                version_included = 0;
                pc += 8;

                /* Look if we are included */
                do
                {
                    unsigned version, endversion;

                    /* Optionally skip leading '0x' */
                    pc++;
                    if ((pc[0] == '0') && (pc[1] == 'x')) pc += 2;

                    /* Now get the version number */
                    endversion = version = strtoul(pc, &pc, 16);

                    /* Check if it's a range */
                    if (pc[0] == '+')
                    {
                        endversion = 0xFFF;
                        pc++;
                    }
                    else if (pc[0] == '-')
                    {
                        /* Optionally skip leading '0x' */
                        pc++;
                        if ((pc[0] == '0') && (pc[1] == 'x')) pc += 2;
                        endversion = strtoul(pc, &pc, 16);
                    }

                    /* Check for degenerate range */
                    if (version > endversion)
                    {
                        fprintf(stderr, "%s line %d: error: invalid version rangen\n", pszSourceFileName, nLine);
                        return -1;
                    }

                    /* Now compare the range with our version */
                    if ((guOsVersion >= version) &&
                        (guOsVersion <= endversion))
                    {
                        version_included = 1;
                    }

                    /* Skip to next arch or end */
                    while (*pc > ',') pc++;

                } while (*pc == ',');
            }
            else if (CompareToken(pc, "-private"))
            {
                exp.uFlags |= FL_PRIVATE;
            }
            else if (CompareToken(pc, "-noname"))
            {
                exp.uFlags |= FL_ORDINAL | FL_NONAME;
            }
            else if (CompareToken(pc, "-ordinal"))
            {
                exp.uFlags |= FL_ORDINAL;
                /* GCC doesn't automatically import by ordinal if an ordinal
                 * is found in the def file. Force it. */
                if (gbImportLib && !gbMSComp)
                    exp.uFlags |= FL_NONAME;
            }
            else if (CompareToken(pc, "-stub"))
            {
                exp.uFlags |= FL_STUB;
            }
            else if (CompareToken(pc, "-norelay"))
            {
                exp.uFlags |= FL_NORELAY;
            }
            else if (CompareToken(pc, "-ret64"))
            {
                exp.uFlags |= FL_RET64;
            }
            else if (CompareToken(pc, "-register"))
            {
                exp.uFlags |= FL_REGISTER;
            }
            else
            {
                fprintf(stderr, "info: ignored option: '%.*s'\n",
                        TokenLength(pc), pc);
            }

            /* Go to next token */
            pc = NextToken(pc);
        }

        //fprintf(stderr, "info: Name:'%.10s'\n", pc);

        /* If arch didn't match ours, skip this entry */
        if (!included || !version_included) continue;

        /* Get name */
        exp.strName.buf = pc;
        exp.strName.len = TokenLength(pc);
        DbgPrint("Got name: '%.*s'\n", exp.strName.len, exp.strName.buf);

        /* Check for autoname */
        if ((exp.strName.len == 1) && (exp.strName.buf[0] == '@'))
        {
            sprintf(namebuffer, "ordinal%d", exp.nOrdinal);
            exp.strName.len = strlen(namebuffer);
            exp.strName.buf = namebuffer;
            exp.uFlags |= FL_ORDINAL | FL_NONAME;
        }

        /* Handle parameters */
        exp.nStackBytes = 0;
        if (exp.nCallingConvention != CC_EXTERN &&
            exp.nCallingConvention != CC_STUB)
        {
            /* Go to next token */
            if (!(pc = NextToken(pc)))
            {
                fprintf(stderr, "%s line %d: error: expected token\n", pszSourceFileName, nLine);
                return -13;
            }

            /* Verify syntax */
            if (*pc++ != '(')
            {
                fprintf(stderr, "%s line %d: error: expected '('\n", pszSourceFileName, nLine);
                return -14;
            }

            /* Skip whitespaces */
            while (*pc == ' ' || *pc == '\t') pc++;

            exp.nStackBytes = 0;
            while (*pc >= '0')
            {
                if (CompareToken(pc, "long"))
                {
                    exp.nStackBytes += 4;
                    exp.anArgs[exp.nArgCount] = ARG_LONG;
                }
                else if (CompareToken(pc, "double"))
                {
                    exp.nStackBytes += 8;
                    exp.anArgs[exp.nArgCount] = ARG_DBL;
                }
                else if (CompareToken(pc, "ptr"))
                {
                    exp.nStackBytes += 4; // sizeof(void*) on x86
                    exp.anArgs[exp.nArgCount] = ARG_PTR;
                }
                else if (CompareToken(pc, "str"))
                {
                    exp.nStackBytes += 4; // sizeof(void*) on x86
                    exp.anArgs[exp.nArgCount] = ARG_STR;
                }
                else if (CompareToken(pc, "wstr"))
                {
                    exp.nStackBytes += 4; // sizeof(void*) on x86
                    exp.anArgs[exp.nArgCount] = ARG_WSTR;
                }
                else if (CompareToken(pc, "int64"))
                {
                    exp.nStackBytes += 8;
                    exp.anArgs[exp.nArgCount] = ARG_INT64;
                }
                else if (CompareToken(pc, "int128"))
                {
                    exp.nStackBytes += 16;
                    exp.anArgs[exp.nArgCount] = ARG_INT128;
                }
                else if (CompareToken(pc, "float"))
                {
                    exp.nStackBytes += 4;
                    exp.anArgs[exp.nArgCount] = ARG_FLOAT;
                }
                else
                    fprintf(stderr, "%s line %d: error: expected type, got: %.10s\n", pszSourceFileName, nLine, pc);

                exp.nArgCount++;

                /* Go to next parameter */
                if (!(pc = NextToken(pc)))
                {
                    fprintf(stderr, "fail5\n");
                    return -15;
                }
            }

            /* Check syntax */
            if (*pc++ != ')')
            {
                fprintf(stderr, "%s line %d: error: expected ')'\n", pszSourceFileName, nLine);
                return -16;
            }
        }

        /* Handle special stub cases */
        if (exp.nCallingConvention == CC_STUB)
        {
            /* Check for c++ mangled name */
            if (pc[0] == '?')
            {
                //printf("Found c++ mangled name...\n");
                //
            }
            else
            {
                /* Check for stdcall name */
                const char *p = ScanToken(pc, '@');
                if (p && (p - pc < exp.strName.len))
                {
                    int i;

                    /* Truncate the name to before the @ */
                    exp.strName.len = (int)(p - pc);
                    if (exp.strName.len < 1)
                    {
                        fprintf(stderr, "%s line %d: error: unexpected @ found\n", pszSourceFileName, nLine);
                        return -1;
                    }
                    exp.nStackBytes = atoi(p + 1);
                    exp.nArgCount =  exp.nStackBytes / 4;
                    exp.nCallingConvention = CC_STDCALL;
                    exp.uFlags |= FL_STUB;
                    for (i = 0; i < exp.nArgCount; i++)
                        exp.anArgs[i] = ARG_LONG;
                }
            }
        }

        /* Get optional redirection */
        pc = NextToken(pc);
        if (pc)
        {
            exp.strTarget.buf = pc;
            exp.strTarget.len = TokenLength(pc);

            /* Check syntax (end of line) */
            if (NextToken(pc))
            {
                 fprintf(stderr, "%s line %d: error: additional tokens after ')'\n", pszSourceFileName, nLine);
                 return -17;
            }

            /* Don't relay-trace forwarded functions */
            exp.uFlags |= FL_NORELAY;
        }
        else
        {
            exp.strTarget.buf = NULL;
            exp.strTarget.len = 0;
        }

        /* Check for no-name without ordinal */
        if ((exp.uFlags & FL_ORDINAL) && (exp.nOrdinal == -1))
        {
            fprintf(stderr, "%s line %d: error: ordinal export without ordinal!\n", pszSourceFileName, nLine);
            return -1;
        }

        if (!gbMSComp && !gbNotPrivateNoWarn && gbImportLib && !(exp.uFlags & FL_PRIVATE))
        {
            for (i = 0; i < ARRAYSIZE(astrShouldBePrivate); i++)
            {
                if (strlen(astrShouldBePrivate[i]) == exp.strName.len &&
                    strncmp(exp.strName.buf, astrShouldBePrivate[i], exp.strName.len) == 0)
                {
                    fprintf(stderr, "%s line %d: warning: export of '%.*s' should be PRIVATE\n",
                            pszSourceFileName, nLine, exp.strName.len, exp.strName.buf);
                }
            }
        }

        OutputLine(fileDest, &exp);
        gbDebug = 0;
    }

    return 0;
}

void usage(void)
{
    printf("syntax: spec2def [<options> ...] <spec file>\n"
           "Possible options:\n"
           "  -h --help               print this help screen\n"
           "  -l=<file>               generate an asm lib stub\n"
           "  -d=<file>               generate a def file\n"
           "  -s=<file>               generate a stub file\n"
           "  --ms                    MSVC compatibility\n"
           "  -n=<name>               name of the dll\n"
           "  --implib                generate a def file for an import library\n"
           "  --no-private-warnings   suppress warnings about symbols that should be -private\n"
           "  -a=<arch>               set architecture to <arch> (i386, x86_64, arm)\n"
           "  --with-tracing          generate wine-like \"+relay\" trace trampolines (needs -s)\n");
}

int main(int argc, char *argv[])
{
    size_t nFileSize;
    char *pszSource, *pszDefFileName = NULL, *pszStubFileName = NULL, *pszLibStubName = NULL;
    char achDllName[40];
    FILE *file;
    int result = 0, i;

    if (argc < 2)
    {
        usage();
        return -1;
    }

    /* Read options */
    for (i = 1; i < argc && *argv[i] == '-'; i++)
    {
        if ((strcasecmp(argv[i], "--help") == 0) ||
            (strcasecmp(argv[i], "-h") == 0))
        {
            usage();
            return 0;
        }
        else if (argv[i][1] == 'd' && argv[i][2] == '=')
        {
            pszDefFileName = argv[i] + 3;
        }
        else if (argv[i][1] == 'l' && argv[i][2] == '=')
        {
            pszLibStubName = argv[i] + 3;
        }
        else if (argv[i][1] == 's' && argv[i][2] == '=')
        {
            pszStubFileName = argv[i] + 3;
        }
        else if (argv[i][1] == 'n' && argv[i][2] == '=')
        {
            pszDllName = argv[i] + 3;
        }
        else if (strcasecmp(argv[i], "--version=0x") == 0)
        {
            guOsVersion = strtoul(argv[i] + sizeof("--version=0x"), NULL, 16);
        }
        else if (strcasecmp(argv[i], "--implib") == 0)
        {
            gbImportLib = 1;
        }
        else if (strcasecmp(argv[i], "--ms") == 0)
        {
            gbMSComp = 1;
        }
        else if (strcasecmp(argv[i], "--no-private-warnings") == 0)
        {
            gbNotPrivateNoWarn = 1;
        }
        else if (strcasecmp(argv[i], "--with-tracing") == 0)
        {
            if (!pszStubFileName)
            {
                fprintf(stderr, "Error: cannot use --with-tracing without -s option.\n");
                return -1;
            }
            gbTracing = 1;
        }
        else if (argv[i][1] == 'a' && argv[i][2] == '=')
        {
            pszArchString = argv[i] + 3;
        }
        else
        {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            return -1;
        }
    }

    if (strcasecmp(pszArchString, "i386") == 0)
    {
        giArch = ARCH_X86;
        gpszUnderscore = "_";
    }
    else if (strcasecmp(pszArchString, "x86_64") == 0) giArch = ARCH_AMD64;
    else if (strcasecmp(pszArchString, "ia64") == 0) giArch = ARCH_IA64;
    else if (strcasecmp(pszArchString, "arm") == 0) giArch = ARCH_ARM;
    else if (strcasecmp(pszArchString, "ppc") == 0) giArch = ARCH_PPC;

    if ((giArch == ARCH_AMD64) || (giArch == ARCH_IA64))
    {
        pszArchString2 = "win64";
    }
    else
        pszArchString2 = "win32";

    /* Set a default dll name */
    if (!pszDllName)
    {
        char *p1, *p2;
        size_t len;

        p1 = strrchr(argv[i], '\\');
        if (!p1) p1 = strrchr(argv[i], '/');
        p2 = p1 = p1 ? p1 + 1 : argv[i];

        /* walk up to '.' */
        while (*p2 != '.' && *p2 != 0) p2++;
        len = p2 - p1;
        if (len >= sizeof(achDllName) - 5)
        {
            fprintf(stderr, "name too long: %s\n", p1);
            return -2;
        }

        strncpy(achDllName, p1, len);
        strncpy(achDllName + len, ".dll", sizeof(achDllName) - len);
        pszDllName = achDllName;
    }

    /* Open input file */
    pszSourceFileName = argv[i];
    file = fopen(pszSourceFileName, "r");
    if (!file)
    {
        fprintf(stderr, "error: could not open file %s\n", pszSourceFileName);
        return -3;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    nFileSize = ftell(file);
    rewind(file);

    /* Allocate memory buffer */
    pszSource = malloc(nFileSize + 1);
    if (!pszSource)
    {
        fclose(file);
        return -4;
    }

    /* Load input file into memory */
    nFileSize = fread(pszSource, 1, nFileSize, file);
    fclose(file);

    /* Zero terminate the source */
    pszSource[nFileSize] = '\0';

    if (pszDefFileName)
    {
        /* Open output file */
        file = fopen(pszDefFileName, "w");
        if (!file)
        {
            fprintf(stderr, "error: could not open output file %s\n", argv[i + 1]);
            return -5;
        }

        OutputHeader_def(file, pszDllName);
        result = ParseFile(pszSource, file, OutputLine_def);
        fclose(file);
    }

    if (pszStubFileName)
    {
        /* Open output file */
        file = fopen(pszStubFileName, "w");
        if (!file)
        {
            fprintf(stderr, "error: could not open output file %s\n", argv[i + 1]);
            return -5;
        }

        OutputHeader_stub(file);
        result = ParseFile(pszSource, file, OutputLine_stub);
        fclose(file);
    }

    if (pszLibStubName)
    {
        /* Open output file */
        file = fopen(pszLibStubName, "w");
        if (!file)
        {
            fprintf(stderr, "error: could not open output file %s\n", argv[i + 1]);
            return -5;
        }

        OutputHeader_asmstub(file, pszDllName);
        result = ParseFile(pszSource, file, OutputLine_asmstub);
        fprintf(file, "\n    END\n");
        fclose(file);
    }

    return result;
}
