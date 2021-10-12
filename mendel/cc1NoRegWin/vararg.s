.global ____builtin_saveregs
____builtin_saveregs:
	st %o0,[%i4+68]
	st %o1,[%i4+72]
	st %o2,[%i4+76]
	st %o3,[%i4+80]
	st %o4,[%i4+84]
	retl
	st %o5,[%i4+88]
