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
size_t sock_buf_size = 0;
int sock_fd = -1;
int sock_fd_tf = -1;
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
struct timespec cope_duration = {0, 0};
struct timespec call_interval = {0, 0};
struct timespec sum_interval = {0, 0};
pthread_t thread;
char thread_cmd;
I1List i1l = {NULL, 0};
I2List i2l = {NULL, 0};
I1F1List i1f1l = {NULL, 0};
Ton_ts tmr_cope = {.ready = 0};
Ton_ts tmr_call = {.ready = 0};
int phone_ind = 0;
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
Mutex alert_mutex = {.created = 0, .attr_initialized = 0};
char alert_state = OFF;
int log_limit = 0;
PeerList peer_list = {NULL, 0};
ProgList prog_list = {NULL, NULL, 0};
Peer *cell_peer = NULL;
char cell_peer_id[NAME_SIZE];
Ton_ts tmr_sum;

int phone_number_group_id = -1;

#include "util.c"
#include "db.c"

int readSettings() {
    FILE* stream = fopen(CONFIG_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: fopen", stderr);
#endif
        return 0;
    }
    int n;
    n = fscanf(stream, "%d\t%255s\t%d\t%ld\t%ld\t%ld\t%ld\t%d\t%ld\t%d\t%32s\t%255s\t%255s\t%255s\n",
            &sock_port,
            pid_path,
            &sock_buf_size,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            &cope_duration.tv_sec,
            &call_interval.tv_sec,
            &log_limit,
            &sum_interval.tv_sec,
            &phone_number_group_id,
            cell_peer_id,
            db_data_path,
            db_public_path,
            db_log_path
            );
    if (n != 14) {
        fclose(stream);
        return 0;
    }
    fclose(stream);
    return 1;
}

int initData() {
    if (!config_getPeerList(&peer_list, &sock_fd_tf, db_public_path)) {
        FREE_LIST(&peer_list);
        return 0;
    }
    cell_peer = getPeerById(cell_peer_id, &peer_list);
    if (cell_peer == NULL) {
        FREE_LIST(&peer_list);
        return 0;
    }
    if (!loadActiveProg(db_data_path, &prog_list, &peer_list)) {
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i1l.item = (int *) malloc(sock_buf_size * sizeof *(i1l.item));
    if (i1l.item == NULL) {
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i2l.item = (I2 *) malloc(sock_buf_size * sizeof *(i2l.item));
    if (i2l.item == NULL) {
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    i1f1l.item = (I1F1 *) malloc(sock_buf_size * sizeof *(i1f1l.item));
    if (i1f1l.item == NULL) {
        FREE_LIST(&i2l);
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        FREE_LIST(&peer_list);
        return 0;
    }
    tmr_sum.ready = 0;
    if (!createThread_ctl()) {
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
#ifdef MODE_DEBUG
    printf("initApp: PID: %d\n", proc_id);
    printf("initApp: sock_port: %d\n", sock_port);
    printf("initApp: sock_buf_size: %d\n", sock_buf_size);
    printf("initApp: pid_path: %s\n", pid_path);
    printf("initApp: cycle_duration: %ld(sec) %ld(nsec)\n", cycle_duration.tv_sec, cycle_duration.tv_nsec);
    printf("initApp: cope_duration: %ld(sec) %ld(nsec)\n", cope_duration.tv_sec, cope_duration.tv_nsec);
    printf("initApp: call_interval: %ld(sec) %ld(nsec)\n", call_interval.tv_sec, call_interval.tv_nsec);
    printf("initApp: sum_interval: %ld(sec) %ld(nsec)\n", sum_interval.tv_sec, sum_interval.tv_nsec);
    printf("initApp: log_limit: %d\n", log_limit);
    printf("initApp: cell_peer_id: %s\n", cell_peer_id);
    printf("initApp: db_data_path: %s\n", db_data_path);
    printf("initApp: db_log_path: %s\n", db_log_path);
    printf("initApp: db_public_path: %s\n", db_public_path);

#endif
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
    char buf_in[sock_buf_size];
    char buf_out[sock_buf_size];
    uint8_t crc;
    int i, j;
    char q[LINE_SIZE];
    crc = 0;
    memset(buf_in, 0, sizeof buf_in);
    acp_initBuf(buf_out, sizeof buf_out);
    if (recvfrom(sock_fd, buf_in, sizeof buf_in, 0, (struct sockaddr*) (&(peer_client.addr)), &(peer_client.addr_size)) < 0) {
#ifdef MODE_DEBUG
        perror("serverRun: recvfrom() error");
#endif
    }
#ifdef MODE_DEBUG
    dumpBuf(buf_in, sizeof buf_in);
#endif    
    if (!crc_check(buf_in, sizeof buf_in)) {
#ifdef MODE_DEBUG
        fputs("WARNING: serverRun: crc check failed\n", stderr);
#endif
        return;
    }
    switch (buf_in[1]) {
        case ACP_CMD_APP_START:
            if (!init_state) {
                *state = APP_INIT_DATA;
            }
            return;
        case ACP_CMD_APP_STOP:
            if (init_state) {
                *state = APP_STOP;
            }
            return;
        case ACP_CMD_APP_RESET:
            *state = APP_RESET;
            return;
        case ACP_CMD_APP_EXIT:
            *state = APP_EXIT;
            return;
        case ACP_CMD_APP_PING:
            if (init_state) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_BUSY);
            } else {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_APP_IDLE);
            }
            return;
        case ACP_CMD_APP_PRINT:
            printAll();
            return;
        case ACP_CMD_APP_HELP:
            printHelp();
            return;
        case ACP_CMD_APP_TIME:
        {
            struct tm *current;
            time_t now;
            time(&now);
            current = localtime(&now);
            if (!acp_bufCatDate(current, buf_out, sock_buf_size)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                return;
            }
            return;
        }
        case ACP_CMD_ALR_ALARM_DISABLE:
            if (lockMutex(&alert_mutex)) {
                alert_state = DISABLE;
                unlockMutex(&alert_mutex);
            }
            return;
        case ACP_CMD_ALR_ALARM_GET:
            if (lockMutex(&alert_mutex)) {
                char *str = getStateStr(alert_state);
                if (!bufCat(buf_out, str, sock_buf_size)) {
                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                    return;
                }
                if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                    return;
                }
                unlockMutex(&alert_mutex);
            }
            return;
        default:
            if (!init_state) {
                return;
            }
            break;
    }

    switch (buf_in[0]) {
        case ACP_QUANTIFIER_BROADCAST:
        case ACP_QUANTIFIER_SPECIFIC:
            break;
        default:
            return;
    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
        case ACP_CMD_START:
        case ACP_CMD_ALR_PROG_ENABLE:
        case ACP_CMD_ALR_PROG_DISABLE:
        case ACP_CMD_ALR_PROG_GET_DATA_RUNTIME:
        case ACP_CMD_ALR_PROG_GET_DATA_INIT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    return;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1(buf_in, &i1l, sock_buf_size);
                    if (i1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        case ACP_CMD_ALR_PROG_SET_GOAL:
        case ACP_CMD_ALR_PROG_SET_DELTA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    return;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI1F1(buf_in, &i1f1l, sock_buf_size);
                    if (i1f1l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        case ACP_CMD_ALR_PROG_SET_SMS:
        case ACP_CMD_ALR_PROG_SET_RING:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    return;
                case ACP_QUANTIFIER_SPECIFIC:
                    acp_parsePackI2(buf_in, &i2l, sock_buf_size);
                    if (i2l.length <= 0) {
                        return;
                    }
                    break;
            }
            break;
        default:
            return;

    }

    switch (buf_in[1]) {
        case ACP_CMD_STOP:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    deleteProgById(curr->id, &prog_list, db_data_path);
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            deleteProgById(i1l.item[i], &prog_list, db_data_path);
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_START:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    loadAllProg(db_data_path, &prog_list, &peer_list);
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        addProgById(i1l.item[i], &prog_list, &peer_list, db_data_path);
                    }
                    break;
            }
            return;
        case ACP_CMD_ALR_PROG_ENABLE:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {

                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        curr->state = INIT;
                        saveProgEnable(curr->id, 1, db_data_path);
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->state = INIT;
                                saveProgEnable(curr->id, 1, db_data_path);
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_ALR_PROG_DISABLE:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {

                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        curr->state = DISABLE;
                        saveProgEnable(curr->id, 0, db_data_path);
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->state = DISABLE;
                                saveProgEnable(curr->id, 0, db_data_path);
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            return;
        case ACP_CMD_ALR_PROG_GET_DATA_INIT:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        if (!bufCatProgInit(curr, buf_out, sock_buf_size)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                if (!bufCatProgInit(curr, buf_out, sock_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    return;
                                }
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_ALR_PROG_GET_DATA_RUNTIME:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                {
                    PROG_LIST_LOOP_DF
                    PROG_LIST_LOOP_ST
                    if (lockProg(curr)) {
                        if (!bufCatProgRuntime(curr, buf_out, sock_buf_size)) {
                            sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                            return;
                        }
                        unlockProg(curr);
                    }
                    PROG_LIST_LOOP_SP
                    break;
                }
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1l.length; i++) {
                        Prog *curr = getProgById(i1l.item[i], &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                if (!bufCatProgRuntime(curr, buf_out, sock_buf_size)) {
                                    sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
                                    return;
                                }
                                unlockProg(curr);
                            }
                        }
                    }
                    break;
            }
            break;
        case ACP_CMD_ALR_PROG_SET_GOAL:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1f1l.length; i++) {
                        Prog *curr = getProgById(i1f1l.item[i].p0, &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->good_value = i1f1l.item[i].p1;
                                unlockProg(curr);
                            }
                        }
                        saveProgGoodValue(i1f1l.item[i].p0, i1f1l.item[i].p1, db_data_path);
                    }
                    break;
            }
            return;
        case ACP_CMD_ALR_PROG_SET_DELTA:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i1f1l.length; i++) {
                        Prog *curr = getProgById(i1f1l.item[i].p0, &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->good_delta = i1f1l.item[i].p1;
                                unlockProg(curr);
                            }
                        }
                        saveProgGoodDelta(i1f1l.item[i].p0, i1f1l.item[i].p1, db_data_path);
                    }
                    break;
            }
            return;
        case ACP_CMD_ALR_PROG_SET_SMS:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Prog *curr = getProgById(i2l.item[i].p0, &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->sms = i2l.item[i].p1;
                                unlockProg(curr);
                            }
                        }
                        saveProgSMS(i2l.item[i].p0, i2l.item[i].p1, db_data_path);
                    }
                    break;
            }
            return;
        case ACP_CMD_ALR_PROG_SET_RING:
            switch (buf_in[0]) {
                case ACP_QUANTIFIER_BROADCAST:
                    break;
                case ACP_QUANTIFIER_SPECIFIC:
                    for (i = 0; i < i2l.length; i++) {
                        Prog *curr = getProgById(i2l.item[i].p0, &prog_list);
                        if (curr != NULL) {
                            if (lockProg(curr)) {
                                curr->ring = i2l.item[i].p1;
                                unlockProg(curr);
                            }
                        }
                        saveProgRing(i2l.item[i].p0, i2l.item[i].p1, db_data_path);
                    }
                    break;
            }
            return;
    }
    if (!sendBufPack(buf_out, ACP_QUANTIFIER_SPECIFIC, ACP_RESP_REQUEST_SUCCEEDED)) {
        sendStrPack(ACP_QUANTIFIER_BROADCAST, ACP_RESP_BUF_OVERFLOW);
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
    return acp_readSensorFTS(s, sock_buf_size);
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
                //callHuman(item, msg, cell_peer, db_public_path);
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
    char *cmd = (char *) arg;
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

            switch (*cmd) {
                case ACP_CMD_APP_STOP:
                case ACP_CMD_APP_RESET:
                case ACP_CMD_APP_EXIT:
                    *cmd = ACP_CMD_APP_NO;
#ifdef MODE_DEBUG
                    puts("threadFunction: exit");
#endif
                    return (EXIT_SUCCESS);
                default:
                    break;
            }
        }

        if (ton_ts(sum_interval, &tmr_sum)) {
            sendSum(cell_peer, db_log_path, db_public_path);
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
                        acp_makeCall(cell_peer, &pn_list.item[LINE_SIZE * phone_ind], sock_buf_size);
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
        switch (*cmd) {
            case ACP_CMD_APP_STOP:
            case ACP_CMD_APP_RESET:
            case ACP_CMD_APP_EXIT:
                *cmd = ACP_CMD_APP_NO;
#ifdef MODE_DEBUG
                puts("threadFunction: exit");
#endif
                return (EXIT_SUCCESS);
            default:
                break;
        }
        sleepRest(cycle_duration, t1);
    }
}

int createThread_ctl() {
    if (pthread_create(&thread, NULL, &threadFunction, (void *) &thread_cmd) != 0) {
        perror("createThreads: pthread_create");
        return 0;
    }
    return 1;
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
    waitThread_ctl(ACP_CMD_APP_EXIT);
    FREE_LIST(&i1f1l);
    FREE_LIST(&i2l);
    FREE_LIST(&i1l);
    freeProg(&prog_list);
    FREE_LIST(&peer_list);
#ifdef MODE_DEBUG
    puts(" done");
#endif
}

void freeApp() {
#ifdef MODE_DEBUG
    puts("freeApp:");
#endif
    freeData();
#ifdef MODE_DEBUG
    puts(" freeData: done");
#endif
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