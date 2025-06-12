#pragma once
//
// Saints Row 2 -> DebugPrint function (lua)
//

#include <windows.h>
#include <vector>

#pragma comment (lib, "lua/lua50.lib")
#pragma comment (lib, "legacy_stdio_definitions.lib")
#pragma comment (lib, "lua/lua50.lib")

extern "C"
{
#include "lua/lualib.h"
}

typedef DWORD(__cdecl* fnLuaRegister)(DWORD*, DWORD*, DWORD*, DWORD*);
typedef DWORD(__cdecl* fnLuaPushCFunction)(DWORD, DWORD);


typedef union {
	void* gc;
	void* p;
	double n;
	int b;
} Value;

typedef struct lua_TObject {
	int tt;
	Value value;
} TObject;


typedef unsigned char lu_byte;
typedef TObject* StkId;  /* index to stack elements */

#define CommonHeader    void *next; lu_byte tt; lu_byte marked

struct lua_State {
	CommonHeader;
	DWORD* top;  /* first free slot in the stack */
	DWORD* base;  /* base of current function */
	void* l_G;
	void* ci;  /* call info for current function */
	StkId stack_last;  /* last free slot in the stack */
	StkId stack;  /* stack base */
	int stacksize;
	void* end_ci;  /* points after end of ci array*/
	void* base_ci;  /* array of CallInfo's */
	unsigned short size_ci;  /* size of array `base_ci' */
	unsigned short nCcalls;  /* number of nested C calls */
	lu_byte hookmask;
	lu_byte allowhook;
	lu_byte hookinit;
	int basehookcount;
	int hookcount;
	void* hook;
	DWORD _gt;  /* table of globals */
	void* openupval;  /* list of open upvalues in this stack */
	void* gclist;
	void* errorJmp;  /* current error recover point */
	void* errfunc;  /* current error handling function (stack index) */
};
