#ifndef DBC_CPU_TUNE_H
#define DBC_CPU_TUNE_H

#include "util/utils.h"

namespace CpuTune {
// 参考命令 virsh capabilities 查出来的 numa 节点
struct Cpu {
    unsigned id;
    unsigned socket_id;
    unsigned core_id;
    std::vector<unsigned> siblings;
};

struct NumaNode {
    std::map<unsigned, Cpu> cpus;
};

struct Topology {
    std::map<unsigned, NumaNode> numas;
};
}  // namespace CpuTune

class CpuTuneManager : public Singleton<CpuTuneManager> {
public:
    CpuTuneManager();

    virtual ~CpuTuneManager();

    // Must run after vmclient initialization
    FResult Init();

    void Exit();

    bool IsHyperThreading() const { return is_hyper_threading_; }

    unsigned int GetHostCpuCount() const { return host_cpu_count_; }

    unsigned int GetCpuMapLength() const { return cpu_maplen_; }

    void UseCpus(const std::vector<unsigned int>& cpus);

    void UnuseCpus(const std::vector<unsigned int>& cpus);

    // It returns non-zero if the bit of the related CPU is set.
    int CpuUsed(const unsigned int cpu) const;

    unsigned int UsedCpuCount() const;

    bool TunableCpus(unsigned int vcpus, std::vector<unsigned int>& cpus);

    void UpdateCpuMap(const std::vector<unsigned char>& cpumap);

protected:

private:
    CpuTune::Topology topology_;
    bool is_hyper_threading_;
    unsigned int host_cpu_count_;
    unsigned int cpu_maplen_;

    mutable RwMutex mutex_;
    std::vector<unsigned char> cpumap_;
};

#endif  // DBC_CPU_TUNE_H
