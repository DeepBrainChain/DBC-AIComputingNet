#ifndef DBC_TIMER_DEF_H
#define DBC_TIMER_DEF_H

/*--------------------timer name------------------------*/
#define TIMER_NAME_FILTER_CLEAN                                             "common_timer_filter_clean"
#define TIMER_NAME_CHANNEL_RECYCLE                                   "channel_recycle_timer"

#define TIMER_NAME_CHECK_PEER_CANDIDATES                     "p2p_timer_check_peer_candidates"
#define TIMER_NAME_DYANMIC_ADJUST_NETWORK                "p2p_timer_dynamic_adjust_network"
#define TIMER_NAME_PEER_INFO_EXCHANGE                             "p2p_timer_peer_info_exchange"
#define TIMER_NAME_DUMP_PEER_CANDIDATES                       "p2p_timer_dump_peer_candidates"


/*--------------------timer interval----------------------*/
//#define TIMER_INTERV_SEC_FILTER_CLEAN               120
#define TIMER_INTERV_MIN_P2P_PEER_INFO_EXCHANGE         3
#define TIMER_INTERV_MIN_P2P_CONNECT_NEW_PEER           1
#define TIMER_INTERV_MIN_P2P_ONE_MINUTE                 1
#define TIMER_INTERV_MIN_P2P_PEER_CANDIDATE_DUMP        10
#define TIMER_INTERVAL_CHANNEL_RECYCLE                  (3 * 1000)

#endif //DBC_TIMER_DEF_H
