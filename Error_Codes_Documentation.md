# DBC-AIComputingNet Error Codes Documentation

## Overview
This document provides a comprehensive list of all error codes used in the DBC-AIComputingNet project. Each error code follows the format `E[Category][Number]` and includes a clear description of the issue.

## Error Code Categories
- **AUTH (E0xx)**: Authentication and Authorization Errors
- **NET (E1xx)**: Network Communication Errors
- **VM (E2xx)**: Virtual Machine Related Errors
- **TASK (E3xx)**: Task Management Errors
- **IMG (E4xx)**: Image Management Errors
- **DISK (E5xx)**: Disk Management Errors
- **SNAP (E6xx)**: Snapshot Management Errors
- **SYS (E7xx)**: System and File Operation Errors
- **APP (E8xx)**: Application and General Errors
- **RPC (E9xx)**: RPC Communication Errors

---

## Authentication Errors (E0xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E001_PASSWORD_ERROR | Password verification failed | Invalid password or credentials |
| E002_CERTIFICATE_ERROR | Certificate verification failed | Invalid or expired certificate |
| E003_PERMISSION_DENIED | Permission denied | Insufficient permissions for operation |
| E004_AUTHENTICATION_FAILED | Authentication failed | User authentication process failed |
| E005_SESSION_EXPIRED | Session expired | User session has expired |

---

## Network Errors (E1xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E100_NETWORK_FAILURE | Network communication failure | General network communication error |
| E101_CONNECTION_TIMEOUT | Connection timeout | Network connection timed out |
| E102_CONNECTION_REFUSED | Connection refused | Remote server refused connection |
| E103_NETWORK_UNREACHABLE | Network unreachable | Target network cannot be accessed |
| E104_DNS_RESOLUTION_FAILED | DNS resolution failed | Domain name resolution failed |
| E105_SSL_HANDSHAKE_FAILED | SSL handshake failed | SSL/TLS handshake process failed |
| E106_PROTOCOL_ERROR | Protocol error | Network protocol violation |
| E107_BANDWIDTH_EXCEEDED | Bandwidth exceeded | Network bandwidth limit exceeded |

---

## Virtual Machine Errors (E2xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E200_VM_CONNECT_ERROR | VM connection error | Unable to connect to virtual machine |
| E201_VM_DOMAIN_NOT_FOUND | VM domain not found | Virtual machine domain does not exist |
| E202_VM_CREATE_FAILED | VM creation failed | Unable to create virtual machine |
| E203_VM_START_FAILED | VM start failed | Unable to start virtual machine |
| E204_VM_STOP_FAILED | VM stop failed | Unable to stop virtual machine |
| E205_VM_DELETE_FAILED | VM deletion failed | Unable to delete virtual machine |
| E206_VM_SUSPEND_FAILED | VM suspend failed | Unable to suspend virtual machine |
| E207_VM_RESUME_FAILED | VM resume failed | Unable to resume virtual machine |
| E208_VM_MIGRATE_FAILED | VM migration failed | Unable to migrate virtual machine |
| E209_VM_SNAPSHOT_FAILED | VM snapshot failed | Unable to create VM snapshot |
| E210_VM_CONFIG_ERROR | VM configuration error | Invalid virtual machine configuration |
| E211_VM_RESOURCE_INSUFFICIENT | VM insufficient resources | Insufficient resources for VM operation |
| E212_VM_HYPERVISOR_ERROR | VM hypervisor error | Hypervisor communication error |

---

## Task Management Errors (E3xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E300_TASK_NOT_FOUND | Task not found | Requested task does not exist |
| E301_TASK_CREATE_FAILED | Task creation failed | Unable to create new task |
| E302_TASK_CREATE_FAILED | Task creation failed | Unable to create task due to system error |
| E303_TASK_START_FAILED | Task start failed | Unable to start task execution |
| E304_TASK_STOP_FAILED | Task stop failed | Unable to stop task execution |
| E305_TASK_DELETE_FAILED | Task deletion failed | Unable to delete task |
| E306_TASK_UPDATE_FAILED | Task update failed | Unable to update task configuration |
| E307_TASK_QUEUE_FULL | Task queue full | Task execution queue is full |
| E308_TASK_TIMEOUT | Task timeout | Task execution timed out |
| E309_TASK_RESOURCE_CONFLICT | Task resource conflict | Resource conflict between tasks |
| E310_TASK_VALIDATION_FAILED | Task validation failed | Task parameter validation failed |

---

## Image Management Errors (E4xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E400_IMAGE_NOT_FOUND | Image not found | Requested image does not exist |
| E401_IMAGE_DOWNLOAD_FAILED | Image download failed | Unable to download image from server |
| E402_IMAGE_UPLOAD_FAILED | Image upload failed | Unable to upload image to server |
| E403_IMAGE_FORMAT_ERROR | Image format error | Unsupported or invalid image format |
| E404_IMAGE_CORRUPTED | Image corrupted | Image file is corrupted |
| E405_IMAGE_SIZE_EXCEEDED | Image size exceeded | Image size exceeds maximum limit |
| E406_IMAGE_METADATA_ERROR | Image metadata error | Invalid or missing image metadata |
| E407_IMAGE_REGISTRY_ERROR | Image registry error | Image registry communication error |

---

## Disk Management Errors (E5xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E500_DISK_NOT_FOUND | Disk not found | Requested disk does not exist |
| E501_DISK_CREATE_FAILED | Disk creation failed | Unable to create new disk |
| E502_DISK_DELETE_FAILED | Disk deletion failed | Unable to delete disk |
| E503_DISK_FORMAT_FAILED | Disk format failed | Unable to format disk |
| E504_DISK_MOUNT_FAILED | Disk mount failed | Unable to mount disk |
| E505_DISK_UNMOUNT_FAILED | Disk unmount failed | Unable to unmount disk |
| E506_DISK_SPACE_INSUFFICIENT | Insufficient disk space | Insufficient disk space for operation |
| E507_DISK_IO_ERROR | Disk I/O error | Disk input/output operation failed |
| E508_DISK_PERMISSION_DENIED | Disk permission denied | Insufficient permissions for disk operation |
| E509_DISK_CORRUPTED | Disk corrupted | Disk is corrupted |

---

## Snapshot Management Errors (E6xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E600_SNAPSHOT_NOT_FOUND | Snapshot not found | Requested snapshot does not exist |
| E601_SNAPSHOT_CREATE_FAILED | Snapshot creation failed | Unable to create snapshot |
| E602_SNAPSHOT_DELETE_FAILED | Snapshot deletion failed | Unable to delete snapshot |
| E603_SNAPSHOT_RESTORE_FAILED | Snapshot restore failed | Unable to restore from snapshot |
| E604_SNAPSHOT_MERGE_FAILED | Snapshot merge failed | Unable to merge snapshots |
| E605_SNAPSHOT_STORAGE_FULL | Snapshot storage full | Snapshot storage space is full |
| E606_SNAPSHOT_METADATA_ERROR | Snapshot metadata error | Invalid snapshot metadata |
| E607_SNAPSHOT_CONCURRENT_ACCESS | Snapshot concurrent access | Concurrent snapshot access detected |

---

## System and File Operation Errors (E7xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E700_FILE_OPERATION_FAILED | File operation failed | General file operation error |
| E701_DATABASE_ERROR | Database operation error | Database connection or operation failed |
| E702_MEMORY_ALLOCATION_FAILED | Memory allocation failed | Unable to allocate memory |
| E703_NULL_POINTER_ERROR | Null pointer error | Null pointer reference detected |
| E704_BAD_PARAMETER | Bad parameter | Invalid or malformed parameter |
| E705_OBJECT_NOT_FOUND | Object not found | Requested object does not exist |
| E706_PERMISSION_DENIED | Permission denied | Insufficient permissions for operation |
| E707_RESOURCE_BUSY | Resource busy | Resource is currently in use |
| E708_RESOURCE_LOCKED | Resource locked | Resource is locked by another process |
| E709_SYSTEM_CALL_FAILED | System call failed | System call execution failed |
| E710_CONFIGURATION_ERROR | Configuration error | Invalid system configuration |
| E711_ENVIRONMENT_ERROR | Environment error | System environment issue |

---

## Application and General Errors (E8xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E800_UNKNOWN_ERROR | Unknown error | Unspecified or unknown error occurred |
| E801_INTERNAL_ERROR | Internal error | Internal system error |
| E802_DEFAULT_ERROR | Default error | General default error code |
| E803_JSON_PARSE_ERROR | JSON parse error | Unable to parse JSON data |
| E804_JSON_GENERATE_ERROR | JSON generation error | Unable to generate JSON data |
| E805_XML_PARSE_ERROR | XML parse error | Unable to parse XML data |
| E806_ENCODING_ERROR | Encoding error | Character encoding/decoding error |
| E807_VALIDATION_ERROR | Validation error | Data validation failed |
| E808_TIMEOUT_ERROR | Timeout error | Operation timed out |
| E809_RATE_LIMIT_EXCEEDED | Rate limit exceeded | Request rate limit exceeded |
| E810_QUOTA_EXCEEDED | Quota exceeded | Resource quota exceeded |
| E811_VERSION_MISMATCH | Version mismatch | Version compatibility issue |

---

## RPC Communication Errors (E9xx)

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| E900_RPC_INVALID_REQUEST | RPC invalid request | Invalid RPC request format |
| E901_RPC_METHOD_NOT_FOUND | RPC method not found | RPC method does not exist |
| E902_RPC_PARAMETER_ERROR | RPC parameter error | Invalid RPC parameters |
| E903_RPC_AUTHENTICATION_FAILED | RPC authentication failed | RPC authentication failed |
| E904_RPC_PERMISSION_DENIED | RPC permission denied | Insufficient RPC permissions |
| E905_RPC_TIMEOUT | RPC timeout | RPC call timed out |
| E906_RPC_CONNECTION_FAILED | RPC connection failed | RPC connection establishment failed |
| E907_RPC_SERIALIZATION_ERROR | RPC serialization error | RPC data serialization failed |
| E908_RPC_DESERIALIZATION_ERROR | RPC deserialization error | RPC data deserialization failed |
| E909_RPC_SERVER_ERROR | RPC server error | RPC server internal error |
| E910_RPC_CLIENT_ERROR | RPC client error | RPC client internal error |

---

## Legacy Error Codes

| Error Code | Description | Issue Details |
|------------|-------------|---------------|
| ERR_SUCCESS | Operation successful | Operation completed successfully |
| ERR_ERROR | General error | General error condition |
| E_DEFAULT | Default error | Legacy default error code (maps to E802_DEFAULT_ERROR) |
