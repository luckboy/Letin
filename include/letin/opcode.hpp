/****************************************************************************
 *   Copyright (C) 2014-2015 Łukasz Szpakowski.                             *
 *                                                                          *
 *   This software is licensed under the GNU Lesser General Public          *
 *   License v3 or later. See the LICENSE file and the GPL file for         *
 *   the full licensing terms.                                              *
 ****************************************************************************/
#ifndef _LETIN_OPCODE_HPP
#define _LETIN_OPCODE_HPP

#include <cstdint>

namespace letin
{
  namespace opcode
  {
    const std::uint32_t INSTR_LET =     0x00;
    const std::uint32_t INSTR_IN =      0x01;
    const std::uint32_t INSTR_RET =     0x02;
    const std::uint32_t INSTR_JC =      0x03;
    const std::uint32_t INSTR_JUMP =    0x04;
    const std::uint32_t INSTR_ARG =     0x05;
    const std::uint32_t INSTR_RETRY =   0x06;
    const std::uint32_t INSTR_LETTUPLE = 0x07;

    const std::uint32_t OP_ILOAD =      0x00;
    const std::uint32_t OP_ILOAD2 =     0x01;
    const std::uint32_t OP_INEG =       0x02;
    const std::uint32_t OP_IADD =       0x03;
    const std::uint32_t OP_ISUB =       0x04;
    const std::uint32_t OP_IMUL =       0x05;
    const std::uint32_t OP_IDIV =       0x06;
    const std::uint32_t OP_IMOD =       0x07;
    const std::uint32_t OP_INOT =       0x08;
    const std::uint32_t OP_IAND =       0x09;
    const std::uint32_t OP_IOR =        0x0a;
    const std::uint32_t OP_IXOR =       0x0b;
    const std::uint32_t OP_ISHL =       0x0c;
    const std::uint32_t OP_ISHR =       0x0d;
    const std::uint32_t OP_ISHRU =      0x0e;
    const std::uint32_t OP_IEQ =        0x0f;
    const std::uint32_t OP_INE =        0x10;
    const std::uint32_t OP_ILT =        0x11;
    const std::uint32_t OP_IGE =        0x12;
    const std::uint32_t OP_IGT =        0x13;
    const std::uint32_t OP_ILE =        0x14;
    const std::uint32_t OP_FLOAD =      0x15;
    const std::uint32_t OP_FLOAD2 =     0x16;
    const std::uint32_t OP_FNEG =       0x17;
    const std::uint32_t OP_FADD =       0x18;
    const std::uint32_t OP_FSUB =       0x19;
    const std::uint32_t OP_FMUL =       0x1a;
    const std::uint32_t OP_FDIV =       0x1b;
    const std::uint32_t OP_FEQ =        0x1c;
    const std::uint32_t OP_FNE =        0x1d;
    const std::uint32_t OP_FLT =        0x1e;
    const std::uint32_t OP_FGE =        0x1f;
    const std::uint32_t OP_FGT =        0x20;
    const std::uint32_t OP_FLE =        0x21;
    const std::uint32_t OP_RLOAD =      0x22;
    const std::uint32_t OP_REQ =        0x23;
    const std::uint32_t OP_RNE =        0x24;
    const std::uint32_t OP_RIARRAY8 =   0x25;
    const std::uint32_t OP_RIARRAY16 =  0x26;
    const std::uint32_t OP_RIARRAY32 =  0x27;
    const std::uint32_t OP_RIARRAY64 =  0x28;
    const std::uint32_t OP_RSFARRAY =   0x29;
    const std::uint32_t OP_RDFARRAY =   0x2a;
    const std::uint32_t OP_RRARRAY =    0x2b;
    const std::uint32_t OP_RTUPLE =     0x2c;
    const std::uint32_t OP_RIANTH8 =    0x2d;
    const std::uint32_t OP_RIANTH16 =   0x2e;
    const std::uint32_t OP_RIANTH32 =   0x2f;
    const std::uint32_t OP_RIANTH64 =   0x30;
    const std::uint32_t OP_RSFANTH =    0x31;
    const std::uint32_t OP_RDFANTH =    0x32;
    const std::uint32_t OP_RRANTH =     0x33;
    const std::uint32_t OP_RTNTH =      0x34;
    const std::uint32_t OP_RIALEN8 =    0x35;
    const std::uint32_t OP_RIALEN16 =   0x36;
    const std::uint32_t OP_RIALEN32 =   0x37;
    const std::uint32_t OP_RIALEN64 =   0x38;
    const std::uint32_t OP_RSFALEN =    0x39;
    const std::uint32_t OP_RDFALEN =    0x3a;
    const std::uint32_t OP_RRALEN =     0x3b;
    const std::uint32_t OP_RTLEN =      0x3c;
    const std::uint32_t OP_RIACAT8 =    0x3d;
    const std::uint32_t OP_RIACAT16 =   0x3e;
    const std::uint32_t OP_RIACAT32 =   0x3f;
    const std::uint32_t OP_RIACAT64 =   0x40;
    const std::uint32_t OP_RSFACAT =    0x41;
    const std::uint32_t OP_RDFACAT =    0x42;
    const std::uint32_t OP_RRACAT =     0x43;
    const std::uint32_t OP_RTCAT =      0x44;
    const std::uint32_t OP_RTYPE =      0x45;
    const std::uint32_t OP_ICALL =      0x46;
    const std::uint32_t OP_FCALL =      0x47;
    const std::uint32_t OP_RCALL =      0x48;
    const std::uint32_t OP_ITOF =       0x49;
    const std::uint32_t OP_FTOI =       0x4a;
    const std::uint32_t OP_INCALL =     0x4b;
    const std::uint32_t OP_FNCALL =     0x4c;
    const std::uint32_t OP_RNCALL =     0x4d;
    const std::uint32_t OP_RUIAFILL8 =  0x4e;
    const std::uint32_t OP_RUIAFILL16 = 0x4f;
    const std::uint32_t OP_RUIAFILL32 = 0x50;
    const std::uint32_t OP_RUIAFILL64 = 0x51;
    const std::uint32_t OP_RUSFAFILL =  0x52;
    const std::uint32_t OP_RUDFAFILL =  0x53;
    const std::uint32_t OP_RURAFILL =   0x54;
    const std::uint32_t OP_RUTFILLI =   0x55;
    const std::uint32_t OP_RUTFILLF =   0x56;
    const std::uint32_t OP_RUTFILLR =   0x57;
    const std::uint32_t OP_RUIANTH8 =   0x58;
    const std::uint32_t OP_RUIANTH16 =  0x59;
    const std::uint32_t OP_RUIANTH32 =  0x5a;
    const std::uint32_t OP_RUIANTH64 =  0x5b;
    const std::uint32_t OP_RUSFANTH =   0x5c;
    const std::uint32_t OP_RUDFANTH =   0x5d;
    const std::uint32_t OP_RURANTH =    0x5e;
    const std::uint32_t OP_RUTNTH =     0x5f;
    const std::uint32_t OP_RUIASNTH8 =  0x60;
    const std::uint32_t OP_RUIASNTH16 = 0x61;
    const std::uint32_t OP_RUIASNTH32 = 0x62;
    const std::uint32_t OP_RUIASNTH64 = 0x63;
    const std::uint32_t OP_RUSFASNTH =  0x64;
    const std::uint32_t OP_RUDFASNTH =  0x65;
    const std::uint32_t OP_RURASNTH =   0x66;
    const std::uint32_t OP_RUTSNTH =    0x67;
    const std::uint32_t OP_RUIALEN8 =   0x68;
    const std::uint32_t OP_RUIALEN16 =  0x69;
    const std::uint32_t OP_RUIALEN32 =  0x6a;
    const std::uint32_t OP_RUIALEN64 =  0x6b;
    const std::uint32_t OP_RUSFALEN =   0x6c;
    const std::uint32_t OP_RUDFALEN =   0x6d;
    const std::uint32_t OP_RURALEN =    0x6e;
    const std::uint32_t OP_RUTLEN =     0x6f;
    const std::uint32_t OP_RUTYPE =     0x70;
    const std::uint32_t OP_RUIATOIA8 =  0x71;
    const std::uint32_t OP_RUIATOIA16 = 0x72;
    const std::uint32_t OP_RUIATOIA32 = 0x73;
    const std::uint32_t OP_RUIATOIA64 = 0x74;
    const std::uint32_t OP_RUSFATOSFA = 0x75;
    const std::uint32_t OP_RUDFATODFA = 0x76;
    const std::uint32_t OP_RURATORA =   0x77;
    const std::uint32_t OP_RUTTOT =     0x78;

    const std::uint32_t ARG_TYPE_LVAR = 0x0;
    const std::uint32_t ARG_TYPE_ARG =  0x1;
    const std::uint32_t ARG_TYPE_IMM =  0x2;
    const std::uint32_t ARG_TYPE_GVAR = 0x3;

    inline std::uint32_t opcode(std::uint32_t instr, std::uint32_t op, std::uint32_t arg_type1 = 0, std::uint32_t arg_type2 = 0, std::uint32_t local_var_count = 2)
    {
      return (instr & 0xff) | ((op & 0xff) << 8) | ((arg_type1 & 0xf) << 16) | ((arg_type2 & 0xf) << 20) | ((local_var_count - 2) << 24);
    }

    inline std::uint32_t opcode_to_instr(std::uint32_t opcode)
    {
      return opcode & 0xff;
    }

    inline std::uint32_t opcode_to_op(std::uint32_t opcode)
    {
      return (opcode >> 8) & 0xff;
    }

    inline std::uint32_t opcode_to_arg_type1(std::uint32_t opcode)
    {
      return (opcode >> 16) & 0xf;
    }

    inline std::uint32_t opcode_to_arg_type2(std::uint32_t opcode)
    {
      return (opcode >> 20) & 0xf;
    }

    inline std::uint32_t opcode_to_local_var_count(std::uint32_t opcode)
    {
      return ((opcode >> 24) & 0xff) + 2;
    }
  }
}

#endif
