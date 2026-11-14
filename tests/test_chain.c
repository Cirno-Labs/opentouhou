#include "stg/chain.h"

#include "test.h"

#include "core/base/utils.h"

#include <stdint.h>

typedef struct {
    char text[16];
    int length;
} LOG;

// Logs its tag on each call and returns status repeat times, then
// final. With remove set, removes that element on its first call.
typedef struct {
    LOG *log;
    char tag;
    CHAIN_STATUS status;
    int repeat;
    CHAIN_STATUS final;
    CHAIN_ELEM *remove;
    RESULT added_result;
    int added_calls;
    int deleted_calls;
} ELEM;

typedef struct {
    char tag;
    CHAIN_STATUS status;
    int16_t priority;
} SPEC;

typedef struct {
    CHAIN chain;
    LOG log;
    CHAIN_ELEM elems[4];
    ELEM args[4];
    int calc_ends;
    // Makes on_calc_end return false.
    bool hold;
} FIXTURE;

static void M_Log(LOG *const log, const char tag)
{
    log->text[log->length++] = tag;
    log->text[log->length] = '\0';
}

static CHAIN_STATUS M_Callback(void *const arg)
{
    ELEM *const elem = arg;
    M_Log(elem->log, elem->tag);
    if (elem->remove != nullptr) {
        CHAIN_ELEM *const target = elem->remove;
        elem->remove = nullptr;
        Chain_Remove(target);
    }
    return elem->repeat-- > 0 ? elem->status : elem->final;
}

static RESULT M_Added(void *const arg)
{
    ELEM *const elem = arg;
    elem->added_calls++;
    return elem->added_result;
}

static void M_Deleted(void *const arg)
{
    ELEM *const elem = arg;
    elem->deleted_calls++;
}

static void M_LogDeleted(void *const arg)
{
    ELEM *const elem = arg;
    M_Log(elem->log, elem->tag);
}

static bool M_CalcEnd(void *const arg)
{
    FIXTURE *const fixture = arg;
    fixture->calc_ends++;
    return !fixture->hold;
}

// Adds one element per spec, in order, to the draw chain when draw is
// set and to the update chain otherwise.
static void M_Build(
    FIXTURE *const fixture, const bool draw, const SPEC *const specs,
    const int count)
{
    *fixture = (FIXTURE) {};
    Chain_Init(&fixture->chain);
    fixture->chain.on_calc_end = M_CalcEnd;
    fixture->chain.on_calc_end_arg = fixture;
    for (int i = 0; i < count; i++) {
        ELEM *const arg = &fixture->args[i];
        *arg = (ELEM) { .log = &fixture->log,
                        .tag = specs[i].tag,
                        .final = specs[i].status };
        CHAIN_ELEM *const elem = &fixture->elems[i];
        Chain_InitElem(elem, M_Callback, arg);
        elem->deleted = M_Deleted;
        Result_Ignore(
            draw ? Chain_AddDraw(&fixture->chain, elem, specs[i].priority)
                 : Chain_AddCalc(&fixture->chain, elem, specs[i].priority));
    }
}

#define BUILD(fixture, draw, ...)                                              \
    M_Build(                                                                   \
        fixture, draw, (const SPEC[]) { __VA_ARGS__ },                         \
        sizeof((const SPEC[]) { __VA_ARGS__ }) / sizeof(SPEC))

TEST(each_status_decides_the_run)
{
    static const struct {
        CHAIN_STATUS status;
        CHAIN_RUN run;
        const char *log;
        int deleted_calls;
    } cases[] = {
        { CHAIN_REMOVE, CHAIN_RUN_CONTINUE, "12", 1 },
        { CHAIN_CONTINUE, CHAIN_RUN_CONTINUE, "12", 0 },
        { CHAIN_STOP, CHAIN_RUN_CONTINUE, "1", 0 },
        { CHAIN_QUIT, CHAIN_RUN_QUIT, "1", 0 },
        { CHAIN_RESET, CHAIN_RUN_RESET, "1", 0 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        FIXTURE fixture;
        BUILD(
            &fixture, false, { '1', cases[i].status, 0 },
            { '2', CHAIN_CONTINUE, 1 });
        CHECK(Chain_RunCalc(&fixture.chain) == cases[i].run);
        CHECK_EQ_STR(fixture.log.text, cases[i].log);
        CHECK_EQ_INT(fixture.args[0].deleted_calls, cases[i].deleted_calls);
        CHECK_EQ_INT(fixture.calc_ends, 1);
    }
}

TEST(again_calls_the_same_callback_again)
{
    FIXTURE fixture;
    BUILD(&fixture, false, { '1', CHAIN_CONTINUE, 0 });
    fixture.args[0].status = CHAIN_AGAIN;
    fixture.args[0].repeat = 2;
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "111");
}

TEST(restart_runs_from_the_start_and_ends_a_calc)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { '1', CHAIN_CONTINUE, 0 },
        { '2', CHAIN_CONTINUE, 1 });
    fixture.args[1].status = CHAIN_RESTART;
    fixture.args[1].repeat = 1;
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "1212");
    CHECK_EQ_INT(fixture.calc_ends, 2);
}

TEST(calc_end_false_holds_a_pending_restart)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { '1', CHAIN_CONTINUE, 0 }, { '2', CHAIN_RESTART, 1 });
    fixture.hold = true;
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "12");
    CHECK_EQ_INT(fixture.calc_ends, 1);
}

TEST(calc_end_false_turns_quit_and_reset_into_continue)
{
    static const CHAIN_STATUS statuses[] = { CHAIN_QUIT, CHAIN_RESET };
    for (size_t i = 0; i < ARRAY_SIZE(statuses); i++) {
        FIXTURE fixture;
        BUILD(&fixture, false, { '1', statuses[i], 0 });
        fixture.hold = true;
        CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
        CHECK_EQ_INT(fixture.calc_ends, 1);
    }
}

TEST(restart_runs_again_without_calc_end)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { '1', CHAIN_CONTINUE, 0 },
        { '2', CHAIN_CONTINUE, 1 });
    fixture.chain.on_calc_end = nullptr;
    fixture.args[1].status = CHAIN_RESTART;
    fixture.args[1].repeat = 1;
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "1212");
}

TEST(an_empty_run_quits)
{
    FIXTURE fixture;
    BUILD(&fixture, false, { '1', CHAIN_CONTINUE, 0 });
    Chain_Remove(&fixture.elems[0]);
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_QUIT);
}

TEST(priority_orders_and_equal_priorities_keep_insertion_order)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { 'c', CHAIN_CONTINUE, 9 }, { 'a', CHAIN_CONTINUE, 0 },
        { 'b', CHAIN_CONTINUE, 5 }, { 'B', CHAIN_CONTINUE, 5 });
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "abBc");
}

TEST(an_element_without_callback_is_skipped_and_not_counted)
{
    FIXTURE fixture;
    BUILD(&fixture, false, { '1', CHAIN_CONTINUE, 0 });
    fixture.elems[0].callback = nullptr;
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_QUIT);
    CHECK_EQ_STR(fixture.log.text, "");
}

TEST(removing_every_element_is_not_an_empty_run)
{
    FIXTURE fixture;
    BUILD(&fixture, false, { '1', CHAIN_REMOVE, 0 });
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK(fixture.chain.calc.next == nullptr);
}

TEST(stopping_on_the_first_callback_is_not_an_empty_run)
{
    FIXTURE fixture;
    BUILD(&fixture, false, { '1', CHAIN_STOP, 0 });
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
}

TEST(removing_the_next_element_skips_it)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { '1', CHAIN_CONTINUE, 0 }, { '2', CHAIN_CONTINUE, 1 },
        { '3', CHAIN_CONTINUE, 2 });
    fixture.args[0].remove = &fixture.elems[1];
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "13");
    CHECK_EQ_INT(fixture.args[1].deleted_calls, 1);
}

TEST(removing_itself_ends_the_run)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { '1', CHAIN_CONTINUE, 0 },
        { '2', CHAIN_CONTINUE, 1 });
    fixture.args[0].remove = &fixture.elems[0];
    CHECK(Chain_RunCalc(&fixture.chain) == CHAIN_RUN_CONTINUE);
    CHECK_EQ_STR(fixture.log.text, "1");
}

TEST(each_status_decides_the_draw_run)
{
    static const struct {
        CHAIN_STATUS status;
        const char *log;
        int deleted_calls;
    } cases[] = {
        { CHAIN_REMOVE, "12", 1 }, { CHAIN_CONTINUE, "12", 0 },
        { CHAIN_STOP, "1", 0 },    { CHAIN_QUIT, "1", 0 },
        { CHAIN_RESET, "1", 0 },   { CHAIN_RESTART, "12", 0 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        FIXTURE fixture;
        BUILD(
            &fixture, true, { '1', cases[i].status, 0 },
            { '2', CHAIN_CONTINUE, 1 });
        Chain_RunDraw(&fixture.chain);
        CHECK_EQ_STR(fixture.log.text, cases[i].log);
        CHECK_EQ_INT(fixture.args[0].deleted_calls, cases[i].deleted_calls);
    }
}

TEST(draw_calls_the_same_callback_again)
{
    FIXTURE fixture;
    BUILD(&fixture, true, { '1', CHAIN_CONTINUE, 0 });
    fixture.args[0].status = CHAIN_AGAIN;
    fixture.args[0].repeat = 2;
    Chain_RunDraw(&fixture.chain);
    CHECK_EQ_STR(fixture.log.text, "111");
}

TEST(draw_treats_restart_as_continue)
{
    FIXTURE fixture;
    BUILD(
        &fixture, true, { '1', CHAIN_RESTART, 0 }, { '2', CHAIN_CONTINUE, 1 });
    Chain_RunDraw(&fixture.chain);
    CHECK_EQ_STR(fixture.log.text, "12");
    CHECK_EQ_INT(fixture.calc_ends, 0);
}

TEST(draw_priority_orders_and_equal_priorities_keep_insertion_order)
{
    FIXTURE fixture;
    BUILD(
        &fixture, true, { 'b', CHAIN_CONTINUE, 5 }, { 'a', CHAIN_CONTINUE, 0 },
        { 'B', CHAIN_CONTINUE, 5 }, { 'A', CHAIN_CONTINUE, 0 });
    Chain_RunDraw(&fixture.chain);
    CHECK_EQ_STR(fixture.log.text, "aAbB");
}

TEST(added_runs_once_and_its_failure_is_returned)
{
    CHAIN chain;
    Chain_Init(&chain);
    ELEM arg = { .added_result = FAIL("broken") };
    CHAIN_ELEM elem;
    Chain_InitElem(&elem, M_Callback, &arg);
    elem.added = M_Added;
    const RESULT result = Chain_AddCalc(&chain, &elem, 0);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK_EQ_INT(arg.added_calls, 1);
    CHECK(elem.added == nullptr);
}

TEST(remove_calls_deleted_once_and_clears_the_callback)
{
    FIXTURE fixture;
    BUILD(&fixture, false, { '1', CHAIN_CONTINUE, 0 });
    Chain_Remove(&fixture.elems[0]);
    Chain_Remove(&fixture.elems[0]);
    CHECK_EQ_INT(fixture.args[0].deleted_calls, 1);
    CHECK(fixture.elems[0].callback == nullptr);
}

TEST(release_removes_the_update_chain_first_in_order)
{
    FIXTURE fixture;
    BUILD(
        &fixture, false, { '1', CHAIN_CONTINUE, 0 },
        { '2', CHAIN_CONTINUE, 1 });
    Chain_InitElem(&fixture.elems[2], M_Callback, &fixture.args[2]);
    fixture.args[2] = (ELEM) { .log = &fixture.log, .tag = '3' };
    Result_Ignore(Chain_AddDraw(&fixture.chain, &fixture.elems[2], 0));
    for (int i = 0; i < 3; i++) {
        fixture.elems[i].deleted = M_LogDeleted;
    }
    Chain_Release(&fixture.chain);
    CHECK_EQ_STR(fixture.log.text, "123");
    CHECK(fixture.chain.calc.next == nullptr);
    CHECK(fixture.chain.draw.next == nullptr);
}
