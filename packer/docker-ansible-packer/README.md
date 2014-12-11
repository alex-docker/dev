docker-ansible-packer
=====================

[packer](http://packer.io) setup for creating ubuntu or centos based [docker](http://docker.io) containers using [ansible](http://ansible.com)

The provided base ubuntu image `egghead/ubuntu-ansible-1.6` comes with ansible 1.6.1 pre-installed.  The provided centos image `slapula/centos6-ansible` comes with ansible 1.6.6.

If you have `packer` & `docker` installed you can use this project to automate creation of new docker images using ansible.

Note: *you don't need ansible locally, since that all lives in the container*

Check out the [usage.json](https://github.com/eggsby/docker-ansible-packer/blob/master/usage.json) file to get started on your own packer images.

    packer build usage.json
    ...
    ==> docker: Provisioning with Ansible...
        docker: Creating Ansible staging directory...
        docker: Creating directory: /tmp/packer-provisioner-ansible-local
        docker: Uploading main Playbook file...
        docker: Executing Ansible: ansible-playbook /tmp/packer-provisioner-ansible-local/provision.yaml -c local -i "127.0.0.1,"
        docker:
        docker: PLAY [all] ********************************************************************
        docker:
        docker: GATHERING FACTS ***************************************************************
        docker: ok: [127.0.0.1]
        docker:
        docker: TASK: [debug msg="Hello, docker!"] ********************************************
        docker: ok: [127.0.0.1] => {
        docker: "msg": "Hello, docker!"
        docker: }
        docker:
        docker: TASK: [shell whoami] **********************************************************
        docker: changed: [127.0.0.1]
        docker:
        docker: PLAY RECAP ********************************************************************
        docker: 127.0.0.1                  : ok=3    changed=1    unreachable=0    failed=0
        docker:
    ==> docker: Exporting the container
    ==> docker: Killing the container: aa2db1214e4134ef556aa5b795542aec74cd1cf6e759a45bc6b063aac9307501
    Build 'docker' finished.
