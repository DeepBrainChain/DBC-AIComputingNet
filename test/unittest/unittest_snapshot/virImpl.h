#ifndef DBCPROJ_UNITTEST_VIR_VIRIMPL_H
#define DBCPROJ_UNITTEST_VIR_VIRIMPL_H

#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

struct DomainSnapshotDiskInfo {
    std::string name;
    std::string snapshot;
    std::string driver_type;
    std::string source_file;
};

struct DomainSnapshotInfo {
    std::string name;
    std::string description;
    std::string state;
    int64_t     creationTime;
    std::vector<DomainSnapshotDiskInfo> disks;
};

std::ostream& operator<<(std::ostream& out, const DomainSnapshotInfo& obj);

int getDomainSnapshotInfo(virDomainSnapshotPtr snapshot, DomainSnapshotInfo &info);

class virDomainSnapshotImpl {
public:
    virDomainSnapshotImpl() = delete;
    explicit virDomainSnapshotImpl(virDomainSnapshotPtr snapshot);
    ~virDomainSnapshotImpl();
    
    /**
     * @brief 将域恢复到本快照。.
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
    int32_t revertDomainToThisSnapshot(int flags);

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
    int32_t deleteSnapshot(int flags);

    int32_t getSnapshotName(std::string &name);

    std::shared_ptr<virDomainSnapshotImpl> getSnapshotParent();

    int32_t getSnapshotXMLDesc(std::string &desc);

    int32_t listAllSnapshotChilden();

    int32_t listAllSnapshotChildenNames(std::vector<std::string> *names);

    int getSnapshotChildenNums();

    int32_t getSnapshotInfo(DomainSnapshotInfo &info);

protected:
    std::shared_ptr<virDomainSnapshot> snapshot_;
};

class virDomainImpl {
public:
    virDomainImpl() = delete;
    explicit virDomainImpl(virDomainPtr domain);
    ~virDomainImpl();

    int32_t startDomain();

    int32_t suspendDomain();

    int32_t resumeDomain();

    int32_t rebootDomain(int flag = 0);

    int32_t shutdownDomain();

    int32_t destroyDomain();

    int32_t resetDomain();

    int32_t undefineDomain();

    int32_t deleteDomain();

    int32_t getDomainDisks(std::vector<std::string> &disks);

    int32_t getDomainInterfaceAddress(std::string &local_ip);

    int32_t setDomainUserPassword(const char *user, const char *password);

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
    int32_t listAllSnapshots(std::vector<std::shared_ptr<virDomainSnapshotImpl>> &snapshots, unsigned int flags);

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
    int32_t listSnapshotNames(std::vector<std::string> &names, int nameslen, unsigned int flags);

    /**
     * @brief 提供此域的快照数 。
     * 此功能将接受VIR_DOMAIN_SNAPSHOT_LIST_TOPOLOGICAL在flags只有virDomainSnapshotListNames（）能够履行它，虽然标志在这里有没有其他作用。
     * 默认情况下，此命令涵盖所有快照。当flags包含VIR_DOMAIN_SNAPSHOT_LIST_ROOTS时，也可以将事情限制为没有父项的快照。
     * 其他过滤器通过与virDomainListAllSnapshots () 中flags记录的值相同的值提供。
     * @param flags    支持的virDomainSnapshotListFlags 的按位或
     *
     * @return 找到的域快照数量或 -1 以防出错。
     */
    int32_t getSnapshotNums(unsigned int flags);

protected:
    std::shared_ptr<virDomain> domain_;
};

class virTool {
public:
    virTool();
    ~virTool();

    bool openConnect(const char *name);

    std::shared_ptr<virDomainImpl> defineDomain(const char *xml_content);

    std::shared_ptr<virDomainImpl> openDomain(const char *domain_name);

protected:
    void DefaultThreadFunc();

protected:
    std::shared_ptr<virConnect> conn_;
    int callback_id_;
    int agent_callback_id_;
    int thread_quit_;
    std::thread *thread_event_loop_;
};

#endif
