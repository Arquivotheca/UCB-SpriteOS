/* 
 * Quick hack to help compile this code in a System V environment.
 * 
 * $Header: /sprite/src/benchmarks/itc/gcc/RCS/sysv.h,v 1.2 93/02/11 17:12:50 kupfer Exp $ SPRITE (Berkeley)
 */

#ifndef _SYSV_H_
#define _SYSV_H_

#ifdef USG
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#define vfork fork
#define bcopy(a,b,c)	memcpy((b),(a),(c))
#define bzero(a,b)	memset((a),0,(b))
#define bcmp(a,b,c)	memcmp((a),(b),(c))
#endif /* USG */

#endif /* _SYSV_H_ */
