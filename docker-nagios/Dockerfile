# Using LTS ubuntu
FROM clouderg/dsubuntu14
MAINTAINER docker@clouderg.com

# Packages: update & install
ENV DEBCONF_FRONTEND noninteractive
RUN sed -i 's~.* universe$~\0 multiverse~' /etc/apt/sources.list
RUN apt-get update -qq
RUN apt-get install -qq -y --no-install-recommends python-pip supervisor build-essential
RUN apt-get install -qq -y --no-install-recommends apache2 libapache2-mod-php5 snmp-mibs-downloader
RUN apt-get install -qq -y --no-install-recommends apache2-utils wget curl samba-common samba-common-bin smbclient snmp whois traceroute
RUN apt-get install -qq -y --no-install-recommends libgd2-xpm-dev libssl-dev libnet-snmp-perl libperl-dev libpq5 libradius1 libsensors4 libsnmp-base libtalloc2 libtdb1 libwbclient0 libmysqlclient15-dev
RUN pip install j2cli



# Const
ENV NAGIOS_PHP_TIMEZONE UTC
ENV NAGIOS_TARBALL http://prdownloads.sourceforge.net/sourceforge/nagios/nagios-4.0.7.tar.gz
ENV NAGIOS_PLUGINS_TARBALL http://www.nagios-plugins.org/download/nagios-plugins-2.0.3.tar.gz
ENV NAGIOS_NRPE_TARBALL http://kent.dl.sourceforge.net/project/nagios/nrpe-2.x/nrpe-2.15/nrpe-2.15.tar.gz
ENV NAGIOS_NSCA_TARBALL http://prdownloads.sourceforge.net/sourceforge/nagios/nsca-2.7.2.tar.gz
# Using manual: https://raymii.org/s/tutorials/Nagios_Core_4_Installation_on_Ubuntu_12.04.html



# Users
RUN groupadd -g 3000 nagios
RUN useradd -u 3000 -g nagios -G www-data -m -s /bin/bash nagios

# Install nagios
RUN mkdir -p /usr/local/src/nagios4
RUN cd /usr/local/src/nagios4 && wget $NAGIOS_TARBALL -O- | tar -zxp --strip-components 1 && ./configure --prefix=/usr/local/nagios --with-nagios-user=nagios --with-nagios-group=nagios --with-command-user=nagios --with-command-group=nagios && make all && make install install-init install-config install-commandmode
RUN ln -s /usr/local/nagios/etc /etc/nagios

# Install nagios plugins
RUN mkdir -p /usr/local/src/nagios4-plugins
RUN cd /usr/local/src/nagios4-plugins && wget $NAGIOS_PLUGINS_TARBALL -O- | tar -zxp --strip-components 1 && ./configure --with-nagios-user=nagios --with-nagios-group=nagios --with-openssl=/usr/bin/openssl --enable-perl-modules --enable-libtap && make && make install

# Install nagios NRPE
RUN mkdir -p /usr/local/src/nagios4-nrpe
RUN cd /usr/local/src/nagios4-nrpe && wget $NAGIOS_NRPE_TARBALL -O- | tar -zxp --strip-components 1 && ./configure --with-ssl=/usr/bin/openssl --with-ssl-lib=/usr/lib/x86_64-linux-gnu && make all && make install install-plugin install-daemon install-daemon-config

# Install nagios NSCA
RUN mkdir -p /usr/local/src/nagios4-nsca
RUN cd /usr/local/src/nagios4-nsca && wget $NAGIOS_NSCA_TARBALL -O- | tar -zxp --strip-components 1 && ./configure --with-nsca-user=nagios --with-nsca-grp=nagios && make all
RUN cd /usr/local/src/nagios4-nsca && cp sample-config/nsca.cfg sample-config/send_nsca.cfg /etc/nagios/ && cp src/send_nsca src/nsca /usr/local/bin/

# Clean-up
RUN rm -rf /usr/local/src/nagios*



# Add files
ADD conf /root/conf

# Configure: php
RUN j2 /root/conf/php.ini > /etc/php5/mods-available/custom.ini
RUN php5enmod custom

# Configure: apache
ADD conf/apache2-site.conf /etc/apache2/sites-available/nagios.conf
RUN a2dissite 000-default
RUN a2ensite nagios
RUN a2enmod cgi

# Configure: supervisor
ADD conf/supervisor-all.conf /etc/supervisor/conf.d/




# Configure: nagios
RUN bash -c 'mkdir -p /etc/nagios/conf.d /etc/nagios/conf.d/{hosts,services,timeperiods,templates,hostgroups,servicegroups,contacts}'

RUN echo 'cfg_dir=/etc/nagios/conf.d/' >> /etc/nagios/nagios.cfg
RUN sed -i 's~^url_html_path=.*~url_html_path=/~' /etc/nagios/cgi.cfg




# Runner
ADD run.sh /root/run.sh



# Declare
VOLUME ["/etc/nagios/", "/usr/local/nagios/var/", "/var/log", "/devops"]
EXPOSE 80
EXPOSE 5666
EXPOSE 5667

CMD ["/root/run.sh"]

