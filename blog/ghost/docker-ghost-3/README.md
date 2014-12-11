ghost-docker
============

#Docker config for a ghost setup with a few extras

- Syntax highlighting with [github flavoured markdown](https://help.github.com/articles/github-flavored-markdown#syntax-highlighting) added
- Disqus added to post pages

Extra: nginx vhost config template with performance optimizations

###Edit the simplified config.template.js and change the following:

- url - set it to the final url for your shiny new Ghost blog
- uncomment and configure the mail settings if you want Ghost to be able to send mail

###Also edit the Dockerfile and set your Disqus shortname (DISQUS_SHORTNAME)