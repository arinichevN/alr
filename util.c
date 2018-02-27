
#include "main.h"

FUN_LLIST_GET_BY_ID(Prog)
extern int getProgByIdFDB(int prog_id, Prog *item, PeerList *peer_list, sqlite3 *dbl, const char *db_path);

void stopProgThread(Prog *item) {
#ifdef MODE_DEBUG
    printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
}

void stopAllProgThreads(ProgList * list) {
    PROG_LIST_LOOP_ST
#ifdef MODE_DEBUG
            printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    PROG_LIST_LOOP_SP

    PROG_LIST_LOOP_ST
            void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
    PROG_LIST_LOOP_SP
}

void freeProg(Prog * item) {
    freeSocketFd(&item->sock_fd);
    freeMutex(&item->mutex);
    free(item->description);
    free(item);
}

void freeProgList(ProgList * list) {
    Prog *item = list->top, *temp;
    while (item != NULL) {
        temp = item;
        item = item->next;
        freeProg(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

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

int checkProg(const Prog *item) {
    if (item->check_interval.tv_sec < 0 || item->check_interval.tv_nsec < 0) {
        fprintf(stderr, "checkProg(): negative check_interval where prog id = %d\n", item->id);
        return 0;
    }
    if (item->cope_duration.tv_sec < 0 || item->cope_duration.tv_nsec < 0) {
        fprintf(stderr, "checkProg(): negative cope_duration where prog id = %d\n", item->id);
        return 0;
    }
    return 1;
}

struct timespec getTimeRestCope(const Prog *item) {
    return getTimeRestTmr(item->cope_duration, item->tmr_cope);
}

struct timespec getTimeRestCheck(const Prog *item) {
    return getTimeRestTmr(item->check_interval, item->tmr_check);
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

int bufCatProgRuntime(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        char *state = getStateStr(item->state);
        struct timespec tm_rest = getTimeRestCope(item);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR "%ld" ACP_DELIMITER_ROW_STR,
                item->id,
                state,
                tm_rest.tv_sec
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgInit(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
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
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
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
    snprintf(q, sizeof q, "log_limit: %d\n", log_limit);
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
    SEND_STR("+------------------------------------------------------------------------------------------------------------------------------------+\n")
    SEND_STR("|                                                          Program init                                                              |\n")
    SEND_STR("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n")
    SEND_STR("|    id     |   description  |sensor_peer| call_peer |sensor_rid |good_value |   delta   |check_intv | cope_dur  | phone_ng  |sms|cal|\n")
    SEND_STR("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n")

    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%16.16s|%11s|%11s|%11d|%11.3f|%11.3f|%11ld|%11ld|%11d|%3d|%3d|\n",
            item->id,
            item->description,
            item->sensor_fts.peer.id,
            item->call_peer.id,
            item->sensor_fts.remote_id,
            item->good_value,
            item->good_delta,
            item->check_interval.tv_sec,
            item->cope_duration.tv_sec,
            item->phone_number_group_id,
            item->sms,
            item->ring
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+----------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+---+---+\n")

    SEND_STR("+-------------------------------------------------------------------------+\n")
    SEND_STR("|                             Program runtime                             |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+------------+------------+\n")
    SEND_STR("|    id     |   state   |snsr_value |snsr_state |check rest s| cope rest s|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+------------+------------+\n")
    PROG_LIST_LOOP_ST
            char *state = getStateStr(item->state);
    struct timespec tm1 = getTimeRestCheck(item);
    struct timespec tm2 = getTimeRestCope(item);
    snprintf(q, sizeof q, "|%11d|%11.11s|%11.3f|%11d|%12ld|%12ld|\n",
            item->id,
            state,
            item->sensor_fts.value.value,
            item->sensor_fts.value.state,
            tm1.tv_sec,
            tm2.tv_sec
            );

    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR_L("+-----------+-----------+-----------+-----------+------------+------------+\n")
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
    snprintf(q, sizeof q, "%s\tenable running program; program id expected\n", ACP_CMD_PROG_ENABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable running program; program id expected\n", ACP_CMD_PROG_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset sms option (0 or 1) for program; program id expected\n", ACP_CMD_CHV_PROG_SET_SMS);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset ring option (0 or 1) for program; program id expected\n", ACP_CMD_CHV_PROG_SET_RING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset goal (float) for program; program id expected\n", ACP_CMD_CHV_PROG_SET_GOAL);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset delta (float) for program; program id expected\n", ACP_CMD_CHV_PROG_SET_DELTA);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog runtime data in format:  progId_state_stateEM_output_timeRestSecToEMSwap; program id expected\n", ACP_CMD_PROG_GET_DATA_RUNTIME);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog initial data in format;  progId_setPoint_mode_ONFdelta_PIDheaterKp_PIDheaterKi_PIDheaterKd_PIDcoolerKp_PIDcoolerKi_PIDcoolerKd; program id expected\n", ACP_CMD_PROG_GET_DATA_INIT);
    SEND_STR_L(q);
}