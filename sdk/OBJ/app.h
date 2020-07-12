#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Common header files
 */
#include "target_test.h"
#include "ev3api.h"


#define __max(a,b) (((a) > (b)) ? (a) : (b))
#define __min(a,b) (((a) < (b)) ? (a) : (b))

/**
 * Task priorities (smaller number has higher priority)
 */
#define PRIORITY_MAIN_TASK 	(TMIN_APP_TPRI + 1) 
/* Main task priority */
/* Must by higher then HIGH_PRIORITY */

#define HIGH_PRIORITY		(TMIN_APP_TPRI + 2)		
#define MID_PRIORITY		(TMIN_APP_TPRI + 3)	
#define LOW_PRIORITY		(TMIN_APP_TPRI + 4)	

#define PRIORITY_PRD_TSK_1 TMIN_APP_TPRI
#define PRIORITY_PRD_TSK_2 TMIN_APP_TPRI

/**
 * Default task stack size in bytes
 */
#ifndef STACK_SIZE
#define	STACK_SIZE 4096
#endif

/**
 * Prototypes for configuration
 */
#ifndef TOPPERS_MACRO_ONLY

extern void	main_task(intptr_t);

#endif /* TOPPERS_MACRO_ONLY */

#ifdef __cplusplus
}
#endif

#endif
