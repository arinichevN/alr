
#include "main.h"


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

int bufCatProgRuntime(const Prog *item, ACPResponse *response) {
    char q[LINE_SIZE];
    char *state = getStateStr(item->state);
    struct timespec tm_rest = getTimeRestCope(item);
    snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
            item->id,
            state,
            tm_rest.tv_sec
            );
    return acp_responseStrCat(response, q);
}

int bufCatProgInit(const Prog *item, ACPResponse *response) {
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
    return acp_responseStrCat(response, q);
}

void printData(ACPResponse *response) {
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    SEND_STR(q)
    snprintf(q, sizeof q, "cope_duration sec: %ld\n", cope_duration.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "call_interval sec: %ld\n", call_interval.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "log_limit: %d\n", log_limit);
    SEND_STR(q)
    snprintf(q, sizeof q, "sum_interval: %ld\n", sum_interval.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "phone_number_group_id: %d\n", phone_number_group_id);
    SEND_STR(q)
    snprintf(q, sizeof q, "call_peer_id: %s\n", call_peer_id);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_public_path: %s\n", db_public_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_log_path: %s\n", db_log_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)
            
  acp_sendPeerListInfo(&peer_list, response, &peer_client);

    snprintf(q, sizeof q, "prog_list length: %d\n", prog_list.length);
    SEND_STR(q)
    SEND_STR("+------------------------------------------------------------------------------------------------------------------------+\n")
    SEND_STR("|                                                     Program init                                                       |\n")
    SEND_STR("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n")
    SEND_STR("|    id     |   description  |sensor_ptr |sensor_rid |good_value |   delta   |check_intv | cope_dur  | phone_ng  |sms|cal|\n")
    SEND_STR("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n")
    PROG_LIST_LOOP_DF
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%16.16s|%11p|%11d|%11f|%11f|%11ld|%11ld|%11d|%3d|%3d|\n",
            curr->id,
            curr->description,
            (void *)&curr->sensor_fts,
            curr->sensor_fts.remote_id,
            curr->good_value,
            curr->good_delta,
            curr->check_interval.tv_sec,
            curr->cope_duration.tv_sec,
            curr->phone_number_group_id,
            curr->sms,
            curr->ring
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n")

    SEND_STR("+-------------------------------------------------------------------------+\n")
    SEND_STR("|                             Program runtime                             |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+------------+------------+\n")
    SEND_STR("|    id     |   state   |snsr_value |snsr_state |check rest s| cope rest s|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+------------+------------+\n")
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
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+-----------+-----------+-----------+------------+------------+\n")

    SEND_STR("+-----------------------------------------------+\n")
    SEND_STR("|                      Alert                    |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|   state   |call rest s|cope rest s|phone index|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
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
        SEND_STR(q)
    }
    SEND_STR("+-----------+-----------+-----------+-----------+\n")
    struct timespec tm1 = getTimeRest_ts(sum_interval, tmr_sum.start);
    snprintf(q, sizeof q, "summary info will be sent in: %ld\n", tm1.tv_sec);
    SEND_STR_L(q);
}

void printHelp(ACPResponse *response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tload prog into RAM and start its execution; program id expected\n", ACP_CMD_PROG_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM; program id expected if\n", ACP_CMD_PROG_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM, after that load it; program id expected\n", ACP_CMD_PROG_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable active alarm\n", ACP_CMD_ALR_ALARM_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget alarm state sting\n", ACP_CMD_ALR_ALARM_GET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tenable running program; program id expected\n", ACP_CMD_PROG_ENABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable running program; program id expected\n", ACP_CMD_PROG_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset sms option (0 or 1) for program; program id expected\n", ACP_CMD_ALR_PROG_SET_SMS);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset ring option (0 or 1) for program; program id expected\n", ACP_CMD_ALR_PROG_SET_RING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset goal (float) for program; program id expected\n", ACP_CMD_ALR_PROG_SET_GOAL);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset delta (float) for program; program id expected\n", ACP_CMD_ALR_PROG_SET_DELTA);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog runtime data in format:  progId_state_stateEM_output_timeRestSecToEMSwap; program id expected\n", ACP_CMD_PROG_GET_DATA_RUNTIME);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog initial data in format;  progId_setPoint_mode_ONFdelta_PIDheaterKp_PIDheaterKi_PIDheaterKd_PIDcoolerKp_PIDcoolerKi_PIDcoolerKd; program id expected\n", ACP_CMD_PROG_GET_DATA_INIT);
    SEND_STR_L(q);
}