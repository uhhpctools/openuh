/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/

#if 0
#define MAX_UNARY_OPS  1 

static const OPCODE Unary_Opcode [MAX_UNARY_OPS] [MTYPE_LAST + 1] = {
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4NEG,      /* MTYPE_I1    */
  OPC_I4NEG,      /* MTYPE_I2   */
  OPC_I4NEG,      /* MTYPE_I4   */
  OPC_I8NEG,      /* MTYPE_I8   */
  OPC_U4NEG,      /* MTYPE_U1   */
  OPC_U4NEG,      /* MTYPE_U2  */
  OPC_U4NEG,      /* MTYPE_U4  */
  OPC_U8NEG,      /* MTYPE_U8  */
  OPC_F4NEG,      /* MTYPE_F4  */
  OPC_F8NEG,      /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQNEG,      /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M   */
  OPC_C4NEG,      /* MTYPE_C4  */
  OPC_C8NEG,      /* MTYPE_C8  */
  OPC_CQNEG,      /* MTYPE_CQ  */
  OPCODE_UNKNOWN     /* MTYPE_V   */

};

static const OPCODE Binary_Opcode [opLAST_BIN_OP][MTYPE_LAST + 1]  = {{ 
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4ADD,      /* MTYPE_I1    */
  OPC_I4ADD,      /* MTYPE_I2   */
  OPC_I4ADD,      /* MTYPE_I4   */
  OPC_I8ADD,      /* MTYPE_I8   */
  OPC_U4ADD,      /* MTYPE_U1   */
  OPC_U4ADD,      /* MTYPE_U2  */
  OPC_U4ADD,      /* MTYPE_U4  */
  OPC_U8ADD,      /* MTYPE_U8  */
  OPC_F4ADD,      /* MTYPE_F4  */
  OPC_F8ADD,      /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQADD,      /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPC_C4ADD,      /* MTYPE_C4 */
  OPC_C8ADD,      /* MTYPE_C8 */
  OPC_CQADD,      /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4SUB,      /* MTYPE_I1    */
  OPC_I4SUB,      /* MTYPE_I2   */
  OPC_I4SUB,      /* MTYPE_I4   */
  OPC_I8SUB,      /* MTYPE_I8   */
  OPC_U4SUB,      /* MTYPE_U1   */
  OPC_U4SUB,      /* MTYPE_U2  */
  OPC_U4SUB,      /* MTYPE_U4  */
  OPC_U8SUB,      /* MTYPE_U8  */
  OPC_F4SUB,      /* MTYPE_F4  */
  OPC_F8SUB,      /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQSUB,      /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPC_C4SUB,      /* MTYPE_C4 */
  OPC_C8SUB,      /* MTYPE_C8 */
  OPC_CQSUB,      /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4MPY,      /* MTYPE_I1    */
  OPC_I4MPY,      /* MTYPE_I2   */
  OPC_I4MPY,      /* MTYPE_I4   */
  OPC_I8MPY,      /* MTYPE_I8   */
  OPC_U4MPY,      /* MTYPE_U1   */
  OPC_U4MPY,      /* MTYPE_U2  */
  OPC_U4MPY,      /* MTYPE_U4  */
  OPC_U8MPY,      /* MTYPE_U8  */
  OPC_F4MPY,      /* MTYPE_F4  */
  OPC_F8MPY,      /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQMPY,      /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPC_C4MPY,      /* MTYPE_C4 */
  OPC_C8MPY,      /* MTYPE_C8 */
  OPC_CQMPY,      /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4DIV,      /* MTYPE_I1    */
  OPC_I4DIV,      /* MTYPE_I2   */
  OPC_I4DIV,      /* MTYPE_I4   */
  OPC_I8DIV,      /* MTYPE_I8   */
  OPC_U4DIV,      /* MTYPE_U1   */
  OPC_U4DIV,      /* MTYPE_U2  */
  OPC_U4DIV,      /* MTYPE_U4  */
  OPC_U8DIV,      /* MTYPE_U8  */
  OPC_F4DIV,      /* MTYPE_F4  */
  OPC_F8DIV,      /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQDIV,      /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPC_C4DIV,      /* MTYPE_C4 */
  OPC_C8DIV,      /* MTYPE_C8 */
  OPC_CQDIV,      /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4GT,      /* MTYPE_I1    */
  OPC_I4GT,      /* MTYPE_I2   */
  OPC_I4GT,      /* MTYPE_I4   */
  OPC_I8GT,      /* MTYPE_I8   */
  OPC_U4GT,      /* MTYPE_U1   */
  OPC_U4GT,      /* MTYPE_U2  */
  OPC_U4GT,      /* MTYPE_U4  */
  OPC_U8GT,       /* MTYPE_U8  */
  OPC_F4GT,       /* MTYPE_F4  */
  OPC_F8GT,       /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQGT,       /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4GE,      /* MTYPE_I1    */
  OPC_I4GE,      /* MTYPE_I2   */
  OPC_I4GE,      /* MTYPE_I4   */
  OPC_I8GE,      /* MTYPE_I8   */
  OPC_U4GE,      /* MTYPE_U1   */
  OPC_U4GE,      /* MTYPE_U2  */
  OPC_U4GE,      /* MTYPE_U4  */
  OPC_U8GE,       /* MTYPE_U8  */
  OPC_F4GE,       /* MTYPE_F4  */
  OPC_F8GE,       /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQGE,       /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4LT,      /* MTYPE_I1    */
  OPC_I4LT,      /* MTYPE_I2   */
  OPC_I4LT,      /* MTYPE_I4   */
  OPC_I8LT,      /* MTYPE_I8   */
  OPC_U4LT,      /* MTYPE_U1   */
  OPC_U4LT,      /* MTYPE_U2  */
  OPC_U4LT,      /* MTYPE_U4  */
  OPC_U8LT,       /* MTYPE_U8  */
  OPC_F4LT,       /* MTYPE_F4  */
  OPC_F8LT,       /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQLT,       /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4LE,      /* MTYPE_I1    */
  OPC_I4LE,      /* MTYPE_I2   */
  OPC_I4LE,      /* MTYPE_I4   */
  OPC_I8LE,      /* MTYPE_I8   */
  OPC_U4LE,      /* MTYPE_U1   */
  OPC_U4LE,      /* MTYPE_U2  */
  OPC_U4LE,      /* MTYPE_U4  */
  OPC_U8LE,       /* MTYPE_U8  */
  OPC_F4LE,       /* MTYPE_F4  */
  OPC_F8LE,       /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQLE,       /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4EQ,      /* MTYPE_I1    */
  OPC_I4EQ,      /* MTYPE_I2   */
  OPC_I4EQ,      /* MTYPE_I4   */
  OPC_I8EQ,      /* MTYPE_I8   */
  OPC_U4EQ,      /* MTYPE_U1   */
  OPC_U4EQ,      /* MTYPE_U2  */
  OPC_U4EQ,      /* MTYPE_U4  */
  OPC_U8EQ,       /* MTYPE_U8  */
  OPC_F4EQ,       /* MTYPE_F4  */
  OPC_F8EQ,       /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQEQ,       /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPC_C4EQ,       /* MTYPE_C4 */
  OPC_C8EQ,       /* MTYPE_C8 */
  OPC_CQEQ,       /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4NE,      /* MTYPE_I1    */
  OPC_I4NE,      /* MTYPE_I2   */
  OPC_I4NE,      /* MTYPE_I4   */
  OPC_I8NE,      /* MTYPE_I8   */
  OPC_U4NE,      /* MTYPE_U1   */
  OPC_U4NE,      /* MTYPE_U2  */
  OPC_U4NE,      /* MTYPE_U4  */
  OPC_U8NE,       /* MTYPE_U8  */
  OPC_F4NE,       /* MTYPE_F4  */
  OPC_F8NE,       /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPC_FQNE,       /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPC_C4NE,       /* MTYPE_C4 */
  OPC_C8NE,       /* MTYPE_C8 */
  OPC_CQNE,      /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4BAND,     /* MTYPE_I1    */
  OPC_I4BAND,     /* MTYPE_I2   */
  OPC_I4BAND,     /* MTYPE_I4   */
  OPC_I8BAND,     /* MTYPE_I8   */
  OPC_U4BAND,     /* MTYPE_U1   */
  OPC_U4BAND,     /* MTYPE_U2  */
  OPC_U4BAND,     /* MTYPE_U4  */
  OPC_U8BAND,     /* MTYPE_U8  */
  OPCODE_UNKNOWN,    /* MTYPE_F4  */
  OPCODE_UNKNOWN,    /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPCODE_UNKNOWN,    /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4BIOR,     /* MTYPE_I1    */
  OPC_I4BIOR,     /* MTYPE_I2   */
  OPC_I4BIOR,     /* MTYPE_I4   */
  OPC_I8BIOR,     /* MTYPE_I8   */
  OPC_U4BIOR,     /* MTYPE_U1   */
  OPC_U4BIOR,     /* MTYPE_U2  */
  OPC_U4BIOR,     /* MTYPE_U4  */
  OPC_U8BIOR,     /* MTYPE_U8  */
  OPCODE_UNKNOWN,    /* MTYPE_F4  */
  OPCODE_UNKNOWN,    /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPCODE_UNKNOWN,    /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
},{
  OPCODE_UNKNOWN,    /* MTYPE_UNKNOWN */
  OPCODE_UNKNOWN,    /* MTYPE_B    */
  OPC_I4BXOR,     /* MTYPE_I1    */
  OPC_I4BXOR,     /* MTYPE_I2   */
  OPC_I4BXOR,     /* MTYPE_I4   */
  OPC_I8BXOR,     /* MTYPE_I8   */
  OPC_U4BXOR,     /* MTYPE_U1   */
  OPC_U4BXOR,     /* MTYPE_U2  */
  OPC_U4BXOR,     /* MTYPE_U4  */
  OPC_U8BXOR,     /* MTYPE_U8  */
  OPCODE_UNKNOWN,    /* MTYPE_F4  */
  OPCODE_UNKNOWN,    /* MTYPE_F8  */
  OPCODE_UNKNOWN,    /* MTYPE_F10  */
  OPCODE_UNKNOWN,    /* MTYPE_F16 */
  OPCODE_UNKNOWN,    /* MTYPE_STR  */
  OPCODE_UNKNOWN,    /* MTYPE_FQ    */
  OPCODE_UNKNOWN,    /* MTYPE_M  */
  OPCODE_UNKNOWN,    /* MTYPE_C4 */
  OPCODE_UNKNOWN,    /* MTYPE_C8 */
  OPCODE_UNKNOWN,    /* MTYPE_CQ */
  OPCODE_UNKNOWN     /* MTYPE_V    */
}
};



/* forward references */



