
#include "main.h"

int app_state = APP_INIT;

char * db_data_path;
char * db_log_path;
char * db_public_path;

int sock_port = -1;
int sock_fd = -1;
int log_limit = 0;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};

Mutex progl_mutex = MUTEX_INITIALIZER;
Mutex db_data_mutex = MUTEX_INITIALIZER;
Mutex db_public_mutex = MUTEX_INITIALIZER;

PeerList peer_list;
ProgList prog_list = {NULL, NULL, 0};

#include "util.c"
#include "db.c"

int readSettings() {
    FILE* stream = fopen(CONFIG_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        perror("readSettings()");
#endif
        return 0;
    }
    char s[LINE_SIZE];
    fgets(s, LINE_SIZE, stream);
    int n;
    char db_data_path_temp[LINE_SIZE];
    char db_log_path_temp[LINE_SIZE];
    char db_public_path_temp[LINE_SIZE];
    n = fscanf(stream, "%d\t%ld\t%ld\t%d\t%255s\t%255s\t%255s\n",
            &sock_port,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            &log_limit,
            db_data_path_temp,
            db_public_path_temp,
            db_log_path_temp

            );
    if (n != 7) {
        fclose(stream);
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): bad row format\n", F);
#endif
        return 0;
    }
    fclose(stream);
    strcpyma(&db_data_path, db_data_path_temp);
    if (db_data_path == NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed to allocate memory for db_data_path\n", F);
#endif
        return 0;
    }
    strcpyma(&db_public_path, db_public_path_temp);
    if (db_public_path == NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed to allocate memory for db_public_path\n", F);
#endif
        return 0;
    }
    strcpyma(&db_log_path, db_log_path_temp);
    if (db_log_path == NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failed to allocate memory for db_log_path\n", F);
#endif
        return 0;
    }
#ifdef MODE_DEBUG
    printf("%s: \n\tsock_port: %d, \n\tcycle_duration: %ld sec %ld nsec, \n\tlog_limit: %d, \n\tdb_data_path: %s, \n\tdb_public_path: %s, \n\tdb_log_path: %s\n", F, sock_port, cycle_duration.tv_sec, cycle_duration.tv_nsec, log_limit, db_data_path, db_public_path, db_log_path);
#endif
    return 1;
}

int initData() {
    if (!config_getPeerList(&peer_list, NULL, db_public_path)) {
        return 0;
    }
    if (!loadActiveProg(&prog_list, &peer_list, db_data_path)) {
        freeProgList(&prog_list);
        freePeerList(&peer_list);
        return 0;
    }
    return 1;
}

void initApp() {
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize progl_mutex\n");
    }
    if (!initMutex(&db_data_mutex)) {
        exit_nicely_e("initApp: failed to initialize db_data_mutex\n");
    }
    if (!initMutex(&db_public_mutex)) {
        exit_nicely_e("initApp: failed to initialize db_public_mutex\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
            
    DEF_SERVER_I1LIST
    DEF_SERVER_I2LIST
    DEF_SERVER_I1F1LIST
            
    if (
            ACP_CMD_IS(ACP_CMD_PROG_STOP) ||
            ACP_CMD_IS(ACP_CMD_PROG_START) ||
            ACP_CMD_IS(ACP_CMD_PROG_RESET) ||
            ACP_CMD_IS(ACP_CMD_PROG_ENABLE) ||
            ACP_CMD_IS(ACP_CMD_PROG_DISABLE) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)
            ) {
        acp_requestDataToI1List(&request, &i1l);
        if (i1l.length <= 0) {
            return;
        }
    } else if (
            ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_GOAL) ||
            ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_DELTA)
            ) {
        acp_requestDataToI1F1List(&request, &i1f1l);
        if (i1f1l.length <= 0) {
            return;
        }
    } else if (
            ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_SMS) ||
            ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_RING)
            ) {
        acp_requestDataToI2List(&request, &i2l);
        if (i2l.length <= 0) {
            return;
        }

    } else {
        return;
    }

    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {
        if (lockMutex(&db_data_mutex)) {
            for (int i = 0; i < i1l.length; i++) {
                Prog *item = getProgById(i1l.item[i], &prog_list);
                if (item != NULL) {
                    deleteProgById(i1l.item[i], &prog_list, db_data_path);
                }
            }
            unlockMutex(&db_data_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {
        if (lockMutex(&db_data_mutex)) {

            for (int i = 0; i < i1l.length; i++) {
                addProgById(i1l.item[i], &prog_list, &peer_list, NULL, db_data_path);
            }
            unlockMutex(&db_data_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {

        if (lockMutex(&db_data_mutex)) {
            for (int i = 0; i < i1l.length; i++) {
                Prog *item = getProgById(i1l.item[i], &prog_list);
                if (item != NULL) {
                    item->state = OFF;
                    deleteProgById(i1l.item[i], &prog_list, db_data_path);
                }
            }
            for (int i = 0; i < i1l.length; i++) {
                addProgById(i1l.item[i], &prog_list, &peer_list, NULL, db_data_path);
            }
            unlockMutex(&db_data_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_ENABLE)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    if (item->state == OFF) {
                        item->state = INIT;

                        if (lockMutex(&db_data_mutex)) {
                            db_saveTableFieldInt("prog", "enable", item->id, 1, NULL, db_data_path);
                            unlockMutex(&db_data_mutex);
                        }
                    }
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_DISABLE)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    if (item->state != OFF) {
                        item->state = DISABLE;

                        if (lockMutex(&db_data_mutex)) {
                            db_saveTableFieldInt("prog", "enable", item->id, 0, NULL, db_data_path);
                            unlockMutex(&db_data_mutex);
                        }
                    }
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgInit(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgRuntime(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_GOAL)) {
        for (int i = 0; i < i1f1l.length; i++) {
            Prog *item = getProgById(i1f1l.item[i].p0, &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->good_value = i1f1l.item[i].p1;
                    unlockMutex(&item->mutex);
                }
            }
            if (lockMutex(&db_data_mutex)) {
                db_saveTableFieldFloat("prog", "good_value", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_data_path);
                unlockMutex(&db_data_mutex);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_DELTA)) {
        for (int i = 0; i < i1f1l.length; i++) {
            Prog *item = getProgById(i1f1l.item[i].p0, &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->good_delta = i1f1l.item[i].p1;
                    unlockMutex(&item->mutex);
                }
            }
            if (lockMutex(&db_data_mutex)) {
                db_saveTableFieldFloat("prog", "good_delta", i1f1l.item[i].p0, i1f1l.item[i].p1, NULL, db_data_path);
                unlockMutex(&db_data_mutex);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_SMS)) {
        for (int i = 0; i < i2l.length; i++) {
            Prog *item = getProgById(i2l.item[i].p0, &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->sms = i2l.item[i].p1;
                    unlockMutex(&item->mutex);
                }
            }
            if (lockMutex(&db_data_mutex)) {
                db_saveTableFieldInt("prog", "sms", i2l.item[i].p0, i2l.item[i].p1, NULL, db_data_path);
                unlockMutex(&db_data_mutex);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_CHV_PROG_SET_RING)) {
        for (int i = 0; i < i2l.length; i++) {
            Prog *item = getProgById(i2l.item[i].p0, &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->ring = i2l.item[i].p1;
                    unlockMutex(&item->mutex);
                }
            }
            if (lockMutex(&db_data_mutex)) {
                db_saveTableFieldInt("prog", "ring", i2l.item[i].p0, i2l.item[i].p1, NULL, db_data_path);
                unlockMutex(&db_data_mutex);
            }
        }
        return;
    }
    acp_responseSend(&response, &peer_client);
}

void progControl(Prog *item) {
    switch (item->state) {
        case INIT:
            item->tmr_check.ready = 0;
            item->tmr_cope.ready = 0;
            item->state = WBAD;
            break;
        case WBAD:
            if (!ton_ts(item->check_interval, &item->tmr_check)) {
                break;
            }
            acp_readSensorFTS(&item->sensor_fts);
            if (BAD_CONDITION) {
                item->state = WCOPE;
            }
            break;
        case WCOPE:
            if (!ton_ts(item->check_interval, &item->tmr_check)) {
                break;
            }
            acp_readSensorFTS(&item->sensor_fts);
            if (GOOD_CONDITION) {
                item->tmr_check.ready = 0;
                item->tmr_cope.ready = 0;
                item->state = WBAD;
            }
            if (ton_ts(item->cope_duration, &item->tmr_cope)) {
                char msg[LINE_SIZE];
                snprintf(msg, sizeof msg, "kontrol parametrov: %s", item->description);

                if (lockMutex(&db_data_mutex)) {
                    log_saveAlert(msg, item->log_limit, db_log_path);
                    unlockMutex(&db_data_mutex);
                }

                if (lockMutex(&db_public_mutex)) {
                    callHuman(item, msg, &item->call_peer, db_public_path);
                    unlockMutex(&db_public_mutex);
                }

                item->state = WGOOD;
            }
            break;
        case WGOOD:
            if (!ton_ts(item->check_interval, &item->tmr_check)) {
                break;
            }
            acp_readSensorFTS(&item->sensor_fts);
            if (GOOD_CONDITION) {
                item->state = WBAD;
            }
            break;
        case DISABLE:
            item->state = OFF;
            break;
        case OFF:
            break;
        default:
            item->state = INIT;
            break;
    }
#ifdef MODE_DEBUG
    char *state = getStateStr(item->state);
    struct timespec tm1 = getTimeRestCheck(item);
    struct timespec tm2 = getTimeRestCope(item);
    printf("progControl: id=%d state=%s expected=%.1f delta=%.1f real=%.1f check_time_rest=%ldsec alert_time_rest=%ldsec\n", item->id, state, item->good_value, item->good_delta, item->sensor_fts.value.value, tm1.tv_sec, tm2.tv_sec);
#endif
}

void cleanup_handler(void *arg) {
    Prog *item = arg;
    printf("cleaning up thread %d\n", item->id);
}

void *threadFunction(void *arg) {
    Prog *item = arg;
#ifdef MODE_DEBUG
    printf("thread for program with id=%d has been started\n", item->id);
#endif
#ifdef MODE_DEBUG
    pthread_cleanup_push(cleanup_handler, item);
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if (threadCancelDisable(&old_state)) {
            if (lockMutex(&item->mutex)) {
                progControl(item);
                unlockMutex(&item->mutex);
            }
            threadSetCancelState(old_state);
        }
        sleepRest(item->cycle_duration, t1);
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop(1);
#endif
}

void freeData() {
    stopAllProgThreads(&prog_list);
    freeProgList(&prog_list);
    freePeerList(&peer_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
    freeMutex(&progl_mutex);
    freeMutex(&db_data_mutex);
    freeMutex(&db_public_mutex);
    free(db_data_path);
    free(db_log_path);
    free(db_public_path);
}

void exit_nicely() {
    freeApp();
#ifdef MODE_DEBUG
    puts("\nBye...");
#endif
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
#ifdef MODE_DEBUG
        printf("main(): %s %d\n", getAppState(app_state), data_initialized);
#endif
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}