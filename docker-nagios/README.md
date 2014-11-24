Nagios
======

Contains:

* Nagios Core
* Nagios Plugins
* Nagios NPRE (Remote Plugin Execution)
* Nagios NSCA (passive checks)
* Apache
* PHP

Processes are managed by supervisor, including cronjobs


Exports
-------

* Nginx on `80`
* Nagios NRPE on `5666`
* Nagios NSCA on `5667`
* `/etc/nagios`: configuration
* `/usr/local/nagios/var/`: nagios runtime

Variables
---------

* `NAGIOS_USER=nagiosadmin`: Web UI username
* `NAGIOS_PASS=nagiosadmin`: Web UI password

Constants in Dockerfile
-----------------------

* `NAGIOS_PHP_TIMEZONE=UTC`: Timezone to use with PHP
* `NAGIOS_TARBALL`: Nagios tarball URL
* `NAGIOS_PLUGINS_TARBALL`: Nagios Plugins tarball URL
* `NAGIOS_NRPE_TARBALL`: Nagios NRPE tarball URL
* `NAGIOS_NSCA_TARBALL`: Nagios NSCA tarball URL

Example
-------

Launch Nagios container:

    $ docker start nagios || docker run --rm -p 80:80 -p 5666:5666 -e NAGIOS_USER=nagios -e NAGIOS_PASS=nagios -ti kolypto/nagios

Enjoy! :)
