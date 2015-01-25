FROM clouderg/dsubuntu14
MAINTAINER Clouderg Admin <admin@clouderg.com>

ENTRYPOINT cat /app/config/ssh_default.pub >> /root/.ssh/authorized_keys && \
    /usr/sbin/sshd -D -e