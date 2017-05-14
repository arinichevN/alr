
#include "main.h"

/*
 * alr
 */

FUN_LLIST_GET_BY_ID(Prog)

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProgList: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

int lockProg(Prog *item) {
    if (pthread_mutex_lock(&(item->mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProg: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProg(Prog *item) {
    if (pthread_mutex_trylock(&(item->mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProg(Prog *item) {
    if (pthread_mutex_unlock(&(item->mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProg: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

struct timespec getTimeRestL(struct timespec interval, Ton_ts tmr) {
    struct timespec out = {-1, -1};
    if (tmr.ready) {
        out = getTimeRest_ts(interval, tmr.start);
    }
    return out;
}

struct timespec getTimeRestCope(const Prog *item) {
    return getTimeRestL(item->cope_duration, item->tmr_cope);
}

struct timespec getTimeRestCheck(const Prog *item) {
    return getTimeRestL(item->check_interval, item->tmr_check);
}

char * getStateStr(char state) {
    switch (state) {
        case OFF:
            return "OFF";
            break;
        case INIT:
            return "INIT";
            break;
        case ACT:
            return "ACT";
            break;
        case WBAD:
            return "WBAD";
            break;
        case WCOPE:
            return "WCOPE";
            break;
        case WGOOD:
            return "WGOOD";
            break;
        case DISABLE:
            return "DISABLE";
            break;

    }
    return "\0";
}

int bufCatProgRuntime(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    char *state = getStateStr(item->state);
    struct timespec tm_rest = getTimeRestCope(item);
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
            item->id,
            state,
            tm_rest.tv_sec
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int bufCatProgInit(const Prog *item, char *buf, size_t buf_size) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%f" ACP_DELIMITER_COLUMN_STR "%f" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
            item->id,
            item->description,
            item->good_value,
            item->good_delta,
            item->check_interval.tv_sec,
            item->cope_duration.tv_sec,
            item->phone_number_group_id,
            item->sms,
            item->ring
            );
    if (bufCat(buf, q, buf_size) == NULL) {
        return 0;
    }
    return 1;
}

int sendStrPack(char qnf, char *cmd) {
    extern Peer peer_client;
    return acp_sendStrPack(qnf, cmd, &peer_client);
}

int sendBufPack(char *buf, char qnf, char *cmd_str) {
    extern Peer peer_client;
    return acp_sendBufPack(buf, qnf, cmd_str, &peer_client);
}

void sendStr(const char *s, uint8_t *crc) {
    acp_sendStr(s, crc, &peer_client);
}

void sendFooter(int8_t crc) {
    acp_sendFooter(crc, &peer_client);
}

void waitThread_ctl(char cmd) {
    thread_cmd = cmd;
    pthread_join(thread, NULL);
}

void printAll() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    size_t i;
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "pid_path: %s\n", pid_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "sock_buf_size: %d\n", sock_buf_size);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cope_duration sec: %ld\n", cope_duration.tv_sec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "call_interval sec: %ld\n", call_interval.tv_sec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "log_limit: %d\n", log_limit);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "sum_interval: %ld\n", sum_interval.tv_sec);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "phone_number_group_id: %d\n", phone_number_group_id);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "cell_peer_id: %s\n", cell_peer_id);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "phone_number_group_id: %d\n", phone_number_group_id);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db_public_path: %s\n", db_public_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "db_log_path: %s\n", db_log_path);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    sendStr(q, &crc);
    snprintf(q, sizeof q, "PID: %d\n", proc_id);
    sendStr(q, &crc);
    sendStr("+-------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                Peer                                             |\n", &crc);
    sendStr("+--------------------------------+-----------+----------------+-----------+-----------+-----------+\n", &crc);
    sendStr("|               id               |  sin_port |      addr      |     fd    |  active   |   link    |\n", &crc);
    sendStr("+--------------------------------+-----------+----------------+-----------+-----------+-----------+\n", &crc);
    for (i = 0; i < peer_list.length; i++) {
        snprintf(q, sizeof q, "|%32.32s|%11u|%16u|%11d|%11d|%11p|\n",
                peer_list.item[i].id,
                peer_list.item[i].addr.sin_port,
                peer_list.item[i].addr.sin_addr.s_addr,
                *peer_list.item[i].fd,
                peer_list.item[i].active,
                &peer_list.item[i]
                );
        sendStr(q, &crc);
    }
    sendStr("+--------------------------------+-----------+----------------+-----------+-----------+-----------+\n", &crc);

    snprintf(q, sizeof q, "prog_list length: %d\n", prog_list.length);
    sendStr(q, &crc);
    sendStr("+------------------------------------------------------------------------------------------------------------------------+\n", &crc);
    sendStr("|                                                     Program init                                                       |\n", &crc);
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n", &crc);
    sendStr("|    id     |   description  |sensor_ptr |sensor_rid |good_value |   delta   |check_intv | cope_dur  | phone_ng  |sms|cal|\n", &crc);
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n", &crc);
    PROG_LIST_LOOP_DF
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%16.16s|%11p|%11d|%11f|%11f|%11ld|%11ld|%11d|%3d|%3d|\n",
            curr->id,
            curr->description,
            &curr->sensor_fts,
            curr->sensor_fts.remote_id,
            curr->good_value,
            curr->good_delta,
            curr->check_interval.tv_sec,
            curr->cope_duration.tv_sec,
            curr->phone_number_group_id,
            curr->sms,
            curr->ring
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n", &crc);

    sendStr("+-------------------------------------------------------------------------+\n", &crc);
    sendStr("|                             Program runtime                             |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+------------+------------+\n", &crc);
    sendStr("|    id     |   state   |snsr_value |snsr_state |check rest s| cope rest s|\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+------------+------------+\n", &crc);
    PROG_LIST_LOOP_ST
            char *state = getStateStr(curr->state);
    struct timespec tm1 = getTimeRestCheck(curr);
    struct timespec tm2 = getTimeRestCope(curr);
    snprintf(q, sizeof q, "|%11d|%11.11s|%11f|%11d|%12ld|%12ld|\n",
            curr->id,
            state,
            curr->sensor_fts.value.value,
            curr->sensor_fts.value.state,
            tm1.tv_sec,
            tm2.tv_sec
            );
    sendStr(q, &crc);
    PROG_LIST_LOOP_SP
    sendStr("+-----------+-----------+-----------+-----------+------------+------------+\n", &crc);

    sendStr("+-----------------------------------------------+\n", &crc);
    sendStr("|                      Alert                    |\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    sendStr("|   state   |call rest s|cope rest s|phone index|\n", &crc);
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    {
        char *state = getStateStr(alert_state);
        struct timespec tm1 = getTimeRestL(call_interval, tmr_call);
        struct timespec tm2 = getTimeRestL(cope_duration, tmr_cope);
        snprintf(q, sizeof q, "|%11.11s|%11ld|%11ld|%11d|\n",
                state,
                tm1.tv_sec,
                tm2.tv_sec,
                phone_ind
                );
        sendStr(q, &crc);
    }
    sendStr("+-----------+-----------+-----------+-----------+\n", &crc);
    struct timespec tm1 = getTimeRest_ts(sum_interval, tmr_sum.start);
    snprintf(q, sizeof q, "summary info will be sent in: %ld\n", tm1.tv_sec);
    sendStr(q, &crc);
    sendFooter(crc);
}

void printHelp() {
    char q[LINE_SIZE];
    uint8_t crc = 0;
    sendStr("COMMAND LIST\n", &crc);
    snprintf(q, sizeof q, "%c\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tterminate process\n", ACP_CMD_APP_EXIT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tload prog into RAM and start its execution; program id expected if '.' quantifier is used\n", ACP_CMD_START);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload program from RAM; program id expected if '.' quantifier is used\n", ACP_CMD_STOP);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tunload program from RAM, after that load it; program id expected if '.' quantifier is used\n", ACP_CMD_RESET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tdisable active alarm\n", ACP_CMD_ALR_ALARM_DISABLE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget alarm state sting\n", ACP_CMD_ALR_ALARM_GET);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tenable running program; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_ENABLE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tdisable running program; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_DISABLE);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset sms option (0 or 1) for program; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_SET_SMS);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset ring option (0 or 1) for program; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_SET_RING);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset goal (float) for program; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_SET_GOAL);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tset delta (float) for program; program_d expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_SET_DELTA);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget prog runtime data in format:  progId_state_stateEM_output_timeRestSecToEMSwap; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_GET_DATA_RUNTIME);
    sendStr(q, &crc);
    snprintf(q, sizeof q, "%c\tget prog initial data in format;  progId_setPoint_mode_ONFdelta_PIDheaterKp_PIDheaterKi_PIDheaterKd_PIDcoolerKp_PIDcoolerKi_PIDcoolerKd; program id expected if '.' quantifier is used\n", ACP_CMD_ALR_PROG_GET_DATA_INIT);
    sendStr(q, &crc);
    sendFooter(crc);
}