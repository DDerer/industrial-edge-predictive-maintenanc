# Orange Pi 5B deployment

目标系统为 Orange Pi 5B ARM64 Ubuntu。当前服务文件使用 `/dev/ttyS4` 和 115200 8N1。
正式部署时会加入安装脚本、udev规则、专用用户、日志轮转及配置文件加载。
