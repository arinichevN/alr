
/*
 * alr
 */


int checkProg(const Prog *item, const ProgList *list) {
    if (item->check_interval.tv_sec < 0 || item->check_interval.tv_nsec < 0) {
        fprintf(stderr, "ERROR: checkProg: negative check_interval where prog id = %d\n", item->id);
        return 0;
    }
    if (item->cope_duration.tv_sec < 0 || item->cope_duration.tv_nsec < 0) {
        fprintf(stderr, "ERROR: checkProg: negative cope_duration where prog id = %d\n", item->id);
        return 0;
    }
    //unique id
    if (getProgById(item->id, list) != NULL) {
        fprintf(stderr, "ERROR: checkProg: prog with id = %d is already running\n", item->id);
        return 0;
    }
    return 1;
}

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
        lockProg(list->last);
        list->last->next = item;
        unlockProg(list->last);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("addProg: prog with id=%d loaded\n", item->id);
#endif
    return 1;
}

void saveProgLoad(int id, int v, sqlite3 *db) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set load=%d where id=%d", v, id);
    if (!db_exec(db, q, 0, 0)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "saveProgLoad: query failed: %s\n", q);
#endif
    }
}

void saveProgEnable(int id, int v, const char* db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        printfe("saveProgEnable: failed to open db: %s\n", db_path);
        return;
    }
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "update prog set enable=%d where id=%d", v, id);
    if (!db_exec(db, q, 0, 0)) {
        printfe("saveProgEnable: query failed: %s\n", q);
    }
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

int loadProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData *data = d;
    Prog *item = (Prog *) malloc(sizeof *(item));
    if (item == NULL) {
        fputs("loadProg_callback: failed to allocate memory\n", stderr);
        return 1;
    }
    memset(item, 0, sizeof *item);
    int load = 0, enable = 0;int i;
    for (i = 0; i < argc; i++) {
        if (strcmp("id", azColName[i]) == 0) {
            item->id = atoi(argv[i]);
        } else if (strcmp("description", azColName[i]) == 0) {
            memset(item->description, 0, sizeof item->description);
            memcpy(item->description, argv[i], sizeof item->description);
        } else if (strcmp("sensor_fts_id", azColName[i]) == 0) {
            if (!config_getSensorFTS(&item->sensor_fts, atoi(argv[i]), data->peer_list, data->db)) {
                free(item);
                return 1;
            }
        } else if (strcmp("good_value", azColName[i]) == 0) {
            item->good_value = atof(argv[i]);
        } else if (strcmp("good_delta", azColName[i]) == 0) {
            item->good_delta = atof(argv[i]);
        } else if (strcmp("check_interval", azColName[i]) == 0) {
            item->check_interval.tv_nsec = 0;
            item->check_interval.tv_sec = atoi(argv[i]);
        } else if (strcmp("cope_duration", azColName[i]) == 0) {
            item->cope_duration.tv_nsec = 0;
            item->cope_duration.tv_sec = atoi(argv[i]);
        } else if (strcmp("phone_number_group_id", azColName[i]) == 0) {
            item->phone_number_group_id = atoi(argv[i]);
        } else if (strcmp("sms", azColName[i]) == 0) {
            item->sms = atoi(argv[i]);
        } else if (strcmp("ring", azColName[i]) == 0) {
            item->ring = atoi(argv[i]);
        } else if (strcmp("enable", azColName[i]) == 0) {
            enable = atoi(argv[i]);
        } else if (strcmp("load", azColName[i]) == 0) {
            load = atoi(argv[i]);
        } else {
            putse("loadProg_callback: unknown column\n");
        }
    }
    if (enable) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    item->next = NULL;

    if (!initMutex(&item->mutex)) {
        free(item);
        return 1;
    }
    if (!checkProg(item, data->prog_list)) {
        free(item);
        return 1;
    }
    if (!addProg(item, data->prog_list)) {
        free(item);
        return 1;
    }
    if (!load) {
        saveProgLoad(item->id, 1, data->db);
    }
    return 0;
}

int addProgById(int prog_id, ProgList *list, PeerList *pl, const char *db_path) {
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "WARNING: addProgById: program with id = %d is being controlled by program\n", rprog->id);
#endif
        return 0;
    }
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data = {db, pl, list};
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select " PROG_FIELDS " from prog where id=%d", prog_id);
    if (!db_exec(db, q, loadProg_callback, (void*) &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "addProgById: query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int deleteProgById(int id, ProgList *list, const char* db_path) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            sqlite3 *db;
            if (db_open(db_path, &db)) {
                saveProgLoad(curr->id, 0, db);
                sqlite3_close(db);
            }
            if (prev != NULL) {
                lockProg(prev);
                prev->next = curr->next;
                unlockProg(prev);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            //we will wait other threads to finish curr program and then we will free it
            lockProg(curr);
            unlockProg(curr);
            free(curr);
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

int loadActiveProg(char *db_path, ProgList *list, PeerList * pl) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data = {db, pl, list};
    char *q = "select " PROG_FIELDS " from prog where load=1";
    if (!db_exec(db, q, loadProg_callback, (void*) &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "loadActiveProg: query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int loadAllProg(char *db_path, ProgList *list, PeerList * pl) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data = {db, pl, list};
    char *q = "select " PROG_FIELDS " from prog";
    if (!db_exec(db, q, loadProg_callback, (void*) &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "loadAllProg: query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int callHuman(Prog *item, char *message, Peer *peer, const char *db_path) {
    S1List pn_list = {.item = NULL, .length = 0};
    if (!config_getPhoneNumberListG(&pn_list, item->phone_number_group_id, db_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "callHuman: error while reading phone book\n");
#endif
        return 0;
    }
    size_t i;
    for (i = 0; i < pn_list.length; i++) {
        if (item->sms) {
            acp_sendSMS(peer, &pn_list.item[LINE_SIZE * i], message, sock_buf_size);
        }
    }
    for (i = 0; i < pn_list.length; i++) {
        if (item->ring) {
            acp_makeCall(peer, &pn_list.item[LINE_SIZE * i], sock_buf_size);
        }
    }
    FREE_LIST(&pn_list);
    return 1;
}

int sendSum(Peer *peer, const char *db_log_path, const char *db_public_path) {
    S1List pn_list = {.item = NULL, .length = 0};
    if (!config_getPhoneNumberListO(&pn_list, db_public_path)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "sendSum: error while reading phone book\n");
#endif
        return 0;
    }
    size_t i;
    if (!file_exist(db_log_path)) {
#ifdef MODE_DEBUG
        fputs("sendSum: file not found\n", stderr);
#endif
        for (i = 0; i < pn_list.length; i++) {
          //  acp_sendSMS(peer, &pn_list.item[LINE_SIZE * i], "net dostupa k jurnalu trevog!", sock_buf_size);
        }
        FREE_LIST(&pn_list);
        return 1;
    }
    sqlite3 *db;
    if (!db_open(db_log_path, &db)) {
#ifdef MODE_DEBUG
        fputs("sendSum: db open failed\n", stderr);
#endif
        FREE_LIST(&pn_list);
        return 0;
    }
    int n = 0;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "select count(*) from alert");
    if (!db_getInt(&n, db, q)) {
#ifdef MODE_DEBUG
        fputs("sendSum: getCount failed", stderr);
#endif
        sqlite3_close(db);
        FREE_LIST(&pn_list);
        return 0;
    }
    sqlite3_close(db);
    char msg[LINE_SIZE];
    snprintf(msg, sizeof msg, "kolichestvo trevog v jurnale: %d", n);
    for (i = 0; i < pn_list.length; i++) {
        acp_sendSMS(peer, &pn_list.item[LINE_SIZE * i], msg, sock_buf_size);
    }
    FREE_LIST(&pn_list);
    return 1;
}


