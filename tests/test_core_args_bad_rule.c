#include "core/args.h"

typedef struct {
    bool fast;
} M_ARGS;

static const ARG_OPTION m_Options[] = {
    ARG_FLAG_OF(M_ARGS, "--fast", fast, "run fast"),
};

static const ARG_RULE m_Rules[] = {
    { ARG_EXCLUSIVE, { "--fast", "--slow" } },
};

static const ARG_SCHEMA m_Schema =
    ARG_SCHEMA_OF("tool", nullptr, m_Options, m_Rules);

int main(void)
{
    char *argv[] = { "tool" };
    M_ARGS args;
    bool help;
    return Result_Ignore(Args_Parse(&m_Schema, 1, argv, &args, &help)) ? 0 : 1;
}
