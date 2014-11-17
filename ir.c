/*
 * ir.c
 *
 *
 * author: BeJo Li
 * mail: bejo.mob@gmail.com
 */

#include <ir.h>
#include <libir.h>

int ir_usage(int, char **);
int version(int, char **);
typedef int (*FUNC)(int, char **);

#define PRE_STR "Version:%s\n"\
"Usage: ir [OPTION...]\n"\
"ir is command line controler for RT400 ChipSet\n\n"\
"Examples:\n"\
"\tir help\t\t#Show help page.\n\n"\
"Command:\n"


void *cmd_tables[CMD_NUM][CMD_LEN] = {
    {"help", "\t\t\tTo show this help", &ir_usage},
    {"revision", "\t\tget ChipSet revision information", &chip_revision},
    {"id", "\t\t\tget ChipSet ID", &chip_id},
    {"wakeup", "\t\twakeup device", &wakeup},
    {"tx", "\t\t\ttransmit IR data", &ir_tx},
    {"stoptx", "\t\tStop transmit IR data", &ir_stop_tx},
    {"shortlearn", "\t\tEnter short learning state for AV device, second VAR isLooseMode", &ir_short_learn},
    {"learntx", "\t\ttransmit IR Learning data", &ir_learn_tx},
    {"prelearntx", "\t\ttransmit pre-record IR Learning data "IR_LEARN_DATA_LOG, &ir_pre_learn_tx},
    {"reset", "\t\tReset IR MCU", &ir_reset},
    {"version", "\t\tshow Version", &version},
    {NULL, NULL, NULL}
};



int version(int argc, char **argv)
{   
    IR_PRINT("Version: %s\n", CMD_VER); 
    return IR_OK;
}       
        
        
int ir_usage(int argc, char **argv)
{   
    int i = 0;
    
    IR_PRINT(PRE_STR, CMD_VER);
    for (i = 0; cmd_tables[i][0]; i++) {
        IR_PRINT("  %s,%-32s\n", (char *)cmd_tables[i][0], (char *)cmd_tables[i][1]);
    }   
    return IR_OK;
}

int ir_cmd_check(void *cmd_tables[CMD_NUM][CMD_LEN], char *cmd)
{
    int i = 0;
    int match = 0;
    int cmd_index = -1;

    for (i = 0; cmd_tables[i][0]; i++){
        DBG("cmd_tables[i][0] = %s, cmd = %s\n", (char *)cmd_tables[i][0], (char *)cmd);
        if(!strncmp(cmd_tables[i][0], cmd, strlen(cmd))){
            match++;
            cmd_index = i;
        }
    }
    if (match == 1) {
        return cmd_index;
    } else {
        return IR_FAIL;
    }
}

void ir_lock(void)
{
    pid_t pid;
    FILE *fp = NULL;
    int timeout = 5;

    // Checking pid for 5s timeout

    while (timeout) {
        if(0 == access(PID_FILE, R_OK)) {
            fp = fopen(PID_FILE, "r");
            if (fp == NULL) {
                IR_PRINT("PID File %s created fail\n\n", PID_FILE);
                exit(IR_FAIL);
            }
            if (timeout == 1 ) {
                fscanf(fp, "%d", &pid);
                kill(pid, SIGKILL);
                fclose(fp);
                IR_PRINT("\nIR is busy and timeout happend!!!\nKill and do new request\n");
                unlink(PID_FILE);
            }
            timeout--;
            sleep(1);
            continue;
        }
        break;
    }
    fp = fopen(PID_FILE, "w+");
    if (fp == NULL) {
        IR_PRINT("PID File %s created fail\n\n", PID_FILE);
        exit(IR_FAIL);
    }

    fprintf(fp, "%d", getpid());
    fclose(fp);
}


int main(int argc,char** argv)
{

    int cmdVector = 0;

    if (argc < 2) {
        ir_usage(argc, argv);
        return IR_FAIL;
    }

    cmdVector = ir_cmd_check(cmd_tables, argv[1]);
    if (cmdVector == IR_FAIL) {
        IR_PRINT("%s: unrecognized option '%s'\n", argv[0], argv[1]);
        ir_usage(argc, argv);
        IR_PRINT("\nTry '%s help' for more information.\n", argv[0]);
        return IR_FAIL;
    }

    ir_lock();

    if (cmd_tables[cmdVector][2]) {
        ((FUNC)cmd_tables[cmdVector][2])(argc, argv);
    } else {
        IR_PRINT("\n%s: Command is not support -- %s\n", argv[0], argv[1]);
        IR_PRINT("\nTry '%s help' for more information.\n", argv[0]);
    }

    unlink(PID_FILE);
    return IR_OK;
}
