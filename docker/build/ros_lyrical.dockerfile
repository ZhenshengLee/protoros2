# https://yeasy.gitbook.io/docker_practice/image/build
# docker build -t ros:lyrical-dev -f ./docker/build/ros_lyrical.dockerfile . --network=host
# docker save ros:lyrical-dev | pzstd -c > ros-lyrical-dev_$(TZ=Asia/Shanghai date +%y%m%d).tzst

FROM ros:lyrical-ros-base-resolute

ENV DEBIAN_FRONTEND=noninteractive
ENV PIP_BREAK_SYSTEM_PACKAGES=1

# 安装deb依赖
RUN set -x; \
    cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources.bak \
    && sed -i 's/archive.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/' /etc/apt/sources.list.d/ubuntu.sources \
    && sed -i 's/security.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/' /etc/apt/sources.list.d/ubuntu.sources \
    && sed -i 's/ports.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/' /etc/apt/sources.list.d/ubuntu.sources \
    && sed -i 's/packages.ros.org/mirrors.aliyun.com/' /etc/apt/sources.list.d/ros2.sources \
    && apt update \
    && apt install -y sudo lsb-release wget cmake bc parallel pigz tmux netcat-openbsd \
    && apt install -y nano gdb gdbserver python3-numpy python3-pip python3-netifaces git rapidjson-dev apt-transport-https gnupg patchelf libacl1-dev software-properties-common python-is-python3 \
    && apt install -y systemd strace ltrace kmod \
    && apt install -y libprotobuf-dev protobuf-compiler \
    && pip3 config set global.trusted-host https://pypi.tuna.tsinghua.edu.cn \
    && pip3 config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple \
    && python3 -m pip install rosbags \
    && rm -rf /var/lib/apt/lists/* \
    && rm /etc/apt/sources.list.d/ubuntu.sources \
    && mv /etc/apt/sources.list.d/ubuntu.sources.bak /etc/apt/sources.list.d/ubuntu.sources
