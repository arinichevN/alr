/*
 * alr
 */

#include "main.h"

char pid_path[LINE_SIZE];

int app_state = APP_INIT;

char db_data_path[LINE_SIZE];
char db_log_path[LINE_SIZE];
char db_public_path[LINE_SIZE];


int pid_file = -1;
int proc_id;
int sock_port = -1;
int sock_fd = -1;
int sock_fd_tf = -1;
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
struct timespec cope_duration = {0, 0};
struct timespec call_interval = {0, 0};
struct timespec sum_interval = {0, 0};
DEF_THREAD
I1List i1l = {NULL, 0};
I2List i2l = {NULL, 0};
I1F1List i1f1l = {NULL, 0};
Ton_ts tmr_cope = {.ready = 0};
Ton_ts tmr_call = {.ready = 0};
int phone_ind = 0;
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
Mutex alert_mutex = {.created = 0, .attr_initialized = 0};
char alert_state = OFF;
unsigned int log_limit = 0;
PeerList peer_list = {NULL, 0};
ProgList prog_list = {NULL, NULL, 0};
Peer *call_peer = NULL;
char call_peer_id[NAME_SIZE];
Ton_ts tmr_sum;

int phone_number_group_id = -1;

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
    n = fscanf(stream, "%d\t%255s\t%ld\t%ld\t%ld\t%ld\t%u\t%ld\t%d\t%32s\t%255s\t%255s\t%255s\n",
            &sock_port,
            pid_path,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            &cope_duration.tv_sec,
            &call_interval.tv_sec,
            &log_limit,
            &sum_interval.tv_sec,
            &phone_number_group_id,
            call_peer_id,
            db_data_path,
            db_public_path,
            db_log_path
            );
    if (n != 13) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad row format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG
    printf("readSettings: \n\tsock_port: %d, \n\tpid_path: %s,\n\tcycle_duration: %ld sec %ld nsec, \n\tcope_duration: %ld sec, \n\tcall_interval: %ld sec, \n\tlog_limit: %u, \n\tsum_interval: %ld sec, \n\tphone_number_group_id: %d, \n\tcall_peer_id: %s, \n\tdb_data_path: %s, \n\tdb_public_path: %s, \n\tdb_log_path: %s\n",
            sock_port,
            pid_path,
            cycle_duration.tv_sec,
            cycle_duration.tv_nsec,
            cope_duration.tv_sec,
            call_interval.tv_sec,
            log_limit,
            sum_interval.tv_sec,
            phone_number_group_id,
            call_peer_id,
            db_data_path,
            db_public_path,
            db_log_path
            );
#endif
    return 1;
}

int initData() {
    if (!config_getPeerList(&peer_list, &sock_fd_tf, db_public_path)) {
        FREE_LIST(&peer_list);
        return 0;
    }
    call_peer = getPeerById(call_peer_id, &peer_list);
    if (call_peer == NULL) {
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!loadActiveProg(db_data_path, &prog_list, &peer_list)) {
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i1l.item = (int *) malloc(ACP_BUFFER_MAX_SIZE * sizeof *(i1l.item));
    if (i1l.item == NULL) {
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i2l.item = (I2 *) malloc(ACP_BUFFER_MAX_SIZE * sizeof *(i2l.item));
    if (i2l.item == NULL) {
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i1f1l.item = (I1F1 *) malloc(ACP_BUFFER_MAX_SIZE * sizeof *(i1f1l.item));
    if (i1f1l.item == NULL) {
        FREE_LIST(&i2l);
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    tmr_sum.ready = 0;
    if (!THREAD_CREATE) {
        FREE_LIST(&i1f1l);
        FREE_LIST(&i2l);
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    return 1;
}

void initApp() {
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize mutex\n");
    }
    if (!initMutex(&alert_mutex)) {
        exit_nicely_e("initApp: failed to initialize mutex\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize udp server\n");
    }

    if (!initClient(&sock_fd_tf, WAIT_RESP_TIMEOUT)) {
        exit_nicely_e("initApp: failed to initialize udp client\n");
    }
}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    if (ACP_CMD_IS(ACP_CMD_ALR_ALARM_DISABLE)) {
        if (lockMutex(&alert_mutex)) {
            alert_state = DISABLE;
            unlockMutex(&alert_mutex);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_ALR_ALARM_GET)) {
        if (lockMutex(&alert_mutex)) {
            char *str = getStateStr(alert_state);
            SEND_STR_L_P(str)
            unlockMutex(&alert_mutex);
        }
        return;
    }

    if (
            ACP_CMD_IS(ACP_CMD_PROG_STOP) ||
            ACP_CMD_IS(ACP_CMD_PROG_START) ||
            ACP_CMD_IS(ACP_CMD_PROG_RESET) ||
            ACP_CMD_IS(ACP_CMD_PROG_ENABLE) ||
            ACP_CMD_IS(ACP_CMD_PROG_DISABLE) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME) ||
            ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)
            ) {
        acp_requestDataToI1List(&request, &i1l, prog_list.length);
        if (i1l.length <= 0) {
            return;
        }
    } else if (
            ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_GOAL) ||
            ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_DELTA)
            ) {
        acp_requestDataToI1F1List(&request, &i1f1l, prog_list.length);
        if (i1f1l.length <= 0) {
            return;
        }
    } else if (
            ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_SMS) ||
            ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_RING)
            ) {
        acp_requestDataToI2List(&request, &i2l, prog_list.length);
        if (i2l.length <= 0) {
            return;
        }

    } else {
        return;
    }

    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, &peer_list, db_data_path);
        }

        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                curr->state = OFF;
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, &peer_list, db_data_path);
        }

        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_ENABLE)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    curr->state = INIT;
                    saveProgEnable(curr->id, 1, db_data_path);
                    unlockProg(curr);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_DISABLE)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    curr->state = DISABLE;
                    saveProgEnable(curr->id, 0, db_data_path);
                    unlockProg(curr);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                if (!bufCatProgInit(curr, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        for (int i = 0; i < i1l.length; i++) {
            Prog *curr = getProgById(i1l.item[i], &prog_list);
            if (curr != NULL) {
                if (!bufCatProgRuntime(curr, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_GOAL)) {
        for (int i = 0; i < i1f1l.length; i++) {
            Prog *curr = getProgById(i1f1l.item[i].p0, &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    curr->good_value = i1f1l.item[i].p1;
                    unlockProg(curr);
                }
            }
            saveProgGoodValue(i1f1l.item[i].p0, i1f1l.item[i].p1, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_DELTA)) {
        for (int i = 0; i < i1f1l.length; i++) {
            Prog *curr = getProgById(i1f1l.item[i].p0, &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    curr->good_delta = i1f1l.item[i].p1;
                    unlockProg(curr);
                }
            }
            saveProgGoodDelta(i1f1l.item[i].p0, i1f1l.item[i].p1, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_SMS)) {
        for (int i = 0; i < i2l.length; i++) {
            Prog *curr = getProgById(i2l.item[i].p0, &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    curr->sms = i2l.item[i].p1;
                    unlockProg(curr);
                }
            }
            saveProgSMS(i2l.item[i].p0, i2l.item[i].p1, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_ALR_PROG_SET_RING)) {
        for (int i = 0; i < i2l.length; i++) {
            Prog *curr = getProgById(i2l.item[i].p0, &prog_list);
            if (curr != NULL) {
                if (lockProg(curr)) {
                    curr->ring = i2l.item[i].p1;
                    unlockProg(curr);
                }
            }
            saveProgRing(i2l.item[i].p0, i2l.item[i].p1, db_data_path);
        }
        return;
    }
    if (!acp_responseSend(&response, &peer_client)) {
        return;
    }
    return;
}

/*
int readFTS(SensorFTS *s) {
    struct timespec now = getCurrentTime();
    s->source->active = 1;
    s->source->time1 = now;
    s->last_read_time = now;
    s->last_return = 1;
    s->value.state = 1;
    s->value.value += 0.001f;
    s->value.tm = now;
    return 1;
}
 */

int readFTS(SensorFTS *s) {
    return acp_readSensorFTS(s);
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
            readFTS(&item->sensor_fts);
            if (BAD_CONDITION) {
                item->state = WCOPE;
            }
            break;
        case WCOPE:
            if (!ton_ts(item->check_interval, &item->tmr_check)) {
                break;
            }
            readFTS(&item->sensor_fts);
            if (GOOD_CONDITION) {
                item->tmr_check.ready = 0;
                item->tmr_cope.ready = 0;
                item->state = WBAD;
            }
            if (ton_ts(item->cope_duration, &item->tmr_cope)) {
                char msg[LINE_SIZE];
                snprintf(msg, sizeof msg, "kontrol parametrov: %s", item->description);
                log_saveAlert(msg, log_limit, db_log_path);
                //callHuman(item, msg, call_peer, db_public_path);
                if (item->ring) {
                    if (lockMutex(&alert_mutex)) {
                        if (alert_state == OFF) {
                            alert_state = INIT;
                        }
                        unlockMutex(&alert_mutex);
                    }
                }
                item->state = WGOOD;
#ifdef MODE_DEBUG
                puts("alert has been sent");
#endif
            }
            break;
        case WGOOD:
            if (!ton_ts(item->check_interval, &item->tmr_check)) {
                break;
            }
            readFTS(&item->sensor_fts);
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

void *threadFunction(void *arg) {
    THREAD_DEF_CMD
#ifdef MODE_DEBUG
            puts("threadFunction: running...");
#endif
    S1List pn_list = {.item = NULL, .length = 0};
    while (1) {
        struct timespec t1 = getCurrentTime();
        lockProgList();
        Prog *curr = prog_list.top;
        unlockProgList();
        while (1) {
            if (curr == NULL) {
                break;
            }

            if (tryLockProg(curr)) {
                progControl(curr);
                Prog *temp = curr;
                curr = curr->next;
                unlockProg(temp);
            }

            THREAD_EXIT_ON_CMD
        }

        if (ton_ts(sum_interval, &tmr_sum)) {
            sendSum(call_peer, db_log_path, db_public_path);
        }
#ifdef MODE_DEBUG
        {
            struct timespec tm1 = getTimeRestL(sum_interval, tmr_sum);
            printf("sum: send_time_rest=%ldsec\n", tm1.tv_sec);
        }
#endif
        if (tryLockMutex(&alert_mutex)) {
            switch (alert_state) {
                case INIT:
                    alert_state = ACT;
                    tmr_call.ready = 0;
                    tmr_cope.ready = 0;
                    phone_ind = 0;
                    FREE_LIST(&pn_list);
                    if (!config_getPhoneNumberListO(&pn_list, db_public_path)) {
                        alert_state = OFF;
                    }
                    break;
                case ACT:
                    if (ton_ts(cope_duration, &tmr_cope)) {
                        phone_ind++;
                        if (phone_ind >= pn_list.length) {
                            alert_state = OFF;
                            break;
                        }
                    }
                    if (ton_ts(call_interval, &tmr_call)) {
                        acp_makeCall(call_peer, &pn_list.item[LINE_SIZE * phone_ind]);
                    }
                    break;
                case DISABLE:
                    alert_state = OFF;
                    break;
                case OFF:
                    break;
                default:
                    alert_state = OFF;
                    break;
            }
            unlockMutex(&alert_mutex);
        }
#ifdef MODE_DEBUG
        char *state = getStateStr(alert_state);
        struct timespec tm1 = getTimeRestL(call_interval, tmr_call);
        struct timespec tm2 = getTimeRestL(cope_duration, tmr_cope);
        printf("alarm:state=%s phone_ind: %d, phone_list_length=%d call_time_rest=%ldsec cope_time_rest=%ldsec\n", state, phone_ind, pn_list.length, tm1.tv_sec, tm2.tv_sec);
#endif
        THREAD_EXIT_ON_CMD
        sleepRest(cycle_duration, t1);
    }
}

void freeProg(ProgList *list) {
    Prog *curr = list->top, *temp;
    while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void freeData() {
#ifdef MODE_DEBUG
    puts("freeData:");
#endif
    THREAD_STOP
    FREE_LIST(&i1f1l);
    FREE_LIST(&i2l);
    FREE_LIST(&i1l);
    freeProg(&prog_list);
    FREE_LIST(&peer_list);
#ifdef MODE_DEBUG
    puts("\tdone");
#endif
}

void freeApp() {
#ifdef MODE_DEBUG
    puts("freeApp:");
#endif
    freeData();
    freeSocketFd(&sock_fd);
#ifdef MODE_DEBUG
    puts(" free sock_fd: done");
#endif
    freeSocketFd(&sock_fd_tf);
#ifdef MODE_DEBUG
    puts(" sock_fd_tf: done");
#endif
    freeMutex(&progl_mutex);
#ifdef MODE_DEBUG
    puts(" free progl_mutex: done");
#endif
    freePid(&pid_file, &proc_id, pid_path);
#ifdef MODE_DEBUG
    puts(" freePid: done");
#endif
#ifdef MODE_DEBUG
    puts(" done");
#endif

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
        switch (app_state) {
            case APP_INIT:
#ifdef MODE_DEBUG
                puts("MAIN: init");
#endif
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
#ifdef MODE_DEBUG
                puts("MAIN: init data");
#endif
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
#ifdef MODE_DEBUG
                puts("MAIN: run");
#endif
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
#ifdef MODE_DEBUG
                puts("MAIN: stop");
#endif
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
#ifdef MODE_DEBUG
                puts("MAIN: reset");
#endif
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
#ifdef MODE_DEBUG
                puts("MAIN: exit");
#endif
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