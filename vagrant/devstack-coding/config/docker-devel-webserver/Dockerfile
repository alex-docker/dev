FROM clouderg/dsubuntu14
MAINTAINER Clouderg Admin <admin@clouderg.com>


ENTRYPOINT cp /app/config/nginx-devel/nginx.conf /etc/nginx/nginx.conf && \
    ln -s /app/config/nginx-devel/frontend.conf /etc/nginx/sites-enabled/ && \
    rm /etc/nginx/sites-enabled/default && \
    /usr/sbin/nginx -c /etc/nginx/nginx.conf
