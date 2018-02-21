
#ifndef CHV_H
#define CHV_H

#include "lib/dbl.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/app.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/chv.h"
#include "lib/acp/prog.h"
#include "lib/acp/mobile.h"
#include "lib/configl.h"
#include "lib/logl.h"


#define APP_NAME chv
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif
#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define WAIT_RESP_TIMEOUT 1

#define PROG_LIST_LOOP_ST {Prog *item = prog_list.top; while (item != NULL) {
#define PROG_LIST_LOOP_SP item = item->next; } item = prog_list.top;}

#define SNR_VALUE item->sensor_fts.value.value
#define SNR_STATE item->sensor_fts.value.state
#define GOOD_VALUE item->good_value
#define DELTA item->good_delta
#define GOOD_CONDITION SNR_STATE && (SNR_VALUE <= GOOD_VALUE+DELTA) && (SNR_VALUE >= GOOD_VALUE-DELTA)
#define BAD_CONDITION !SNR_STATE || (SNR_VALUE > GOOD_VALUE+DELTA) || (SNR_VALUE < GOOD_VALUE-DELTA)
#define STATUS_SUCCESS "success"
#define STATUS_FAILURE "failure"

#define PROG_FIELDS "id,description,sensor_fts_id,call_peer_id,good_value,good_delta,check_interval,cope_duration,phone_number_group_id,sms,ring,enable,load"

enum {
    INIT,
    OFF,
    DISABLE,
    ACT,
    WBAD,
    WCOPE,
    WGOOD,
    WTIME
} StateAPP;

struct prog_st {
    int id;
    char * description;
    SensorFTS sensor_fts;
    Peer call_peer;
    int phone_number_group_id;
    double good_value;
    double good_delta;
    struct timespec check_interval;
    struct timespec cope_duration;
    int sms;
    int ring;

    char state;
    Ton_ts tmr_check;
    Ton_ts tmr_cope;

    struct timespec cycle_duration;
    int log_limit;
    int sock_fd;

    pthread_t thread;
    Mutex mutex;
    struct prog_st *next;
};

typedef struct prog_st Prog;

DEC_LLIST(Prog)

typedef struct {
    ProgList *prog_list;
    PeerList *peer_list;
    Prog * prog;
    sqlite3 *db_data;
} ProgData;

extern int readSettings();

extern int initData();

extern void initApp();

extern void serverRun(int *state, int init_state);

extern void progControl(Prog *item);

extern void *threadFunction(void *arg);

extern void freeData();

extern void freeApp();

extern void exit_nicely();

extern void exit_nicely_e(char *s);

#endif 

