#ifndef DBC_TIMER_DEF_H
#define DBC_TIMER_DEF_H

/*--------------------timer name------------------------*/
#define TIMER_NAME_FILTER_CLEAN              "common_timer_filter_clean"
#define TIMER_NAME_CHANNEL_RECYCLE           "channel_recycle_timer"


/*--------------------timer interval----------------------*/
//#define TIMER_INTERV_SEC_FILTER_CLEAN               120
#define TIMER_INTERV_MIN_P2P_PEER_INFO_EXCHANGE         3
#define TIMER_INTERV_MIN_P2P_CONNECT_NEW_PEER           1
#define TIMER_INTERV_MIN_P2P_ONE_MINUTE                 1
#define TIMER_INTERV_MIN_P2P_PEER_CANDIDATE_DUMP        10
#define TIMER_INTERVAL_CHANNEL_RECYCLE                  (3 * 1000)

#define DEFAULT_TIMER_INTERVAL  100 //unit:ms

#define INVALID_TIMER_ID                    0x0
#define MAX_TIMER_ID                        0xFFFFFFFF

#endif
