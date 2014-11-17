/*
 * libir.h
 *
 *
 * author: BeJo Li
 * mail: bejo.mob@gmail.com
 *
 */
#ifndef __LIBIR_H__
#define __LIBIR_H__

UINT8 checksum(UINT8 *, int);
int ir_serial_init(char *, int *);
UINT8 *alloc_code(int);

#endif /* __LIBIR_H__ */
