### Docker etcd ###

Builds a docker container with the etcd service discovery tool<br />
Once runing the container will be listening on 4001 (etcd comms) and 7001 (raft protocol comms)

Pass arguments into the command when starting the container to change etcd behavior

For intance:  `docker run -i -t cpuguy83/etcd --help`

