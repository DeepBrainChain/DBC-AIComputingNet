#ifndef DBC_DEF_H
#define DBC_DEF_H

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <cassert>
#include <memory>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <string.h>
#include <cstring>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#define MAX_LIST_TASK_COUNT                                     10000
#define MAX_TASK_COUNT_PER_REQ                                  10
#define MAX_TASK_COUNT_ON_PROVIDER                              10000
#define MAX_TASK_COUNT_IN_TRAINING_QUEUE                        1000
#define MAX_TASK_SHOWN_ON_LIST                                  100

#define GET_LOG_HEAD                                            0
#define GET_LOG_TAIL                                            1
#define MAX_NUMBER_OF_LINES                                     100
#define DEFAULT_NUMBER_OF_LINES                                 100
#define MAX_LOG_CONTENT_SIZE                                    (8 * 1024)

#define MAX_ENTRY_FILE_NAME_LEN                                 128
#define MAX_ENGINE_IMGE_NAME_LEN                                128

#define AI_TRAINING_MAX_TASK_COUNT                                  64

#define SERVICE_NAME_AI_TRAINING "ai_training"


enum DBC_NODE_TYPE {
    DBC_COMPUTE_NODE,
    DBC_CLIENT_NODE,
    DBC_SEED_NODE
};

enum get_peers_flag
{
    flag_active,
    flag_global
};

enum ETaskOp {
    T_OP_None,
    T_OP_Create,    //����
    T_OP_Start,     //����
    T_OP_Stop,      //ֹͣ
    T_OP_ReStart,   //����(reboot domain)
    T_OP_ForceReboot,  //ǿ������(destroy domain && start domain)
    T_OP_Reset,     //reset
    T_OP_Delete,    //ɾ��
    T_OP_CreateSnapshot  //��������
};

struct ETaskEvent {
    std::string task_id;
    ETaskOp op;
    std::string image_server;
};

enum ETaskStatus {
    TS_ShutOff,     //�ر�״̬
    TS_Creating,    //���ڴ���
    TS_Running,     //����������
    TS_Starting,    //��������
    TS_Stopping,    //����ֹͣ
    TS_Restarting,  //��������
    TS_Resetting,   //����reset
    TS_Deleting,    //����ɾ��
    TS_CreatingSnapshot,  //���ڴ�������
    TS_PMSuspended, //������ѽ���˯��״̬

    TS_Error = 100,   //����
    TS_CreateError,
    TS_StartError,
    TS_StopError,
    TS_RestartError,
    TS_ResetError,
    TS_DeleteError,
    TS_CreateSnapshotError
};

enum ETaskLogDirection {
    LD_Head,
    LD_Tail
};

std::string task_status_string(int32_t status);
std::string task_operation_string(int32_t op);
std::string vm_status_string(virDomainState status);

// ϵͳ�̴�С��GB��
static const int32_t g_disk_system_size = 350;
// �ڴ�Ԥ����С��GB��
static const int32_t g_reserved_memory = 32;
// cpuԤ��������ÿ������cpu������������
static const int32_t g_reserved_physical_cores_per_cpu = 1;
// �������¼�û���
static const char* g_vm_ubuntu_login_username = "dbc";
static const char* g_vm_windows_login_username = "Administrator";


enum MACHINE_STATUS {
    MS_NONE,
    MS_VERIFY, //��֤�׶�
    MS_ONLINE, //��֤�����߽׶�
    MS_RENNTED //������
};

enum USER_ROLE {
    UR_NONE,
    UR_VERIFIER,
    UR_RENTER_WALLET,
    UR_RENTER_SESSION_ID
};

struct AuthoriseResult {
    bool success = false;
    std::string errmsg;

    MACHINE_STATUS machine_status;
    USER_ROLE user_role = USER_ROLE::UR_NONE;

    std::string rent_wallet;
    int64_t rent_end = 0;
};

struct TaskCreateParams {
    std::string task_id;
    std::string login_password;
    std::string image_name;     // system disk image name
    std::string custom_image_name;
    std::string data_file_name; // data disk file name
    int16_t ssh_port;
    int16_t rdp_port;
    std::vector<std::string> custom_port;
    int32_t cpu_sockets = 0;
    int32_t cpu_cores = 0;
    int32_t cpu_threads = 0;

    std::map<std::string, std::list<std::string>> gpus;

    uint64_t mem_size; // KB

    std::map<int32_t, uint64_t> disks; //������(��λ��KB)��index��1��ʼ

    std::string vm_xml;
    std::string vm_xml_url;

    int16_t vnc_port;
    std::string vnc_password;

    std::string operation_system; //����ϵͳ(��generic, ubuntu 18.04, windows 10)��Ĭ��ubuntu������win����Ϊ��windowsϵͳ������ȫСд��
    std::string bios_mode; //BIOSģʽ(��legacy,uefi)��Ĭ�ϴ�ͳBIOS������ȫСд��

    std::vector<std::string> multicast; //�鲥��ַ(�磺"230.0.0.1:5558")

    std::string network_name;
    std::string vxlan_vni;
};

struct ParseVmXmlParams {
    std::string task_id;
    std::string image_name;
    std::string data_file_name;

    int32_t cpu_sockets;
    int32_t cpu_cores;
    int32_t cpu_threads;

    std::map<std::string, std::list<std::string>> gpus;

    uint64_t mem_size; // KB

    std::map<int32_t, uint64_t> disks; //������(��λ��KB)��index��1��ʼ

    int16_t vnc_port;
    std::string vnc_password;

    std::string operation_system; //����ϵͳ(��generic, ubuntu 18.04, windows 10)��Ĭ��ubuntu������win����Ϊ��windowsϵͳ������ȫСд��
    std::string bios_mode; //BIOSģʽ(��legacy,uefi)��Ĭ�ϴ�ͳBIOS������ȫСд��

    std::vector<std::string> multicast; //�鲥��ַ(�磺"230.0.0.1:5558")
};

#endif
