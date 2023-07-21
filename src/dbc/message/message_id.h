#ifndef DBC_MESSAGE_ID_H
#define DBC_MESSAGE_ID_H

#include <iostream>
#include <string>

#define VER_REQ  "ver_req"
#define VER_RESP "ver_resp"

#define SHAKE_HAND_REQ  "shake_hand_req"
#define SHAKE_HAND_RESP "shake_hand_resp"

#define P2P_GET_PEER_NODES_RESP "get_peer_nodes_resp"

#define BINARY_FORWARD_MSG "binary_forward_message"

// list images
#define NODE_LIST_IMAGES_REQ   "node_list_images_req"
#define NODE_LIST_IMAGES_RSP   "node_list_images_rsp"
#define NODE_LIST_IMAGES_TIMER "node_list_images_timer"

// download image
#define NODE_DOWNLOAD_IMAGE_REQ   "node_download_image_req"
#define NODE_DOWNLOAD_IMAGE_RSP   "node_download_image_rsp"
#define NODE_DOWNLOAD_IMAGE_TIMER "node_download_image_timer"

// download image progress
#define NODE_DOWNLOAD_IMAGE_PROGRESS_REQ   "node_download_image_progress_req"
#define NODE_DOWNLOAD_IMAGE_PROGRESS_RSP   "node_download_image_progress_rsp"
#define NODE_DOWNLOAD_IMAGE_PROGRESS_TIMER "node_download_image_progress_timer"

// stop download image
#define NODE_STOP_DOWNLOAD_IMAGE_REQ   "node_stop_download_image_req"
#define NODE_STOP_DOWNLOAD_IMAGE_RSP   "node_stop_download_image_rsp"
#define NODE_STOP_DOWNLOAD_IMAGE_TIMER "node_stop_download_image_timer"

// upload image
#define NODE_UPLOAD_IMAGE_REQ   "node_upload_image_req"
#define NODE_UPLOAD_IMAGE_RSP   "node_upload_image_rsp"
#define NODE_UPLOAD_IMAGE_TIMER "node_upload_image_timer"

// upload image progress
#define NODE_UPLOAD_IMAGE_PROGRESS_REQ   "node_upload_image_progress_req"
#define NODE_UPLOAD_IMAGE_PROGRESS_RSP   "node_upload_image_progress_rsp"
#define NODE_UPLOAD_IMAGE_PROGRESS_TIMER "node_upload_image_progress_timer"

// stop upload image
#define NODE_STOP_UPLOAD_IMAGE_REQ   "node_stop_upload_image_req"
#define NODE_STOP_UPLOAD_IMAGE_RSP   "node_stop_upload_image_rsp"
#define NODE_STOP_UPLOAD_IMAGE_TIMER "node_stop_upload_image_timer"

// delete image
#define NODE_DELETE_IMAGE_REQ   "node_delete_image_req"
#define NODE_DELETE_IMAGE_RSP   "node_delete_image_rsp"
#define NODE_DELETE_IMAGE_TIMER "node_delete_image_timer"

// create task
#define NODE_CREATE_TASK_REQ   "node_create_task_req"
#define NODE_CREATE_TASK_RSP   "node_create_task_rsp"
#define NODE_CREATE_TASK_TIMER "node_create_task_timer"

// start task
#define NODE_START_TASK_REQ   "node_start_task_req"
#define NODE_START_TASK_RSP   "node_start_task_rsp"
#define NODE_START_TASK_TIMER "node_start_task_timer"

// shutdown task
#define NODE_SHUTDOWN_TASK_REQ   "node_shutdown_task_req"
#define NODE_SHUTDOWN_TASK_RSP   "node_shutdown_task_rsp"
#define NODE_SHUTDOWN_TASK_TIMER "node_shutdown_task_timer"

// poweroff task
#define NODE_POWEROFF_TASK_REQ   "node_poweroff_task_req"
#define NODE_POWEROFF_TASK_RSP   "node_poweroff_task_rsp"
#define NODE_POWEROFF_TASK_TIMER "node_poweroff_task_timer"

// stop task
#define NODE_STOP_TASK_REQ   "node_stop_task_req"
#define NODE_STOP_TASK_RSP   "node_stop_task_rsp"
#define NODE_STOP_TASK_TIMER "node_stop_task_timer"

// restart task
#define NODE_RESTART_TASK_REQ   "node_restart_task_req"
#define NODE_RESTART_TASK_RSP   "node_restart_task_rsp"
#define NODE_RESTART_TASK_TIMER "node_restart_task_timer"

// reset task
#define NODE_RESET_TASK_REQ   "node_reset_task_req"
#define NODE_RESET_TASK_RSP   "node_reset_task_rsp"
#define NODE_RESET_TASK_TIMER "node_reset_task_timer"

// delete task
#define NODE_DELETE_TASK_REQ   "node_delete_task_req"
#define NODE_DELETE_TASK_RSP   "node_delete_task_rsp"
#define NODE_DELETE_TASK_TIMER "node_delete_task_timer"

// task logs
#define NODE_TASK_LOGS_REQ   "node_task_logs_req"
#define NODE_TASK_LOGS_RSP   "node_task_logs_rsp"
#define NODE_TASK_LOGS_TIMER "node_task_logs_timer"

// list task
#define NODE_LIST_TASK_REQ   "node_list_task_req"
#define NODE_LIST_TASK_RSP   "node_list_task_rsp"
#define NODE_LIST_TASK_TIMER "node_list_task_timer"

// modify task
#define NODE_MODIFY_TASK_REQ   "node_modify_task_req"
#define NODE_MODIFY_TASK_RSP   "node_modify_task_rsp"
#define NODE_MODIFY_TASK_TIMER "node_modify_task_timer"

// set task user password
#define NODE_PASSWD_TASK_REQ   "node_passwd_task_req"
#define NODE_PASSWD_TASK_RSP   "node_passwd_task_rsp"
#define NODE_PASSWD_TASK_TIMER "node_passwd_task_timer"

// list nodes
#define NODE_QUERY_NODE_INFO_REQ   "node_query_node_info_req"
#define NODE_QUERY_NODE_INFO_RSP   "node_query_node_info_rsp"
#define NODE_QUERY_NODE_INFO_TIMER "node_query_node_info_timer"

// query node rent orders
#define QUERY_NODE_RENT_ORDERS_REQ   "query_node_rent_orders_req"
#define QUERY_NODE_RENT_ORDERS_RSP   "query_node_rent_orders_rsp"
#define QUERY_NODE_RENT_ORDERS_TIMER "query_node_rent_orders_timer"

// free memory
#define NODE_FREE_MEMORY_REQ   "node_free_memory_req"
#define NODE_FREE_MEMORY_RSP   "node_free_memory_rsp"
#define NODE_FREE_MEMORY_TIMER "node_free_memory_timer"

// get node session_id
#define NODE_SESSION_ID_REQ   "node_session_id_req"
#define NODE_SESSION_ID_RSP   "node_session_id_rsp"
#define NODE_SESSION_ID_TIMER "node_session_id_timer"

// list snapshot
#define NODE_LIST_SNAPSHOT_REQ   "node_list_snapshot_req"
#define NODE_LIST_SNAPSHOT_RSP   "node_list_snapshot_rsp"
#define NODE_LIST_SNAPSHOT_TIMER "node_list_snapshot_timer"

// create snapshot
#define NODE_CREATE_SNAPSHOT_REQ   "node_create_snapshot_req"
#define NODE_CREATE_SNAPSHOT_RSP   "node_create_snapshot_rsp"
#define NODE_CREATE_SNAPSHOT_TIMER "node_create_snapshot_timer"

// delete snapshot
#define NODE_DELETE_SNAPSHOT_REQ   "node_delete_snapshot_req"
#define NODE_DELETE_SNAPSHOT_RSP   "node_delete_snapshot_rsp"
#define NODE_DELETE_SNAPSHOT_TIMER "node_delete_snapshot_timer"

// list disk
#define NODE_LIST_DISK_REQ   "node_list_disk_req"
#define NODE_LIST_DISK_RSP   "node_list_disk_rsp"
#define NODE_LIST_DISK_TIMER "node_list_disk_timer"

// resize disk
#define NODE_RESIZE_DISK_REQ   "node_resize_disk_req"
#define NODE_RESIZE_DISK_RSP   "node_resize_disk_rsp"
#define NODE_RESIZE_DISK_TIMER "node_resize_disk_timer"

// add disk
#define NODE_ADD_DISK_REQ   "node_add_disk_req"
#define NODE_ADD_DISK_RSP   "node_add_disk_rsp"
#define NODE_ADD_DISK_TIMER "node_add_disk_timer"

// delete disk
#define NODE_DELETE_DISK_REQ   "node_delete_disk_req"
#define NODE_DELETE_DISK_RSP   "node_delete_disk_rsp"
#define NODE_DELETE_DISK_TIMER "node_delete_disk_timer"

// list monitor server
#define NODE_LIST_MONITOR_SERVER_REQ   "node_list_monitor_server_req"
#define NODE_LIST_MONITOR_SERVER_RSP   "node_list_monitor_server_rsp"
#define NODE_LIST_MONITOR_SERVER_TIMER "node_list_monitor_server_timer"

// set monitor server
#define NODE_SET_MONITOR_SERVER_REQ   "node_set_monitor_server_req"
#define NODE_SET_MONITOR_SERVER_RSP   "node_set_monitor_server_rsp"
#define NODE_SET_MONITOR_SERVER_TIMER "node_set_monitor_server_timer"

// list local area network
#define NODE_LIST_LAN_REQ   "node_list_lan_req"
#define NODE_LIST_LAN_RSP   "node_list_lan_rsp"
#define NODE_LIST_LAN_TIMER "node_list_lan_timer"

// create local area network
#define NODE_CREATE_LAN_REQ   "node_create_lan_req"
#define NODE_CREATE_LAN_RSP   "node_create_lan_rsp"
#define NODE_CREATE_LAN_TIMER "node_create_lan_timer"

// delete local area network
#define NODE_DELETE_LAN_REQ   "node_delete_lan_req"
#define NODE_DELETE_LAN_RSP   "node_delete_lan_rsp"
#define NODE_DELETE_LAN_TIMER "node_delete_lan_timer"

// list bare metal
#define NODE_LIST_BARE_METAL_REQ   "node_list_bare_metal_req"
#define NODE_LIST_BARE_METAL_RSP   "node_list_bare_metal_rsp"
#define NODE_LIST_BARE_METAL_TIMER "node_list_bare_metal_timer"

// delete bare metal
#define NODE_ADD_BARE_METAL_REQ   "node_add_bare_metal_req"
#define NODE_ADD_BARE_METAL_RSP   "node_add_bare_metal_rsp"
#define NODE_ADD_BARE_METAL_TIMER "node_add_bare_metal_timer"

// delete bare metal
#define NODE_DELETE_BARE_METAL_REQ   "node_delete_bare_metal_req"
#define NODE_DELETE_BARE_METAL_RSP   "node_delete_bare_metal_rsp"
#define NODE_DELETE_BARE_METAL_TIMER "node_delete_bare_metal_timer"

// bare metal power
#define NODE_BARE_METAL_POWER_REQ   "node_bare_metal_power_req"
#define NODE_BARE_METAL_POWER_RSP   "node_bare_metal_power_rsp"
#define NODE_BARE_METAL_POWER_TIMER "node_bare_metal_power_timer"

// bare metal boot device order
#define NODE_BARE_METAL_BOOTDEV_REQ   "node_bare_metal_bootdev_req"
#define NODE_BARE_METAL_BOOTDEV_RSP   "node_bare_metal_bootdev_rsp"
#define NODE_BARE_METAL_BOOTDEV_TIMER "node_bare_metal_bootdev_timer"

// list deeplink info
#define NODE_LIST_DEEPLINK_INFO_REQ   "node_list_deeplink_info_req"
#define NODE_LIST_DEEPLINK_INFO_RSP   "node_list_deeplink_info_rsp"
#define NODE_LIST_DEEPLINK_INFO_TIMER "node_list_deeplink_info_timer"

// set deeplink info
#define NODE_SET_DEEPLINK_INFO_REQ   "node_set_deeplink_info_req"
#define NODE_SET_DEEPLINK_INFO_RSP   "node_set_deeplink_info_rsp"
#define NODE_SET_DEEPLINK_INFO_TIMER "node_set_deeplink_info_timer"

// service broadcast
#define SERVICE_BROADCAST_REQ "service_broadcast_req"

#endif  // DBC_MESSAGE_ID_H
