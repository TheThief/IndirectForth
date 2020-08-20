
// Forth virtual machine that uses an indirect IP register.
// i.e. the instruction that is actually executed *isn't* the one IP points at, but dereferenced from the address that IP points at
// this means that native forth words execute without a call/return :)
// longer words (i.e. forth words) have the "call" as their first instruction (it's actually more of a "step in" instruction)
// the resulting code looks a lot like "Direct Threaded" Forth, in that the definition of a word starts with a DOCOL instruction and follows with the addresses of other words
// but without the need for a "next" epilogue on every native word
// downside: can't "compile" forth words into multi-instruction native words... as there are no multi-instruction native words

#include <cinttypes>
#include <memory>
#include <utility>
#include <iostream>

typedef void(*voidfunc)();

//////////////////////////////////////////////////////////////////////////
// | Link  |Len| Name  | code  | ...
// |   2   |   |   X   |   2   | ...
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// | Link  |Len| Name  | DOVAR | Value |
// |   2   |   |   X   |   2   |   2   |
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Len is 3 bits flags and 5 bits length (0-31)
// Flag 0x80 is IMMEDIATE word
// Flag 0x40 is HIDDEN word
// Flag 0x20 is 0
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// 0x0000 Return Stack
// 0x1000 Parameter Stack
// 0x2000 User Memory (words)
// 0xFF00 PAD (128 bytes)
// 0xFF80 TIB (128 bytes)
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// : CMOVE         ( source dest len -- )
//   >R SWAP       ( dest source     R:len)
//   BEGIN         ( dest source     R:len)
//    R> DUP 1- >R ( dest source len R:len-1)
//   WHILE         ( dest source     R:len-1)
//    DUP C@       ( dest source C   R:len-1)
//    >R 1+ SWAP   ( source+1 dest   R:len-1 C)
//    DUP R> C! 1+ ( source+1 dest+1 R:len-1)
//   REPEAT        ( source+1 dest+1 R:len-1)
//   R> DROP 2DROP ( -- )
//   ;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// : CREATE          ( -- )
//   PARSE-WORD      ( addr len)
//   LATEST          ( addr len LATEST)
//   HERE @ LATEST !                  ( write current location to LATEST)
//   , DUP C,        ( addr len)      ( write old-LATEST and len to dictionary)
//   HERE @ SWAP     ( addr here len)
//   CMOVE           ( )              ( write name to dictionary)
//   DOVAR ,         ( )              ( write DOVAR as the code)
//   ;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// : PARSE          ( char -- addr len)
//   >IN @ SWAP     ( old>IN char) ( preserve the initial >IN)
//   BEGIN                ( loop through word until delimiter or end of TIB is encountered)
//   >IN @ #TIB @ <         ( check >IN is still inside the TIB)
//   WHILE
//   TIB >IN @ + c@ OVER <> ( fetch current character in the TIB and compare against supplied character)
//   ANDWHILE
//   1 >IN +! REPEAT        ( increment >IN)
//   DROP           ( old>IN)
//   DUP TIB +      ( old>IN addr)
//   SWAP >IN @ SWAP - ( addr len)
//   >IN @ #TIB @ < IF    ( if >IN is inside the TIB, we need to advance past the delimiter)
//   1 >IN +! THEN
//   ;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// : PARSE-WORD     (char -- addr len)
//   BEGIN                ( skip leading chars)
//   >IN @ #TIB @ <         ( check >IN is still inside the TIB)
//   WHILE
//   TIB >IN @ + c@ OVER =  ( fetch current character in the TIB and compare against supplied character)
//   ANDWHILE
//   1 >IN +! REPEAT        ( increment >IN)
//   PARSE                ( parse the word)
//   ;
//////////////////////////////////////////////////////////////////////////

uint8_t Init[] =
{
  0,  0, 5, 'D', 'O', 'C', 'O', 'N', 1, 0, 1, 0, // constant that outputs the DOCON instruction
  0, 32, 5, 'D', 'O', 'V', 'A', 'R', 1, 0, 2, 0, // constant that outputs the DOVAR instruction
 12, 32, 4, 'E', 'X', 'I', 'T', 3, 0,
 24, 32, 4, 'D', 'R', 'O', 'P', 4, 0,
 33, 32, 4, 'S', 'W', 'A', 'P', 5, 0,
 42, 32, 3, 'D', 'U', 'P', 6, 0,
 51, 32, 3, 'R', 'O', 'T', 7, 0,
 59, 32, 4, 'O', 'V', 'E', 'R', 8, 0,
 67, 32, 1, '+', 9, 0,
 76, 32, 1, '-', 10, 0,
 82, 32, 1, '*', 11, 0,
 88, 32, 4, '/', 'M', 'O', 'D', 12, 0,
 94, 32, 1, '=', 13, 0,
103, 32, 1, '<', 14, 0,
109, 32, 3, 'A', 'N', 'D', 15, 0,
115, 32, 2, 'O', 'R', 16, 0,
123, 32, 3, 'X', 'O', 'R', 17, 0,
130, 32, 6, 'I', 'N', 'V', 'E', 'R', 'T', 18, 0,
138, 32, 4, 'L', 'I', 'T', 19, 0,
149, 32, 1, '!', 20, 0,
157, 32, 1, '@', 21, 0,
163, 32, 2, 'C', '!', 22, 0,
169, 32, 2, 'C', '@', 23, 0,
176, 32, 5, 'S', 'T', 'A', 'T', 'E', 2, 0, 0, 0, // Variable
183, 32, 6, 'L', 'A', 'T', 'E', 'S', 'T', 2, 0, 8, 34, // Variable
195, 32, 4, 'H', 'E', 'R', 'E', 2, 0, 48, 34, // Variable
208, 32, 4, 'B', 'A', 'S', 'E', 2, 0, 10, 0, // Variable
219, 32, 3, 'K', 'E', 'Y', 24, 0,
230, 32, 4, 'E', 'M', 'I', 'T', 25, 0,
238, 32, 6, 'B', 'R', 'A', 'N', 'C', 'H', 26, 0,
247, 32, 7, '0', 'B', 'R', 'A', 'N', 'C', 'H', 27, 0,
  2, 33, 5, 'D', 'O', 'C', 'O', 'L', 1, 0, 0, 0, // Constant that outputs the DOCOL instruction
 14, 33, 2, '>', 'R', 28, 0,
 26, 33, 2, 'R', '>', 29, 0,
 33, 33, 2, '+', '!', 30, 0,
 40, 32, 4, 'D', 'S', 'P', '@', 31, 0,
 47, 32, 4, 'D', 'S', 'P', '!', 32, 0,
 56, 32, 4, 'R', 'S', 'P', '@', 33, 0,
 65, 32, 4, 'R', 'S', 'P', '!', 34, 0,
 74, 33, 1, ',', 0, 0, 215, 32, 167, 32, 161, 32, 155, 32, 2, 0, 215, 32, 45, 33, 31, 32, // : , HERE @ ! 2 HERE +! ; // fetch here, store at that location, add 2 to here
 83, 33, 2, 'C', ',', 0, 0, 215, 32, 167, 32, 174, 32, 155, 32, 1, 0, 215, 32, 45, 33, 31, 32, // same as , but calls C! and only adds 1
105, 33, 5, 'C', 'M', 'O', 'V', 'E', 0, 0, 31, 33, 49, 32, /* BEGIN */ 38, 33, 57, 32, 155, 32, 1, 0, 86, 32, 31, 33, /* WHILE */ 12, 33, 32, 0, 57, 32, 181, 32, 31, 33, 155, 32, 1, 0, 80, 32, 49, 32, 57, 32, 38, 33, 174, 32, 155, 32, 1, 0, 80, 32, /* REPEAT */ 0, 33, (uint8_t)-44, (uint8_t)-1, 38, 33, 40, 32, 40, 32, 40, 32, 31, 32, // see above
128, 33, 4, '>', 'C', 'F', 'A', 0, 0, 155, 32, 2, 0, 80, 32, 57, 32, 167, 32, 80, 32, 155, 32, 1, 0, 80, 32, 31, 32, // : >CFA 2 + DUP @ + 1 + ;
198, 33, 0x80|1, '[', 0, 0, 155, 32, 0, 0, 191, 32, 161, 32, 31, 32, // : [ IMMEDIATE 0 STATE ! ; // exit compilation mode
227, 33, 1, ']', 0, 0, 155, 32, 1, 0, 191, 32, 161, 32, 31, 32, // : ] 1 STATE ! ] ; // enter compilation mode
243, 33, 0x80|9, 'I', 'M', 'M', 'E', 'D', 'I', 'A', 'T', 'E', 0, 0, 204, 32, 167, 32, 155, 32, 2, 0, 80, 32, 57, 32, 167, 32, 155, 32, 0x80, 0, 136, 32, 49, 32, 161, 32, 31, 32, // : IMMEDIATE LATEST @ 2 + DUP @ 0x80 XOR SWAP ! ] ; IMMEDIATE // IMMEDIATE is an IMMEDIATE word - got to love that recursive definition.
  3, 34, 3, 'T', 'I', 'B', 1, 0, 0x80, 0xff, // Constant that outputs the address of the TIB
 43, 34, 4, '#', 'T', 'I', 'B', 2, 0, 0, 0, // Variable containing the number of characters in the TIB
 53, 34, 3, 'B', 'L', 'K', 2, 0, 0, 0, // Variable containing the current input block. Block 0 is TIB, others currently unsupported
 64, 34, 3, '>', 'I', 'N', 2, 0, 0, 0, // Variable containing the offset to the current character being parsed in the BLK/TIB
 71, 34, 2, 'B', 'L', 1, 0, ' ', 0, // Constant for the "space" character
 84, 34, 3, 'P', 'A', 'D', 1, 0, 0x80, 0x00, // Constant that outputs the address of the PAD
 93, 34, 5, 'P', 'A', 'R', 'S', 'E', 0, 0, 77, 34, 167, 32, 49, 32, /* BEGIN */ 77, 34, 167, 32, 60, 34, 167, 32, 113, 32, /* WHILE */ /*, ,*/ 31, 32, // see above
XXX, 34, 10, 'P', 'A', 'R', 'S', 'E', '-', 'W', 'O', 'R', 'D', 0, 0, /*, ,*/ 31, 32, // see above
XXX, 34, 6, 'C', 'R', 'E', 'A', 'T', 'E', 0, 0, 204, 32, 215, 32, 167, 32, 204, 32, 161, 32, 87, 33, 57, 32, 110, 33, 215, 32, 167, 32, 49, 32, 136, 33, 20, 32, 87, 33, 31, 32, // see above

0xff, 0xff, 1, ':', 0, 0, 0xff, 0xff, 207, 33, 22, 33, 87, 33, 32, 34, 31, 32, // : : CREATE DOES> ] ; // defining : using : , got to love it!
};

std::unique_ptr<uint8_t[]> Memory;

template<typename T>
T& Mem(uint16_t addr)
{
	return *(T*)(&Memory[addr]);
}

uint16_t IP = 0x0000; // QUIT
uint16_t RSP = 0x0000;
uint16_t PSP = 0x1000;

template<typename T>
T& PStack(int16_t offset)
{
	return *(T*)(&Memory[PSP + offset]);
}

template<typename T>
T& RStack(int16_t offset)
{
	return *(T*)(&Memory[RSP + offset]);
}

template<typename T>
void PStack_Push(T Value)
{
	PStack<T>(PSP) = Value;
	PSP += sizeof(T);
}

template<typename T>
void RStack_Push(T Value)
{
	RStack<T>(RSP) = Value;
	RSP += sizeof(T);
}

template<typename T>
T PStack_Pop()
{
	PSP -= sizeof(T);
	T Value = PStack<T>(PSP);
	return Value;
}

template<typename T>
T RStack_Pop()
{
	RSP -= sizeof(T);
	T Value = RStack<T>(RSP);
	return Value;
}

void DOCOL()
{
	RStack_Push(IP); // Store return IP on return stack
	IP = Mem<uint16_t>(IP - 2) + 2; // Move IP to the threaded instructions to execute
}

void DOCON()
{
	// Constant follows the compiled call to DOCON
	PStack_Push(Mem<int16_t>(Mem<uint16_t>(IP - 2) + 2)); // retrieve constant and store it to the stack
}

void DOVAR()
{
	// Variable follows the compiled call to DOVAR
	PStack_Push(Mem<uint16_t>(IP - 2) + 2); // retrieve variable's address and store it to the stack
}

void EXIT()
{
	IP = RStack_Pop<uint16_t>(); // Retrieve return IP from return stack
}

void DROP()
{
	PSP -= 2;
}

void SWAP()
{
	std::swap(PStack<uint16_t>(-2), PStack<uint16_t>(-4));
}

void DUP()
{
	PStack_Push(PStack<int16_t>(-2));
}

void ROT()
{
	uint16_t t = Mem<uint16_t>(PSP - 6);
	Mem<uint16_t>(PSP - 6) = Mem<uint16_t>(PSP - 4);
	Mem<uint16_t>(PSP - 4) = Mem<uint16_t>(PSP - 2);
	Mem<uint16_t>(PSP - 2) = t;
}

void OVER()
{
	PStack_Push(PStack<int16_t>(-4));
}

void ADD()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) += Value;
}

void SUB()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) -= Value;
}

void MUL()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) *= Value;
}

void DIVMOD()
{
	int16_t Div = PStack<int16_t>(-4) / PStack<int16_t>(-2);
	int16_t Mod = PStack<int16_t>(-4) % PStack<int16_t>(-2);
	PStack<int16_t>(-4) = Mod;
	PStack<int16_t>(-2) = Div;
}

void EQU()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) = (int16_t)(PStack<int16_t>(-2) == Value ? -1 : 0);
}

void LT()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) = (int16_t)(PStack<int16_t>(-2) < Value ? -1 : 0);
}

void AND()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) &= Value;
}

void OR()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) |= Value;
}

void XOR()
{
	int16_t Value = PStack_Pop<int16_t>();
	PStack<int16_t>(-2) ^= Value;
}

void INVERT()
{
	PStack<int16_t>(-2) = ~PStack<int16_t>(-2);
}

void LIT()
{
	PStack_Push(Mem<int16_t>(IP));
	IP += 2;
}

void STORE()
{
	uint16_t addr = PStack_Pop<uint16_t>();
	Mem<int16_t>(addr) = PStack_Pop<int16_t>();
}

void FETCH()
{
	uint16_t addr = PStack_Pop<uint16_t>();
	PStack_Push(Mem<int16_t>(addr));
}

void STOREBYTE()
{
	uint16_t addr = PStack_Pop<uint16_t>();
	Mem<int8_t>(addr) = (int8_t)PStack_Pop<int16_t>();
}

void FETCHBYTE()
{
	uint16_t addr = PStack_Pop<uint16_t>();
	PStack_Push((int16_t)Mem<int8_t>(addr));
}

void KEY()
{
	PStack_Push((int16_t)std::wcin.get());
}

void EMIT()
{
	std::wcout.put(PStack_Pop<int16_t>());
}

void BRANCH()
{
	IP += Mem<int16_t>(IP); // reads literal following the branch instruction and adds that to the IP
}

void ZBRANCH()
{
	if (PStack_Pop<int16_t>() == 0)
	{
		IP += Mem<int16_t>(IP);
	}
	else
	{
		IP += 2;
	}
}

void TOR()
{
	RStack_Push(PStack_Pop<int16_t>()); // Store parameter on return stack
}

void FROMR()
{
	PStack_Push(RStack_Pop<int16_t>()); // Retrieve parameter from return stack
}

void ADD_STORE()
{
	uint16_t addr = PStack_Pop<uint16_t>();
	Mem<int16_t>(addr) += PStack_Pop<int16_t>();
}

void DSP_FETCH()
{
	PStack_Push(PSP);
}

void DSP_STORE()
{
	PSP = PStack_Pop<uint16_t>();
}

void RSP_FETCH()
{
	PStack_Push(RSP);
}

void RSP_STORE()
{
	RSP = PStack_Pop<uint16_t>();
}

voidfunc NativeFuncs[] =
{
	&DOCOL,
	&DOCON,
	&DOVAR,
	&EXIT,
	&DROP,
	&SWAP,
	&DUP,
	&ROT,
	&OVER,
	&ADD,
	&SUB,
	&MUL,
	&DIVMOD,
	&EQU,
	&LT,
	&AND,
	&OR,
	&XOR,
	&INVERT,
	&LIT,
	&STORE,
	&FETCH,
	&STOREBYTE,
	&FETCHBYTE,
	&KEY,
	&EMIT,
	&BRANCH,
	&ZBRANCH,
	&TOR,
	&FROMR,
	&ADD_STORE,
	&DSP_FETCH,
	&DSP_STORE,
	&RSP_FETCH,
	&RSP_STORE,
};

int __cdecl main(int Argc, char*Argv[])
{
	Memory = std::make_unique<uint8_t[]>(65536);
	memcpy(&Memory[0x2000], Init, sizeof(Init));

	while (true)
	{
		// indirect threaded requires double-lookup
		uint16_t id = Mem<uint16_t>(Mem<uint16_t>(IP));
		IP += 2;
		(*NativeFuncs[id])();
	}
}
