FROM clouderg/dsubuntu14
MAINTAINER docker@clouderg.com
 
# Upgrade
RUN apt-get update && apt-get upgrade -y && apt-get dist-upgrade -y
 
ENV DEBIAN_FRONTEND noninteractive
RUN echo "deb http://repo.percona.com/apt precise main" >> /etc/apt/sources.list.d/percona.list
RUN echo "deb-src http://repo.percona.com/apt precise main" >> /etc/apt/sources.list.d/percona.list
RUN apt-key adv --keyserver keys.gnupg.net --recv-keys 1C4CBDCDCD2EFD2A
 
RUN apt-get update
RUN apt-get install -y percona-xtradb-cluster-56 qpress xtrabackup
 
RUN apt-get install -y python-software-properties vim wget curl netcat

# Add VOLUMEs to allow backup of config and databases
VOLUME  ["/etc/mysql", "/var/lib/mysql", "/var/log/mysql", "/devops"]
