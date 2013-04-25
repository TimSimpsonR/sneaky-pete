rm -rf /var/lib/vz/private/$1/agent/*
mkdir -p /var/lib/vz/private/$1/agent
sudo cp -rf * /var/lib/vz/private/$1/agent/
