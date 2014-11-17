/*
 * ir.h
 *
 *
 * author: BeJo Li
 * mail: bejo.mob@gmail.com
 *
 */
#ifndef __IR_H__
#define __IR_H__

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>


#if 0
#define DBG(fmt, args...) fprintf(stderr, "[%s] %s(%d): " fmt, __FILE__,__FUNCTION__, __LINE__, ##args)
#else
#define DBG(fmt, args...)
#endif

#define IR_PRINT(fmt, args...) printf(fmt, ##args)

#define PID_FILE    "/tmp/ir.pid"
#define IR_LEARN_DATA_LOG "/tmp/ir_learn_data.log"

#define IR_OK   0
#define IR_FAIL -1
#define IR_EXCEPT   1
#define IR_ACK2 2
#define IR_LEARN_ACK2 3

#define IR_SERIAL_TTY   "/dev/ttyS0"
#define UINT8   unsigned char

#define IR_CMD_BUF  256
#define IR_RECV_BUF  1024

#define CMD_NUM 256
#define CMD_LEN 3
#define CMD_VER "0.3"

typedef struct cmd_info {
    int fd;
    UINT8 type;
    UINT8 isLooseLearn; // for IR learning
} CMD_INFO;

typedef struct cmd_hdr {
#define SYNC_KEY0   0x45
#define SYNC_KEY1   0x34 
        UINT8 sync_key[2];
        UINT8 cmd_id;
        UINT8 len;
        UINT8 data[0];
} CMD_HDR;

typedef struct cmd_reply {
        UINT8 cmd;
        UINT8 len;
        UINT8 status;
        UINT8 data[0];
} CMD_REPLY;

typedef struct tx_data {
#define CONTI_TX  0x1
#define SINGLE_TX  0x81  
        UINT8 tx_type; // 0x1 -> continuous, 0x81 -> single tx
#define CUSTOM_CODE0    0x00
#define CUSTOM_CODE1    0x00
#define CUSTOM_CODE2    0x00
#define CUSTOM_CODE3    0x00
        UINT8 custom_code[4];
        UINT8 data[0];
} TX_DATA;

typedef struct tx_learn_data {
#define CONTI_TX  0x1
#define SINGLE_TX  0x81  
        UINT8 tx_type; // 0x1 -> continuous, 0x81 -> single tx
        UINT8 data[0];
} TX_LEARN_DATA;


// Command table
#define STOP_IR_TX          0x0
#define IR_CMD_ACK2         0x0
#define GET_REVISION        0X09
#define STANDBY             0X0b
#define WAKE_UP             0X0c
#define GET_ID              0X23
#define LEARN_IR_SHORT_DATA 0X24
#define TX_LEARN_IR_DATA    0X25
#define TX_IR_DATA          0X26
#define LEARN_IR_LONG_DATA  0X24
#define GET_IR_SIGNATURE    0X30
#define FW_UPGRADE          0X40
#define TEST_41             0X41

// Ack status
#define NoERR 0x30
#define ERR 0x40


// control function

extern int wakeup(int, char **);
extern int chip_revision(int, char **);
extern int chip_id(int, char **);
extern int chip_sign(int, char **);
extern int ir_tx(int, char **);
extern int ir_stop_tx(int, char **);
extern int ir_loop_sparkle(int, char **);
extern int ir_reset(int, char **);
extern int ir_short_learn(int, char **);
extern int ir_learn_tx(int, char **);
extern int ir_pre_learn_tx(int, char **);

#endif /* __IR_H__ */
