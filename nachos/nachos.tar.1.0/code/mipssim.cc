
/* $Revision: 1.2 $ on $Date: 1991/12/20 07:50:34 $
 * $Source: /home/ygdrasil/a/faustus/cs162/hw1+/RCS/machine.cc,v $
 *
 * This code has been adapted from Ousterhout's MIPSSIM package.
 * Byte ordering is little-endian, so we can be compatible with
 * DEC RISC systems.
 */

#include "machine.h"
#include "mipssim.h"
#include "addrspace.h"
#include "system.h"

bool
Machine::OneInstruction()
{
    // New values for "loadReg" and "loadValue" registers.
    int nextLoadReg = 0, nextLoadValue = 0;

    // Special case -- if the current value of the PC is -4, return.  This
    // is an annoying hack.  The return value is in R2.
    if (registers[PCReg] == -4) {
	registers[registers[LoadReg]] = registers[LoadValueReg];
	registers[LoadReg] = 0;
	registers[LoadValueReg] = 0;
	int retval = registers[2];
	printf("*** program exit status was %d ***\n", retval);
	return (FALSE);
    }
    
    // Fetch instruction, checking for page fault.
    int raw;
    if (!space->ReadMem(registers[PCReg], 4, &raw))
	return (TRUE);
    
    // What's Irish and sits outside your house?  Patty O'Furniture!
    DecodedInstruction instr = Decode(raw);

    DEBUG('m', "At PC = 0x%x: ", registers[PCReg]);
    
    // Compute next pc, but don't install in case there's an error or branch.
    int pcAfter = registers[NextPCReg] + 4;

    // Execute the instruction.
    int sum, diff, tmp, value;
    unsigned int rs, rt, imm;
    switch (instr.opCode) {
	
      case OP_ADD:
	DEBUG('m', "Executing: ADD r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	sum = registers[instr.rs] + registers[instr.rt];
	if (!((registers[instr.rs] ^ registers[instr.rt]) & SIGN_BIT) &&
	    ((registers[instr.rs] ^ sum) & SIGN_BIT)) {
	    RaiseException(OverflowException);
	    return (TRUE);
	}
	registers[instr.rd] = sum;
	break;
	
      case OP_ADDI:
	DEBUG('m', "Executing: ADDI r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	sum = registers[instr.rs] + instr.extra;
	if (!((registers[instr.rs] ^ instr.extra) & SIGN_BIT) &&
	    ((instr.extra ^ sum) & SIGN_BIT)) {
	    RaiseException(OverflowException);
	    return (TRUE);
	}
	registers[instr.rt] = sum;
	break;
	
      case OP_ADDIU:
	DEBUG('m', "Executing: ADDIU r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	registers[instr.rt] = registers[instr.rs] + instr.extra;
	break;
	
      case OP_ADDU:
	DEBUG('m', "Executing: ADDU r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	registers[instr.rd] = registers[instr.rs] + registers[instr.rt];
	break;
	
      case OP_AND:
	DEBUG('m', "Executing: AND r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	registers[instr.rd] = registers[instr.rs] & registers[instr.rt];
	break;
	
      case OP_ANDI:
	DEBUG('m', "Executing: ANDI r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	registers[instr.rt] = registers[instr.rs] & (instr.extra & 0xffff);
	break;
	
      case OP_BEQ:
	DEBUG('m', "Executing: BEQ r%d,r%d,%d\n", instr.rs, instr.rt,
	      instr.extra);
	if (registers[instr.rs] == registers[instr.rt])
	    pcAfter = registers[NextPCReg] + IndexToAddr(instr.extra);
	break;
	
      case OP_BGEZAL:
	registers[R31] = registers[NextPCReg] + 4;
      case OP_BGEZ:
	DEBUG('m', "Executing: BGEZ(AL) r%d,%d\n", instr.rs, instr.extra);
	if (!(registers[instr.rs] & SIGN_BIT))
	    pcAfter = registers[NextPCReg] + IndexToAddr(instr.extra);
	break;
	
      case OP_BGTZ:
	DEBUG('m', "Executing: BGTZ r%d,%d\n", instr.rs, instr.extra);
	if (registers[instr.rs] > 0)
	    pcAfter = registers[NextPCReg] + IndexToAddr(instr.extra);
	break;
	
      case OP_BLEZ:
	DEBUG('m', "Executing: BLEZ r%d,%d\n", instr.rs, instr.extra);
	if (registers[instr.rs] <= 0)
	    pcAfter = registers[NextPCReg] + IndexToAddr(instr.extra);
	break;
	
      case OP_BLTZAL:
	registers[R31] = registers[NextPCReg] + 4;
      case OP_BLTZ:
	DEBUG('m', "Executing: BLTZ(AL) r%d,%d\n", instr.rs, instr.extra);
	if (registers[instr.rs] & SIGN_BIT)
	    pcAfter = registers[NextPCReg] + IndexToAddr(instr.extra);
	break;
	
      case OP_BNE:
	DEBUG('m', "Executing: BEQ r%d,r%d,%d\n", instr.rs, instr.rt,
	      instr.extra);
	if (registers[instr.rs] != registers[instr.rt])
	    pcAfter = registers[NextPCReg] + IndexToAddr(instr.extra);
	break;
	
      case OP_DIV:
	DEBUG('m', "Executing: DIV r%d,r%d\n", instr.rs, instr.rt);
	if (registers[instr.rt] == 0) {
	    registers[LoReg] = 0;
	    registers[HiReg] = 0;
	} else {
	    registers[LoReg] =  registers[instr.rs] / registers[instr.rt];
	    registers[HiReg] = registers[instr.rs] % registers[instr.rt];
	}
	break;
	
      case OP_DIVU:	  
	  DEBUG('m', "Executing: DIVU r%d,r%d\n", instr.rs, instr.rt);
	  rs = (unsigned int) registers[instr.rs];
	  rt = (unsigned int) registers[instr.rt];
	  if (rt == 0) {
	      registers[LoReg] = 0;
	      registers[HiReg] = 0;
	  } else {
	      tmp = rs / rt;
	      registers[LoReg] = (int) tmp;
	      tmp = rs % rt;
	      registers[HiReg] = (int) tmp;
	  }
	  break;
	
      case OP_JAL:
	registers[R31] = registers[NextPCReg] + 4;
      case OP_J:
	DEBUG('m', "Executing: J(AL) %d\n", instr.extra);
	pcAfter = (pcAfter & 0xf0000000) | IndexToAddr(instr.extra);
	break;
	
      case OP_JALR:
	registers[instr.rd] = registers[NextPCReg] + 4;
      case OP_JR:
	DEBUG('m', "Executing: J(AL)R r%d,r%d\n", instr.rd, instr.rs);
	pcAfter = registers[instr.rs];
	break;
	
      case OP_LB:
      case OP_LBU:
	DEBUG('m', "Executing: LB(U) r%d,%d(r%d)\n", instr.rt, instr.extra,
	      instr.rs);
	tmp = registers[instr.rs] + instr.extra;
	if (!space->ReadMem(tmp & ~0x3, 4, &value))
	    return (TRUE);
	
#ifdef BIG_ENDIAN
	switch (tmp & 0x3) {
	  case 0:
	    value >>= 24;
	    break;
	  case 1:
	    value >>= 16;
	    break;
	  case 2:
	    value >>= 8;
	}
	if ((value & 0x80) && (instr.opCode == OP_LB))
	    value |= 0xffffff00;
	else
	    value &= 0xff;
#else
	switch (tmp & 0x3) {
	  case 0:
	    // value >>= 0;
	    break;
	  case 1:
	    value >>= 8;
	    break;
	  case 2:
	    value >>= 16;
	    break;
	  case 3:
	    value >>= 24;
	    break;
	}
	if ((value & 0x80) && (instr.opCode == OP_LB))
	    value |= 0xffffff00;
	else
	    value &= 0xff;
#endif
	nextLoadReg = instr.rt;
	nextLoadValue = value;
	break;
	
      case OP_LH:
      case OP_LHU:	  
	DEBUG('m', "Executing: LH(U) r%d,%d(r%d)\n", instr.rt, instr.extra,
	      instr.rs);
	tmp = registers[instr.rs] + instr.extra;
	if (tmp & 0x1) {
	    badVirtualAddress = tmp;
	    RaiseException(AddressErrorException);
	    return (TRUE);
	}
	if (!space->ReadMem(tmp, 4, &value))
	    return (TRUE);
#ifdef BIG_ENDIAN
	if (!(tmp & 0x2))
	    value >>= 16;
#else
	if (tmp & 0x2)
	    value >>= 16;
#endif	
	if ((value & 0x8000) && (instr.opCode == OP_LH))
	    value |= 0xffff0000;
	else
	    value &= 0xffff;
	nextLoadReg = instr.rt;
	nextLoadValue = value;
	break;
      	
      case OP_LUI:
	DEBUG('m', "Executing: LUI r%d,%d\n", instr.rt, instr.extra);
	registers[instr.rt] = instr.extra << 16;
	break;
	
      case OP_LW:
	DEBUG('m', "Executing: LW r%d,%d(r%d)\n", instr.rt, instr.extra,
	      instr.rs);
	tmp = registers[instr.rs] + instr.extra;
	if (tmp & 0x3) {
	    badVirtualAddress = tmp;
	    RaiseException(AddressErrorException);
	    return (TRUE);
	}
	if (!space->ReadMem(tmp, 4, &value))
	    return (TRUE);
	nextLoadReg = instr.rt;
	nextLoadValue = value;
	break;
    	
      case OP_LWL:	  
	  DEBUG('m', "Executing: LWL r%d,%d(r%d)\n", instr.rt, instr.extra,
		instr.rs);
	tmp = registers[instr.rs] + instr.extra;
	if (!space->ReadMem(tmp, 4, &value))
	    return (TRUE);
	if (registers[LoadReg] == instr.rt)
	    nextLoadValue = registers[LoadValueReg];
	else
	    nextLoadValue = registers[instr.rt];
#ifdef BIG_ENDIAN
	switch (tmp & 0x3) {
	  case 0:
	    nextLoadValue = value;
	    break;
	  case 1:
	    nextLoadValue = (nextLoadValue & 0xff) | (value << 8);
	    break;
	  case 2:
	    nextLoadValue = (nextLoadValue & 0xffff) | (value << 16);
	    break;
	  case 3:
	    nextLoadValue = (nextLoadValue & 0xffffff) | (value << 24);
	    break;
	}
#else
	abort();
	switch (tmp & 0x3) {
	  case 0:
	    nextLoadValue = value;
	    break;
	  case 1:
	    nextLoadValue = (nextLoadValue & 0xff) | (value << 8);
	    break;
	  case 2:
	    nextLoadValue = (nextLoadValue & 0xffff) | (value << 16);
	    break;
	  case 3:
	    nextLoadValue = (nextLoadValue & 0xffffff) | (value << 24);
	    break;
	}
#endif
	nextLoadReg = instr.rt;
	break;
      	
      case OP_LWR:	  
	DEBUG('m', "Executing: LWR r%d,%d(r%d)\n", instr.rt, instr.extra,
	      instr.rs);
	tmp = registers[instr.rs] + instr.extra;
	if (!space->ReadMem(tmp, 4, &value))
	    return (TRUE);
	if (registers[LoadReg] == instr.rt)
	    nextLoadValue = registers[LoadValueReg];
	else
	    nextLoadValue = registers[instr.rt];
#ifdef BIG_ENDIAN
	switch (tmp & 0x3) {
	  case 0:
	    nextLoadValue = (nextLoadValue & 0xffffff00) |
		((value >> 24) & 0xff);
	    break;
	  case 1:
	    nextLoadValue = (nextLoadValue & 0xffff0000) |
		((value >> 16) & 0xffff);
	    break;
	  case 2:
	    nextLoadValue = (nextLoadValue & 0xff000000)
		| ((value >> 8) & 0xffffff);
	    break;
	  case 3:
	    nextLoadValue = value;
	    break;
	}
#else
	abort();
	switch (tmp & 0x3) {
	  case 0:
	    nextLoadValue = (nextLoadValue & 0xffffff00) |
		((value >> 24) & 0xff);
	    break;
	  case 1:
	    nextLoadValue = (nextLoadValue & 0xffff0000) |
		((value >> 16) & 0xffff);
	    break;
	  case 2:
	    nextLoadValue = (nextLoadValue & 0xff000000)
		| ((value >> 8) & 0xffffff);
	    break;
	  case 3:
	    nextLoadValue = value;
	    break;
	}
#endif
	nextLoadReg = instr.rt;
	break;
    	
      case OP_MFHI:
	DEBUG('m', "Executing: MFHI r%d\n", instr.rd);
	registers[instr.rd] = registers[HiReg];
	break;
	
      case OP_MFLO:
	DEBUG('m', "Executing: MFLO r%d\n", instr.rd);
	registers[instr.rd] = registers[LoReg];
	break;
	
      case OP_MTHI:
	DEBUG('m', "Executing: MTHI r%d\n", instr.rs);
	registers[HiReg] = registers[instr.rs];
	break;
	
      case OP_MTLO:
	DEBUG('m', "Executing: MTLO r%d\n", instr.rs);
	registers[LoReg] = registers[instr.rs];
	break;
	
      case OP_MULT:
	DEBUG('m', "Executing: MULT r%d,r%d\n", instr.rs, instr.rt);
	Mult(registers[instr.rs], registers[instr.rt], TRUE,
	     &registers[HiReg], &registers[LoReg]);
	break;
	
      case OP_MULTU:
	DEBUG('m', "Executing: MULTU r%d,r%d\n", instr.rs, instr.rt);
	Mult(registers[instr.rs], registers[instr.rt], FALSE,
	     &registers[HiReg], &registers[LoReg]);
	break;
	
      case OP_NOR:
	DEBUG('m', "Executing: NOR r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	registers[instr.rd] = ~(registers[instr.rs] | registers[instr.rt]);
	break;
	
      case OP_OR:
	DEBUG('m', "Executing: OR r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	registers[instr.rd] = registers[instr.rs] | registers[instr.rs];
	break;
	
      case OP_ORI:
	DEBUG('m', "Executing: ORI r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	registers[instr.rt] = registers[instr.rs] | (instr.extra & 0xffff);
	break;
	
      case OP_SB:
	DEBUG('m', "Executing: SB r%d,%d(r%d)\n", instr.rt, instr.extra,
	      instr.rs);
	if (!space->WriteMem((unsigned) (registers[instr.rs] + instr.extra), 1,
		      registers[instr.rt]))
	    return (TRUE);
	break;
	
      case OP_SH:
	DEBUG('m', "Executing: SH r%d,%d(r%d)\n", instr.rt, instr.extra,
	      instr.rs);
	if (!space->WriteMem((unsigned) (registers[instr.rs] + instr.extra), 2,
		      registers[instr.rt]))
	    return (TRUE);
	break;
	
      case OP_SLL:
	DEBUG('m', "Executing: SLL r%d,r%d,%d\n", instr.rd, instr.rt,
	      instr.extra);
	registers[instr.rd] = registers[instr.rt] << instr.extra;
	break;
	
      case OP_SLLV:
	DEBUG('m', "Executing: SLLV r%d,r%d,r%d\n", instr.rd, instr.rt, instr.rs);
	registers[instr.rd] = registers[instr.rt] <<
	    (registers[instr.rs] & 0x1f);
	break;
	
      case OP_SLT:
	DEBUG('m', "Executing: SLT r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	if (registers[instr.rs] < registers[instr.rt])
	    registers[instr.rd] = 1;
	else
	    registers[instr.rd] = 0;
	break;
	
      case OP_SLTI:
	DEBUG('m', "Executing: SLTI r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	if (registers[instr.rs] < instr.extra)
	    registers[instr.rt] = 1;
	else
	    registers[instr.rt] = 0;
	break;
	
      case OP_SLTIU:	  
	DEBUG('m', "Executing: SLTIU r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	rs = registers[instr.rs];
	imm = instr.extra;
	if (rs < imm)
	    registers[instr.rt] = 1;
	else
	    registers[instr.rt] = 0;
	break;
      	
      case OP_SLTU:	  
	DEBUG('m', "Executing: SLTU r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	rs = registers[instr.rs];
	rt = registers[instr.rt];
	if (rs < rt)
	    registers[instr.rd] = 1;
	else
	    registers[instr.rd] = 0;
	break;
      	
      case OP_SRA:
	DEBUG('m', "Executing: SRA r%d,r%d,%d\n", instr.rd, instr.rt,
	      instr.extra);
	registers[instr.rd] = registers[instr.rt] >> instr.extra;
	break;
	
      case OP_SRAV:
	DEBUG('m', "Executing: SRAV r%d,r%d,r%d\n", instr.rd, instr.rt, instr.rs);
	registers[instr.rd] = registers[instr.rt] >>
	    (registers[instr.rs] & 0x1f);
	break;
	
      case OP_SRL:
	DEBUG('m', "Executing: SRL r%d,r%d,%d\n", instr.rd, instr.rt,
	      instr.extra);
	tmp = registers[instr.rt];
	tmp >>= instr.extra;
	registers[instr.rd] = tmp;
	break;
	
      case OP_SRLV:
	DEBUG('m', "Executing: SRLV r%d,r%d,r%d\n", instr.rd, instr.rt, instr.rs);
	tmp = registers[instr.rt];
	tmp >>= (registers[instr.rs] & 0x1f);
	registers[instr.rd] = tmp;
	break;
	
      case OP_SUB:	  
	DEBUG('m', "Executing: SUB r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	diff = registers[instr.rs] - registers[instr.rt];
	if (((registers[instr.rs] ^ registers[instr.rt]) & SIGN_BIT) &&
	    ((registers[instr.rs] ^ diff) & SIGN_BIT)) {
	    RaiseException(OverflowException);
	    return (TRUE);
	}
	registers[instr.rd] = diff;
	break;
      	
      case OP_SUBU:
	DEBUG('m', "Executing: SUBU r%d,r%d,r%d\n", instr.rd, instr.rs, instr.rt);
	registers[instr.rd] = registers[instr.rs] - registers[instr.rt];
	break;
	
      case OP_SW:
	DEBUG('m', "Executing: SW r%d,r%d,%d\n", instr.rt, instr.rs, instr.extra);
	if (!space->WriteMem((unsigned) (registers[instr.rs] + instr.extra), 4, 
		      registers[instr.rt]))
	    return (TRUE);
	break;
	
      case OP_SWL:	  
	DEBUG('m', "Executing: SWL r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	tmp = registers[instr.rs] + instr.extra;
	if (!space->ReadMem((tmp & ~0x3), 4, &value))
	    return (TRUE);
#ifdef BIG_ENDIAN
	switch (tmp & 0x3) {
	  case 0:
	    value = registers[instr.rt];
	    break;
	  case 1:
	    value = (value & 0xff000000) | ((registers[instr.rt] >> 8) &
					    0xffffff);
	    break;
	  case 2:
	    value = (value & 0xffff0000) | ((registers[instr.rt] >> 16) &
					    0xffff);
	    break;
	  case 3:
	    value = (value & 0xffffff00) | ((registers[instr.rt] >> 24) &
					    0xff);
	    break;
	}
#else
	abort();
#endif
	if (!space->WriteMem((tmp & ~0x3), 4, value))
	    return (TRUE);
	break;
    	
      case OP_SWR:	  
	DEBUG('m', "Executing: SWR r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	tmp = registers[instr.rs] + instr.extra;
	if (!space->ReadMem((tmp & ~0x3), 4, &value))
	    return (TRUE);
#ifdef BIG_ENDIAN
	switch (tmp & 0x3) {
	  case 0:
	    value = (value & 0xffffff) | (registers[instr.rt] << 24);
	    break;
	  case 1:
	    value = (value & 0xffff) | (registers[instr.rt] << 16);
	    break;
	  case 2:
	    value = (value & 0xff) | (registers[instr.rt] << 8);
	    break;
	  case 3:
	    value = registers[instr.rt];
	    break;
	}
#else
	abort();
#endif
	if (!space->WriteMem((tmp & ~0x3), 4, value))
	    return (TRUE);
	break;
    	
      case OP_SYSCALL:
	DEBUG('m', "Executing: SYSCALL\n");
	RaiseException(SyscallException);
	// return (TRUE); I don't think we should return now.
	break;
	
      case OP_XOR:
	DEBUG('m', "Executing: XOR r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	registers[instr.rd] = registers[instr.rs] ^ registers[instr.rt];
	break;
	
      case OP_XORI:
	DEBUG('m', "Executing: XORI r%d,r%d,%d\n", instr.rt, instr.rs,
	      instr.extra);
	registers[instr.rt] = registers[instr.rs] ^ (instr.extra & 0xffff);
	break;
	
      case OP_RES:
      case OP_UNIMP:
	DEBUG('m', "Executing: Reserved instruction\n");
	RaiseException(IllegalInstrException);
	return (TRUE);
	
      default:
	abort();
    }
    
    // Now we have successfully executed the instruction.
    
    // Simulate effects of delayed load.
    // NOTE -- RaiseException is responsible for doing the delayed load also.
    
    registers[registers[LoadReg]] = registers[LoadValueReg];
    registers[LoadReg] = nextLoadReg;
    registers[LoadValueReg] = nextLoadValue;
    
    // Make sure R0 stays zero.
    registers[0] = 0;
    
    // Advance program counters.
    registers[PrevPCReg] = registers[PCReg];
    registers[PCReg] = registers[NextPCReg];
    registers[NextPCReg] = pcAfter;
    
    return (TRUE);
}

DecodedInstruction
Machine::Decode(int value)
{
    DEBUG('m', "DECODING: %8.8x = ", value);
    
    DecodedInstruction decoded;
    OpInfo *opPtr;
    
    decoded.rs = (value >> 21) & 0x1f;
    decoded.rt = (value >> 16) & 0x1f;
    decoded.rd = (value >> 11) & 0x1f;
    opPtr = &opTable[(value >> 26) & 0x3f];
    decoded.opCode = opPtr->opCode;
    if (opPtr->format == IFMT) {
	decoded.extra = value & 0xffff;
	if (decoded.extra & 0x8000) {
	    decoded.extra |= 0xffff0000;
	}
    } else if (opPtr->format == RFMT) {
	decoded.extra = (value >> 6) & 0x1f;
    } else {
	decoded.extra = value & 0x3ffffff;
    }
    if (decoded.opCode == SPECIAL) {
	decoded.opCode = specialTable[value & 0x3f];
    } else if (decoded.opCode == BCOND) {
	int i;
	i = value & 0x1f0000;
	if (i == 0) {
	    decoded.opCode = OP_BLTZ;
	} else if (i == 0x10000) {
	    decoded.opCode = OP_BGEZ;
	} else if (i == 0x100000) {
	    decoded.opCode = OP_BLTZAL;
	} else if (i == 0x110000) {
	    decoded.opCode = OP_BGEZAL;
	} else {
	    decoded.opCode = OP_UNIMP;
	}
    }
    
    DEBUG('m', "%s\n", decoded.string());
    
    return (decoded);
}

char*
DecodedInstruction::string()
{
    static char buffer[100];
    
    sprintf(buffer, "Op %d, regs: s=%d, t=%d, d=%d, ex=%d",
	    opCode, rs, rt, rd, extra);
    
    return (buffer);
}

// Simulate R2000 multiplication.
// The words at *hiPtr and *loPtr are overwritten with the
// double-length result of the multiplication.

void
Machine::Mult(int a, int b, bool signedArith, int* hiPtr, int* loPtr)
{
    if ((a == 0) || (b == 0)) {
	*hiPtr = *loPtr = 0;
	return;
    }

    // Compute the sign of the result, then make everything positive
    // so unsigned computation can be done in the main loop.
    bool negative = FALSE;
    if (signedArith) {
	if (a < 0) {
	    negative = !negative;
	    a = -a;
	}
	if (b < 0) {
	    negative = !negative;
	    b = -b;
	}
    }

    // Compute the result in unsigned arithmetic (check a's bits one at
    // a time, and add in a shifted value of b).
    unsigned int bLo = b;
    unsigned int bHi = 0;
    unsigned int lo = 0;
    unsigned int hi = 0;
    for (int i = 0; i < 32; i++) {
	if (a & 1) {
	    lo += bLo;
	    if (lo < bLo)  // Carry out of the low bits?
		hi += 1;
	    hi += bHi;
	    if ((a & 0xfffffffe) == 0)
		break;
	}
	bHi <<= 1;
	if (bLo & 0x80000000)
	    bHi |= 1;
	
	bLo <<= 1;
	a >>= 1;
    }

    // If the result is supposed to be negative, compute the two's
    // complement of the double-word result.
    if (negative) {
	hi = ~hi;
	lo = ~lo;
	lo++;
	if (lo == 0)
	    hi++;
    }
    
    *hiPtr = (int) hi;
    *loPtr = (int) lo;
}
