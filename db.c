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

int addProgById(int prog_id, ProgList *list, PeerList *peer_list, sqlite3 *db_data, const char *db_data_path) {
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): program with id = %d is being controlled by program\n", F, rprog->id);
#endif
        return 0;
    }

    Prog *item = malloc(sizeof *(item));
    if (item == NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed to allocate memory for program with id=%d\n", F, prog_id);
#endif
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;

    item->cycle_duration = cycle_duration;

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
    item->sensor.peer.fd = &item->sock_fd;
    item->em.peer.fd = &item->sock_fd;
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
            db_saveTableFieldInt("prog", "load", curr->id, 0, NULL, db_data_path);
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
            int id = atoi(DB_COLUMN_VALUE);
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
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int load = 0, enable = 0;
    int c = 0;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            item->id = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("sensor_id")) {
            if (!config_getSensorFTS(&item->sensor, atoi(DB_COLUMN_VALUE), data->peer_list, data->db_data)) {
                item->sensor.peer.fd=NULL;
            }
            c++;
        } else if (DB_COLUMN_IS("em_id")) {
            if (!config_getEM(&item->em, atoi(DB_COLUMN_VALUE), data->peer_list, data->db_data)) {
                item->em.peer.fd=NULL;
            }
            c++;
        } else if (DB_COLUMN_IS("good_value")) {
            item->good_value = atof(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("good_delta")) {
            item->good_delta = atof(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("check_interval_sec")) {
            item->check_interval.tv_nsec = 0;
            item->check_interval.tv_sec = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("cope_duration_sec")) {
            item->cope_duration.tv_nsec = 0;
            item->cope_duration.tv_sec = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("enable")) {
            enable = atoi(DB_COLUMN_VALUE);
            c++;
        } else if (DB_COLUMN_IS("load")) {
            load = atoi(DB_COLUMN_VALUE);
            c++;
        } else {
            fprintf(stderr, "%s(): unknown column: %s\n", F, DB_COLUMN_NAME);
        }
    }
#define N 9
    if (c != N) {
        fprintf(stderr, "%s(): required %d columns but %d found\n", F, N, c);
        return EXIT_FAILURE;
    }
#undef N
    if (enable) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    if (!load) {
        db_saveTableFieldInt("prog", "load", item->id, 1, data->db_data, NULL);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Prog *item, PeerList *peer_list, sqlite3 *dbl, const char *db_path) {
    if (dbl != NULL && db_path != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): db xor db_path expected\n",F);
#endif
        return 0;
    }
    sqlite3 *db;
    int close = 0;
    if (db_path != NULL) {
        if (!db_open(db_path, &db)) {
            return 0;
        }
        close = 1;
    } else {
        db = dbl;
    }
    char q[LINE_SIZE];
    ProgData data = {.peer_list = peer_list, .prog = item, .db_data = db};
    snprintf(q, sizeof q, "select * from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
        if (close)sqlite3_close(db);
        return 0;
    }
    if (close)sqlite3_close(db);
    return 1;
}


