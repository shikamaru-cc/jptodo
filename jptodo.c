#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#include "botlib.h"

#define JPTODO_CREATE_TODO_LIST \
    "CREATE TABLE IF NOT EXISTS TodoList(" \
        "id INTEGER PRIMARY KEY AUTOINCREMENT," \
        "topic TEXT NOT NULL," \
        "content TEXT NOT NULL," \
        "create_at DATETIME NOT NULL" \
    ");"

#define JPTD_SELECT_UNFINISHED \
    "SELECT id, topic, content, DATETIME(create_at, 'localtime') " \
    "FROM TodoList ORDER BY topic, create_at"

#define JPTD_SELECT_TOPIC_UNFINISHED \
    "SELECT id, topic, content, DATETIME(create_at, 'localtime') " \
    "FROM TodoList WHERE topic=?s ORDER BY topic, create_at"

#define JPTD_ADD_ENTRY \
    "INSERT INTO TodoList(topic, content, create_at) VALUES(?s, ?s, DATETIME('now'))"

#define JPTD_DELETE_ENTRY "DELETE FROM TodoList WHERE id=?i"

typedef long long BitMap;

#define BitMapSize ((int)(sizeof(long long)*8))

inline void BitMapEmpty(BitMap *bitmap) {
    *bitmap = 0;
}

int BitMapSet(BitMap *bitmap, int i) {
    if (i <= 0 || i > BitMapSize) {
        return 0;
    }
    *bitmap |= 1<<(i-1);
    return 1;
}

int BitMapGet(BitMap *bitmap, int i) {
    if (i <= 0 || i > BitMapSize) {
        return 0;
    }
    return *bitmap & (1<<(i-1)) ? 1 : 0;
}

typedef struct TodoEntry {
    int64_t id;
    sds topic;
    sds content;
    sds create_at;
    struct TodoEntry *next;
} TodoEntry;

TodoEntry *todoEntryNew() {
    TodoEntry *entry = xmalloc(sizeof(TodoEntry));
    if (entry == NULL) return NULL;
    entry->id = 0;
    entry->topic = entry->content = entry->create_at = NULL;
    entry->next = NULL;
    return entry;
}

void todoEntryFree(TodoEntry *entry) {
    if (entry == NULL) return;
    sdsfree(entry->topic);
    sdsfree(entry->content);
    sdsfree(entry->create_at);
    todoEntryFree(entry->next);
    xfree(entry);
}

TodoEntry *todoGetUnfinishedEntries(sqlite3 *dbhandle, const char *topic) {
    sqlRow row;
    if (topic) {
        sqlSelect(dbhandle, &row, JPTD_SELECT_TOPIC_UNFINISHED, topic);
    } else {
        sqlSelect(dbhandle, &row, JPTD_SELECT_UNFINISHED);
    }
    TodoEntry *entries = NULL, *entry = NULL;
    TodoEntry **next_entry = &entries;
    while (sqlNextRow(&row)) {
        entry = todoEntryNew();
        if (entry == NULL) break;

        entry->id = row.col[0].i;
        entry->topic = sdsnewlen(row.col[1].s, row.col[1].i);
        entry->content = sdsnewlen(row.col[2].s, row.col[2].i);
        entry->create_at = sdsnewlen(row.col[3].s, row.col[3].i);

        *next_entry = entry;
        next_entry = &entry->next;
    }
    sqlEnd(&row);
    return entries;
}

sds printTodoEntries(TodoEntry *entry) {
    if (entry == NULL) return sdsnew("Nothing to do\n");
    sds s = sdsempty();
    sds current_topic = NULL;
    int i = 1;
    while (entry) {
        if (current_topic == NULL || sdscmp(entry->topic, current_topic)) {
            s = sdscatprintf(s, "%s:\n", entry->topic);
            current_topic = entry->topic;
            i = 1;
        }
        s = sdscatprintf(s, "    %-3d %s\n", i, entry->content);
        entry = entry->next;
        i++;
    }
    return s;
}

int todoCount(sqlite3 *dbhandle, const char *topic) {
    sqlRow row;
    sqlSelect(dbhandle, &row, "SELECT COUNT(*) FROM TodoList WHERE topic=?s", topic);
    int count = sqlNextRow(&row) ? row.col[0].i : -1;
    sqlEnd(&row);
    return count;
}

int todoAdd(sqlite3 *dbhandle, const char *topic, const char *content) {
    return sqlInsert(dbhandle, JPTD_ADD_ENTRY, topic, content) ? 1 : 0;
}

#define HELP_MSG \
    "/add <topic> <content>\n" \
    "/ls\n" \
    "/ls <topic>\n" \
    "/del <topic> 1 2 3\n"

void handleRequest(sqlite3 *dbhandle, BotRequest *br) {
    if (br->argc == 0) return;

    sds cmd = br->argv[0];
    int cmdlen = sdslen(cmd);

    printf("Receive request: %s\n", br->request);

    if (!strcasecmp(cmd, "/add") && br->argc >= 2) {
        sds topic = br->argv[1];

        int count = todoCount(dbhandle, topic);
        if (count < 0) {
            botSendMessage(br->target, "OOPS! Failed to add!", br->msg_id);
            return;
        } else if (count > BitMapSize) {
            botSendMessage(br->target, "OOPS! You have too many todos! Finish them!", br->msg_id);
            return;
        }

        sds content = sdsdup(br->request);
        sdsrange(content, cmdlen, -1);
        sdstrim(content, " ");
        sdsrange(content, sdslen(topic), -1);
        sdstrim(content, " ");

        todoAdd(dbhandle, topic, content);
        botSendMessage(br->target, "Ok, remember to do.", br->msg_id);

        sdsfree(content);

    } else if (!strcasecmp(cmd, "/ls")) {
        TodoEntry *entry = todoGetUnfinishedEntries(dbhandle, (br->argc > 1) ? br->argv[1] : NULL);
        sds msg = printTodoEntries(entry);
        botSendMessage(br->target, msg, 0);
        todoEntryFree(entry);
        sdsfree(msg);

    } else if (!strcasecmp(cmd, "/del") && br->argc >= 3) {
        BitMap todel; BitMapEmpty(&todel);
        for (int i = 2; i < br->argc; i++)
            BitMapSet(&todel, atoi(br->argv[i]));

        TodoEntry *entries = todoGetUnfinishedEntries(dbhandle, br->argv[1]);
        TodoEntry *entry = entries;
        int i = 1;
        while (entry) {
            if (BitMapGet(&todel, i)) {
                sqlQuery(dbhandle, JPTD_DELETE_ENTRY, entry->id);
            }
            entry = entry->next;
            i++;
        }
        botSendMessage(br->target, "GOOD!", br->msg_id);

        todoEntryFree(entries);

    } else {
        botSendMessage(br->target, HELP_MSG, 0);
    }
}

int main(int argc, char **argv) {
    static char *triggers[] = {
        NULL,
    };
    startBot(JPTODO_CREATE_TODO_LIST, argc, argv, TB_FLAGS_NONE, handleRequest, NULL, triggers);
    return 0; /* Never reached. */
}
