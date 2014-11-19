docker build -t clouderg/dubuntu14 .

# to use

#docker run --privileged -t -i clouderg/dubuntu14			#log to stdout
#docker run --privileged -t -i -e LOG=file clouderg/dubuntu14		#log to file /var/log/docker.log
#docker run --privileged -d -p 4444 -e PORT=4444 clouderg/dubuntu14	#Run Docker-in-Docker and expose the inside Docker to the outside world
