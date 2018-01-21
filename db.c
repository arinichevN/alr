#include "main.h"

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "addProg: ERROR: can not load prog with id=%d - list length exceeded\n", item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockProgList();
        list->top = item;
        unlockProgList();
    } else {
        lockMutex(&list->last->mutex);
        list->last->next = item;
        unlockMutex(&list->last->mutex);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("addProg: prog with id=%d loaded\n", item->id);
#endif
    return 1;
}

void saveProgGoodDelta(int id, float value, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgGoodDelta: failed to open db where id=%d\n", id);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set good_delta=%f where id=%d", value, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgGoodDelta: query failed: %s\n", q);
    }
    sqlite3_close(db);
}

void saveProgGoodValue(int id, float value, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgGoodValue: failed to open db where id=%d\n", id);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set good_value=%f where id=%d", value, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgGoodValue: query failed: %s\n", q);
    }
    sqlite3_close(db);
}

void saveProgSMS(int id, int value, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgSMS: failed to open db where id=%d\n", id);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set sms=%d where id=%d", value, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgSMS: query failed: %s\n", q);
    }
    sqlite3_close(db);
}

void saveProgRing(int id, int value, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgRing: failed to open db where id=%d\n", id);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set ring=%d where id=%d", value, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgRing: query failed: %s\n", q);
    }
    sqlite3_close(db);
}

int addProgById(int prog_id, ProgList *list, PeerList *peer_list, sqlite3 *db_data, const char *db_data_path) {
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "addProgById(): program with id = %d is being controlled by program\n", rprog->id);
#endif
        return 0;
    }

    Prog *item = malloc(sizeof *(item));
    if (item == NULL) {
        fputs("addProgById(): failed to allocate memory\n", stderr);
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;

    item->cycle_duration = cycle_duration;
    item->log_limit = log_limit;

    if (!initMutex(&item->mutex)) {
        free(item);
        return 0;
    }
    if (!initClient(&item->sock_fd, WAIT_RESP_TIMEOUT)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!getProgByIdFDB(item->id, item, peer_list, db_data, db_data_path)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    item->sensor_fts.peer.fd = &item->sock_fd;
    item->call_peer.fd = &item->sock_fd;
    if (!checkProg(item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!addProg(item, list)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!createMThread(&item->thread, &threadFunction, item)) {
        freeSocketFd(&item->sock_fd);
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    return 1;
}

int deleteProgById(int id, ProgList *list, char* db_data_path) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            if (prev != NULL) {
                lockMutex(&prev->mutex);
                prev->next = curr->next;
                unlockMutex(&prev->mutex);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            stopProgThread(curr);
            config_saveProgLoad(curr->id, 0, NULL, db_data_path);
            freeProg(curr);
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int loadActiveProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData *data = d;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            int id = atoi(argv[i]);
            addProgById(id, data->prog_list, data->peer_list, data->db_data, NULL);
        } else {
            fputs("loadActiveProg_callback(): unknown column\n", stderr);
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveProg(ProgList *list, PeerList *peer_list, char *db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data = {.prog_list = list, .peer_list = peer_list, .db_data = db};
    char *q = "select id from prog where load=1";
    if (!db_exec(db, q, loadActiveProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "loadActiveProg(): query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int callHuman(Prog *item, char *message, Peer *peer, const char *db_path) {
    S1List pn_list;
    if (!config_getPhoneNumberListO(&pn_list, db_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "callHuman: error while reading phone book\n");
#endif
        return 0;
    }
    if (item->sms) {
        for (int i = 0; i < pn_list.length; i++) {
            acp_sendSMS(peer, &pn_list.item[LINE_SIZE * i], message);
        }
    }
    if (item->ring) {
        for (int i = 0; i < pn_list.length; i++) {
            acp_makeCall(peer, &pn_list.item[LINE_SIZE * i]);
        }
    }
    FREE_LIST(&pn_list);
    return 1;
}

int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int load = 0, enable = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            item->id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("sensor_fts_id")) {
            if (!config_getSensorFTS(&item->sensor_fts, atoi(argv[i]), data->peer_list, data->db_data)) {
                free(item);
                return 1;
            }
        } else if (DB_COLUMN_IS("call_peer_id")) {
            Peer *peer = getPeerById(argv[i], data->peer_list);
            if (peer == NULL) {
                return EXIT_FAILURE;
            }
            item->call_peer = *peer;
        } else if (DB_COLUMN_IS("description")) {
            memcpy(item->description, argv[i], sizeof item->description);
        } else if (DB_COLUMN_IS("good_value")) {
            item->good_value = atof(argv[i]);
        } else if (DB_COLUMN_IS("good_delta")) {
            item->good_delta = atof(argv[i]);
        } else if (DB_COLUMN_IS("check_interval")) {
            item->check_interval.tv_nsec = 0;
            item->check_interval.tv_sec = atoi(argv[i]);
        } else if (DB_COLUMN_IS("cope_duration")) {
            item->cope_duration.tv_nsec = 0;
            item->cope_duration.tv_sec = atoi(argv[i]);
        } else if (DB_COLUMN_IS("phone_number_group_id")) {
            item->phone_number_group_id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("sms")) {
            item->sms = atoi(argv[i]);
        } else if (DB_COLUMN_IS("ring")) {
            item->ring = atoi(argv[i]);
        } else if (DB_COLUMN_IS("enable")) {
            enable = atoi(argv[i]);
        } else if (DB_COLUMN_IS("load")) {
            load = atoi(argv[i]);
        } else {
            fputs("getProg_callback: unknown column\n", stderr);
        }
    }

    if (enable) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    if (!load) {
        config_saveProgLoad(item->id, 1, data->db_data, NULL);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Prog *item, PeerList *peer_list, sqlite3 *dbl, const char *db_path) {
    if (dbl != NULL && db_path != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "getProgByIdFDB(): db xor db_path expected\n");
#endif
        return 0;
    }
    sqlite3 *db;
    if (db_path != NULL) {
        if (!db_open(db_path, &db)) {
            return 0;
        }
    } else {
        db = dbl;
    }
    char q[LINE_SIZE];
    ProgData data = {.peer_list = peer_list, .prog = item, .db_data = db};
    snprintf(q, sizeof q, "select " PROG_FIELDS " from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "getProgByIdFDB(): query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}


