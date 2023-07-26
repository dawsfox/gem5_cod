#ifndef __CODELET_IMPL_H__
#define __CODELET_IMPL_H__

/* Each of these functions should be tied to a Codelet name so 
 * that the SU knows what fire function to send with the runtime
 * Codelet. dest, src1, and src2 are set by the SU. Source params
 * may be immediate values or unused, based on the SCM program but
 * the function definition must always include them. 
 * */
void emptyCodFire(void * dest, void * src1, void * src2);

#endif