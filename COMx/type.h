#ifndef _TYPE_H
#define _TYPE_H
#define DEBUG
/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
typedef signed long  s32;
typedef signed short s16;
typedef signed char  s8;

typedef signed long  const sc32;  /* Read Only */
typedef signed short const sc16;  /* Read Only */
typedef signed char  const sc8;   /* Read Only */

typedef unsigned long  u32;
typedef unsigned short u16;
typedef unsigned char  u8;

typedef unsigned long  const uc32;  /* Read Only */
typedef unsigned short const uc16;  /* Read Only */
typedef unsigned char  const uc8;   /* Read Only */

typedef enum {FALSE = 0, TRUE = !FALSE} bool;
typedef enum
{
	RE_SUCCESS,
	RE_INVALID_POINTER,
	RE_INVALID_PARAMETER,
	RE_OP_FAIL,
}re_error_enum;
#define H_VAL16(val) (u8)((u16)(val) >> 8)
#define L_VAL16(val) (u8)((u16)(val) & 0xff)
#define H_VAL4(val) (u8)(((u8)(val) >> 4)& 0x0f)
#define L_VAL4(val) (u8)((u8)(val) & 0x0f)
#endif
