# internet cafe power manager tool

Implement the status commands such as power on/off, compile and package them and put them in the installation directory of dbc, and finally add a configuration item `netbar_power_manager=/home/dbtu/dbc_baremetal_node/power_manager` in the `conf/core.conf` configuration file.

实现开机/关机等状态命令，编译打包后放入 dbc 的安装目录中，最后在 `conf/core.conf` 配置文件中添加一条配置项 `netbar_power_manager=/home/dbtu/dbc_baremetal_node/power_manager`。

## command line

```shell
power_manager --poweron '{"uuid":"1c697ac9084e","ipmi_hostname": "192.168.0.110"}'
power_manager --poweroff '{"uuid":"1c697ac9084e","ipmi_hostname": "192.168.0.110"}'
power_manager --status '{"uuid":"1c697ac9084e","ipmi_hostname": "192.168.0.110"}'
```

## Python example

[Python code implementation](./main.py)

Use PyInstaller to package:

```shell
pip3 install pyinstaller
python3 -m PyInstaller -F -n power_manager main.py
```

## CPP example

[CPP code implementation](./main.cpp)
