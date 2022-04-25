#ifndef _VIR_HELPER_H_
#define _VIR_HELPER_H_

#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <libvirt/libvirt-qemu.h>

namespace vir_helper {

struct domainDiskInfo {
  std::string driver_name;
  std::string driver_type;
  std::string source_file;
  std::string target_dev;
  std::string target_bus;
};

std::ostream& operator<<(std::ostream& out, const domainDiskInfo& obj);

typedef struct domainInterfaceIPAddress domainIPAddress;
struct domainInterfaceIPAddress {
  int type;                /* virIPAddrType */
  std::string addr;        /* IP address */
  unsigned int prefix;     /* IP address prefix */
};
struct domainInterface {
  std::string name;               /* interface name */
  std::string hwaddr;             /* hardware address, may be NULL */
  std::vector<domainIPAddress> addrs;    /* array of IP addresses */
};

struct domainSnapshotDiskInfo {
  std::string name;
  std::string snapshot;
  std::string driver_type;
  std::string source_file;
};

struct domainSnapshotInfo {
  std::string name;
  std::string description;
  std::string state;
  int64_t     creationTime;
  std::vector<domainSnapshotDiskInfo> disks;
};

std::ostream& operator<<(std::ostream& out, const domainSnapshotInfo& obj);

int getDomainSnapshotInfo(virDomainSnapshotPtr snapshot, domainSnapshotInfo &info);

std::shared_ptr<virError> getLastError();

////////////////////////////////////////////////////////////////////////////////

class virNWFilterImpl {
public:
  virNWFilterImpl() = delete;
  explicit virNWFilterImpl(virNWFilterPtr nwfilter);
  ~virNWFilterImpl() = default;

  /**
   * @brief Get the public name for the network filter
   * @param name   the public name for the network filter
   *
   * @return -1 in case of error, 0 in case of success
   */
  int getNWFilterName(std::string &name);

  /**
   * @brief Get the UUID for a network filter
   * @param uuid   a VIR_UUID_BUFLEN bytes array
   *
   * @return -1 in case of error, 0 in case of success
   */
  int getNWFilterUUID(unsigned char * uuid);

  /**
   * @brief Get the UUID for a network filter as string. For more information about UUID see RFC4122.
   * @param buf   a VIR_UUID_STRING_BUFLEN bytes array
   *
   * @return -1 in case of error, 0 in case of success
   */
  int getNWFilterUUIDString(char * buf);

  /**
   * @brief Provide an XML description of the network filter.
   * The description may be reused later to redefine the network filter with virNWFilterCreateXML().
   * @param flags  extra flags; not used yet, so callers should always pass 0
   *
   * @return a 0 terminated UTF-8 encoded XML instance, or empty string in case of error
   */
  std::string getNWFilterXMLDesc(unsigned int flags = 0);

  /**
   * @brief Undefine the nwfilter object.
   * This call will not succeed if a running VM is referencing the filter.
   * This does not free the associated virNWFilterPtr object.
   *
   * @return 0 in case of success and -1 in case of failure.
   */
  int undefineNWFilter();

private:
  std::shared_ptr<virNWFilter> nwfilter_;
};

class virDomainSnapshotImpl {
public:
  virDomainSnapshotImpl() = delete;
  explicit virDomainSnapshotImpl(virDomainSnapshotPtr snapshot);
  ~virDomainSnapshotImpl() = default;

  /**
   * @brief 将域恢复到本快照.
   * @param flags   通常，域将恢复到创建快照时域所处的相同状态（无论是不活动、正在运行还是暂停），但磁盘快照默认恢复到不活动状态。
   * 在覆盖快照状态中包含VIR_DOMAIN_SNAPSHOT_REVERT_RUNNINGflags以保证恢复后的运行域；
   * 或包含VIR_DOMAIN_SNAPSHOT_REVERT_PAUSED以flags保证恢复后暂停的域。这两个标志是互斥的。
   * 虽然持久域不需要任何一个标志，但不可能将瞬态域恢复到非活动状态，因此瞬态域需要使用这两个标志之一。
   *
   * 恢复到任何快照会丢弃自上次快照以来所做的所有配置更改。此外，从正在运行的域恢复到快照是一种数据丢失形式，因为它会丢弃当时来宾 RAM 中的任何内容。
   * 由于保留快照的本质暗示了回滚状态的意图，因此这些有损效果通常不需要额外的确认。
   *
   * 从 libvirt 7.10.0 开始，VM 进程总是重新启动，因此以下段落不再有效。如果快照元数据缺少完整的 VM XML，则无法再恢复到此类快照。
   *
   * 但是，有两种特殊情况，默认情况下会拒绝恢复，并且flags必须包含VIR_DOMAIN_SNAPSHOT_REVERT_FORCE以确认风险。
   * 1) 任何尝试恢复到缺少元数据来执行 ABI 兼容性检查的快照（通常情况下，当virDomainSnapshotGetXMLDesc()列出缺少完整 <domain> 的快照时，
   * 例如在 libvirt 0.9.5 之前创建的那些）。
   * 2) 任何将正在运行的域恢复到需要启动新的管理程序实例而不是重用现有管理程序的活动状态的尝试（因为这将终止与域的所有连接，
   * 例如 VNC 或 Spice 图形） - 这种情况出现来自可证明 ABI 不兼容的活动快照，以及来自flags请求在恢复后启动域的非活动快照。
   *
   * @return 0 in case of success and -1 in case of failure.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int revertDomainToThisSnapshot(int flags);

  /**
   * @brief 删除快照.
   * @param flags   删除标志。如果flags为 0，则仅删除此快照，并且此快照的更改会自动合并到子快照中。
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_DELETE_CHILDREN，则删除此快照和任何后代快照。
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_DELETE_CHILDREN_ONLY，则删除所有后代快照，但保留此快照。这两个标志是互斥的。
   *
   * @return 0 in case of success and -1 in case of failure.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int deleteSnapshot(int flags);

  int getSnapshotName(std::string &name);

  std::shared_ptr<virDomainSnapshotImpl> getSnapshotParent();

  /**
   * @brief Provide an XML description of the domain snapshot, with a top-level element of <domainsnapshot>.
   * No security-sensitive data will be included unless flags contains VIR_DOMAIN_SNAPSHOT_XML_SECURE;
   * this flag is rejected on read-only connections.
   * @param flags   bitwise-OR of supported virDomainSnapshotXMLFlags
   *
   * @return a 0 terminated UTF-8 encoded XML instance or empty string in case of error.
   */
  std::string getSnapshotXMLDesc(unsigned int flags);

  int listAllSnapshotChilden();

  int listAllSnapshotChildenNames(std::vector<std::string> *names);

  int getSnapshotChildenNums();

  int getSnapshotInfo(domainSnapshotInfo &info);

protected:
  std::shared_ptr<virDomainSnapshot> snapshot_;
};

////////////////////////////////////////////////////////////////////////////////

class virDomainImpl {
public:
  virDomainImpl() = delete;
  explicit virDomainImpl(virDomainPtr domain);
  ~virDomainImpl() = default;

  int startDomain();

  /**
   * @brief Suspends an active domain, the process is frozen without further access to CPU resources and I/O but the memory used by the domain
   * at the hypervisor level will stay allocated. Use virDomainResume() to reactivate the domain. This function may require privileged access.
   * Moreover, suspend may not be supported if domain is in some special state like VIR_DOMAIN_PMSUSPENDED.
   * @param domain   a domain object
   *
   * @return 0 in case of success and -1 in case of failure.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int suspendDomain();

  /**
   * @brief Resume a suspended domain, the process is restarted from the state where it was frozen by calling virDomainSuspend().
   * This function may require privileged access Moreover, resume may not be supported if domain is in some special state like VIR_DOMAIN_PMSUSPENDED.
   * @param domain   a domain object
   *
   * @return 0 in case of success and -1 in case of failure.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int resumeDomain();

  int rebootDomain(int flag = 0);

  int shutdownDomain();

  int destroyDomain();

  int resetDomain();

  int undefineDomain();

  /**
   * @brief destroy, undefine domain and delete image file
   *
   * @return 0 in case of success and -1 in case of failure.
   */
  int deleteDomain();

  /**
   * @brief Provide an XML description of the domain.
   * The description may be reused later to relaunch the domain with virDomainCreateXML().
   * @param flags  bitwise-OR of virDomainXMLFlags
   *
   * @return a 0 terminated UTF-8 encoded XML instance, or empty string in case of error.
   */
  std::string getDomainXMLDesc(unsigned int flags);

  int hasNvram();

  int getDomainDisks(std::vector<domainDiskInfo> &disks);

  int getDomainInterfaceAddress(std::vector<domainInterface> &difaces, unsigned int source = VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_LEASE);

  int setDomainUserPassword(const char *user, const char *password);

  /**
   * @brief Determine if the domain is currently running
   *
   * @return 1 if running, 0 if inactive, -1 on error
   */
  int isDomainActive();

  /**
   * @brief Extract information about a domain.
   * Note that if the connection used to get the domain is limited only a partial set of the information can be extracted.
   * @param info    pointer to a virDomainInfo structure allocated by the user
   *
   * @return 0 in case of success and -1 in case of failure.
   */
  int getDomainInfo(virDomainInfoPtr info);

  int getDomainCPUStats(virTypedParameterPtr params, unsigned int nparams, int start_cpu, unsigned int ncpus, unsigned int flags);

  int getDomainMemoryStats(virDomainMemoryStatPtr stats, unsigned int nr_stats, unsigned int flags);

  int getDomainBlockInfo(const char *disk, virDomainBlockInfoPtr info, unsigned int flags);

  int getDomainBlockStats(const char *disk, virDomainBlockStatsPtr stats, size_t size);

  int getDomainNetworkStats(const char *device, virDomainInterfaceStatsPtr stats, size_t size);

  std::string QemuAgentCommand(const char *cmd, int timeout, unsigned int flags);

  /**
   * @brief Extract domain state. Each state can be accompanied with a reason (if known) which led to the state.
   * @param state    returned state of the domain (one of virDomainState)
   * @param reason   returned reason which led to state (one of virDomain*Reason corresponding to the current state); it is allowed to be NULL
   * @param flags    extra flags; not used yet, so callers should always pass 0
   *
   * @return 0 in case of success and -1 in case of failure.
   */
  int getDomainState(int *state, int *reason, unsigned int flags);

  int getDomainStatsList(unsigned int stats);

  /**
   * @brief Extract information about a domain's block device.
   * @param disk    path to the block device, or device shorthand
   *
   * @return 0 in case of success and -1 in case of failure.
   */
  int getDomainBlockInfo(const char *disk);

  /**
   * @brief 查询提供的域的统计信息doms。请注意，中的所有域doms必须共享相同的连接。
   * 根据stats字段报告正在运行的 VM 的各种参数的统计信息。统计信息作为每个查询域的结构数组返回。
   * 该结构包含一个类型化参数数组，其中包含单个统计信息。
   * 每个统计字段的类型参数名称由一个点分隔的字符串组成，该字符串包含所请求组的名称，后跟统计值的组特定描述。
   *
   * 统计组使用stats参数启用，该参数是枚举virDomainStatsTypes的二进制或。统计组记录在virConnectGetAllDomainStats 中。
   *
   * stats 使用 0 返回给定管理程序支持的所有统计数据组。
   *
   * 指定VIR_CONNECT_GET_ALL_DOMAINS_STATS_ENFORCE_STATS为flags使函数返回错误的情况下，一些统计类型的stats没有被守护的认可。
   * 然而，即使有这个标志，如果信息不可用，管理程序可能会忽略已知组中的个别字段；
   * 作为一个极端的例子，如果统计数据仅对正在运行的域有意义，则支持的组可能会为离线域生成零字段。
   *
   * 传递VIR_CONNECT_GET_ALL_DOMAINS_STATS_NOWAIT的flags手段时，libvirt的是无法为
   * 任何域的（无论何种原因）获取的统计信息只返回域统计的一个子集。该子集是不涉及查询底层管理程序的统计信息。
   *
   * 请注意，flags此函数可能会拒绝任何域列表过滤标志。
   * @param doms   以 NULL 结尾的域数组
   * @param stats  要返回的统计信息，二进制或 virDomainStatsTypes
   * @param retStats    将用返回的统计数据数组填充的指针
   * @param flags       额外的标志；二进制或 virConnectGetAllDomainStatsFlags
   *
   * @return 成功时返回的统计结构的计数，错误时返回 -1。
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int getDomainBlockParameters();
  int getDomainBlockIoTune(const char *disk);

  /**
   * @brief Get a list of mapping information for each mounted file systems within the specified guest and the disks.
   *
   * @return the number of returned mount points, or -1 in case of error.
   */
  int getDomainFSInfo();

  /**
   * @brief Queries the guest agent for the various information about the guest system.
   * The reported data depends on the guest agent implementation.
   *
   * @return 0 on success, -1 on error.
   */
  int getDomainGuestInfo();

  /**
   * @brief 根据包含在 xmlDesc 中的快照 xml 创建域的新快照，并带有顶级元素 <domainsnapshot>
   * @param xmlDesc   包含域快照的 XML 描述的字符串。
   * @param flags   virDomainSnapshotCreateFlags 的按位或。
   * 如果flags为 0，则域可以处于活动状态，在这种情况下，快照将是完整的系统快照（捕获磁盘状态和运行时 VM 状态，例如 RAM 内容），
   * 其中恢复快照与从休眠状态恢复相同（ TCP 连接可能已超时，但其他一切都会从停止的地方恢复）；或者域可以处于非活动状态，在这种情况下，快照仅包含启动前的磁盘状态。
   * 新创建的快照成为当前快照（请参阅virDomainSnapshotCurrent ()），并且是任何先前当前快照的子级。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_VALIDATE，则xmlDesc根据 <domainsnapshot> XML 架构进行验证。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE，则这是在删除元数据之前恢复先前从virDomainSnapshotGetXMLDesc ()捕获的快照元数据的请求，而不是创建新快照。
   * 这可用于在目标上重新创建快照层次结构，然后在源上将其删除，以允许迁移（因为如果快照元数据仍保留在源计算机上，迁移通常会失败）。
   * 请注意，虽然原创可以省略一些元素xmlDesc（并且 libvirt 将根据当时的域状态提供合理的默认值），
   * 重新定义必须提供更多元素（因为在此期间域可能已更改，因此 libvirt 不再有办法重新提供正确的默认值）。
   * 重新定义快照元数据时，除非同时存在VIR_DOMAIN_SNAPSHOT_CREATE_CURRENT标志，否则域的当前快照将不会被更改。
   * 在没有VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE 的情况下请求VIR_DOMAIN_SNAPSHOT_CREATE_CURRENT标志是错误的.
   * 在某些虚拟机管理程序上，重新定义现有快照可用于更改要在还原期间使用的域 XML 的主机特定部分（例如与磁盘设备关联的支持文件名），但不得更改来宾可见的布局。
   * 当重新定义一个不存在的快照名称时，管理程序可能会验证恢复到快照似乎是可能的（例如，磁盘映像具有请求名称的快照内容）。并非所有管理程序都支持这些标志。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_NO_METADATA，则域的磁盘映像将根据 修改xmlDesc，
   * 但 libvirt 不跟踪任何元数据（类似于立即使用VIR_DOMAIN_SNAPSHOT_DELETE_METADATA_ONLY调用virDomainSnapshotDelete () ）。
   * 此标志与VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE不兼容。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_HALT，则快照完成后域将处于非活动状态，无论之前是否处于活动状态；否则，一个正在运行的域将在快照之后仍然运行。
   * 此标志在瞬态域上无效，并且与VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE不兼容。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_LIVE，则创建快照时域不会暂停。这会增加内存转储文件的大小，但会减少来宾在拍摄快照时的停机时间。某些管理程序仅在外部快照期间支持此标志。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY，则快照将仅限于 中描述的磁盘xmlDesc，并且不会保存 VM 状态。
   * 对于活动的来宾，磁盘映像可能不一致（就像断电一样），并使用VIR_DOMAIN_SNAPSHOT_CREATE_HALT标志指定它有数据丢失的风险。
   *
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_QUIESCE，则 libvirt 将尝试使用来宾代理来冻结和解冻域操作系统中使用的所有文件系统。
   * 但是，如果来宾代理不存在，则会引发错误。此外，此标志还需要传递VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY。
   * 为了更好地控制和错误恢复，用户应该在拍摄快照之前手动调用virDomainFSFreeze，然后调用 virDomainFSThaw来恢复虚拟机，而不是使用VIR_DOMAIN_SNAPSHOT_CREATE_QUIESCE。
   *
   * 默认情况下，如果快照涉及外部文件，并且任何目标文件已作为非空常规文件存在，则拒绝快照以避免丢失这些文件的内容。
   * 但是，如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_REUSE_EXT，则必须使用正确的图像格式和元数据（包括后备存储路径）手动预先创建
   * 目标文件（这允许管理应用程序预先创建具有相关后备文件名的文件，而不是默认的创建带有绝对后备文件名）。
   * 请注意，仅将快照 XML 中指定的文件作为快照插入，因此在预先创建的映像中设置不正确的元数据可能会导致 VM 无法启动或其他块作业可能会失败。
   *
   * 请注意，尽管 libvirt 更喜欢在没有其他影响的情况下预先报告错误，但某些虚拟机管理程序具有某些类型的故障，即使来宾配置已部分更改，
   * 整个命令也很容易失败（例如，如果对两个磁盘的磁盘快照请求在第二个磁盘上失败，但第一个磁盘更改无法回滚）。
   * 如果此 API 调用失败，因此通常需要跟进virDomainGetXMLDesc () 并检查每个磁盘以确定是否发生了任何部分更改。
   * 但是，如果flags包含VIR_DOMAIN_SNAPSHOT_CREATE_ATOMIC, 然后 libvirt 保证此命令不会更改任何磁盘，除非可以原子地完成整个更改集，
   * 从而使故障恢复更简单（请注意，磁盘更改后仍然可能失败，但仅在极少数情况下运行内存或磁盘空间不足）。
   *
   * 如果当前存在块复制操作，一些管理程序可能会阻止此操作；在这种情况下，首先使用virDomainBlockJobAbort () 停止块复制。
   *
   * @return 一个（不透明的）新virDomainSnapshotImpl成功或 NULL 失败.
   *     -<em>nullptr</em> fail
   *     -<em>virDomainSnapshotImpl pointer</em> succeed
   */
  std::shared_ptr<virDomainSnapshotImpl> createSnapshot(const char *xmlDesc, unsigned int flags);

  std::shared_ptr<virDomainSnapshotImpl> getSnapshotByName(const char *name);

  /**
   * @brief 收集给定域的域快照列表并分配一个数组来存储这些对象。该 API 解决了virDomainSnapshotListNames () 中固有的竞争问题。
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_LIST_TOPOLOGICAL并且snaps是非 NULL，并且没有其他连接正在修改快照，那么可以保证对于结果列表中的任何快照，
   * 列表中较晚的快照都不能通过从较早的那个开始的virDomainSnapshotGetParent ()序列到达快照；否则，未指定结果列表中快照的顺序。
   *
   * 默认情况下，此命令涵盖所有快照。当flags包含VIR_DOMAIN_SNAPSHOT_LIST_ROOTS时，也可以将事情限制为没有父母的快照。下面列出的组中提供了其他过滤器。
   * 在一个组内，位是互斥的，所有可能的快照都由该组中的一个位来描述。某些管理程序可能会拒绝无法区分过滤的特定标志。如果所选过滤器标志集形成不可能的组合，则管理程序可能返回 0 或错误。
   *
   * 第一组flags是VIR_DOMAIN_SNAPSHOT_LIST_LEAVES和VIR_DOMAIN_SNAPSHOT_LIST_NO_LEAVES，根据没有其他子节点的快照（叶快照）进行过滤。
   *
   * 下一组flags是VIR_DOMAIN_SNAPSHOT_LIST_METADATA和VIR_DOMAIN_SNAPSHOT_LIST_NO_METADATA，用于根据快照是否具有阻止删除对域的最后一个引用的元数据来过滤快照。
   *
   * 下一组flags是VIR_DOMAIN_SNAPSHOT_LIST_INACTIVE、VIR_DOMAIN_SNAPSHOT_LIST_ACTIVE和VIR_DOMAIN_SNAPSHOT_LIST_DISK_ONLY，用于根据快照跟踪的域状态过滤快照。
   *
   * 下一组flags是VIR_DOMAIN_SNAPSHOT_LIST_INTERNAL和VIR_DOMAIN_SNAPSHOT_LIST_EXTERNAL，用于根据快照是存储在磁盘映像中还是作为附加文件来过滤快照。
   *
   * @param snapshots  用于收集快照列表的数组
   * @param flags      支持的virDomainSnapshotListFlags 的按位或
   *
   * @return 0 in case of success and -1 in case of failure.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int listAllSnapshots(std::vector<std::shared_ptr<virDomainSnapshotImpl>> &snapshots, unsigned int flags);

  /**
   * @brief 收集给定域的域快照列表，并将其名称存储在names. 使用的值nameslen可以由getSnapshotNums () 和flags 确定.
   * 如果flags包含VIR_DOMAIN_SNAPSHOT_LIST_TOPOLOGICAL，并且没有其他连接正在修改快照，则可以保证对于结果列表中的任何快照，
   * 从该较早快照开始的virDomainSnapshotGetParent ()序列无法到达列表中较晚的快照；否则，未指定结果列表中快照的顺序。
   *
   * 默认情况下，此命令涵盖所有快照。当flags包含VIR_DOMAIN_SNAPSHOT_LIST_ROOTS时，也可以将事情限制为没有父项的快照。
   * 其他过滤器通过与virDomainListAllSnapshots () 中flags记录的值相同的值提供。
   *
   * 请注意，此命令本质上是活泼的：另一个连接可以在对virDomainSnapshotNum () 的调用和此调用之间定义新的快照。
   * 如果返回值小于nameslen ，则只能保证列出所有当前定义的快照。同样，如果另一个连接在此期间删除了快照，
   * 那么您应该准备好virDomainSnapshotLookupByName () 在将名称从此调用转换为快照对象时失败。
   *
   * 不鼓励使用此函数，而是使用listAllSnapshots() 。
   * @param names       用于收集快照名称列表的数组
   * @param nameslen    names的大小
   * @param flags       支持的virDomainSnapshotListFlags 的按位或
   *
   * @return 找到的域快照数量或 -1 以防出错。调用者负责为数组的每个成员调用 free().
   */
  int listSnapshotNames(std::vector<std::string> &names, int nameslen, unsigned int flags);

  /**
   * @brief 提供此域的快照数 。
   * 此功能将接受VIR_DOMAIN_SNAPSHOT_LIST_TOPOLOGICAL在flags只有virDomainSnapshotListNames（）能够履行它，虽然标志在这里有没有其他作用。
   * 默认情况下，此命令涵盖所有快照。当flags包含VIR_DOMAIN_SNAPSHOT_LIST_ROOTS时，也可以将事情限制为没有父项的快照。
   * 其他过滤器通过与virDomainListAllSnapshots () 中flags记录的值相同的值提供。
   * @param flags    支持的virDomainSnapshotListFlags 的按位或
   *
   * @return 找到的域快照数量或 -1 以防出错。
   */
  int getSnapshotNums(unsigned int flags);

  int blockCommit(const char *disk, const char *base, const char *top, unsigned long bandwith, unsigned int flags);

  int blockPull(const char *disk, unsigned long bandwith, unsigned int flags);

  int blockRebase(const char *disk, const char *base, unsigned long bandwith, unsigned int flags);

  int getBlockJobInfo(const char *disk, virDomainBlockJobInfoPtr info, unsigned int flags);

protected:
  std::shared_ptr<virDomain> domain_;
};

////////////////////////////////////////////////////////////////////////////////

class virHelper {
public:
  explicit virHelper(bool enableEvent = false);
  ~virHelper();

  // host
  /**
   * @brief Provides version information. libVer is the version of the library and will always be set unless an error occurs,
   * in which case an error code will be returned. typeVer exists for historical compatibility;
   * if it is not NULL it will duplicate libVer (it was originally intended to return hypervisor information based on type,
   * but due to the design of remote clients this is not reliable).
   * To get the version of the running hypervisor use the virConnectGetVersion() function instead.
   * To get the libvirt library version used by a connection use the virConnectGetLibVersion() instead.
   * This function includes a call to virInitialize() when necessary.
   * @param libVer   return value for the library version (OUT)
   * @param type     ignored; pass NULL
   * @param typeVer  pass NULL; for historical purposes duplicates libVer if non-NULL
   *
   * @return -1 in case of failure, 0 otherwise, and values for libVer and typeVer have the format major * 1,000,000 + minor * 1,000 + release.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  static int getVersion(unsigned long *libVer, const char *type, unsigned long *typeVer);

  /**
   * @brief Get the version level of the Hypervisor running. This may work only with hypervisor call, i.e. with privileged access to the hypervisor,
   * not with a Read-Only connection.
   * @param conn     pointer to the hypervisor connection
   * @param hvVer    return value for the version of the running hypervisor (OUT)
   *
   * @return -1 in case of error, 0 otherwise. if the version can't be extracted by lack of capacities returns 0 and hvVer is 0,
   * otherwise hvVer value is major * 1,000,000 + minor * 1,000 + release
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int getConnectVersion(unsigned long *hvVer);
  /**
   * @brief Provides libVer, which is the version of libvirt used by the daemon running on the conn host
   * @param conn     pointer to the hypervisor connection
   * @param libVer   returns the libvirt library version used on the connection (OUT)
   *
   * @return -1 in case of failure, 0 otherwise, and values for libVer have the format major * 1,000,000 + minor * 1,000 + release.
   *     -<em>-1</em> fail
   *     -<em>0</em> succeed
   */
  int getConnectLibVersion(unsigned long *libVer);

  bool openConnect(const char *name);
  bool openConnectReadOnly(const char *name);

  // domain
  /**
   * @brief Collect a possibly-filtered list of all domains, and return an allocated array of information for each.
   * @param domains  a variable to store the array containing domain objects
   * @param flags    bitwise-OR of virConnectListAllDomainsFlags
   *
   * @return the number of domains found or -1 and sets domains to NULL in case of error.
   */
  int listAllDomains(std::vector<std::shared_ptr<virDomainImpl>> &domains, unsigned int flags);

  /**
   * @brief Collect the list of active domains, and store their IDs in array ids
   * The use of this function is discouraged. Instead, use virConnectListAllDomains().
   * @param ids       array to collect the list of IDs of active domains
   * @param maxids    size of ids
   *
   * @return the number of domains found or -1 in case of error.
   */
  int listDomains(int *ids, int maxids);

  /**
   * @brief Provides the number of active domains.
   *
   * @return the number of domain found or -1 in case of error
   */
  int numOfDomains();

  std::shared_ptr<virDomainImpl> openDomainByID(int id);
  std::shared_ptr<virDomainImpl> openDomainByName(const char *domain_name);
  std::shared_ptr<virDomainImpl> openDomainByUUID(const unsigned char *uuid);
  std::shared_ptr<virDomainImpl> openDomainByUUIDString(const char *uuid);

  std::shared_ptr<virDomainImpl> defineDomain(const char *xml_content);

  std::shared_ptr<virDomainImpl> createDomain(const char *xml_content);

  // nwfilter
  /**
   * @brief Define a new network filter, based on an XML description similar to the one returned by virNWFilterGetXMLDesc()
   * @param xmlDesc     an XML description of the nwfilter
   *
   * @return a new nwfilter object or NULL in case of failure
   */
  std::shared_ptr<virNWFilterImpl> defineNWFilter(const char * xmlDesc);

  /**
   * @brief Try to lookup a network filter on the given hypervisor based on its name.
   * @param name     name for the network filter
   *
   * @return a new nwfilter object or nullptr in case of failure.
   * If the network filter cannot be found, then VIR_ERR_NO_NWFILTER error is raised.
   */
  std::shared_ptr<virNWFilterImpl> openNWFilterByName(const char * name);
  std::shared_ptr<virNWFilterImpl> openNWFilterByUUID(const unsigned char * uuid);
  std::shared_ptr<virNWFilterImpl> openNWFilterByUUIDString(const char * uuidstr);

  /**
   * @brief Provides the number of nwfilters.
   *
   * @return the number of nwfilters found or -1 in case of error
   */
  int numOfNWFilters();

  /**
   * @brief Collect the list of network filters, and store their names in names
   * The use of this function is discouraged. Instead, use virConnectListAllNWFilters().
   * @param names     array to collect the list of names of network filters
   * @param maxnames  size of names
   *
   * @return the number of network filters found or -1 in case of error
   */
  int listNWFilters(std::vector<std::string> &names, int maxnames);

  /**
   * @brief Collect the list of network filters, and allocate an array to store those objects.
   * @param filters   a variable to store the array containing the network filter objects
   * @param flags     extra flags; not used yet, so callers should always pass 0
   *
   * @return the number of network filters found or -1 in case of error
   */
  int listAllNWFilters(std::vector<std::shared_ptr<virNWFilterImpl>> &filters, unsigned int flags);

protected:
  void DefaultThreadFunc();

protected:
  std::shared_ptr<virConnect> conn_;
  bool enable_event_;
  int dom_event_lifecycle_callback_id_;
  int dom_event_agent_callback_id_;
  int dom_event_block_job_callback_id_;
  int thread_quit_;
  std::thread *thread_event_loop_;
};
} // namespace virTool

#endif // _VIR_HELPER_H_
