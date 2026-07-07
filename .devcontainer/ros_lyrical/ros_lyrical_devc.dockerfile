# docker build -t ros:lyrical-devc -f ./.devcontainer/ros_lyrical/ros_lyrical_devc.dockerfile . --build-arg USERNAME=$USER
# docker save ros:lyrical-devc | pzstd -c > ros-lyrical-devc_$(TZ=Asia/Shanghai date +%y%m%d).tzst

# devc镜像

FROM ros:lyrical-dev

ARG GA_FTP_SERVER=172.27.224.1
ARG DEBIAN_FRONTEND=noninteractive

ARG USERNAME=zs
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN set -x; \
    cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources.bak \
    && sed -i 's/archive.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/' /etc/apt/sources.list.d/ubuntu.sources \
    && sed -i 's/security.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/' /etc/apt/sources.list.d/ubuntu.sources \
    && sed -i 's/ports.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/' /etc/apt/sources.list.d/ubuntu.sources \
    && pip3 config set global.trusted-host https://pypi.tuna.tsinghua.edu.cn \
    && pip3 config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple \
    && if id -u $USER_UID >/dev/null 2>&1; then userdel -r $(id -un $USER_UID); fi \
    && if ! getent group $USER_GID >/dev/null 2>&1; then groupadd --gid $USER_GID $USERNAME; fi \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && usermod -aG dialout,video $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME \
    && rm -rf /tmp/* \
    && apt clean && rm -rf /var/lib/apt/lists/* \
    && rm /etc/apt/sources.list.d/ubuntu.sources \
    && mv /etc/apt/sources.list.d/ubuntu.sources.bak /etc/apt/sources.list.d/ubuntu.sources

# ********************************************************
# * Anything else you want to do like clean up goes here *
# ********************************************************

# [Optional] Set the default user. Omit if you want to keep the default as root.
USER $USERNAME
