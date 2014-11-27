# Ghost + syntax highlight + disqus in a Docker container
# VERSION 0.1

FROM clouderg/dsaubuntu14

MAINTAINER docker@clouderg.com

WORKDIR /data/ghost

ENV WORKDIR /data/ghost
ENV DEBIAN_FRONTEND noninteractive
# EDIT THIS
ENV DISQUS_SHORTNAME 'THIS IS MY SHORTNAME'

# Install dependencies for Node.js installation
RUN apt-get install -y --force-yes python g++ make software-properties-common curl dialog
RUN add-apt-repository ppa:chris-lea/node.js
RUN apt-get update
# Install unzip
RUN apt-get install -y --force-yes unzip
# Install nodejs & npm
RUN apt-get install -y --force-yes rlwrap
RUN apt-get install -y --force-yes nodejs-dev
RUN apt-get install -y --force-yes nodejs 
RUN apt-get install -y --force-yes npm

# Add Ghost zip to image
ADD ./ghost-0.3.3-pimped.zip /tmp/
# Unzip Ghost zip to WORKDIR
RUN unzip -uo /tmp/ghost-0.3.3-pimped.zip -d .
# Add custom config js to WORKDIR
ADD ./config.template.js /data/ghost/config.js
# Install Ghost with NPM
RUN npm install --production

RUN sed -i s/VAR_DISQUS_SHORTNAME/${DISQUS_SHORTNAME}/g content/themes/casper/post.hbs

# Expose port 2368
EXPOSE 2368
# Run Ghost
CMD ["npm","start"]
