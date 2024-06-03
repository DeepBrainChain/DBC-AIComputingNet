#include <string>

const char* help_context =
    "power_manager poweron|poweroff|status "
    "'{\"uuid\":\"3156995b\",\"ipmi_hostname\": \"192.168.0.110\"}'";

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]).find("help") != std::string::npos) {
        printf("%s\n", help_context);
        return 0;
    } else if (argc != 3) {
        printf("%s\n", help_context);
        return 1;
    }
    std::string command(argv[1]);
    if (command == "poweron") {
        // 将 argv[2] 中指定的机器开机
        printf("Power on\n");
    } else if (command == "poweroff") {
        // 将 argv[2] 中指定的机器关机
        printf("Power off\n");
    } else if (command == "status") {
        // 查询 argv[2] 中指定的机器电源状态并输出到控制台
        printf("Power is on\n");
    } else {
        // 不支持的命令参数返回非0值即可
        printf("Not supported\n");
        return 1;
    }
    return 0;
}
