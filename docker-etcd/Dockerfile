FROM clouderg/dsubuntu14

RUN apt-get install -y wget ca-certificates

RUN wget https://github.com/coreos/etcd/releases/download/v0.2.0-rc1/etcd-v0.2.0-rc1-Linux-x86_64.tar.gz
RUN tar -zxvf etcd-v0.2.0-rc1-Linux-x86_64.tar.gz && mv etcd-v0.2.0-rc1-Linux-x86_64 /opt/etcd
RUN ln -s /opt/etcd/etcd /usr/local/bin/etcd

ENTRYPOINT ["/usr/local/bin/etcd"]
EXPOSE 4001
EXPOSE 7001
CMD ["-c","0.0.0.0:4001", "-s", "0.0.0.0:7001"]
