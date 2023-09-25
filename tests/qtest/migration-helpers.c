/*
 * QTest migration helpers
 *
 * Copyright (c) 2016-2018 Red Hat, Inc. and/or its affiliates
 *   based on the vhost-user-test.c that is:
 *      Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qapi/qmp/qjson.h"
#include "qapi/qmp/qlist.h"
#include "qapi/qmp/qstring.h"

#include "migration-helpers.h"

/*
 * Number of seconds we wait when looking for migration
 * status changes, to avoid test suite hanging forever
 * when things go wrong. Needs to be higher enough to
 * avoid false positives on loaded hosts.
 */
#define MIGRATION_STATUS_WAIT_TIMEOUT 120

bool migrate_watch_for_stop(QTestState *who, const char *name,
                            QDict *event, void *opaque)
{
    bool *seen = opaque;

    if (g_str_equal(name, "STOP")) {
        *seen = true;
        return true;
    }

    return false;
}

bool migrate_watch_for_resume(QTestState *who, const char *name,
                              QDict *event, void *opaque)
{
    bool *seen = opaque;

    if (g_str_equal(name, "RESUME")) {
        *seen = true;
        return true;
    }

    return false;
}

/*
 * Send QMP command "migrate".
 * Arguments are built from @fmt... (formatted like
 * qobject_from_jsonf_nofail()) with "uri": @uri spliced in.
 */
void migrate_qmp(QTestState *who, const char *uri, const char *fmt, ...)
{
    va_list ap;
    QDict *args;

    va_start(ap, fmt);
    args = qdict_from_vjsonf_nofail(fmt, ap);
    va_end(ap);

    g_assert(!qdict_haskey(args, "uri"));
    qdict_put_str(args, "uri", uri);

    qtest_qmp_assert_success(who,
                             "{ 'execute': 'migrate', 'arguments': %p}", args);
}

/*
 * Note: caller is responsible to free the returned object via
 * qobject_unref() after use
 */
QDict *migrate_query(QTestState *who)
{
    return qtest_qmp_assert_success_ref(who, "{ 'execute': 'query-migrate' }");
}

QDict *migrate_query_not_failed(QTestState *who)
{
    const char *status;
    QDict *rsp = migrate_query(who);
    status = qdict_get_str(rsp, "status");
    if (g_str_equal(status, "failed")) {
        g_printerr("query-migrate shows failed migration: %s\n",
                   qdict_get_str(rsp, "error-desc"));
    }
    g_assert(!g_str_equal(status, "failed"));
    return rsp;
}

/*
 * Note: caller is responsible to free the returned object via
 * g_free() after use
 */
static gchar *migrate_query_status(QTestState *who)
{
    QDict *rsp_return = migrate_query(who);
    gchar *status = g_strdup(qdict_get_str(rsp_return, "status"));

    g_assert(status);
    qobject_unref(rsp_return);

    return status;
}

static bool check_migration_status(QTestState *who, const char *goal,
                                   const char **ungoals)
{
    bool ready;
    char *current_status;
    const char **ungoal;

    current_status = migrate_query_status(who);
    ready = strcmp(current_status, goal) == 0;
    if (!ungoals) {
        g_assert_cmpstr(current_status, !=, "failed");
        /*
         * If looking for a state other than completed,
         * completion of migration would cause the test to
         * hang.
         */
        if (strcmp(goal, "completed") != 0) {
            g_assert_cmpstr(current_status, !=, "completed");
        }
    } else {
        for (ungoal = ungoals; *ungoal; ungoal++) {
            g_assert_cmpstr(current_status, !=,  *ungoal);
        }
    }
    g_free(current_status);
    return ready;
}

void wait_for_migration_status(QTestState *who,
                               const char *goal, const char **ungoals)
{
    g_test_timer_start();
    while (!check_migration_status(who, goal, ungoals)) {
        usleep(1000);

        g_assert(g_test_timer_elapsed() < MIGRATION_STATUS_WAIT_TIMEOUT);
    }
}

void wait_for_migration_complete(QTestState *who)
{
    wait_for_migration_status(who, "completed", NULL);
}

void wait_for_migration_fail(QTestState *from, bool allow_active)
{
    g_test_timer_start();
    QDict *rsp_return;
    char *status;
    bool failed;

    do {
        status = migrate_query_status(from);
        bool result = !strcmp(status, "setup") || !strcmp(status, "failed") ||
            (allow_active && !strcmp(status, "active"));
        if (!result) {
            fprintf(stderr, "%s: unexpected status status=%s allow_active=%d\n",
                    __func__, status, allow_active);
        }
        g_assert(result);
        failed = !strcmp(status, "failed");
        g_free(status);

        g_assert(g_test_timer_elapsed() < MIGRATION_STATUS_WAIT_TIMEOUT);
    } while (!failed);

    /* Is the machine currently running? */
    rsp_return = qtest_qmp_assert_success_ref(from,
                                              "{ 'execute': 'query-status' }");
    g_assert(qdict_haskey(rsp_return, "running"));
    g_assert(qdict_get_bool(rsp_return, "running"));
    qobject_unref(rsp_return);
}

static char *query_pkg_version(QTestState *who)
{
    QDict *rsp;
    char *pkg;

    rsp = qtest_qmp_assert_success_ref(who, "{ 'execute': 'query-version' }");
    g_assert(rsp);

    pkg = g_strdup(qdict_get_str(rsp, "package"));
    qobject_unref(rsp);

    return pkg;
}

static QList *query_machines(void)
{
    QDict *response;
    QList *list;
    QTestState *qts;

    qts = qtest_init("-machine none");
    response = qtest_qmp(qts, "{ 'execute': 'query-machines' }");
    g_assert(response);
    list = qdict_get_qlist(response, "return");
    g_assert(list);

    qtest_quit(qts);
    return list;
}

static char *get_default_machine(QList *list)
{
    QDict *info;
    QListEntry *entry;
    QString *qstr;
    char *name = NULL;

    QLIST_FOREACH_ENTRY(list, entry) {
        info = qobject_to(QDict, qlist_entry_obj(entry));
        g_assert(info);

        if (qdict_get(info, "is-default")) {
            qstr = qobject_to(QString, qdict_get(info, "name"));
            g_assert(qstr);
            name = g_strdup(qstring_get_str(qstr));
            break;
        }
    }

    g_assert(name);
    return name;
}

static bool search_default_machine(QList *list, const char *theirs)
{
    QDict *info;
    QListEntry *entry;
    QString *qstr;

    if (!theirs) {
        return false;
    }

    QLIST_FOREACH_ENTRY(list, entry) {
        info = qobject_to(QDict, qlist_entry_obj(entry));
        g_assert(info);

        qstr = qobject_to(QString, qdict_get(info, "name"));
        g_assert(qstr);

        if (g_str_equal(qstring_get_str(qstr), theirs)) {
            return true;
        }
    }
    return false;
}

/*
 * We need to ensure that both QEMU instances set via the QTEST_QEMU_*
 * vars will use the same machine type. Use a custom query_machines
 * function because the generic one in libqtest has a cache that would
 * return the same machines for both binaries.
 */
char *find_common_machine_type(const char *bin)
{
    QList *m1, *m2;
    g_autofree char *def1 = NULL;
    g_autofree char *def2 = NULL;
    const char *qemu_bin = getenv("QTEST_QEMU_BINARY");

    m1 = query_machines();

    g_setenv("QTEST_QEMU_BINARY", bin, true);
    m2 = query_machines();
    g_setenv("QTEST_QEMU_BINARY", qemu_bin, true);

    def1 = get_default_machine(m1);
    def2 = get_default_machine(m2);

    if (g_str_equal(def1, def2)) {
        /* either can be used */
        return g_strdup(def1);
    }

    if (search_default_machine(m1, def2)) {
        return g_strdup(def2);
    }

    if (search_default_machine(m2, def1)) {
        return g_strdup(def1);
    }

    g_assert_not_reached();
}

/*
 * Init a guest for migration tests using an alternate QEMU binary for
 * either the source or destination, depending on @var. The other
 * binary should be set as usual via QTEST_QEMU_BINARY.
 *
 * Expected values:
 *   QTEST_QEMU_SRC
 *   QTEST_QEMU_DST
 *
 * Warning: The generic parts of qtest could be using
 * QTEST_QEMU_BINARY to query for properties before we reach the
 * migration code. If the alternate binary is too dissimilar that
 * could cause issues.
 */
static QTestState *init_vm(const char *extra_args, const char *var)
{
    const char *alt_bin = getenv(var);
    const char *qemu_bin = getenv("QTEST_QEMU_BINARY");
    g_autofree char *pkg = NULL;
    bool src = !!strstr(var, "SRC");
    QTestState *qts;

    if (alt_bin) {
        g_setenv("QTEST_QEMU_BINARY", alt_bin, true);
    }

    qts = qtest_init(extra_args);
    pkg = query_pkg_version(qts);

    g_test_message("Using %s (%s) as migration %s",
                   alt_bin ? alt_bin : qemu_bin,
                   pkg,
                   src ? "source" : "destination");

    if (alt_bin) {
        /* restore the original */
        g_setenv("QTEST_QEMU_BINARY", qemu_bin, true);
    }
    return qts;
}

QTestState *mig_init_src(const char *extra_args)
{
    return init_vm(extra_args, "QTEST_QEMU_SRC");
}

QTestState *mig_init_dst(const char *extra_args)
{
    return init_vm(extra_args, "QTEST_QEMU_DST");
}
