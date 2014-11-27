#! /usr/bin/env bash
set -eu # exit on error or undefined variable

# Variables
export NAGIOS_USER=${NAGIOS_USER:-"nagiosadmin"}
export NAGIOS_PASS=${NAGIOS_PASS:-"nagiosadmin"}

# Templating
htpasswd -cb /etc/nagios/htpasswd.users $NAGIOS_USER $NAGIOS_PASS

# Logging
rm -f /usr/local/nagios/var/nagios.lock /var/run/apache2/apache2.pid
LOGFILES=$(echo /usr/local/nagios/var/nagios.log /var/log/{supervisord,apache2/error}.log)
( umask 0 && truncate -s0 $LOGFILES ) && tail --pid $$ -n0 -F $LOGFILES &

# Launch
exec /usr/bin/supervisord -n
