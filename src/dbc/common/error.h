#ifndef DBC_ERROR_H
#define DBC_ERROR_H

#include <iostream>
#include <string>

// 错误码
typedef int32_t ERRCODE;
enum {
    ERR_SUCCESS = 0,    
    ERR_ERROR = -1,     
};

struct FResult {
    FResult(int32_t _errcode, std::string _errmsg) {
        errcode = _errcode;
        errmsg = std::move(_errmsg);
    }

    int32_t errcode = ERR_SUCCESS;
    std::string errmsg = "success";
};

#define FResultOk FResult(ERR_SUCCESS, "ok")
#define FResultError FResult(ERR_ERROR, "error");


// ============================================================================
// AUTHENTICATION & AUTHORIZATION ERRORS (E001-E099)
// ============================================================================
#define E001_PASSWORD_ERROR                                 (-1001)    // Password authentication failed
#define E002_AUTHORITY_CHECK_FAILED                         (-1002)    // Authority verification failed
#define E003_SIGNATURE_VERIFY_FAILED                        (-1003)    // Digital signature verification failed
#define E004_NONCE_ALREADY_EXISTS                           (-1004)    // Nonce already exists
#define E005_WALLET_EMPTY                                   (-1005)    // Wallet address is empty
#define E006_SESSION_INVALID                                (-1006)    // Session ID is invalid
#define E007_MULTISIG_THRESHOLD_INVALID                     (-1007)    // Multisig threshold is invalid
#define E008_MULTISIG_WALLET_EMPTY                          (-1008)    // Multisig wallet is empty

// ============================================================================
// NETWORK & COMMUNICATION ERRORS (E100-E199)
// ============================================================================
#define E100_NETWORK_FAILURE                                (-1100)    // Network communication failure
#define E101_CONNECTION_TIMEOUT                             (-1101)    // Connection timeout
#define E102_READ_TIMEOUT                                   (-1102)    // Read operation timeout
#define E103_MAINNET_COMMUNICATION_ERROR                    (-1103)    // Mainnet communication error
#define E104_INACTIVE_CHANNEL                               (-1104)    // No active communication channel
#define E105_MESSAGE_QUEUE_FULL                            (-1105)    // Message queue is full
#define E106_OFFLINE_STATUS                                 (-1106)    // Device is offline

// ============================================================================
// VIRTUAL MACHINE ERRORS (E200-E299)
// ============================================================================
#define E200_VM_CONNECT_ERROR                               (-1200)    // Libvirt connection error
#define E201_VM_DOMAIN_NOT_FOUND                            (-1201)    // VM domain not found
#define E202_VM_DOMAIN_ALREADY_EXISTS                       (-1202)    // VM domain already exists
#define E203_VM_INTERNAL_ERROR                              (-1203)    // VM internal system error
#define E204_VM_CREATE_FAILED                               (-1204)    // VM creation failed
#define E205_VM_START_FAILED                                (-1205)    // VM start failed
#define E206_VM_SHUTDOWN_FAILED                             (-1206)    // VM shutdown failed
#define E207_VM_RESTART_FAILED                              (-1207)    // VM restart failed
#define E208_VM_RESET_FAILED                                (-1208)    // VM reset failed
#define E209_VM_DELETE_FAILED                               (-1209)    // VM deletion failed
#define E210_VM_POWEROFF_FAILED                             (-1210)    // VM power off failed
#define E211_VM_NETWORK_FILTER_CREATE_FAILED                (-1211)    // VM network filter creation failed
#define E212_VM_BIOS_MODE_INVALID                           (-1212)    // VM BIOS mode is invalid

// ============================================================================
// TASK MANAGEMENT ERRORS (E300-E399)
// ============================================================================
#define E300_TASK_NOT_FOUND                                 (-1300)    // Task not found
#define E301_TASK_ALREADY_EXISTS                            (-1301)    // Task already exists
#define E302_TASK_CREATE_FAILED                             (-1302)    // Task creation failed
#define E303_TASK_START_FAILED                              (-1303)    // Task start failed
#define E304_TASK_STOP_FAILED                               (-1304)    // Task stop failed
#define E305_TASK_MODIFY_FAILED                             (-1305)    // Task modification failed
#define E306_TASK_STATUS_INVALID                            (-1306)    // Task status is invalid
#define E307_TASK_RESOURCE_INSUFFICIENT                     (-1307)    // Insufficient resources for task
#define E308_TASK_GPU_ALLOCATION_FAILED                     (-1308)    // GPU allocation failed
#define E309_TASK_CPU_CHECK_FAILED                          (-1309)    // CPU resource check failed
#define E310_TASK_MEMORY_CHECK_FAILED                       (-1310)    // Memory resource check failed
#define E311_TASK_DISK_CHECK_FAILED                         (-1311)    // Disk resource check failed
#define E312_TASK_RUNNING_DOMAINS_EXIST                     (-1312)    // Other running domains exist

// ============================================================================
// IMAGE MANAGEMENT ERRORS (E400-E499)
// ============================================================================
#define E400_IMAGE_NOT_FOUND                                (-1400)    // Docker image not found
#define E401_IMAGE_PULL_FAILED                              (-1401)    // Image pull failed
#define E402_IMAGE_PULLING_IN_PROGRESS                      (-1402)    // Image is currently being pulled
#define E403_IMAGE_UPLOAD_FAILED                             (-1403)    // Image upload failed
#define E404_IMAGE_DELETE_FAILED                            (-1404)    // Image deletion failed
#define E405_IMAGE_DOWNLOAD_PROGRESS_FAILED                 (-1405)    // Image download progress check failed
#define E406_IMAGE_UPLOAD_PROGRESS_FAILED                   (-1406)    // Image upload progress check failed
#define E407_IMAGE_STOP_DOWNLOAD_FAILED                     (-1407)    // Stop image download failed
#define E408_IMAGE_STOP_UPLOAD_FAILED                       (-1408)    // Stop image upload failed

// ============================================================================
// DISK MANAGEMENT ERRORS (E500-E599)
// ============================================================================
#define E500_DISK_NOT_FOUND                                 (-1500)    // Disk not found
#define E501_DISK_RESIZE_FAILED                             (-1501)    // Disk resize failed
#define E502_DISK_ADD_FAILED                                 (-1502)    // Disk addition failed
#define E503_DISK_DELETE_FAILED                             (-1503)    // Disk deletion failed
#define E504_DISK_SPACE_INSUFFICIENT                         (-1504)    // Insufficient disk space
#define E505_DISK_SIZE_INVALID                               (-1505)    // Invalid disk size
#define E506_DISK_FREE_SPACE_CHECK_FAILED                   (-1506)    // Disk free space check failed

// ============================================================================
// SNAPSHOT MANAGEMENT ERRORS (E600-E699)
// ============================================================================
#define E600_SNAPSHOT_NOT_FOUND                             (-1600)    // Snapshot not found
#define E601_SNAPSHOT_CREATE_FAILED                         (-1601)    // Snapshot creation failed
#define E602_SNAPSHOT_DELETE_FAILED                         (-1602)    // Snapshot deletion failed
#define E603_SNAPSHOT_UPLOAD_FAILED                          (-1603)    // Snapshot upload failed
#define E604_SNAPSHOT_DOWNLOAD_FAILED                        (-1604)    // Snapshot download failed

// ============================================================================
// SYSTEM & INFRASTRUCTURE ERRORS (E700-E799)
// ============================================================================
#define E700_FILE_OPERATION_FAILED                          (-1700)    // File operation failed
#define E701_DATABASE_ERROR                                  (-1701)    // Database operation error
#define E702_MEMORY_ALLOCATION_FAILED                       (-1702)    // Memory allocation failed
#define E703_NULL_POINTER_ERROR                              (-1703)    // Null pointer error
#define E704_BAD_PARAMETER                                  (-1704)    // Invalid parameter
#define E705_OBJECT_NOT_FOUND                               (-1705)    // Specified object not found
#define E706_OBJECT_ALREADY_EXISTS                          (-1706)    // Object already exists
#define E707_CONTAINER_NOT_FOUND                            (-1707)    // Container not found
#define E708_CONTAINER_START_FAILED                         (-1708)    // Container start failed
#define E709_NOT_COMPUTING_NODE                             (-1709)    // Not a computing node
#define E710_BILLING_DISABLED                               (-1710)    // Billing system disabled

// ============================================================================
// APPLICATION & PROCESS ERRORS (E800-E899)
// ============================================================================
#define E800_EXIT_FAILURE                                    (-1800)    // Application exit due to failure
#define E801_COMMAND_LINE_PARSE_ERROR                       (-1801)    // Command line parsing error
#define E802_DEFAULT_ERROR                                   (-1802)    // Default error, common use
#define E803_JSON_PARSE_ERROR                               (-1803)    // JSON parsing error
#define E804_HTTP_REQUEST_ERROR                             (-1804)    // HTTP request error
#define E805_HTTP_BODY_EMPTY                                (-1805)    // HTTP request body is empty
#define E806_URI_INVALID                                    (-1806)    // Invalid URI format
#define E807_REQUEST_METHOD_NOT_SUPPORTED                   (-1807)    // HTTP request method not supported

// ============================================================================
// LEGACY ERROR CODES (for backward compatibility)
// ============================================================================
#define E_EXIT_FAILURE                                      E800_EXIT_FAILURE
#define E_EXIT_PARSE_COMMAND_LINE                           E801_COMMAND_LINE_PARSE_ERROR
#define E_DEFAULT                                           E802_DEFAULT_ERROR
#define E_BAD_PARAM                                         E704_BAD_PARAMETER
#define E_NULL_POINTER                                      E703_NULL_POINTER_ERROR
#define E_MSG_QUEUE_FULL                                    E105_MESSAGE_QUEUE_FULL
#define E_FILE_FAILURE                                      E700_FILE_OPERATION_FAILED
#define E_NOT_FOUND                                         E705_OBJECT_NOT_FOUND
#define E_EXISTED                                           E706_OBJECT_ALREADY_EXISTS
#define E_INACTIVE_CHANNEL                                  E104_INACTIVE_CHANNEL
#define E_NONCE                                             E004_NONCE_ALREADY_EXISTS
#define E_IMAGE_NOT_FOUND                                   E400_IMAGE_NOT_FOUND
#define E_PULLING_IMAGE                                     E402_IMAGE_PULLING_IN_PROGRESS
#define E_NO_DISK_SPACE                                     E504_DISK_SPACE_INSUFFICIENT
#define E_NETWORK_FAILURE                                   E100_NETWORK_FAILURE
#define E_BILL_DISABLE                                      E710_BILLING_DISABLED
#define E_NOT_COMPUTING_NODE                                E709_NOT_COMPUTING_NODE
#define E_NO_START_CONTAINER                                E708_CONTAINER_START_FAILED
#define E_CONTAINER_NOT_FOUND                               E707_CONTAINER_NOT_FOUND
#define E_VIRT_CONNECT_ERROR                                E200_VM_CONNECT_ERROR
#define E_VIRT_DOMAIN_NOT_FOUND                             E201_VM_DOMAIN_NOT_FOUND
#define E_VIRT_INTERNAL_ERROR                               E203_VM_INTERNAL_ERROR
#define E_VIRT_DOMAIN_EXIST                                 E202_VM_DOMAIN_ALREADY_EXISTS

// ============================================================================
// RPC ERROR CODES (E900-E999)
// ============================================================================
enum rpc_error_code
{
    RPC_RESPONSERR_SUCCESS = 0,

    //Error code before a request is processed
    RPC_INVALID_REQUEST = -32600,//The inspection request is illegal.
    RPC_METHOD_NOT_FOUND = -32601,//Check that the requested method does not exist
    RPC_REQUEST_INTERRUPTED = -32602,//Request is interrupted.Try again later.

    //Error code during waiting for response
    RPC_RESPONSE_TIMEOUT = -32700, //call timeout
    RPC_RESPONSE_ERROR = -32701,// response error

    RPC_MISC_ERROR = -1,  //!< std::exception thrown in command handling
    RPC_TYPE_ERROR = -3,  //!< Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY = -5,  //!< Invalid address or key
    RPC_OUT_OF_MEMORY = -7,  //!< Ran out of memory during operation
    RPC_DATABASE_ERROR = -9, //!< Database error
    RPC_DESERIALIZATION_ERROR = -11, //!< Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR = -13, //!< General error during transaction or block submission
    RPC_VERIFY_REJECTED = -15, //!< Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN = -17, //!< Transaction already in chain
    RPC_IN_WARMUP = -19, //!< Client still warming up
    RPC_SYSTEM_BUSYING = -21,//!<Work queue depth exceeded
    RPC_METHOD_DEPRECATED = -23, //!< RPC method is deprecated
    RPC_INVALID_PARAMS = -25, //!<
};

// ============================================================================
// ERROR CODE UTILITY FUNCTIONS
// ============================================================================

// Get error code category from error code
inline std::string get_error_category(int32_t errcode) {
    if (errcode >= -1009 && errcode <= -1001) return "AUTH";
    if (errcode >= -1106 && errcode <= -1100) return "NET";
    if (errcode >= -1212 && errcode <= -1200) return "VM";
    if (errcode >= -1312 && errcode <= -1300) return "TASK";
    if (errcode >= -1408 && errcode <= -1400) return "IMG";
    if (errcode >= -1506 && errcode <= -1500) return "DISK";
    if (errcode >= -1604 && errcode <= -1600) return "SNAP";
    if (errcode >= -1710 && errcode <= -1700) return "SYS";
    if (errcode >= -1807 && errcode <= -1800) return "APP";
    if (errcode >= -32701 && errcode <= -32600) return "RPC";
    return "UNKNOWN";
}

// Get error description from error code
inline std::string get_error_description(int32_t errcode) {
    switch (errcode) {
        case E001_PASSWORD_ERROR: return "Password authentication failed";
        case E002_AUTHORITY_CHECK_FAILED: return "Authority verification failed";
        case E003_SIGNATURE_VERIFY_FAILED: return "Digital signature verification failed";
        case E004_NONCE_ALREADY_EXISTS: return "Nonce already exists";
        case E005_WALLET_EMPTY: return "Wallet address is empty";
        case E006_SESSION_INVALID: return "Session ID is invalid";
        case E007_MULTISIG_THRESHOLD_INVALID: return "Multisig threshold is invalid";
        case E008_MULTISIG_WALLET_EMPTY: return "Multisig wallet is empty";
        case E100_NETWORK_FAILURE: return "Network communication failure";
        case E101_CONNECTION_TIMEOUT: return "Connection timeout";
        case E102_READ_TIMEOUT: return "Read operation timeout";
        case E103_MAINNET_COMMUNICATION_ERROR: return "Mainnet communication error";
        case E104_INACTIVE_CHANNEL: return "No active communication channel";
        case E105_MESSAGE_QUEUE_FULL: return "Message queue is full";
        case E106_OFFLINE_STATUS: return "Device is offline";
        case E200_VM_CONNECT_ERROR: return "Libvirt connection error";
        case E201_VM_DOMAIN_NOT_FOUND: return "VM domain not found";
        case E202_VM_DOMAIN_ALREADY_EXISTS: return "VM domain already exists";
        case E203_VM_INTERNAL_ERROR: return "VM internal system error";
        case E204_VM_CREATE_FAILED: return "VM creation failed";
        case E205_VM_START_FAILED: return "VM start failed";
        case E206_VM_SHUTDOWN_FAILED: return "VM shutdown failed";
        case E207_VM_RESTART_FAILED: return "VM restart failed";
        case E208_VM_RESET_FAILED: return "VM reset failed";
        case E209_VM_DELETE_FAILED: return "VM deletion failed";
        case E210_VM_POWEROFF_FAILED: return "VM power off failed";
        case E211_VM_NETWORK_FILTER_CREATE_FAILED: return "VM network filter creation failed";
        case E212_VM_BIOS_MODE_INVALID: return "VM BIOS mode is invalid";
        case E300_TASK_NOT_FOUND: return "Task not found";
        case E301_TASK_ALREADY_EXISTS: return "Task already exists";
        case E302_TASK_CREATE_FAILED: return "Task creation failed";
        case E303_TASK_START_FAILED: return "Task start failed";
        case E304_TASK_STOP_FAILED: return "Task stop failed";
        case E305_TASK_MODIFY_FAILED: return "Task modification failed";
        case E306_TASK_STATUS_INVALID: return "Task status is invalid";
        case E307_TASK_RESOURCE_INSUFFICIENT: return "Insufficient resources for task";
        case E308_TASK_GPU_ALLOCATION_FAILED: return "GPU allocation failed";
        case E309_TASK_CPU_CHECK_FAILED: return "CPU resource check failed";
        case E310_TASK_MEMORY_CHECK_FAILED: return "Memory resource check failed";
        case E311_TASK_DISK_CHECK_FAILED: return "Disk resource check failed";
        case E312_TASK_RUNNING_DOMAINS_EXIST: return "Other running domains exist";
        case E400_IMAGE_NOT_FOUND: return "Docker image not found";
        case E401_IMAGE_PULL_FAILED: return "Image pull failed";
        case E402_IMAGE_PULLING_IN_PROGRESS: return "Image is currently being pulled";
        case E403_IMAGE_UPLOAD_FAILED: return "Image upload failed";
        case E404_IMAGE_DELETE_FAILED: return "Image deletion failed";
        case E405_IMAGE_DOWNLOAD_PROGRESS_FAILED: return "Image download progress check failed";
        case E406_IMAGE_UPLOAD_PROGRESS_FAILED: return "Image upload progress check failed";
        case E407_IMAGE_STOP_DOWNLOAD_FAILED: return "Stop image download failed";
        case E408_IMAGE_STOP_UPLOAD_FAILED: return "Stop image upload failed";
        case E500_DISK_NOT_FOUND: return "Disk not found";
        case E501_DISK_RESIZE_FAILED: return "Disk resize failed";
        case E502_DISK_ADD_FAILED: return "Disk addition failed";
        case E503_DISK_DELETE_FAILED: return "Disk deletion failed";
        case E504_DISK_SPACE_INSUFFICIENT: return "Insufficient disk space";
        case E505_DISK_SIZE_INVALID: return "Invalid disk size";
        case E506_DISK_FREE_SPACE_CHECK_FAILED: return "Disk free space check failed";
        case E600_SNAPSHOT_NOT_FOUND: return "Snapshot not found";
        case E601_SNAPSHOT_CREATE_FAILED: return "Snapshot creation failed";
        case E602_SNAPSHOT_DELETE_FAILED: return "Snapshot deletion failed";
        case E603_SNAPSHOT_UPLOAD_FAILED: return "Snapshot upload failed";
        case E604_SNAPSHOT_DOWNLOAD_FAILED: return "Snapshot download failed";
        case E700_FILE_OPERATION_FAILED: return "File operation failed";
        case E701_DATABASE_ERROR: return "Database operation error";
        case E702_MEMORY_ALLOCATION_FAILED: return "Memory allocation failed";
        case E703_NULL_POINTER_ERROR: return "Null pointer error";
        case E704_BAD_PARAMETER: return "Invalid parameter";
        case E705_OBJECT_NOT_FOUND: return "Specified object not found";
        case E706_OBJECT_ALREADY_EXISTS: return "Object already exists";
        case E707_CONTAINER_NOT_FOUND: return "Container not found";
        case E708_CONTAINER_START_FAILED: return "Container start failed";
        case E709_NOT_COMPUTING_NODE: return "Not a computing node";
        case E710_BILLING_DISABLED: return "Billing system disabled";
        case E800_EXIT_FAILURE: return "Application exit due to failure";
        case E801_COMMAND_LINE_PARSE_ERROR: return "Command line parsing error";
        case E802_DEFAULT_ERROR: return "Default error, common use";
        case E803_JSON_PARSE_ERROR: return "JSON parsing error";
        case E804_HTTP_REQUEST_ERROR: return "HTTP request error";
        case E805_HTTP_BODY_EMPTY: return "HTTP request body is empty";
        case E806_URI_INVALID: return "Invalid URI format";
        case E807_REQUEST_METHOD_NOT_SUPPORTED: return "HTTP request method not supported";
        default: return "Unknown error";
    }
}



#endif // DBC_ERROR_H
