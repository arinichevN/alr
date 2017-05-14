
#include "reg.h"

char * reg_getStateStr(char state) {
    switch (state) {
        case REG_OFF:
            return "OFF";
            break;
        case REG_INIT:
            return "INIT";
            break;
        case REG_DO:
            return "DO";
            break;
        case REG_WAIT:
            return "WAIT";
            break;
        case REG_BUSY:
            return "BUSY";
            break;
        case REG_COOLER:
            return "COOLER";
            break;
        case REG_HEATER:
            return "HEATER";
            break;
        case REG_DISABLE:
            return "DISABLE";
            break;
        case REG_MODE_PID:
            return "PID";
            break;
        case REG_MODE_ONF:
            return "ONF";
            break;
    }
    return "\0";
}

int reg_controlEM(EM *item, float output) {
    if (item == NULL) {
        return 0;
    }
    return acp_setEMDutyCycleR(item, output);
}