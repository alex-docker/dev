#!/bin/bash
REGISTRY_NAME=${REGISTRY_NAME:-Docker Registry}
CACHE_REDIS_PASSWORD=${REDIS_PASSWORD:-docker}
CACHE_LRU_REDIS_PASSWORD=${REDIS_PASSWORD:-docker}
PASSWORD_FILE=${USER_DB:-/etc/registry.users}
ETCD_ENDPOINT=${ETCD_ENDPOINT:-127.0.0.1:4001}
SERVER_NAME=${SERVER_NAME:-\"\"}
export CACHE_REDIS_PASSWORD
export CACHE_LRU_REDIS_PASSWORD

# nginx config
cat << EOF > /etc/nginx/nginx.conf
  daemon off;
  user root;
  events {
    worker_connections 2048;
  }

  http {

  include /etc/nginx/mime.types;
  upstream registry {
    server localhost:5000;
  }

  server {
    listen 80;
    server_name $SERVER_NAME;

    proxy_set_header Host \$http_host;   # required for docker client's sake
    proxy_set_header X-Real-IP \$remote_addr; # pass on real client's IP
    proxy_set_header Authorization  "";

    client_max_body_size 0; # disable any limits to avoid HTTP 413 for large image uploads

    # required to avoid HTTP 411: see Issue #1486 (https://github.com/dotcloud/docker/issues/1486)
    chunked_transfer_encoding on;

    location /v1/_ping {
      auth_basic off;
      proxy_pass http://registry;
    }

    location /v1/users {
      auth_basic off;
      proxy_pass http://registry;
    }

    location / {
      auth_basic "$REGISTRY_NAME";
      auth_basic_user_file $PASSWORD_FILE;

      if (\$http_x_forwarded_proto != "https") {
        rewrite ^(.*)\$ https://\$host\$uri permanent;
      }
      add_header Strict-Transport-Security "max-age=31536000; includeSubDomains;";
      proxy_pass http://registry;
    }

  }
}
EOF

# redis config
cat << EOF >> /etc/redis.conf
daemonize no
requirepass $CACHE_REDIS_PASSWORD
maxmemory 2mb
maxmemory-policy allkeys-lru
EOF

# supervisor config
cat << EOF > /etc/supervisor/supervisor.conf
[supervisord]
nodaemon=false

[unix_http_server]
file=/var/run/supervisor.sock

[rpcinterface:supervisor]
supervisor.rpcinterface_factory = supervisor.rpcinterface:make_main_rpcinterface

[supervisorctl]
serverurl=unix:///var/run//supervisor.sock

[program:redis]
priority=05
user=root
command=/usr/bin/redis-server /etc/redis.conf
directory=/var/lib/redis
autostart=true
autorestart=true
stopsignal=QUIT

[program:registry]
priority=10
user=root
command=docker-registry
autostart=true
autorestart=true
stopsignal=QUIT

[program:nginx]
priority=50
user=root
command=/usr/sbin/nginx
directory=/tmp
autostart=true
autorestart=true

[program:confd]
priority=50
user=root
command=confd -interval 60 -node $ETCD_ENDPOINT -verbose

directory=/tmp
autostart=true
autorestart=true
EOF

# run supervisor
supervisord -c /etc/supervisor/supervisor.conf -n
