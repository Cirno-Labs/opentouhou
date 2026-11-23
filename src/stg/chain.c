#include "stg/chain.h"

CHAIN g_Chain;

#include "core/base/assert.h"

static RESULT M_Add(
    CHAIN_ELEM *const head, CHAIN_ELEM *const elem, const int16_t priority)
{
    ASSERT(priority >= 0);
    elem->priority = priority;
    CHAIN_ELEM *cur = head;
    while (cur->next != nullptr && cur->priority <= priority) {
        cur = cur->next;
    }
    if (cur->priority > priority) {
        elem->next = cur;
        elem->prev = cur->prev;
        elem->prev->next = elem;
        cur->prev = elem;
    } else {
        elem->next = nullptr;
        elem->prev = cur;
        cur->next = elem;
    }

    RESULT (*const added)(void *) = elem->added;
    elem->added = nullptr;
    return added != nullptr ? added(elem->arg) : OK;
}

// Returns the status that ended the run, or CHAIN_CONTINUE at the end of
// the list. count is the number of callbacks that ran. CHAIN_RESTART ends
// the run only when restart is set.
static CHAIN_STATUS M_Run(
    CHAIN_ELEM *elem, const bool restart, int *const count)
{
    *count = 0;
    while (elem != nullptr) {
        if (elem->callback == nullptr) {
            elem = elem->next;
            continue;
        }
        CHAIN_STATUS status;
        do {
            status = elem->callback(elem->arg);
        } while (status == CHAIN_AGAIN);
        if (status == CHAIN_STOP || status == CHAIN_QUIT
            || status == CHAIN_RESET || (status == CHAIN_RESTART && restart)) {
            return status;
        }
        (*count)++;
        CHAIN_ELEM *const next = elem->next;
        if (status == CHAIN_REMOVE) {
            Chain_Remove(elem);
        }
        elem = next;
    }
    return CHAIN_CONTINUE;
}

static bool M_CalcEnd(void)
{
    return g_Chain.on_calc_end == nullptr
        || g_Chain.on_calc_end(g_Chain.on_calc_end_arg);
}

void Chain_Init(void)
{
    Chain_InitElem(&g_Chain.calc, nullptr, nullptr);
    Chain_InitElem(&g_Chain.draw, nullptr, nullptr);
    g_Chain.on_calc_end = nullptr;
    g_Chain.on_calc_end_arg = nullptr;
}

void Chain_InitElem(
    CHAIN_ELEM *const elem, CHAIN_STATUS (*const callback)(void *),
    void *const arg)
{
    *elem = (CHAIN_ELEM) { .callback = callback, .arg = arg };
}

RESULT Chain_AddCalc(CHAIN_ELEM *const elem, const int16_t priority)
{
    return M_Add(&g_Chain.calc, elem, priority);
}

RESULT Chain_AddDraw(CHAIN_ELEM *const elem, const int16_t priority)
{
    return M_Add(&g_Chain.draw, elem, priority);
}

void Chain_Remove(CHAIN_ELEM *const elem)
{
    if (elem->prev == nullptr) {
        return;
    }
    elem->callback = nullptr;
    elem->prev->next = elem->next;
    if (elem->next != nullptr) {
        elem->next->prev = elem->prev;
    }
    elem->prev = nullptr;
    elem->next = nullptr;

    void (*const deleted)(void *) = elem->deleted;
    elem->deleted = nullptr;
    if (deleted != nullptr) {
        deleted(elem->arg);
    }
}

CHAIN_RUN Chain_RunCalc(void)
{
    int count;
    CHAIN_STATUS status;
    while ((status = M_Run(&g_Chain.calc, true, &count)) == CHAIN_RESTART) {
        if (!M_CalcEnd()) {
            return CHAIN_RUN_CONTINUE;
        }
    }
    if (!M_CalcEnd()) {
        return CHAIN_RUN_CONTINUE;
    }
    if (status == CHAIN_QUIT || (status == CHAIN_CONTINUE && count == 0)) {
        return CHAIN_RUN_QUIT;
    }
    return status == CHAIN_RESET ? CHAIN_RUN_RESET : CHAIN_RUN_CONTINUE;
}

void Chain_RunDraw(void)
{
    int count;
    M_Run(&g_Chain.draw, false, &count);
}

void Chain_Release(void)
{
    while (g_Chain.calc.next != nullptr) {
        Chain_Remove(g_Chain.calc.next);
    }
    while (g_Chain.draw.next != nullptr) {
        Chain_Remove(g_Chain.draw.next);
    }
}
