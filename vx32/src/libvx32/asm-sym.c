#define class struct
#define private public
#include "vx32.h"
#include "vx32impl.h"
void bogus() {
__asm ("\n#define ASDF mAgIc%0" : : "i" (12345));
#ifndef i386
__asm ("\n#define FLATCODE mAgIc%0" : : "i" (FLATCODE));
__asm ("\n#define FLATDATA mAgIc%0" : : "i" (FLATDATA));
#endif
__asm ("\n#define VXEMU_DATASEL mAgIc%0" : : "i" (&((struct vxemu*)0)->datasel));
__asm ("\n#define VXEMU_EMUSEL mAgIc%0" : : "i" (&((struct vxemu*)0)->emusel));
__asm ("\n#define VXEMU_EMUPTR mAgIc%0" : : "i" (&((struct vxemu*)0)->emuptr));
__asm ("\n#define VXEMU_REG mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg));
__asm ("\n#define VXEMU_EAX mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[EAX]));
__asm ("\n#define VXEMU_ECX mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[ECX]));
__asm ("\n#define VXEMU_EDX mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[EDX]));
__asm ("\n#define VXEMU_EBX mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[EBX]));
__asm ("\n#define VXEMU_ESP mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[ESP]));
__asm ("\n#define VXEMU_EBP mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[EBP]));
__asm ("\n#define VXEMU_ESI mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[ESI]));
__asm ("\n#define VXEMU_EDI mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.reg[EDI]));
__asm ("\n#define VXEMU_EIP mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.eip));
__asm ("\n#define VXEMU_EFLAGS mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu.eflags));
__asm ("\n#define VXEMU_TRAPNO mAgIc%0" : : "i" (&((struct vxemu*)0)->cpu_trap));
__asm ("\n#define VXEMU_JMPINFO mAgIc%0" : : "i" (&((struct vxemu*)0)->jmpinfo));
__asm ("\n#define VXEMU_HOST_SS mAgIc%0" : : "i" (&((struct vxemu*)0)->host_ss));
__asm ("\n#define VXEMU_HOST_DS mAgIc%0" : : "i" (&((struct vxemu*)0)->host_ds));
__asm ("\n#define VXEMU_HOST_ES mAgIc%0" : : "i" (&((struct vxemu*)0)->host_es));
__asm ("\n#define VXEMU_HOST_VS mAgIc%0" : : "i" (&((struct vxemu*)0)->host_vs));
#ifdef i386
__asm ("\n#define VXEMU_HOST_ESP mAgIc%0" : : "i" (&((struct vxemu*)0)->host_esp));
#else
__asm ("\n#define VXEMU_HOST_RSP mAgIc%0" : : "i" (&((struct vxemu*)0)->host_rsp));
__asm ("\n#define VXEMU_RUNPTR mAgIc%0" : : "i" (&((struct vxemu*)0)->runptr));
__asm ("\n#define VXEMU_RETPTR mAgIc%0" : : "i" (&((struct vxemu*)0)->retptr));
#endif
__asm ("\n#define VXEMU_ETABLEN mAgIc%0" : : "i" (&((struct vxemu*)0)->etablen));
__asm ("\n#define VXEMU_ETABMASK mAgIc%0" : : "i" (&((struct vxemu*)0)->etabmask));
__asm ("\n#define VXEMU_ETAB mAgIc%0" : : "i" (&((struct vxemu*)0)->etab));
}
