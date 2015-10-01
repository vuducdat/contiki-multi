#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

/* Evaluation */
#undef RPL_CONF_OF1
#define RPL_CONF_OF1 rpl_mrhof

#undef RPL_CONF_OF2
#define RPL_CONF_OF2 rpl_mrhof

// #undef RPL_CONF_DAG_MC
// #define RPL_CONF_DAG_MC RPL_DAG_MC_ENERGY

// default 12, ietf 3
#undef RPL_CONF_DIO_INTERVAL_MIN
#define RPL_CONF_DIO_INTERVAL_MIN 14

// default 8, ietf 20=2.3 hr
#undef RPL_CONF_DIO_INTERVAL_DOUBLINGS
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 12

/* for our own simulations */
#define CONTROL_TRAFFIC 2

/* Config routing table */
/* #undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 12 */ 

#endif /* __PROJECT_CONF_H__ */
