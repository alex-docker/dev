# Docker Registry (private)
This uses the `registry` as a base and adds basic auth via Nginx. Note that you should provide your own SSL

# Usage
To run a private registry,

`docker run -i -t colegleason/docker-private-registry`

# Management
To add users, you must add the htpasswd hash to etcd like this:

`curl -L -v -X PUT http://$ETCD_ENDPOINT/v2/keys/registry/users/username -d
value=$(htpasswd -nb username password)`

# Environment
* `REGISTRY_NAME`: Custom name for registry (used when prompted for auth)

# Ports
* 80

# Running on S3
To run with Amazon S3 as the backing store, you will need the following environment variables:

* `AWS_ACCESS_KEY_ID`: Your AWS Access Key ID (make sure it has S3 access)
* `AWS_SECRET_KEY`: Your AWS Secret Key
* `S3_BUCKET`: Your S3 bucket to store images
* `SETTINGS_FLAVOR`: This must be set to `prod`
