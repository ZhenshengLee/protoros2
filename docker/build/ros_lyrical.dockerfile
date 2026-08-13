# https://yeasy.gitbook.io/docker_practice/image/build
# docker build -t ros:lyrical-dev -f ./docker/build/ros_lyrical.dockerfile . --network=host --build-arg GA_FTP_SERVER=$(ip route show default | cut -d' ' -f3)
# docker save ros:lyrical-ros-base-resolute ros:lyrical-dev | pzstd -c > ros-lyrical-dev_$(TZ=Asia/Shanghai date +%y%m%d).tzst

# add ecal prebuilt(u20.04+gcc9.4+pb3.21.7) binary

FROM ros:lyrical-ros-base-resolute

ARG GA_FTP_SERVER=192.168.160.1
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
    && apt install -y clang-tidy libprotobuf-dev libabsl-dev protobuf-compiler python3-jinja2 python3-inflection python3-multipledispatch python3-networkx python3-scipy \
    && apt install -y ros-lyrical-demo-nodes-cpp ros-lyrical-osrf-testing-tools-cpp ros-lyrical-test-msgs \
    && apt install -y iceoryx iceoryx-doc libiceoryx-*-dev libflatbuffers-dev flatbuffers-compiler libturbojpeg0-dev libjpeg-turbo8-dev \
    && pip3 config set global.trusted-host https://pypi.tuna.tsinghua.edu.cn \
    && pip3 config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple \
    && python3 -m pip install rosbags mcap protobuf==5.29.6 \
    && wget --ftp-user=zs --ftp-password=zs -q -O- ftp://${GA_FTP_SERVER}/DiskT/docker_res/dbg/tmux_plugins.tgz | tar -xz -C /opt \
    && mkdir -p /opt/gsd/x86_64/ \
    && wget --ftp-user=zs --ftp-password=zs -q -O- ftp://${GA_FTP_SERVER}/DiskT/docker_res/3rdparty/ecal-5.13-x86-gcc9.4.0.tgz | tar -xz -C /opt/gsd/x86_64/ \
    && wget --ftp-user=zs --ftp-password=zs -q -O- ftp://${GA_FTP_SERVER}/DiskT/docker_res/3rdparty/iceoryx2-0.9.3-x86-u2604.tgz | tar -xz -C /opt/gsd/x86_64/ \
    && wget --ftp-user=zs --ftp-password=zs -q -O /usr/bin/mcap ftp://${GA_FTP_SERVER}/DiskT/docker_res/mlops/mcap-linux-amd64 \
    && chmod 755 /usr/bin/mcap \
    && rm -rf /var/lib/apt/lists/* \
    && rm /etc/apt/sources.list.d/ubuntu.sources \
    && mv /etc/apt/sources.list.d/ubuntu.sources.bak /etc/apt/sources.list.d/ubuntu.sources
